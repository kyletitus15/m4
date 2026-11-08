#include "aiws/gui/workspace_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QString>
#include <QPushButton>
#include <QLabel>

namespace aiws::gui {

WorkspaceWidget::WorkspaceWidget(QWidget* parent) : QWidget(parent) {
    // TODO: Construct and lay out the Workspace and Selected Document areas,
    // connect local widget events, and establish initial presentation state.

    //workspace
    document_list_ = new QListWidget(this);
    document_list_->setObjectName("documentList");
    document_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    document_list_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    add_document_button_ = new QPushButton("Add Document", this);
    add_document_button_->setObjectName("addDocumentButton");

    build_corpus_button_ = new QPushButton("Build Corpus", this);
    build_corpus_button_->setObjectName("buildCorpusButton");
    build_corpus_button_->setEnabled(false);

    corpus_status_label_ = new QLabel("Corpus: Not built", this);
    corpus_status_label_->setObjectName("corpusStatusLabel");

    auto* button = new QHBoxLayout();
    button->addWidget(add_document_button_);
    button->addWidget(build_corpus_button_);
    auto* ws = new QGroupBox("Workspace", this);
    auto* ws_layout = new QVBoxLayout(ws);
    ws_layout->addWidget(document_list_, 1);
    ws_layout->addLayout(button);
    ws_layout->addWidget(corpus_status_label_);

    //selected doc
    document_title_label_ = new QLabel("No document selected", this);
    document_title_label_->setObjectName("documentTitleLabel");
    document_title_label_->setStyleSheet("font-weight: bold;");

    document_text_ = new QPlainTextEdit(this);
    document_text_->setObjectName("documentText");
    document_text_->setReadOnly(true);
    document_text_->setPlaceholderText("Select a document to view its text");

    auto* doc = new QGroupBox("Selected document", this);
    auto*doc_layout = new QVBoxLayout(doc);
    doc_layout->addWidget(document_title_label_);
    doc_layout->addWidget(document_text_, 1);

    //put together
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(ws, 1);
    outer->addWidget(doc, 2);

    //connections
    connect(add_document_button_, &QPushButton::clicked, this, &WorkspaceWidget::add_document_requested);
    connect(build_corpus_button_, &QPushButton::clicked, this, &WorkspaceWidget::build_corpus_requested);
    connect(document_list_, &QListWidget::currentRowChanged, this, &WorkspaceWidget::document_selected);
     

}

void WorkspaceWidget::add_document(const QString& title) {
    // TODO
    document_list_->addItem(title);
}

void WorkspaceWidget::select_document(int row) {
    // TODO
    if (row < 0 || row >= document_list_->count()) { return; }
    document_list_->setCurrentRow(row);
}

void WorkspaceWidget::show_selected_document(const QString& title,
                                              const QString& text) {
    // TODO
    document_title_label_->setText(title);
    document_text_->setPlainText(text);
}

void WorkspaceWidget::clear_selected_document() {
    // TODO
    document_title_label_->setText("No document selected");
    document_text_->clear();
}

void WorkspaceWidget::set_corpus_status(const QString& text) {
    // TODO
    corpus_status_label_->setText(text);
}

void WorkspaceWidget::set_build_corpus_enabled(bool enabled) {
    // TODO
    build_corpus_button_->setEnabled(enabled);
}

void WorkspaceWidget::reset() {
    // TODO
    document_list_->clear(); 
    clear_selected_document();  
    corpus_status_label_->setText("Corpus: Not built");
    build_corpus_button_->setEnabled(false);
}

}  // namespace aiws::gui
