#include "aiws/gui/corpus_state.hpp"

namespace aiws::gui {

namespace {

// Shared by the states that may start a build.
std::unique_ptr<CorpusState> start_new_build(CorpusContext& ctx) {
    if (ctx.workspace_empty()) { return nullptr; }
    ctx.clear_outputs();
    return std::make_unique<BuildingState>(ctx.start_build());
}

std::unique_ptr<CorpusState> out_of_date(const QString& message) {
    return std::make_unique<StaleState>("Corpus: Out of date, rebuild needed", message);
}

}  // namespace

std::unique_ptr<CorpusState> CorpusState::on_reset(CorpusContext&) {
    return std::make_unique<NotBuiltState>();
}

// ---- NotBuilt ----
void NotBuiltState::on_enter(CorpusContext& ctx) {
    ctx.show_corpus_label("Corpus: Not built");
}
std::unique_ptr<CorpusState> NotBuiltState::on_build_requested(CorpusContext& ctx) {
    return start_new_build(ctx);
}

// ---- Building ----
void BuildingState::on_enter(CorpusContext& ctx) {
    ctx.show_corpus_label("Corpus: Building...");
    ctx.show_status("Building corpus...");
}
std::unique_ptr<CorpusState> BuildingState::on_workspace_changed(CorpusContext& ctx) {
    ctx.cancel_build();
    return out_of_date("Build cancelled: workspace changed");
}
std::unique_ptr<CorpusState> BuildingState::on_reset(CorpusContext& ctx) {
    ctx.cancel_build();
    return std::make_unique<NotBuiltState>();
}
std::unique_ptr<CorpusState> BuildingState::on_build_finished(CorpusContext& ctx, BuildResult& result) {
    if (result.id != job_id_) { return nullptr; }   // from an earlier, cancelled build
    if (!result.error.empty() || !result.core) {
        const QString why = result.error.empty() ? QString("cancelled")
                                                 : QString::fromStdString(result.error);
        return std::make_unique<StaleState>("Corpus: Build failed", "Corpus build failed: " + why);
    }
    ctx.install_corpus(std::move(*result.core));
    return std::make_unique<CurrentState>();
}

// ---- Current ----
void CurrentState::on_enter(CorpusContext& ctx) {
    ctx.show_corpus_label(QString("Corpus: %1 chunks").arg(static_cast<qulonglong>(ctx.chunk_count())));
    ctx.show_status("Corpus built");
}
std::unique_ptr<CorpusState> CurrentState::on_build_requested(CorpusContext& ctx) {
    return start_new_build(ctx);
}
std::unique_ptr<CorpusState> CurrentState::on_workspace_changed(CorpusContext& ctx) {
    ctx.clear_outputs();
    return out_of_date("Corpus out of date");
}

// ---- Stale ----
void StaleState::on_enter(CorpusContext& ctx) {
    ctx.show_corpus_label(label_);
    ctx.show_status(message_);
}
std::unique_ptr<CorpusState> StaleState::on_build_requested(CorpusContext& ctx) {
    return start_new_build(ctx);
}

}  // namespace aiws::gui