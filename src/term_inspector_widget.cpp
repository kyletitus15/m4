#include "aiws/gui/term_inspector_widget.hpp"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

namespace aiws::gui {

    namespace {
        const QString kNeutral = QStringLiteral("\u2014");
    }

TermInspectorWidget::TermInspectorWidget(QWidget* parent) : QWidget(parent) {
    // TODO: Construct and lay out the Term Inspector, connect local events,
    // and establish initial state.

    //term edit
    term_edit_ = new QLineEdit(this);
    term_edit_->setObjectName("termEdit");
    term_edit_->setPlaceholderText("Enter a term");

    //inspect
    inspect_button_ = new QPushButton("Inspect", this);
    inspect_button_->setObjectName("inspectButton");
    inspect_button_->setEnabled(false);

    //doc & chunk
    document_frequency_value_ = new QLabel(kNeutral, this);
    document_frequency_value_->setObjectName("documentFrequencyValue");
    chunk_frequency_value_ = new QLabel(kNeutral, this);
    chunk_frequency_value_->setObjectName("chunkFrequencyValue");

    //put together
    auto* group = new QGroupBox("Term Inspector", this);
    auto* grid = new QGridLayout(group);
    grid->addWidget(new QLabel("Term:", group), 0, 0);
    grid->addWidget(term_edit_, 0, 1);
    grid->addWidget(inspect_button_, 0, 2);
    grid->addWidget(new QLabel("Document frequency (DF):", group), 1, 0, 1, 2);
    grid->addWidget(document_frequency_value_, 1, 2);
    grid->addWidget(new QLabel("Selected-chunk TF:", group), 2, 0, 1, 2);
    grid->addWidget(chunk_frequency_value_, 2, 2);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(3, 1);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(group);
    
    //connections
    connect(inspect_button_, &QPushButton::clicked, this, &TermInspectorWidget::request_inspection);

}

void TermInspectorWidget::set_inspection_enabled(bool enabled) {
    // TODO
    inspect_button_->setEnabled(enabled);
}

void TermInspectorWidget::set_selected_chunk(const QString& chunk_id) {
    // TODO
    selected_chunk_id_ = chunk_id;
}

void TermInspectorWidget::set_statistics(std::size_t document_frequency,
                                         bool has_chunk_frequency,
                                         std::size_t chunk_frequency) {
    // TODO
    document_frequency_value_->setText(
        QString::number(static_cast<qulonglong>(document_frequency)));
    chunk_frequency_value_->setText(has_chunk_frequency ? QString::number(static_cast<qulonglong>(chunk_frequency)) : kNeutral);
}

void TermInspectorWidget::clear_statistics() {
    // TODO
    document_frequency_value_->setText(kNeutral);
    chunk_frequency_value_->setText(kNeutral);
}

void TermInspectorWidget::reset() {
    // TODO
    term_edit_->clear();
    selected_chunk_id_.clear();
    clear_statistics();
    inspect_button_->setEnabled(false);
}

void TermInspectorWidget::request_inspection() {
    // TODO
    const QString term = term_edit_->text().trimmed();
    if (term.isEmpty()) { return; }
    emit term_inspection_requested(term, selected_chunk_id_);
}

}  // namespace aiws::gui
