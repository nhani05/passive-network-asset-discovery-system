#include "pnad/gui/AnalysisSessionModel.hpp"

#include "pnad/storage/SQLiteWriter.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace asset_discovery::gui {
namespace {

QString csvCell(QString value)
{
    value.replace("\"", "\"\"");
    return "\"" + value + "\"";
}

QJsonObject sessionJson(const AnalysisSessionItem& session)
{
    QJsonObject object;
    object.insert("id", static_cast<qint64>(session.id));
    object.insert("mode", session.mode);
    object.insert("source", session.source);
    object.insert("startTime", session.startTime);
    object.insert("endTime", session.endTime);
    object.insert("status", session.status);
    object.insert("assetCount", session.assetCount);
    object.insert("newAssets", "Unavailable");
    object.insert("highEvents", "Unavailable");
    object.insert("eventCount", session.eventCount);
    object.insert("droppedCounters", "Unavailable");
    object.insert("errorSummary", session.errorSummary);
    object.insert("storageContext", session.storageContext);
    return object;
}

QVariantMap sessionMap(const AnalysisSessionItem& session)
{
    QVariantMap map;
    map.insert("sessionId", static_cast<qlonglong>(session.id));
    map.insert("mode", session.mode);
    map.insert("source", session.source);
    map.insert("startTime", session.startTime);
    map.insert("endTime", session.endTime);
    map.insert("status", session.status);
    map.insert("assetCount", session.assetCount);
    map.insert("eventCount", session.eventCount);
    map.insert("errorSummary", session.errorSummary);
    map.insert("storageContext", session.storageContext);
    return map;
}

} // namespace

AnalysisSessionModel::AnalysisSessionModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int AnalysisSessionModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return sessions_.size();
}

int AnalysisSessionModel::completedSessionCount() const
{
    int count = 0;
    for (const auto& session : sessions_) {
        if (session.status.compare(QStringLiteral("Completed"), Qt::CaseInsensitive) == 0) {
            ++count;
        }
    }
    return count;
}

QVariant AnalysisSessionModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= sessions_.size()) {
        return QVariant();
    }

    const auto& session = sessions_[index.row()];
    switch (role) {
    case IdRole:
        return QVariant::fromValue<qlonglong>(session.id);
    case ModeRole:
        return session.mode;
    case SourceRole:
        return session.source;
    case StartTimeRole:
        return session.startTime;
    case EndTimeRole:
        return session.endTime;
    case StatusRole:
        return session.status;
    case AssetCountRole:
        return session.assetCount;
    case EventCountRole:
        return session.eventCount;
    case ErrorSummaryRole:
        return session.errorSummary;
    case StorageContextRole:
        return session.storageContext;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> AnalysisSessionModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "sessionId";
    roles[ModeRole] = "mode";
    roles[SourceRole] = "source";
    roles[StartTimeRole] = "startTime";
    roles[EndTimeRole] = "endTime";
    roles[StatusRole] = "status";
    roles[AssetCountRole] = "assetCount";
    roles[EventCountRole] = "eventCount";
    roles[ErrorSummaryRole] = "errorSummary";
    roles[StorageContextRole] = "storageContext";
    return roles;
}

void AnalysisSessionModel::reloadFromDatabase(const QString& dbPath)
{
    std::optional<std::string> error;
    QVector<AnalysisSessionItem> loaded;

    try {
        storage::SQLiteWriter writer(dbPath.toStdString());
        const auto sessions = writer.loadRecentAnalysisSessions(50, error);
        loaded.reserve(static_cast<int>(sessions.size()));
        for (const auto& session : sessions) {
            AnalysisSessionItem item;
            item.id = session.id;
            item.mode = QString::fromStdString(session.mode);
            item.source = QString::fromStdString(session.source);
            item.startTime = QString::fromStdString(session.startTime);
            item.endTime = QString::fromStdString(session.endTime);
            item.status = QString::fromStdString(session.status);
            item.assetCount = session.assetCount;
            item.eventCount = session.eventCount;
            item.errorSummary = QString::fromStdString(session.errorSummary);
            item.storageContext = QString::fromStdString(session.storageContext);
            loaded.push_back(std::move(item));
        }
    } catch (const std::exception& exception) {
        error = exception.what();
    }

    beginResetModel();
    sessions_ = std::move(loaded);
    endResetModel();
    emit sessionsChanged();

    const QString nextError = error.has_value() ? QString::fromStdString(*error) : QString();
    if (lastError_ != nextError) {
        lastError_ = nextError;
        emit lastErrorChanged();
    }
}

QString AnalysisSessionModel::latestSourceForMode(const QString& mode) const
{
    for (const auto& session : sessions_) {
        if (session.mode == mode && !session.source.isEmpty()) {
            return session.source;
        }
    }
    return {};
}

QVariantMap AnalysisSessionModel::latestSession() const
{
    if (sessions_.isEmpty()) {
        return {};
    }
    return sessionMap(sessions_.first());
}

QString AnalysisSessionModel::latestSessionLabel() const
{
    if (sessions_.isEmpty()) {
        return "No sessions";
    }
    const auto& session = sessions_.first();
    return session.mode + " | " + session.status;
}

int AnalysisSessionModel::latestAssetCount() const
{
    return sessions_.isEmpty() ? 0 : sessions_.first().assetCount;
}

int AnalysisSessionModel::latestEventCount() const
{
    return sessions_.isEmpty() ? 0 : sessions_.first().eventCount;
}

bool AnalysisSessionModel::exportSummaryToFile(const QString& path, const QString& format) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    const QString normalized = format.trimmed().toLower();
    if (normalized == "json") {
        QJsonArray sessions;
        for (const auto& session : sessions_) {
            sessions.push_back(sessionJson(session));
        }
        file.write(QJsonDocument(sessions).toJson(QJsonDocument::Indented));
        return true;
    }

    QTextStream stream(&file);
    if (normalized == "csv") {
        stream << "mode,source,start_time,end_time,status,asset_count,new_assets,high_events,event_count,dropped_counters,error_summary,storage_context\n";
        for (const auto& session : sessions_) {
            stream << csvCell(session.mode) << ","
                   << csvCell(session.source) << ","
                   << csvCell(session.startTime) << ","
                   << csvCell(session.endTime) << ","
                   << csvCell(session.status) << ","
                   << session.assetCount << ","
                   << csvCell("Unavailable") << ","
                   << csvCell("Unavailable") << ","
                   << session.eventCount << ","
                   << csvCell("Unavailable") << ","
                   << csvCell(session.errorSummary) << ","
                   << csvCell(session.storageContext) << "\n";
        }
        return true;
    }

    for (const auto& session : sessions_) {
        stream << session.mode << " | " << session.status << " | "
               << session.source << " | assets=" << session.assetCount
               << " events=" << session.eventCount << "\n";
    }
    return true;
}

} // namespace asset_discovery::gui
