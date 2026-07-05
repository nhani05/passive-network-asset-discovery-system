#include "pnad/gui/AssetModel.hpp"
#include <sqlite3.h>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QTextStream>
#include <algorithm>

namespace asset_discovery::gui {
namespace {

int severityScore(const QString& severity)
{
    const QString normalized = severity.trimmed().toLower();
    if (normalized == "high") {
        return 3;
    }
    if (normalized == "warning" || normalized == "medium") {
        return 2;
    }
    if (normalized == "info") {
        return 1;
    }
    return 0;
}

QString riskFromScore(int score)
{
    if (score >= 3) {
        return "High";
    }
    if (score >= 2) {
        return "Suspicious";
    }
    return "Normal";
}

double parseTimestampEpoch(const QString& timestampStr)
{
    bool ok = false;
    const double seconds = timestampStr.toDouble(&ok);
    if (ok) {
        return seconds;
    }

    const auto iso = QDateTime::fromString(timestampStr, Qt::ISODate);
    if (iso.isValid()) {
        return static_cast<double>(iso.toSecsSinceEpoch());
    }

    const auto display = QDateTime::fromString(timestampStr, "yyyy-MM-dd HH:mm:ss");
    if (display.isValid()) {
        return static_cast<double>(display.toSecsSinceEpoch());
    }

    return 0;
}

QString statusForLastSeen(double lastSeenEpoch)
{
    if (lastSeenEpoch <= 0) {
        return "Unknown";
    }

    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    const double ageSeconds = static_cast<double>(now) - lastSeenEpoch;
    if (ageSeconds <= 15 * 60) {
        return "Active";
    }
    if (ageSeconds <= 24 * 60 * 60) {
        return "Recently Seen";
    }
    return "Offline";
}

QString sourceSummary(const QStringList& sources)
{
    return sources.isEmpty() ? QString("Unknown") : sources.join(", ");
}

QStringList parseJsonStringArray(const char* jsonStr)
{
    QStringList list;
    if (!jsonStr) return list;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonStr));
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const auto& val : arr) {
            list.append(val.toString());
        }
    }
    return list;
}

QString extractVendor(const char* refMetadataJson, const char* hintsJson)
{
    if (hintsJson) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray(hintsJson));
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const auto& val : arr) {
                QJsonObject obj = val.toObject();
                if (obj.value("category").toString() == "vendor") {
                    return obj.value("value").toString();
                }
            }
        }
    }
    if (refMetadataJson) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray(refMetadataJson));
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QJsonObject record = obj.value("mac.oui.registrant").toObject();
            QJsonArray values = record.value("values").toArray();
            if (!values.isEmpty()) {
                return values.at(0).toString();
            }
        }
    }
    return "Unknown";
}

QString extractOs(const char* hintsJson)
{
    if (hintsJson) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray(hintsJson));
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const auto& val : arr) {
                QJsonObject obj = val.toObject();
                if (obj.value("category").toString() == "os") {
                    return obj.value("value").toString();
                }
            }
        }
    }
    return "Unknown";
}

QString extractRole(const char* hintsJson)
{
    if (hintsJson) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray(hintsJson));
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const auto& val : arr) {
                QJsonObject obj = val.toObject();
                if (obj.value("category").toString() == "role") {
                    return obj.value("value").toString();
                }
            }
        }
    }
    return "Unknown";
}

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

QVariantMap assetToMap(const AssetItem& asset)
{
    QVariantMap map;
    map.insert("macAddress", asset.macAddress);
    map.insert("ipAddresses", asset.ipAddresses);
    map.insert("hostname", asset.hostname.isEmpty() ? "-" : asset.hostname);
    map.insert("firstSeen", asset.firstSeen);
    map.insert("lastSeen", asset.lastSeen);
    map.insert("discoverySources", asset.discoverySources);
    map.insert("vendor", asset.vendor);
    map.insert("deviceType", asset.deviceType);
    map.insert("os", asset.os);
    map.insert("rawObservedMetadata", asset.rawObservedMetadata);
    map.insert("status", asset.status);
    map.insert("risk", asset.risk);
    map.insert("sourceSummary", asset.sourceSummary);
    map.insert("lastSeenSortKey", asset.lastSeenEpoch);
    return map;
}

QString csvCell(QString value)
{
    value.replace("\"", "\"\"");
    return "\"" + value + "\"";
}

QJsonObject assetToJson(const AssetItem& asset)
{
    QJsonObject object;
    object.insert("macAddress", asset.macAddress);
    object.insert("ipAddresses", QJsonArray::fromStringList(asset.ipAddresses));
    object.insert("hostname", asset.hostname);
    object.insert("firstSeen", asset.firstSeen);
    object.insert("lastSeen", asset.lastSeen);
    object.insert("discoverySources", QJsonArray::fromStringList(asset.discoverySources));
    object.insert("vendor", asset.vendor);
    object.insert("role", asset.deviceType);
    object.insert("os", asset.os);
    object.insert("status", asset.status);
    object.insert("risk", asset.risk);
    object.insert("sourceSummary", asset.sourceSummary);
    object.insert("observedMetadata", asset.rawObservedMetadata);
    return object;
}

} // namespace

AssetModel::AssetModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int AssetModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return assets_.size();
}

QVariant AssetModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= assets_.size()) {
        return QVariant();
    }

    const auto& asset = assets_[index.row()];
    switch (role) {
    case MacRole:
        return asset.macAddress;
    case IpsRole:
        return asset.ipAddresses;
    case HostnameRole:
        return asset.hostname.isEmpty() ? "-" : asset.hostname;
    case FirstSeenRole:
        return asset.firstSeen;
    case LastSeenRole:
        return asset.lastSeen;
    case SourcesRole:
        return asset.discoverySources;
    case VendorRole:
        return asset.vendor;
    case DeviceTypeRole:
        return asset.deviceType;
    case OsRole:
        return asset.os;
    case RawMetadataRole:
        return asset.rawObservedMetadata;
    case StatusRole:
        return asset.status;
    case RiskRole:
        return asset.risk;
    case SourceSummaryRole:
        return asset.sourceSummary;
    case LastSeenSortRole:
        return asset.lastSeenEpoch;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> AssetModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[MacRole] = "macAddress";
    roles[IpsRole] = "ipAddresses";
    roles[HostnameRole] = "hostname";
    roles[FirstSeenRole] = "firstSeen";
    roles[LastSeenRole] = "lastSeen";
    roles[SourcesRole] = "discoverySources";
    roles[VendorRole] = "vendor";
    roles[DeviceTypeRole] = "deviceType";
    roles[OsRole] = "os";
    roles[RawMetadataRole] = "rawObservedMetadata";
    roles[StatusRole] = "status";
    roles[RiskRole] = "risk";
    roles[SourceSummaryRole] = "sourceSummary";
    roles[LastSeenSortRole] = "lastSeenSortKey";
    return roles;
}

void AssetModel::reloadFromDatabase(const QString& dbPath)
{
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.toUtf8().constData(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        qWarning() << "Failed to open SQLite database for AssetModel:" << dbPath;
        return;
    }

    QHash<QString, int> riskByMac;
    QHash<QString, int> riskByIp;
    QVector<AssetTimelineItem> newTimeline;
    const char* eventSql = "SELECT event_time, event_type, severity, ip_address, mac_address, message "
                           "FROM asset_events ORDER BY id DESC;";
    sqlite3_stmt* eventStmt = nullptr;
    if (sqlite3_prepare_v2(db, eventSql, -1, &eventStmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(eventStmt) == SQLITE_ROW) {
            const auto columnText = [eventStmt](int column) {
                const char* value = reinterpret_cast<const char*>(sqlite3_column_text(eventStmt, column));
                return value ? QString::fromUtf8(value) : QString();
            };
            const QString eventTimeRaw = columnText(0);
            const QString eventType = columnText(1);
            const QString severity = columnText(2);
            const QString ip = columnText(3);
            const QString mac = columnText(4);
            const QString message = columnText(5);
            const int score = severityScore(severity);

            if (!mac.isEmpty()) {
                riskByMac[mac.toLower()] = std::max(riskByMac.value(mac.toLower(), 0), score);
            }
            if (!ip.isEmpty()) {
                riskByIp[ip.toLower()] = std::max(riskByIp.value(ip.toLower(), 0), score);
            }

            AssetTimelineItem timelineItem;
            timelineItem.time = formatRelativeTime(eventTimeRaw);
            timelineItem.sortKey = parseTimestampEpoch(eventTimeRaw);
            timelineItem.type = eventType;
            timelineItem.severity = severity;
            timelineItem.ipAddress = ip;
            timelineItem.macAddress = mac;
            timelineItem.message = message;
            newTimeline.push_back(std::move(timelineItem));
        }
    } else {
        qWarning() << "Failed to prepare event risk query for AssetModel:" << sqlite3_errmsg(db);
    }
    if (eventStmt) {
        sqlite3_finalize(eventStmt);
    }

    const char* sql = "SELECT mac_address, ip_addresses, hostname, first_seen, last_seen, "
                      "discovery_sources, reference_metadata, derived_hints, observed_metadata FROM assets ORDER BY mac_address;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        qWarning() << "Failed to prepare select query for AssetModel:" << sqlite3_errmsg(db);
        sqlite3_close(db);
        return;
    }

    QVector<AssetItem> newAssets;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AssetItem item;
        item.macAddress = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        item.ipAddresses = parseJsonStringArray(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));

        const char* hostname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        item.hostname = hostname ? QString::fromUtf8(hostname) : "";

        const QString firstSeenRaw = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        const QString lastSeenRaw = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        item.firstSeenEpoch = parseTimestampEpoch(firstSeenRaw);
        item.lastSeenEpoch = parseTimestampEpoch(lastSeenRaw);
        item.firstSeen = formatRelativeTime(firstSeenRaw);
        item.lastSeen = formatRelativeTime(lastSeenRaw);

        item.discoverySources = parseJsonStringArray(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));

        const char* refMetadataJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        const char* hintsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        const char* obsMetadataJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));

        item.vendor = extractVendor(refMetadataJson, hintsJson);
        item.os = extractOs(hintsJson);
        item.deviceType = extractRole(hintsJson);
        item.rawObservedMetadata = obsMetadataJson ? QString::fromUtf8(obsMetadataJson) : "{}";
        item.status = statusForLastSeen(item.lastSeenEpoch);
        item.sourceSummary = sourceSummary(item.discoverySources);

        int riskScore = riskByMac.value(item.macAddress.toLower(), 0);
        for (const auto& ip : item.ipAddresses) {
            riskScore = std::max(riskScore, riskByIp.value(ip.toLower(), 0));
        }
        item.risk = riskFromScore(riskScore);

        newAssets.push_back(item);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    beginResetModel();
    assets_ = std::move(newAssets);
    timeline_ = std::move(newTimeline);
    endResetModel();
    emit assetsChanged();
}

bool AssetModel::matchesSearch(int row, const QString& query) const
{
    if (row < 0 || row >= assets_.size()) {
        return false;
    }
    const QString normalized = query.trimmed().toLower();
    if (normalized.isEmpty()) {
        return true;
    }
    const auto& asset = assets_[row];
    if (asset.macAddress.toLower().contains(normalized)
        || asset.hostname.toLower().contains(normalized)
        || asset.vendor.toLower().contains(normalized)
        || asset.deviceType.toLower().contains(normalized)
        || asset.os.toLower().contains(normalized)) {
        return true;
    }
    for (const auto& ip : asset.ipAddresses) {
        if (ip.toLower().contains(normalized)) {
            return true;
        }
    }
    for (const auto& source : asset.discoverySources) {
        if (source.toLower().contains(normalized)) {
            return true;
        }
    }
    return false;
}

bool AssetModel::matchesFilters(
    int row,
    const QString& query,
    const QString& statusFilter,
    const QString& riskFilter,
    const QString& sourceFilter) const
{
    if (row < 0 || row >= assets_.size()) {
        return false;
    }
    if (!matchesSearch(row, query)) {
        return false;
    }

    const auto& asset = assets_[row];
    const QString status = statusFilter.trimmed().toLower();
    if (!status.isEmpty() && status != "all" && asset.status.toLower() != status) {
        return false;
    }

    const QString risk = riskFilter.trimmed().toLower();
    if (!risk.isEmpty() && risk != "all" && asset.risk.toLower() != risk) {
        return false;
    }

    const QString source = sourceFilter.trimmed().toLower();
    if (!source.isEmpty() && source != "all") {
        bool found = false;
        for (const auto& discoverySource : asset.discoverySources) {
            if (discoverySource.toLower().contains(source)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

QString AssetModel::networkGroupForRow(int row) const
{
    if (row < 0 || row >= assets_.size()) {
        return "Unassigned network";
    }
    const auto& asset = assets_[row];
    if (!asset.ipAddresses.isEmpty()) {
        const QStringList octets = asset.ipAddresses.first().split(".");
        if (octets.size() >= 4) {
            return octets[0] + "." + octets[1] + "." + octets[2] + ".0/24";
        }
    }
    if (!asset.discoverySources.isEmpty()) {
        return "Source: " + asset.discoverySources.first();
    }
    return "Unassigned network";
}

QVariantMap AssetModel::get(int row) const
{
    if (row < 0 || row >= assets_.size()) {
        return {};
    }
    return assetToMap(assets_[row]);
}

int AssetModel::rowForIdentity(const QString& identity) const
{
    const QString normalized = identity.trimmed().toLower();
    if (normalized.isEmpty()) {
        return -1;
    }

    for (int row = 0; row < assets_.size(); ++row) {
        const auto& asset = assets_[row];
        if (asset.macAddress.toLower() == normalized) {
            return row;
        }
        for (const auto& ip : asset.ipAddresses) {
            if (ip.toLower() == normalized) {
                return row;
            }
        }
    }
    return -1;
}

QVariantMap AssetModel::assetForIdentity(const QString& identity) const
{
    const int row = rowForIdentity(identity);
    return row >= 0 ? get(row) : QVariantMap();
}

QVariantList AssetModel::timelineForAsset(const QString& identity) const
{
    const int row = rowForIdentity(identity);
    if (row < 0) {
        return {};
    }

    const auto& asset = assets_[row];
    QVector<AssetTimelineItem> items;
    AssetTimelineItem first;
    first.time = asset.firstSeen;
    first.sortKey = asset.firstSeenEpoch;
    first.type = "first_seen";
    first.severity = "info";
    first.macAddress = asset.macAddress;
    first.ipAddress = asset.ipAddresses.isEmpty() ? QString() : asset.ipAddresses.first();
    first.message = "Asset first observed via " + asset.sourceSummary;
    items.push_back(std::move(first));

    for (const auto& source : asset.discoverySources) {
        AssetTimelineItem sourceItem;
        sourceItem.time = asset.firstSeen;
        sourceItem.sortKey = asset.firstSeenEpoch + 0.001;
        sourceItem.type = source;
        sourceItem.severity = "info";
        sourceItem.macAddress = asset.macAddress;
        sourceItem.ipAddress = asset.ipAddresses.isEmpty() ? QString() : asset.ipAddresses.first();
        sourceItem.message = source.toUpper() + " evidence observed";
        items.push_back(std::move(sourceItem));
    }

    for (const auto& event : timeline_) {
        bool related = event.macAddress.compare(asset.macAddress, Qt::CaseInsensitive) == 0;
        if (!related) {
            for (const auto& ip : asset.ipAddresses) {
                if (event.ipAddress.compare(ip, Qt::CaseInsensitive) == 0) {
                    related = true;
                    break;
                }
            }
        }
        if (related) {
            items.push_back(event);
        }
    }

    AssetTimelineItem last;
    last.time = asset.lastSeen;
    last.sortKey = asset.lastSeenEpoch;
    last.type = "last_seen";
    last.severity = "info";
    last.macAddress = asset.macAddress;
    last.ipAddress = asset.ipAddresses.isEmpty() ? QString() : asset.ipAddresses.first();
    last.message = "Asset last observed";
    items.push_back(std::move(last));

    std::sort(items.begin(), items.end(), [](const auto& left, const auto& right) {
        return left.sortKey < right.sortKey;
    });

    QVariantList result;
    for (const auto& item : items) {
        QVariantMap map;
        map.insert("time", item.time);
        map.insert("type", item.type);
        map.insert("severity", item.severity);
        map.insert("ipAddress", item.ipAddress);
        map.insert("macAddress", item.macAddress);
        map.insert("message", item.message);
        result.push_back(map);
    }
    return result;
}

void AssetModel::sortByLastSeenDescending()
{
    beginResetModel();
    std::sort(assets_.begin(), assets_.end(), [](const auto& left, const auto& right) {
        return left.lastSeenEpoch > right.lastSeenEpoch;
    });
    endResetModel();
    emit assetsChanged();
}

void AssetModel::sortByMacAddress()
{
    beginResetModel();
    std::sort(assets_.begin(), assets_.end(), [](const auto& left, const auto& right) {
        return left.macAddress < right.macAddress;
    });
    endResetModel();
    emit assetsChanged();
}

int AssetModel::activeAssetCount() const
{
    int count = 0;
    for (const auto& asset : assets_) {
        if (asset.status == "Active") {
            ++count;
        }
    }
    return count;
}

int AssetModel::newAssetsTodayCount() const
{
    int count = 0;
    const QDate today = QDateTime::currentDateTime().date();
    for (const auto& asset : assets_) {
        if (asset.firstSeenEpoch > 0
            && QDateTime::fromSecsSinceEpoch(static_cast<qint64>(asset.firstSeenEpoch)).date() == today) {
            ++count;
        }
    }
    return count;
}

int AssetModel::highRiskAssetCount() const
{
    int count = 0;
    for (const auto& asset : assets_) {
        if (asset.risk == "High") {
            ++count;
        }
    }
    return count;
}

QString AssetModel::newestAssetLabel() const
{
    if (assets_.isEmpty()) {
        return "None";
    }

    const auto newest = std::max_element(assets_.begin(), assets_.end(), [](const auto& left, const auto& right) {
        return left.firstSeenEpoch < right.firstSeenEpoch;
    });
    const QString primaryIp = newest->ipAddresses.isEmpty() ? QString("No IP") : newest->ipAddresses.first();
    return primaryIp + " | " + newest->macAddress;
}

bool AssetModel::exportToFile(const QString& path, const QString& format) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    const QString normalized = format.trimmed().toLower();
    if (normalized == "json") {
        QJsonArray rows;
        for (const auto& asset : assets_) {
            rows.push_back(assetToJson(asset));
        }
        file.write(QJsonDocument(rows).toJson(QJsonDocument::Indented));
        return true;
    }

    QTextStream stream(&file);
    if (normalized == "csv") {
        stream << "mac_address,ip_addresses,hostname,vendor,role,os,status,risk,discovery_sources,first_seen,last_seen\n";
        for (const auto& asset : assets_) {
            stream << csvCell(asset.macAddress) << ","
                   << csvCell(asset.ipAddresses.join("; ")) << ","
                   << csvCell(asset.hostname) << ","
                   << csvCell(asset.vendor) << ","
                   << csvCell(asset.deviceType) << ","
                   << csvCell(asset.os) << ","
                   << csvCell(asset.status) << ","
                   << csvCell(asset.risk) << ","
                   << csvCell(asset.discoverySources.join("; ")) << ","
                   << csvCell(asset.firstSeen) << ","
                   << csvCell(asset.lastSeen) << "\n";
        }
        return true;
    }

    for (const auto& asset : assets_) {
        stream << asset.macAddress << " | " << asset.ipAddresses.join(", ")
               << " | " << asset.hostname << " | " << asset.discoverySources.join(", ") << "\n";
    }
    return true;
}

} // namespace asset_discovery::gui
