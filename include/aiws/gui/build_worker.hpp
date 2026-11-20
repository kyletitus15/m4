#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "aiws/processing_core.hpp"
#include "aiws/workspace.hpp"

class QObject;

namespace aiws::gui {

struct BuildResult {
    std::uint64_t id{0};
    std::unique_ptr<ProcessingCore> core;
    std::string error;
    bool cancelled{false};
};

using BuildFunction = void (*)(const Workspace& snapshot, ProcessingCore& core);

// One build thread for the whole run of the application (R1).
// All public functions are called from the GUI thread only.
class BuildWorker {
public:
    explicit BuildWorker(QObject* notify_target);
    ~BuildWorker();
    BuildWorker(const BuildWorker&) = delete;
    BuildWorker& operator=(const BuildWorker&) = delete;

    std::uint64_t submit(Workspace snapshot, int delay_ms, BuildFunction function);
    void cancel();                          // running or queued build becomes unwanted
    std::optional<BuildResult> take_result();
    void shutdown();                        // idempotent: stop and join

private:
    struct Request {
        std::uint64_t id{0};
        int delay_ms{0};
        BuildFunction function{nullptr};
        Workspace snapshot;
    };

    void run();                             // worker thread body
    BuildResult execute(const Request& request);
    bool still_wanted(std::uint64_t id) const noexcept;
    bool wait_build_time(std::uint64_t id, long long total_ms) const;

    QObject* target_;
    std::uint64_t next_id_{0};              // GUI thread only
    std::atomic<std::uint64_t> wanted_id_{0};

    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Request> pending_;        // guarded by mutex_
    std::optional<BuildResult> finished_;   // guarded by mutex_
    bool stop_{false};                      // guarded by mutex_

    std::thread thread_;                    // keep LAST: starts after the members above exist
};

}  // namespace aiws::gui