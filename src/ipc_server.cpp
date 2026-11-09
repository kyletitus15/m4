#include "aiws/ipc_protocol.hpp"
#include "aiws/ipc_server.hpp"
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <QObject>
#include <QMetaObject>
#include <QByteArray>
#include <cerrno>
#include <cstring>
#include <system_error>
#include <vector>

namespace aiws::ipc {
    
    namespace {

    constexpr int kRequestSeconds = 4;

    void set_cloexec(int fd) {
        const int flags = ::fcntl(fd, F_GETFD, 0);
        if (flags >= 0) { ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC); }
    }

    std::string errno_text(int e) { return std::system_category().message(e); }

    bool check_readable(const std::string& path, std::string& why) {
        if (path.find('\0') != std::string::npos) { why = "path contains a NUL byte"; return false; }
        if (path.front() != '/') { why = "path must be absolute"; return false; }
        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);  // NONBLOCK: a FIFO can't hang us
        if (fd < 0) { why = path + ": " + errno_text(errno); return false; }
        struct stat st {};
        const bool regular = (::fstat(fd, &st) == 0) && S_ISREG(st.st_mode);
        ::close(fd);
        if (!regular) { why = path + ": not a regular file"; return false; }
        return true;
    }

    bool send_reply(int fd, unsigned char status, std::string text, Clock::time_point deadline) {
        if (text.size() > kMaxPayload) { text.resize(kMaxPayload); }
        std::vector<unsigned char> out(kReplyHeaderSize + text.size());
        out[0] = status;
        put_u32(out.data() + 1, static_cast<std::uint32_t>(text.size()));
        std::memcpy(out.data() + kReplyHeaderSize, text.data(), text.size());
        return write_all(fd, out.data(), out.size(), deadline);
    }

    } 

IpcServer::IpcServer(QObject* target, std::string socket_path)
    : target_(target), path_(std::move(socket_path)) {}

IpcServer::~IpcServer() { stop(); }

bool IpcServer::start(std::string& error) {
    if (thread_.joinable()) { error = "already started"; return false; }

    sockaddr_un addr {};
    if (path_.size() >= sizeof(addr.sun_path)) { error = "socket path too long"; return false; }
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path_.c_str(), path_.size() + 1);

    const int probe = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (probe >= 0) {
        const bool alive = ::connect(probe, reinterpret_cast<sockaddr*>(&addr), sizeof addr) == 0;
        ::close(probe);
        if (alive) { error = "another application is already listening on " + path_; return false; }
    }
    struct stat st {};
    if (::lstat(path_.c_str(), &st) == 0) {
        if (!S_ISSOCK(st.st_mode)) { error = path_ + " exists and is not a socket"; return false; }
        ::unlink(path_.c_str());
    }

    listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) { error = errno_text(errno); return false; }
    set_cloexec(listen_fd_);
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        error = "bind: " + errno_text(errno);
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    bound_ = true;
    ::chmod(path_.c_str(), S_IRUSR | S_IWUSR); 
    if (::listen(listen_fd_, 16) != 0) {
        error = "listen: " + errno_text(errno);
        stop();
        return false;
    }

    stop_.store(false);
    try {
        thread_ = std::thread(&IpcServer::run, this);
    } catch (const std::exception& e) {
        error = std::string("could not start thread: ") + e.what();
        stop();
        return false;
    }
    return true;
}

void IpcServer::stop() noexcept {
    stop_.store(true);
    try {
        if (thread_.joinable()) { thread_.join(); }
    } catch (...) {
    }
    if (listen_fd_ >= 0) { ::close(listen_fd_); listen_fd_ = -1; }
    if (bound_) { ::unlink(path_.c_str()); bound_ = false; }
}

void IpcServer::run() {
    while (!stop_.load()) {
        pollfd pfd{listen_fd_, POLLIN, 0};
        const int r = ::poll(&pfd, 1, 50);  
        if (r <= 0) { continue; }
        const int fd = ::accept(listen_fd_, nullptr, nullptr);
        if (fd < 0) { continue; }
        set_cloexec(fd);
        serve_one(fd);
        ::close(fd);
    }
}

void IpcServer::serve_one(int fd) {
    const auto deadline = Clock::now() + std::chrono::seconds(kRequestSeconds);

    unsigned char header[kRequestHeaderSize];
    if (!read_exact(fd, header, sizeof header, deadline, &stop_)) { return; }
    if (std::memcmp(header, kMagic, 4) != 0 || header[4] != kVersion) {
        send_reply(fd, kStatusRejected, "bad request header", deadline);
        return;
    }
    if (header[5] != kOpAdd) { send_reply(fd, kStatusRejected, "unknown command", deadline); return; }
    const std::uint32_t n = get_u32(header + 6);
    if (n == 0 || n > kMaxPayload) { send_reply(fd, kStatusRejected, "bad path length", deadline); return; }

    std::string path(n, '\0');
    if (!read_exact(fd, path.data(), n, deadline, &stop_)) { return; }

    std::string why;
    if (!check_readable(path, why)) { send_reply(fd, kStatusRejected, why, deadline); return; }

    QMetaObject::invokeMethod(target_, "remote_add", Qt::QueuedConnection,
                              Q_ARG(QByteArray, QByteArray(path.data(), static_cast<qsizetype>(n))));
    send_reply(fd, kStatusOk, "queued", deadline);
}

} 