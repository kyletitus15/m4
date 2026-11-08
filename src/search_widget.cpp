#include "aiws/gui/search_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableView>
#include <QGroupBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QString>
#include <QLabel>
#include <QPushButton>
#include "aiws/gui/search_result_model.hpp"
#include <utility>

namespace aiws::gui {

    namespace {
        constexpr int kResCount = 3;
        constexpr int kMinCount = 1;
        constexpr int kMaxCount = 100;
    }

SearchWidget::SearchWidget(QWidget* parent) : QWidget(parent) {
    // TODO: Construct and lay out the Query, Search Results, and Selected Result
    // areas; configure the table/model; connect local events; establish initial state.

    //query
    query_edit_ = new QLineEdit(this);
    query_edit_->setObjectName("queryEdit");
    query_edit_->setPlaceholderText("Enter a query");

    result_count_spin_ = new QSpinBox(this);
    result_count_spin_->setObjectName("resultCountSpin");
    result_count_spin_->setRange(kMinCount, kMaxCount);
    result_count_spin_->setValue(kResCount);

    search_button_ = new QPushButton("Search", this);
    search_button_->setObjectName("searchButton");
    search_button_->setEnabled(false);

    auto* q_row = new QHBoxLayout();
    q_row->addWidget(new QLabel("Query:", this));
    q_row->addWidget(query_edit_, 1);
    q_row->addWidget(new QLabel("Results (k):", this));
    q_row->addWidget(result_count_spin_);
    q_row->addWidget(search_button_);

    //results
    result_model_ = new SearchResultModel(this);
    results_view_ = new QTableView(this);
    results_view_->setObjectName("resultsView");
    results_view_->setModel(result_model_);
    results_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    results_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);  
    results_view_->setWordWrap(false);
    results_view_->setTextElideMode(Qt::ElideRight);
    results_view_->verticalHeader()->hide();  
    results_view_->horizontalHeader()->setStretchLastSection(true);
    results_view_->setColumnWidth(0, 50);   
    results_view_->setColumnWidth(1, 200);  
    results_view_->setColumnWidth(2, 60);   
    results_view_->setColumnWidth(3, 80);   

    //selected result
    selected_result_text_ = new QPlainTextEdit(this);
    selected_result_text_->setObjectName("selectedResultText");
    selected_result_text_->setReadOnly(true);
    selected_result_text_->setPlaceholderText("Select a result to see its details");

    view_source_button_ = new QPushButton("View Source", this);
    view_source_button_->setObjectName("viewSourceButton");
    view_source_button_->setEnabled(false);

    auto* s_row = new QHBoxLayout();
    s_row->addWidget(new QLabel("Selected Result: ", this));
    s_row->addStretch(1);
    s_row->addWidget(view_source_button_);

    auto* group = new QGroupBox("Query / Search", this);
    auto* group_layout = new QVBoxLayout(group);
    group_layout->addLayout(q_row);
    group_layout->addWidget(results_view_, 3);
    group_layout->addLayout(s_row);
    group_layout->addWidget(selected_result_text_, 2);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(group);

    //connections
    connect(query_edit_, &QLineEdit::textChanged, this, &SearchWidget::query_changed);
    connect(search_button_, &QPushButton::clicked, this, &SearchWidget::request_search);
    connect(results_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SearchWidget::show_selected_result);
    connect(view_source_button_, &QPushButton::clicked, this, &SearchWidget::request_source_document);
}

QString SearchWidget::query() const {
    // TODO
    return query_edit_->text();
}

int SearchWidget::result_count() const {
    // TODO
    return result_count_spin_->value();
}

bool SearchWidget::has_query() const {
    // TODO
    return !query_edit_->text().trimmed().isEmpty();
}

void SearchWidget::set_search_enabled(bool enabled) {
    // TODO
    search_button_->setEnabled(enabled);
}

void SearchWidget::set_results(std::vector<SearchResult> results) {
    // TODO
    result_model_->set_results(std::move(results));
    results_view_->clearSelection();
    show_selected_result();  
}

void SearchWidget::clear_results() {
    // TODO
    result_model_->clear();
    results_view_->clearSelection();
    show_selected_result();
}

void SearchWidget::reset() {
    // TODO
    query_edit_->clear();
    result_count_spin_->setValue(kResCount);
    clear_results();
    search_button_->setEnabled(false);
}

void SearchWidget::request_search() {
    // TODO
    if (!search_button_->isEnabled() || !has_query()) {
        return;
    }
    emit search_requested(query_edit_->text(), result_count_spin_->value());
}

void SearchWidget::show_selected_result() {
    // TODO
    const SearchResult* res = nullptr;
    int row = -1;
    const QModelIndexList rows = results_view_->selectionModel()->selectedRows();
    if (!rows.isEmpty()) {
        row = rows.first().row();
        res = result_model_->result_at(row);
    }

    if (res == nullptr) {
        selected_result_text_->clear();
        view_source_button_->setEnabled(false);
        emit selected_chunk_changed(QString());
        return;
    }

    const QString details = QString("Rank: %1\n"
                "Document: %2\n"
                "Chunk: %3\n"
                "Chunk ID: %4\n"
                "Score: %5\n"
                "Matched terms: %6\n\n"
                "%7").arg(row+1)
                     .arg(QString::fromStdString(res->document_id))
                     .arg(static_cast<qulonglong>(res->chunk_sequence))
                     .arg(QString::fromStdString(res->chunk_id))
                     .arg(res->score, 0, 'f', 4)
                     .arg(static_cast<qulonglong>(res->matched_terms))
                     .arg(QString::fromStdString(res->text));
    selected_result_text_->setPlainText(details);
    view_source_button_->setEnabled(true);
    emit selected_chunk_changed(QString::fromStdString(res->chunk_id));
}

void SearchWidget::request_source_document() {
    // TODO
    const QModelIndexList rows = results_view_->selectionModel()->selectedRows();
    if (rows.isEmpty()) { return; }
    const SearchResult* res = result_model_->result_at(rows.first().row());
    if (res == nullptr) { return; }
    emit source_document_requested(QString::fromStdString(res->document_id));
}

}  // namespace aiws::gui
