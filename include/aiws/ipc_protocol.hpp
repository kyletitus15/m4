#pragma once

#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace aiws::ipc {

using Clock = std::chrono::steady_clock;

inline constexpr unsigned char kMagic[4] = {'A', 'I', 'W', 'S'};
inline constexpr unsigned char kVersion = 1;
inline constexpr unsigned char kOpAdd = 1;
inline constexpr std::size_t kRequestHeaderSize = 10;  
inline constexpr std::size_t kReplyHeaderSize = 5;   
inline constexpr std::uint32_t kMaxPayload = 65536;
inline constexpr unsigned char kStatusOk = 0;
inline constexpr unsigned char kStatusRejected = 1;

#ifdef MSG_NOSIGNAL
inline constexpr int kSendFlags = MSG_NOSIGNAL;
#else
inline constexpr int kSendFlags = 0;
#endif

inline std::string default_socket_path() {
    const char* user = std::getenv("USER");
    if (user != nullptr && *user != '\0') { return std::string("/tmp/aiws-") + user + ".sock"; }
    return "/tmp/aiws-" + std::to_string(static_cast<unsigned long>(::getuid())) + ".sock";
}

inline void put_u32(unsigned char* out, std::uint32_t v) {
    out[0] = static_cast<unsigned char>(v >> 24);
    out[1] = static_cast<unsigned char>(v >> 16);
    out[2] = static_cast<unsigned char>(v >> 8);
    out[3] = static_cast<unsigned char>(v);
}

inline std::uint32_t get_u32(const unsigned char* in) {
    return (static_cast<std::uint32_t>(in[0]) << 24) | (static_cast<std::uint32_t>(in[1]) << 16) | (static_cast<std::uint32_t>(in[2]) << 8) | static_cast<std::uint32_t>(in[3]);
}

// Both helpers wake every 50 ms so they can notice `stop` and the deadline.
inline bool write_all(int fd, const void* data, std::size_t n, Clock::time_point deadline,
                      const std::atomic<bool>* stop = nullptr) {
    const char* p = static_cast<const char*>(data);
    while (n > 0) {
        if (stop != nullptr && stop->load()) { return false; }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if (left <= 0) { return false; }
        pollfd pfd{fd, POLLOUT, 0};
        const int r = ::poll(&pfd, 1, static_cast<int>(std::min<long long>(left, 50)));
        if (r < 0) { if (errno == EINTR) { continue; } return false; }
        if (r == 0) { continue; }
        const ssize_t w = ::send(fd, p, n, kSendFlags);
        if (w < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) { continue; }
            return false;
        }
        p += w;
        n -= static_cast<std::size_t>(w);
    }
    return true;
}

inline bool read_exact(int fd, void* data, std::size_t n, Clock::time_point deadline,
                       const std::atomic<bool>* stop = nullptr) {
    char* p = static_cast<char*>(data);
    while (n > 0) {
        if (stop != nullptr && stop->load()) { return false; }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if (left <= 0) { return false; }
        pollfd pfd{fd, POLLIN, 0};
        const int r = ::poll(&pfd, 1, static_cast<int>(std::min<long long>(left, 50)));
        if (r < 0) { if (errno == EINTR) { continue; } return false; }
        if (r == 0) { continue; }
        const ssize_t got = ::recv(fd, p, n, 0);
        if (got == 0) { return false; }  // peer closed early
        if (got < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) { continue; }
            return false;
        }
        p += got;
        n -= static_cast<std::size_t>(got);
    }
    return true;
}
