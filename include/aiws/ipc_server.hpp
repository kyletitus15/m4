#pragma once

#include <atomic>
#include <string>
#include <thread>

class QObject;

namespace aiws::ipc {

class IpcServer {
public:
    IpcServer(QObject* target, std::string socket_path);
    ~IpcServer();
    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    bool start(std::string& error); 
    void stop() noexcept;           
    const std::string& socket_path() const noexcept { return path_; }

private:
    void run();
    void serve_one(int fd);

    QObject* target_;   
    std::string path_;  
    int listen_fd_{-1}; 
    bool bound_{false};
    std::atomic<bool> stop_{false};  // GUI side writes, server thread reads
    std::thread thread_;
};

}  // namespace aiws::ipc