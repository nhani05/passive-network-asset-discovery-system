#include "pnad/gui/EventModel.hpp"
#include <sqlite3.h>
#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace asset_discovery::gui {
namespace {

QString formatRelativeTime(const QString& timestampStr)
{
    bool ok = false;
    double seconds = timestampStr.toDouble(&ok);
    if (!ok) {
        return timestampStr;
    }
    QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(seconds));
    return dt.toString("yyyy-MM-dd HH:mm:ss");
}

QString csvCell(QString value)
{
    value.replace("\"", "\"\"");
    return "\"" + value + "\"";
}

QString readableTypeLabel(const QString& rawType)
{
    const QString normalized = rawType.trimmed();
    if (normalized.isEmpty()) {
        return "Unknown Event";
    }

    QStringList words = normalized.split("_", Qt::SkipEmptyParts);
    for (auto& word : words) {
        if (!word.isEmpty()) {
            word[0] = word[0].toUpper();
        }
    }
    return words.join(" ");
}

QString assetLabel(const EventItem& event)
{
    if (!event.ipAddress.isEmpty() && !event.macAddress.isEmpty()) {
        return event.ipAddress + " | " + event.macAddress;
    }
    if (!event.ipAddress.isEmpty()) {
        return event.ipAddress;
    }
    if (!event.macAddress.isEmpty()) {
        return event.macAddress;
    }
    return "Unknown asset";
}

QJsonObject eventToJson(const EventItem& event)
{
    QJsonObject object;
    object.insert("id", event.id);
    object.insert("time", event.eventTime);
    object.insert("type", event.eventType);
    object.insert("label", readableTypeLabel(event.eventType));
    object.insert("severity", event.severity);
    object.insert("ipAddress", event.ipAddress);
    object.insert("macAddress", event.macAddress);
    object.insert("oldIp", event.oldIp);
    object.insert("newIp", event.newIp);
    object.insert("oldMac", event.oldMac);
    object.insert("newMac", event.newMac);
    object.insert("hostname", event.hostname);
    object.insert("protocol", event.protocol);
    object.insert("interface", event.interface);
    object.insert("message", event.message);
    object.insert("metadata", event.rawMetadata);
    return object;
}

QVariantMap eventToMap(const EventItem& event)
{
    QVariantMap map;
    map.insert("eventId", event.id);
    map.insert("eventTime", event.eventTime);
    map.insert("eventType", event.eventType);
    map.insert("eventLabel", readableTypeLabel(event.eventType));
    map.insert("severity", event.severity);
    map.insert("ipAddress", event.ipAddress.isEmpty() ? "-" : event.ipAddress);
    map.insert("macAddress", event.macAddress.isEmpty() ? "-" : event.macAddress);
    map.insert("oldIp", event.oldIp);
    map.insert("newIp", event.newIp);
    map.insert("oldMac", event.oldMac);
    map.insert("newMac", event.newMac);
    map.insert("hostname", event.hostname.isEmpty() ? "-" : event.hostname);
    map.insert("protocol", event.protocol.isEmpty() ? "-" : event.protocol);
    map.insert("interface", event.interface.isEmpty() ? "-" : event.interface);
    map.insert("message", event.message);
    map.insert("assetLabel", assetLabel(event));
    map.insert("rawMetadata", event.rawMetadata);
    map.insert("resolved", false);
    return map;
}

} // namespace

EventModel::EventModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int EventModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return events_.size();
}

QVariant EventModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= events_.size()) {
        return QVariant();
    }

    const auto& event = events_[index.row()];
    switch (role) {
    case IdRole:
        return event.id;
    case EventTimeRole:
        return event.eventTime;
    case EventTypeRole:
        return event.eventType;
    case SeverityRole:
        return event.severity;
    case IpAddressRole:
        return event.ipAddress.isEmpty() ? "-" : event.ipAddress;
    case MacAddressRole:
        return event.macAddress.isEmpty() ? "-" : event.macAddress;
    case OldIpRole:
        return event.oldIp;
    case NewIpRole:
        return event.newIp;
    case OldMacRole:
        return event.oldMac;
    case NewMacRole:
        return event.newMac;
    case HostnameRole:
        return event.hostname.isEmpty() ? "-" : event.hostname;
    case ProtocolRole:
        return event.protocol.isEmpty() ? "-" : event.protocol;
    case InterfaceRole:
        return event.interface.isEmpty() ? "-" : event.interface;
    case MessageRole:
        return event.message;
    case RawMetadataRole:
        return event.rawMetadata;
    case EventLabelRole:
        return readableTypeLabel(event.eventType);
    case AssetLabelRole:
        return assetLabel(event);
    case ResolvedRole:
        return false;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> EventModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "eventId";
    roles[EventTimeRole] = "eventTime";
    roles[EventTypeRole] = "eventType";
    roles[SeverityRole] = "severity";
    roles[IpAddressRole] = "ipAddress";
    roles[MacAddressRole] = "macAddress";
    roles[OldIpRole] = "oldIp";
    roles[NewIpRole] = "newIp";
    roles[OldMacRole] = "oldMac";
    roles[NewMacRole] = "newMac";
    roles[HostnameRole] = "hostname";
    roles[ProtocolRole] = "protocol";
    roles[InterfaceRole] = "interface";
    roles[MessageRole] = "message";
    roles[RawMetadataRole] = "rawMetadata";
    roles[EventLabelRole] = "eventLabel";
    roles[AssetLabelRole] = "assetLabel";
    roles[ResolvedRole] = "resolved";
    return roles;
}

void EventModel::reloadFromDatabase(const QString& dbPath)
{
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.toUtf8().constData(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        qWarning() << "Failed to open SQLite database for EventModel:" << dbPath;
        return;
    }

    const char* sql = "SELECT id, event_time, event_type, severity, ip_address, mac_address, "
                      "old_ip, new_ip, old_mac, new_mac, hostname, protocol, interface, message, metadata "
                      "FROM asset_events ORDER BY id DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        qWarning() << "Failed to prepare select query for EventModel:" << sqlite3_errmsg(db);
        sqlite3_close(db);
        return;
    }

    QVector<EventItem> newEvents;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        EventItem item;
        item.id = sqlite3_column_int(stmt, 0);
        item.eventTime = formatRelativeTime(QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))));
        item.eventType = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        item.severity = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));

        const char* ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        item.ipAddress = ip ? QString::fromUtf8(ip) : "";

        const char* mac = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        item.macAddress = mac ? QString::fromUtf8(mac) : "";

        const char* oldIp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        item.oldIp = oldIp ? QString::fromUtf8(oldIp) : "";

        const char* newIp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        item.newIp = newIp ? QString::fromUtf8(newIp) : "";

        const char* oldMac = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        item.oldMac = oldMac ? QString::fromUtf8(oldMac) : "";

        const char* newMac = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        item.newMac = newMac ? QString::fromUtf8(newMac) : "";

        const char* hostname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        item.hostname = hostname ? QString::fromUtf8(hostname) : "";

        const char* protocol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        item.protocol = protocol ? QString::fromUtf8(protocol) : "";

        const char* interface = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        item.interface = interface ? QString::fromUtf8(interface) : "";

        const char* message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
        item.message = message ? QString::fromUtf8(message) : "";

        const char* metadata = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
        item.rawMetadata = metadata ? QString::fromUtf8(metadata) : "{}";

        newEvents.push_back(item);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    beginResetModel();
    events_ = std::move(newEvents);
    endResetModel();
    emit eventsChanged();
}

bool EventModel::matchesFilter(int row, const QString& query, const QString& severityFilter) const
{
    if (row < 0 || row >= events_.size()) {
        return false;
    }
    const auto& event = events_[row];
    const QString normalizedSeverity = severityFilter.trimmed().toLower();
    if (!normalizedSeverity.isEmpty()
        && normalizedSeverity != "all"
        && event.severity.toLower() != normalizedSeverity) {
        return false;
    }

    const QString normalized = query.trimmed().toLower();
    if (normalized.isEmpty()) {
        return true;
    }
    return event.severity.toLower().contains(normalized)
        || event.eventType.toLower().contains(normalized)
        || readableTypeLabel(event.eventType).toLower().contains(normalized)
        || event.message.toLower().contains(normalized)
        || event.ipAddress.toLower().contains(normalized)
        || event.macAddress.toLower().contains(normalized)
        || event.hostname.toLower().contains(normalized)
        || event.protocol.toLower().contains(normalized)
        || event.interface.toLower().contains(normalized);
}

QString EventModel::relatedAssetQuery(int row) const
{
    if (row < 0 || row >= events_.size()) {
        return {};
    }
    const auto& event = events_[row];
    if (!event.macAddress.isEmpty()) {
        return event.macAddress;
    }
    return event.ipAddress;
}

QString EventModel::readableEventType(int row) const
{
    if (row < 0 || row >= events_.size()) {
        return {};
    }
    return readableTypeLabel(events_[row].eventType);
}

QVariantMap EventModel::get(int row) const
{
    if (row < 0 || row >= events_.size()) {
        return {};
    }
    return eventToMap(events_[row]);
}

int EventModel::highSeverityCount() const
{
    int count = 0;
    for (const auto& event : events_) {
        if (event.severity.compare("high", Qt::CaseInsensitive) == 0) {
            ++count;
        }
    }
    return count;
}

int EventModel::warningSeverityCount() const
{
    int count = 0;
    for (const auto& event : events_) {
        if (event.severity.compare("warning", Qt::CaseInsensitive) == 0
            || event.severity.compare("medium", Qt::CaseInsensitive) == 0) {
            ++count;
        }
    }
    return count;
}

int EventModel::unresolvedCount() const
{
    return events_.size();
}

bool EventModel::exportEvidenceToFile(int row, const QString& path, const QString& format) const
{
    if (row < 0 || row >= events_.size()) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    const auto& event = events_[row];
    const QString normalized = format.trimmed().toLower();
    if (normalized == "json") {
        QJsonObject evidence = eventToJson(event);
        evidence.insert("assetIdentity", assetLabel(event));
        evidence.insert("exportType", "event_evidence");
        file.write(QJsonDocument(evidence).toJson(QJsonDocument::Indented));
        return true;
    }

    QTextStream stream(&file);
    if (normalized == "csv") {
        stream << "id,time,type,label,severity,asset,ip_address,mac_address,old_ip,new_ip,old_mac,new_mac,hostname,protocol,interface,message,metadata\n";
        stream << event.id << ","
               << csvCell(event.eventTime) << ","
               << csvCell(event.eventType) << ","
               << csvCell(readableTypeLabel(event.eventType)) << ","
               << csvCell(event.severity) << ","
               << csvCell(assetLabel(event)) << ","
               << csvCell(event.ipAddress) << ","
               << csvCell(event.macAddress) << ","
               << csvCell(event.oldIp) << ","
               << csvCell(event.newIp) << ","
               << csvCell(event.oldMac) << ","
               << csvCell(event.newMac) << ","
               << csvCell(event.hostname) << ","
               << csvCell(event.protocol) << ","
               << csvCell(event.interface) << ","
               << csvCell(event.message) << ","
               << csvCell(event.rawMetadata) << "\n";
        return true;
    }

    stream << "Event evidence\n"
           << "Time: " << event.eventTime << "\n"
           << "Severity: " << event.severity << "\n"
           << "Type: " << readableTypeLabel(event.eventType) << " (" << event.eventType << ")\n"
           << "Asset: " << assetLabel(event) << "\n"
           << "Protocol/interface: " << event.protocol << " / " << event.interface << "\n"
           << "Message: " << event.message << "\n"
           << "Metadata: " << event.rawMetadata << "\n";
    return true;
}

bool EventModel::exportToFile(const QString& path, const QString& format) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    const QString normalized = format.trimmed().toLower();
    if (normalized == "json") {
        QJsonArray rows;
        for (const auto& event : events_) {
            rows.push_back(eventToJson(event));
        }
        file.write(QJsonDocument(rows).toJson(QJsonDocument::Indented));
        return true;
    }

    QTextStream stream(&file);
    if (normalized == "csv") {
        stream << "time,type,label,severity,asset,ip_address,mac_address,hostname,protocol,interface,message,metadata\n";
        for (const auto& event : events_) {
            stream << csvCell(event.eventTime) << ","
                   << csvCell(event.eventType) << ","
                   << csvCell(readableTypeLabel(event.eventType)) << ","
                   << csvCell(event.severity) << ","
                   << csvCell(assetLabel(event)) << ","
                   << csvCell(event.ipAddress) << ","
                   << csvCell(event.macAddress) << ","
                   << csvCell(event.hostname) << ","
                   << csvCell(event.protocol) << ","
                   << csvCell(event.interface) << ","
                   << csvCell(event.message) << ","
                   << csvCell(event.rawMetadata) << "\n";
        }
        return true;
    }

    for (const auto& event : events_) {
        stream << event.eventTime << " | " << event.severity << " | "
               << event.eventType << " | " << event.message << "\n";
    }
    return true;
}

} // namespace asset_discovery::gui
