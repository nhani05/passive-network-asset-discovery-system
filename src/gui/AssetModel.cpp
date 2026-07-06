#include "pnad/gui/AssetModel.hpp"

#include "pnad/constants/CliConstants.hpp"

#include <sqlite3.h>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace asset_discovery::gui {
namespace {

QString formatRelativeTime(const QString& timestampStr);

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

QStringList variantStringList(const QVariant& value)
{
    if (value.canConvert<QStringList>()) {
        return value.toStringList();
    }
    QStringList output;
    const auto list = value.toList();
    for (const auto& item : list) {
        output.append(item.toString());
    }
    return output;
}

AssetItem assetItemFromDto(const QVariantMap& dto)
{
    AssetItem item;
    item.macAddress = dto.value("macAddress").toString();
    item.ipAddresses = variantStringList(dto.value("ipAddresses"));
    item.hostname = dto.value("hostname").toString();

    item.firstSeen = formatRelativeTime(dto.value("firstSeen").toString());
    item.lastSeen = formatRelativeTime(dto.value("lastSeen").toString());
    item.discoverySources = variantStringList(dto.value("discoverySources"));
    return item;
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
    object.insert("mac", asset.macAddress);
    object.insert("ip", asset.ipAddresses.join(", "));
    object.insert("hostname", asset.hostname);
    object.insert("first_seen", asset.firstSeen);
    object.insert("last_seen", asset.lastSeen);
    object.insert("protocols", QJsonArray::fromStringList(asset.discoverySources));
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

    const char* sql = "SELECT mac_address, ip_addresses, hostname, first_seen, last_seen, "
                      "discovery_sources FROM assets ORDER BY mac_address;";
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
        item.firstSeen = formatRelativeTime(firstSeenRaw);
        item.lastSeen = formatRelativeTime(lastSeenRaw);
        item.discoverySources = parseJsonStringArray(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));

        newAssets.push_back(item);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    beginResetModel();
    assets_ = std::move(newAssets);
    endResetModel();
    emit assetsChanged();
}

void AssetModel::loadAssetDtos(const QVariantList& assets)
{
    QVector<AssetItem> nextAssets;
    nextAssets.reserve(assets.size());
    for (const auto& asset : assets) {
        nextAssets.push_back(assetItemFromDto(asset.toMap()));
    }

    beginResetModel();
    assets_ = std::move(nextAssets);
    endResetModel();
    emit assetsChanged();
}

void AssetModel::applyAssetDto(const QVariantMap& asset)
{
    const auto item = assetItemFromDto(asset);
    const auto normalizedMac = item.macAddress.toLower();
    for (int row = 0; row < assets_.size(); ++row) {
        if (assets_[row].macAddress.toLower() == normalizedMac) {
            assets_[row] = item;
            const auto idx = index(row, 0);
            emit dataChanged(idx, idx);
            emit assetsChanged();
            return;
        }
    }

    beginInsertRows(QModelIndex(), assets_.size(), assets_.size());
    assets_.push_back(item);
    endInsertRows();
    emit assetsChanged();
}

QVariantMap AssetModel::get(int row) const
{
    if (row < 0 || row >= assets_.size()) {
        return {};
    }
    return assetToMap(assets_[row]);
}

bool AssetModel::exportToFile(const QString& path, const QString& format) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    const QString normalized = format.trimmed().toLower();
    if (normalized == QString::fromLatin1(constants::cli::OutputJson)) {
        QJsonArray rows;
        for (const auto& asset : assets_) {
            rows.push_back(assetToJson(asset));
        }
        file.write(QJsonDocument(rows).toJson(QJsonDocument::Indented));
        return true;
    }

    QTextStream stream(&file);
    if (normalized == QString::fromLatin1(constants::cli::OutputCsv)) {
        stream << "ip,mac,hostname,first_seen,last_seen,protocols\n";
        for (const auto& asset : assets_) {
            stream << csvCell(asset.ipAddresses.join("; ")) << ","
                   << csvCell(asset.macAddress) << ","
                   << csvCell(asset.hostname) << ","
                   << csvCell(asset.firstSeen) << ","
                   << csvCell(asset.lastSeen) << ","
                   << csvCell(asset.discoverySources.join("; ")) << "\n";
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
