#pragma once

#include <QString>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "aiws/gui/build_worker.hpp"

namespace aiws::gui {

// What the states may ask of the application. Implemented by MainWindow.
class CorpusContext {
public:
    virtual ~CorpusContext() = default;
    virtual bool workspace_empty() const = 0;
    virtual std::uint64_t start_build() = 0;     // snapshot -> worker, returns job id
    virtual void cancel_build() = 0;
    virtual void install_corpus(ProcessingCore&& core) = 0;
    virtual std::size_t chunk_count() const = 0;
    virtual void clear_outputs() = 0;
    virtual void show_corpus_label(const QString& text) = 0;
    virtual void show_status(const QString& text) = 0;
};

// State pattern: each event returns the next state, or nullptr to stay.
// Defaults ignore the event. Used on the GUI thread only.
class CorpusState {
public:
    virtual ~CorpusState() = default;
    virtual void on_enter(CorpusContext&) {}
    virtual std::unique_ptr<CorpusState> on_build_requested(CorpusContext&) { return nullptr; }
    virtual std::unique_ptr<CorpusState> on_workspace_changed(CorpusContext&) { return nullptr; }
    virtual std::unique_ptr<CorpusState> on_reset(CorpusContext&);   // default: NotBuilt
    virtual std::unique_ptr<CorpusState> on_build_finished(CorpusContext&, BuildResult&) { return nullptr; }
    virtual bool can_build() const { return true; }
    virtual bool corpus_current() const { return false; }
};

class NotBuiltState final : public CorpusState {
public:
    void on_enter(CorpusContext& ctx) override;
    std::unique_ptr<CorpusState> on_build_requested(CorpusContext& ctx) override;
};

class BuildingState final : public CorpusState {
public:
    explicit BuildingState(std::uint64_t job_id) : job_id_(job_id) {}
    void on_enter(CorpusContext& ctx) override;
    std::unique_ptr<CorpusState> on_workspace_changed(CorpusContext& ctx) override;
    std::unique_ptr<CorpusState> on_reset(CorpusContext& ctx) override;
    std::unique_ptr<CorpusState> on_build_finished(CorpusContext& ctx, BuildResult& result) override;
    bool can_build() const override { return false; }
private:
    std::uint64_t job_id_;
};

class CurrentState final : public CorpusState {
public:
    void on_enter(CorpusContext& ctx) override;
    std::unique_ptr<CorpusState> on_build_requested(CorpusContext& ctx) override;
    std::unique_ptr<CorpusState> on_workspace_changed(CorpusContext& ctx) override;
    bool corpus_current() const override { return true; }
};

class StaleState final : public CorpusState {
public:
    StaleState(QString label, QString message)
        : label_(std::move(label)), message_(std::move(message)) {}
    void on_enter(CorpusContext& ctx) override;
    std::unique_ptr<CorpusState> on_build_requested(CorpusContext& ctx) override;
private:
    QString label_;
    QString message_;
};

}  // namespace aiws::gui