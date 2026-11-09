#include "aiws/ipc_protocol.hpp"
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

using namespace aiws::ipc;

int usage() {
    std::cerr << "usage: aiws_send [--socket PATH] add FILE\n";
    return 2;
}

int open_connection(const std::string& path, std::string& error) {
    sockaddr_un addr {};
    if (path.size() >= sizeof(addr.sun_path)) { error = "socket path too long"; return -1; }
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { error = std::system_category().message(errno); return -1; }
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

    int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
    int err = (rc == 0) ? 0 : errno;
    if (rc < 0 && (err == EINPROGRESS || err == EAGAIN)) {
        pollfd pfd{fd, POLLOUT, 0};
        int pr = 0;
        do { pr = ::poll(&pfd, 1, 2000); } while (pr < 0 && errno == EINTR);
        if (pr == 0) { ::close(fd); error = "timed out connecting"; return -1; }
        socklen_t len = sizeof err;
        ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
    }
    if (err != 0) {
        ::close(fd);
        error = std::system_category().message(err);
        return -1;
    }
    return fd;
}

}
int main(int argc, char* argv[]) {
    std::signal(SIGPIPE, SIG_IGN);

    std::string socket_path = default_socket_path();
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--socket") {
            if (i + 1 >= argc) { return usage(); }
            socket_path = argv[++i];
        } else {
            positional.push_back(arg);
        }
    }
    if (positional.size() != 2 || positional[0] != "add") { return usage(); }

    std::string file; 
    try {
        std::filesystem::path p(positional[1]);
        if (p.is_relative()) { p = std::filesystem::current_path() / p; }
        file = p.string();
    } catch (const std::exception& e) {
        std::cerr << "aiws_send: " << e.what() << "\n";
        return 2;
    }
    if (file.empty() || file.size() > kMaxPayload) {
        std::cerr << "aiws_send: path is empty or too long\n";
        return 1;
    }

    std::string error;
    const int fd = open_connection(socket_path, error);
    if (fd < 0) {
        std::cerr << "aiws_send: cannot reach an application at " << socket_path << ": " << error << "\n";
        return 2;
    }

    const auto deadline = Clock::now() + std::chrono::seconds(5);  
    std::vector<unsigned char> request(kRequestHeaderSize + file.size());
    std::memcpy(request.data(), kMagic, 4);
    request[4] = kVersion;
    request[5] = kOpAdd;
    put_u32(request.data() + 6, static_cast<std::uint32_t>(file.size()));
    std::memcpy(request.data() + kRequestHeaderSize, file.data(), file.size());

    unsigned char header[kReplyHeaderSize];
    if (!write_all(fd, request.data(), request.size(), deadline) ||
        !read_exact(fd, header, sizeof header, deadline)) {
        std::cerr << "aiws_send: communication with the application failed\n";
        ::close(fd);
        return 2;
    }
    const std::uint32_t n = get_u32(header + 1);
    std::string text(n <= kMaxPayload ? n : 0, '\0');
    if (n > kMaxPayload || !read_exact(fd, text.data(), text.size(), deadline)) {
        std::cerr << "aiws_send: malformed reply\n";
        ::close(fd);
        return 2;
    }
    ::close(fd);

    if (header[0] == kStatusOk) { return 0; }
    if (header[0] == kStatusRejected) {
        std::cerr << "aiws_send: rejected: " << text << "\n";
        return 1;
    }
    std::cerr << "aiws_send: unknown reply status\n";
    return 2;
}