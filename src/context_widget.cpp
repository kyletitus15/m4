#include "aiws/gui/context_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QString>
#include <QSlider>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>

namespace aiws::gui {
    namespace {
        constexpr int kMinBudget = 50;
        constexpr int kMaxBudget = 1000;
        constexpr int kBudgetStep = 50;
        constexpr int kBudget = 300;
}

ContextWidget::ContextWidget(QWidget* parent) : QWidget(parent) {
    // TODO: Construct and lay out the Context area, configure the token-budget
    // control, connect local events, and establish initial state.
    
    //slider
    context_budget_slider_ = new QSlider(Qt::Horizontal, this);
    context_budget_slider_->setObjectName("budgetSlider");
    context_budget_slider_->setRange(kMinBudget, kMaxBudget);
    context_budget_slider_->setSingleStep(kBudgetStep);
    context_budget_slider_->setPageStep(kBudgetStep);
    context_budget_slider_->setTickPosition(QSlider::TicksBelow);
    context_budget_slider_->setTickInterval(kBudgetStep);
    context_budget_slider_->setValue(kBudget);

    //button
    build_context_button_ = new QPushButton("Build Context", this);
    build_context_button_->setObjectName("buildContextButton");
    build_context_button_->setEnabled(false);

    //budget 
    budget_value_label_ = new QLabel(QString::number(kBudget), this);
    budget_value_label_->setObjectName("budgetValueLabel");
    budget_value_label_->setMinimumWidth(40);
    budget_value_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    //context
    context_text_ = new QPlainTextEdit(this);
    context_text_->setObjectName("contextText");
    context_text_->setReadOnly(true);
    context_text_->setPlaceholderText("Built context will appear here");

    //put together
    auto* controls = new QHBoxLayout();
    controls->addWidget(new QLabel("Token budget:", this));
    controls->addWidget(context_budget_slider_, 1);
    controls->addWidget(budget_value_label_);
    controls->addWidget(build_context_button_);
    auto* group = new QGroupBox("Context", this);
    auto* group_layout = new QVBoxLayout(group);
    group_layout->addLayout(controls);
    group_layout->addWidget(context_text_, 1);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(group);

    //connections
    connect(context_budget_slider_, &QSlider::valueChanged, this, &ContextWidget::budget_changed);
    connect(build_context_button_, &QPushButton::clicked, this, &ContextWidget::request_context);


}

int ContextWidget::token_budget() const {
    // TODO
    return context_budget_slider_->value();
}

void ContextWidget::set_build_context_enabled(bool enabled) {
    // TODO
    build_context_button_->setEnabled(enabled);
}

void ContextWidget::set_context(const std::vector<ContextItem>& items) {
    // TODO
    if (items.empty()) {
        context_text_->setPlainText("No context items returned.");
        return;
    }
    QStringList block;
    for (const ContextItem& i : items) {
        block << QString("Document: %1\n"
                          "Chunk: %2\n"
                          "Tokens: %3\n"
                          "Score: %4\n"
                          "Truncated: %5\n"
                          "Text: %6").arg(QString::fromStdString(i.document_id))
                                     .arg(static_cast<qulonglong>(i.chunk_sequence))
                                     .arg(static_cast<qulonglong>(i.token_count))
                                     .arg(i.score, 0, 'f', 4)
                                     .arg(i.truncated ? "Yes" : "No")
                                     .arg(QString::fromStdString(i.text));
    }
        context_text_->setPlainText(block.join("\n\n----------------------------------------\n\n"));
}


void ContextWidget::clear_context() {
    // TODO
    context_text_->clear();

}

void ContextWidget::reset() {
    // TODO
    context_budget_slider_->setValue(kBudget);  // label updates via budget_changed
    clear_context();
    build_context_button_->setEnabled(false);
}

void ContextWidget::request_context() {
    // TODO
    if (!build_context_button_->isEnabled()) { return; }
    emit build_context_requested(context_budget_slider_->value());
}

void ContextWidget::budget_changed(int value) {
    // TODO
    const int snap = ((value + kBudgetStep / 2) / kBudgetStep) * kBudgetStep;
    if (snap != value) {
        context_budget_slider_->setValue(snap);  // re-enters with the snapped value
        return;
    }
    budget_value_label_->setText(QString::number(value));
}

}  // namespace aiws::gui
