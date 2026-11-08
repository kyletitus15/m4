#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QString>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QMetaObject>
#include <QObject>
#include <QDebug>


#include <string>
#include <utility>
#include <vector>
#include <exception>
#include <cstddef>
#include <algorithm>
#include <chrono>
#include <system_error>
#include <thread>

#include "aiws/gui/main_window.hpp"
#include "aiws/gui/context_widget.hpp"
#include "aiws/gui/resettable.hpp"
#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/term_inspector_widget.hpp"
#include "aiws/gui/workspace_widget.hpp"

namespace aiws::gui {

namespace {

    constexpr int kSleepMs = 10;
    #ifdef AIWS_FAULT_LOSE_FAILURE
    constexpr bool kFailure = true;
    #else
    constexpr bool kFailure = false;
    #endif

    void default_build(const Workspace& snapshot, ProcessingCore& core) {
        core.rebuild(snapshot);
    }

    bool simulate_delay(long long total_ms, const std::atomic<bool>* cancel) {
        long long left = total_ms;
        while (left > 0) {
            if (cancel->load()) { return false; }
            const long long step = std::min<long long>(left, kSleepMs);
            std::this_thread:: sleep_for(std::chrono::milliseconds(step));
            left = left - step;
        }
        return !cancel->load();
    }

    void run_build(Workspace snapshot, int delay, BuildFunction function, const std::atomic<bool>* cancel, std::promise<BuildResult> promise, QObject* target) {
        BuildResult res;
        try {
            const long long total = static_cast<long long>(delay) * static_cast<long long>(snapshot.documents().size());
            if (!simulate_delay(total, cancel)) {
                res.cancelled = true;
            }
            else {
                auto core = std::make_unique<ProcessingCore>();
                function(snapshot, *core);
                res.core = std::move(core);
            }
        } catch (const std::exception& e) {
            res.error = e.what();
            if (res.error.empty()) { res.error = "build failed"; }
        } catch (...) {
            res.error = "unknown error";
        }
        const bool failed = !res.error.empty();
        promise.set_value(std::move(res));
        if (failed && kFailure) { return; }
        qDebug() << "worker: posting build_finished";
        QMetaObject::invokeMethod(target, "build_finished", Qt::QueuedConnection);
    }

} //namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // TODO: Construct the M3 application window, compose the GUI components,
    // establish layouts/presentation, connect events, and establish initial state.
    build_func_ = &default_build;
    setWindowTitle("AI Workspace");
    resize(900, 900);

    auto* central = new QWidget(this);
    auto* main = new QVBoxLayout(central);
    main->setContentsMargins(10, 10, 10, 10);
    main->setSpacing(8);

    //header
    auto* header = new QLabel("AI Workspace", central);
    header->setAlignment(Qt::AlignCenter);
    header->setStyleSheet("background-color: #541717; color: white; font-size: 20px; font-weight: bold; padding: 10px;");

    //layout widget
    workspace_widget_ = new WorkspaceWidget(central);
    search_widget_ = new SearchWidget(central);
    term_inspector_widget_ = new TermInspectorWidget(central);
    context_widget_ = new ContextWidget(central);

    auto* bottom = new QHBoxLayout();
    bottom->addWidget(term_inspector_widget_, 1);
    bottom->addWidget(context_widget_, 2);

    // footer widget
    status_label_ = new QLabel("Ready", central);
    status_label_->setObjectName("statusLabel");
    reset_button_ = new QPushButton("Reset", central);
    reset_button_->setObjectName("resetButton");
    reset_button_->setStyleSheet("background-color: #f2ab2e; color: black; font-weight: bold; padding: 6px 16px; border-radius: 4px;");
    exit_button_ = new QPushButton("Exit", central);
    exit_button_->setObjectName("exitButton");
    exit_button_->setStyleSheet("background-color: #3646a3; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px;");

    auto* footer = new QHBoxLayout();
    footer->addWidget(status_label_);
    footer->addWidget(reset_button_);
    footer->addWidget(exit_button_);

    //main
    main->addWidget(header);
    main->addWidget(workspace_widget_, 2);
    main->addWidget(search_widget_, 3);
    main->addLayout(bottom, 2);
    main->addLayout(footer);
    setCentralWidget(central);

    //connections//

    //workspace -> mainwindow
    connect(workspace_widget_, &WorkspaceWidget::add_document_requested, this, &MainWindow::add_document);
    connect(workspace_widget_, &WorkspaceWidget::build_corpus_requested, this, &MainWindow::build_corpus);
    connect(workspace_widget_, &WorkspaceWidget::document_selected, this, &MainWindow::document_selected);    

    //search -> mainwindow
    connect(search_widget_, &SearchWidget::search_requested, this, &MainWindow::search_requested);
    connect(search_widget_, &SearchWidget::query_changed, this, &MainWindow::query_changed);
    connect(search_widget_, &SearchWidget::source_document_requested, this, &MainWindow::source_document_requested);
    
    //search -> term inspector
    connect(search_widget_, &SearchWidget::selected_chunk_changed, term_inspector_widget_, &TermInspectorWidget::set_selected_chunk);

    // term inspector -> mainwindow
    connect(term_inspector_widget_, &TermInspectorWidget::term_inspection_requested, this, &MainWindow::term_inspection_requested);
    
    //context -> mainwindow
    connect(context_widget_, &ContextWidget::build_context_requested, this, &MainWindow::build_context_requested);
    
    //reset & exit
    connect(reset_button_, &QPushButton::clicked, this, &MainWindow::reset_application);
    connect(exit_button_, &QPushButton::clicked, this, &MainWindow::exit_application);

    update_action_states();
    status_label_->setText("Ready");
}

MainWindow::~MainWindow() {
    cancel_requested_.store(true);
    if (build_thread_.joinable()) { build_thread_.join(); }
}

void MainWindow::set_exit_callback(ExitCallback callback) noexcept {
    // TODO
    exit_callback_ = callback;
}

void MainWindow::set_build_delay_ms(int ms_doc) {
    build_delay_ = std::max(0, ms_doc);
}

void MainWindow::set_build_function(BuildFunction func) noexcept {
    build_func_ = (func != nullptr) ? func : &default_build;
}

bool MainWindow::add_document_file(const QString& path) {
    const QFileInfo info(path);
    QFile file(path);
    if (!info.isFile() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QString txt = QString::fromUtf8(file.readAll());
    file.close();
    const QString title = info.fileName();

    try {
        workspace_.add_document(Document(info.absoluteFilePath().toStdString(), title.toStdString(), txt.toStdString()));
    
    } catch (const std::exception&) {
        return false;
    }
    ++ws_version_;
    workspace_widget_->add_document(title);
    invalidate_corpus();
    update_action_states();
    status_label_->setText("Added document: " + title);
    return true;
}

void MainWindow::add_document() {
    // TODO
    const QString path = QFileDialog::getOpenFileName(this, "Add Document", QString(), "Text files (*.txt);;All files (*)");
    if (path.isEmpty()) { return; }
    if (!add_document_file(path)) {
        QMessageBox::warning(this, "Add Document", "Could not read the selected file.");
        status_label_->setText("Failed to read doc");
    }
}

void MainWindow::build_corpus() {
    // TODO
    if (workspace_.documents().empty() || worker_running_) {
        return;
    }
    clear_processing_outputs();
    state_ = CorpusState::Building;
    job_version_ = ws_version_;
    std::promise<BuildResult> promise;
    build_future_ = promise.get_future();
    worker_running_ = true;

    try { build_thread_ = std::thread(run_build, workspace_, build_delay_, build_func_, &cancel_requested_, std::move(promise), this); 
    } catch (const std::exception& e) {
        worker_running_ = false;
        build_future_ = std::future<BuildResult>();
        state_ = CorpusState::Stale;
        workspace_widget_->set_corpus_status("Corpus: Build failed");
        status_label_->setText("Corpus build failed: " + QString::fromUtf8(e.what()));
        update_action_states();
        return;
    }

    workspace_widget_->set_corpus_status("Corpus: Building...");
    update_action_states();
    status_label_->setText("Building corpus...");
}

bool MainWindow::build_is_current(int job_v) const noexcept {
    #ifdef AIWS_FAILURE_ACCEPT_STALE
    (void)job_v;
    return true;
    #else
    return job_v == ws_version_;
    #endif
}

void MainWindow::build_finished() {
    qDebug() << "GUI: build_finished called";
    if (!worker_running_ || !build_future_.valid()) { return;}
    BuildResult res = build_future_.get();
    qDebug() << "got result; error =" << QString::fromStdString(res.error) << "core =" << (res.core != nullptr) << "job =" << job_version_ << "ws =" << ws_version_;
    if (build_thread_.joinable()) { build_thread_.join();}
    worker_running_ = false;
    qDebug() << "worker_running_cleared";
    const bool ws_empty = workspace_.documents().empty();

    if (!build_is_current(job_version_)) {
        state_ = ws_empty ? CorpusState::NotBuilt : CorpusState::Stale;
        if (!ws_empty) {
            workspace_widget_->set_corpus_status("Corpus: Out of date, rebuild needed");
            status_label_->setText("Discarded out-of-date build, rebuild needed");
        }
        update_action_states();
        return;
    }

    //r3
    if (!res.error.empty() || !res.core) {
        state_ = ws_empty ? CorpusState::NotBuilt : CorpusState::Stale;
        workspace_widget_->set_corpus_status("Corpus: Build failed");
        status_label_->setText("Corpus build failed: " + QString::fromStdString(res.error.empty() ? "cancelled" : res.error));
        update_action_states();
        return;
    }

    processing_core_ = std::move(*res.core);
    clear_processing_outputs();
    state_ = CorpusState::Current;
    workspace_widget_->set_corpus_status(QString("Corpus: %1 chunks").arg(static_cast<qulonglong>(processing_core_.chunk_count())));
    status_label_->setText("Corpus built");
    update_action_states();
}

void MainWindow::document_selected(int row) {
    // TODO
    const auto& docs = workspace_.documents();
    if (row < 0 || static_cast<std::size_t>(row) >= docs.size()) {
        workspace_widget_->clear_selected_document();
        return;
    }
    const Document& doc = docs[static_cast<std::size_t>(row)];
    workspace_widget_->show_selected_document(QString::fromStdString(doc.title()), QString::fromStdString(doc.text()));
}

void MainWindow::search_requested(const QString& query, int k) {
    // TODO
    if (!corpus_current() || query.trimmed().isEmpty()) {
        return;
    }

    std::vector<SearchResult> res = processing_core_.search(query.toStdString(), k);
    const std::size_t cnt = res.size();

    search_widget_->set_results(std::move(res));
    context_widget_->clear_context();
    term_inspector_widget_->set_selected_chunk(QString());

    update_action_states();
    status_label_->setText(QString("Search complete: %1 result(s)").arg(static_cast<qulonglong>(cnt)));
}

void MainWindow::query_changed(const QString& query) {
    // TODO
    (void)query;  // To suppress unused-parameter warning.
    update_action_states();
}

void MainWindow::build_context_requested(int token_budget) {
    // TODO
    if (!corpus_current() || !search_widget_->has_query()) {
        return;
    }
    const std::vector<ContextItem> items = processing_core_.build_context(search_widget_->query().toStdString(), search_widget_->result_count(), static_cast<std::size_t>(token_budget));
    context_widget_->set_context(items);
    status_label_->setText(QString("Context built: %1 item(s)").arg(static_cast<qulonglong>(items.size())));
}

void MainWindow::source_document_requested(const QString& document_id) {
    // TODO
    const std::string id = document_id.toStdString();
    const auto& docs = workspace_.documents();
    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (docs[i].id() == id) {
            workspace_widget_->select_document(static_cast<int>(i));
            status_label_->setText("Showing source document " + QString::fromStdString(docs[i].title()));
            return;
        }
    }
    status_label_->setText("Source document not found");
}

void MainWindow::term_inspection_requested(const QString& term,
                                           const QString& chunk_id) {
    // TODO
    const QString trim = term.trimmed();
    if (!corpus_current() || trim.isEmpty()) {
        return;
    }

    const std::string t = trim.toStdString();
    const std::size_t df = processing_core_.document_frequency(t);
    const bool tf_there = !chunk_id.isEmpty();
    const std::size_t tf = tf_there ? processing_core_.term_frequency(t, chunk_id.toStdString()) : 0;
    term_inspector_widget_->set_statistics(df, tf_there, tf);
    status_label_->setText("Inspected term: " + trim);
}

void MainWindow::reset_application() {
    // TODO
    ++ws_version_;
    workspace_.clear();
    state_ = CorpusState::NotBuilt;
    const std::vector<Resettable*> comp = {workspace_widget_, search_widget_, term_inspector_widget_, context_widget_};
    for (Resettable* c : comp) {
        c->reset();
    }
    update_action_states();
    status_label_->setText("Application reset.");
}

void MainWindow::exit_application() {
    // TODO
    if (exit_callback_ != nullptr) {
        exit_callback_(this);
    }
}

void MainWindow::update_action_states() {
    // TODO
    const bool process = corpus_current() && search_widget_->has_query();
    workspace_widget_->set_build_corpus_enabled(!worker_running_ && !workspace_.documents().empty());
    search_widget_->set_search_enabled(process);
    context_widget_->set_build_context_enabled(process);
    term_inspector_widget_->set_inspection_enabled(corpus_current());
}

void MainWindow::invalidate_corpus() {
    // TODO
    clear_processing_outputs();
    if (state_ == CorpusState::Current) {
        state_ = CorpusState::Stale;
        workspace_widget_->set_corpus_status("Corpus: Out of date, rebuild");
    }
    else if (state_ == CorpusState::Building) {
        workspace_widget_->set_corpus_status("Corpus: Building... (workspace change)");
    }
    update_action_states();
}

void MainWindow::clear_processing_outputs() {
    // TODO
    search_widget_->clear_results();
    context_widget_->clear_context();
    term_inspector_widget_->clear_statistics();
    term_inspector_widget_->set_selected_chunk(QString());
}

}  // namespace aiws::gui
