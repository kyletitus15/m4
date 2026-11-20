#include "aiws/gui/build_worker.hpp"

#include <QMetaObject>
#include <QObject>

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>

namespace aiws::gui {

namespace { constexpr long long kSleepMs = 10; }

BuildWorker::BuildWorker(QObject* notify_target)
    : target_(notify_target) {
    thread_ = std::thread(&BuildWorker::run, this);
}

BuildWorker::~BuildWorker() { shutdown(); }

std::uint64_t BuildWorker::submit(Workspace snapshot, int delay_ms, BuildFunction function) {
    const std::uint64_t id = ++next_id_;
    wanted_id_.store(id);                   // any older build is now unwanted
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = Request{id, delay_ms, function, std::move(snapshot)};
    }
    cv_.notify_one();
    return id;
}

void BuildWorker::cancel() {
    wanted_id_.store(++next_id_);           // matches no request
}

std::optional<BuildResult> BuildWorker::take_result() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::optional<BuildResult> out = std::move(finished_);
    finished_.reset();
    return out;
}

void BuildWorker::shutdown() {
    if (!thread_.joinable()) { return; }
    wanted_id_.store(++next_id_);           // stops a running build at its next check
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();                       // wakes an idle worker
    thread_.join();
}

bool BuildWorker::still_wanted(std::uint64_t id) const noexcept {
    return wanted_id_.load() == id;
}

bool BuildWorker::wait_build_time(std::uint64_t id, long long total_ms) const {
    long long left = total_ms;
    while (left > 0) {
        if (!still_wanted(id)) { return false; }
        const long long step = std::min(left, kSleepMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(step));   // simulated build time only
        left -= step;
    }
    return still_wanted(id);
}

BuildResult BuildWorker::execute(const Request& request) {
    BuildResult res;
    res.id = request.id;
    try {
        const long long total = static_cast<long long>(request.delay_ms) *
                                static_cast<long long>(request.snapshot.documents().size());
        if (!wait_build_time(request.id, total)) {
            res.cancelled = true;           // outdated or cancelled: never built
            return res;
        }
        auto core = std::make_unique<ProcessingCore>();
        request.function(request.snapshot, *core);
        if (!still_wanted(request.id)) {
            res.cancelled = true;
            return res;
        }
        res.core = std::move(core);
    } catch (const std::exception& e) {
        res.error = e.what();
        if (res.error.empty()) { res.error = "build failed"; }
    } catch (...) {
        res.error = "unknown error";
    }
    return res;
}

void BuildWorker::run() {
    for (;;) {
        Request request;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || pending_.has_value(); });
            if (stop_) { return; }
            request = std::move(*pending_);
            pending_.reset();
        }
        BuildResult res = execute(request);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            finished_ = std::move(res);
        }
        QMetaObject::invokeMethod(target_, "build_finished", Qt::QueuedConnection);
    }
}

}  // namespace aiws::gui