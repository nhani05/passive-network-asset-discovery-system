#include "pnad/gui/LogModel.hpp"

#include <QHash>

namespace asset_discovery::gui {
namespace {

constexpr int kMaxCoreLogRows = 200;

LogItem logFromDto(const QVariantMap& dto)
{
    LogItem item;
    item.timestamp = dto.value("timestamp", dto.value("time")).toString();
    item.severity = dto.value("severity", "info").toString();
    item.source = dto.value("source", "runtime").toString();
    item.message = dto.value("message").toString();
    return item;
}

} // namespace

LogModel::LogModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int LogModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return logs_.size();
}

QVariant LogModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= logs_.size()) {
        return {};
    }
    const auto& item = logs_[index.row()];
    switch (role) {
    case TimestampRole:
        return item.timestamp;
    case SeverityRole:
        return item.severity;
    case SourceRole:
        return item.source;
    case MessageRole:
        return item.message;
    default:
        return {};
    }
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TimestampRole] = "timestamp";
    roles[SeverityRole] = "severity";
    roles[SourceRole] = "source";
    roles[MessageRole] = "message";
    return roles;
}

void LogModel::loadLogDtos(const QVariantList& logs)
{
    QVector<LogItem> nextLogs;
    nextLogs.reserve(logs.size());
    for (const auto& log : logs) {
        nextLogs.push_back(logFromDto(log.toMap()));
    }

    beginResetModel();
    logs_ = std::move(nextLogs);
    endResetModel();
}

void LogModel::appendLogDto(const QVariantMap& log)
{
    beginInsertRows(QModelIndex(), 0, 0);
    logs_.push_front(logFromDto(log));
    endInsertRows();

    while (logs_.size() > kMaxCoreLogRows) {
        const int last = logs_.size() - 1;
        beginRemoveRows(QModelIndex(), last, last);
        logs_.removeLast();
        endRemoveRows();
    }
}

void LogModel::clear()
{
    beginResetModel();
    logs_.clear();
    endResetModel();
}

} // namespace asset_discovery::gui
