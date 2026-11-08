#include "aiws/gui/search_result_model.hpp"
#include <QString>
#include <QVariant>
#include <utility>

namespace aiws::gui {

namespace {
    constexpr int kColRank = 0;
    constexpr int kColDoc = 1;
    constexpr int kColChunk = 2;
    constexpr int kColScore = 3;
    constexpr int kColText = 4;
    constexpr int kColCnt = 5;

}

SearchResultModel::SearchResultModel(QObject* parent)
    : QAbstractTableModel(parent) {
    // TODO
}

int SearchResultModel::rowCount(const QModelIndex& parent) const {
    // TODO
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(results_.size());
}

int SearchResultModel::columnCount(const QModelIndex& parent) const {
    // TODO
    if (parent.isValid()) { return 0; }
    return kColCnt;
}

QVariant SearchResultModel::data(const QModelIndex& index, int role) const {
    // TODO
    if (!index.isValid() || role != Qt::DisplayRole) { return {}; }
    const SearchResult* res = result_at(index.row());
    if (res == nullptr) { return {}; }

    switch (index.column()) {
        case kColRank:
            return index.row() + 1;
        case kColDoc:
            return QString::fromStdString(res->document_id);
        case kColChunk:
            return static_cast<qulonglong>(res->chunk_sequence);
        case kColScore:
            return static_cast<double>(res->score);
        case kColText:
            return QString::fromStdString(res->text);
        default:
            return {};
    }
}

QVariant SearchResultModel::headerData(int section,
                                       Qt::Orientation orientation,
                                       int role) const {
    // TODO
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
        case kColRank:
            return QStringLiteral("Rank");
        case kColDoc:
            return QStringLiteral("Document");
        case kColChunk:
            return QStringLiteral("Chunk");
        case kColScore:
            return QStringLiteral("Score");
        case kColText:
            return QStringLiteral("Text");
        default: return{};
    }
}

void SearchResultModel::set_results(std::vector<SearchResult> results) {
    // TODO
    beginResetModel();
    results_ = std::move(results);
    endResetModel();
}

void SearchResultModel::clear() {
    // TODO
    beginResetModel();
    results_.clear();
    endResetModel();
}

const SearchResult* SearchResultModel::result_at(int row) const noexcept {
    // TODO
    if (row < 0 || static_cast<std::size_t>(row) >= results_.size()) { return nullptr; }
    return &results_[static_cast<std::size_t>(row)];
}

}  // namespace aiws::gui
