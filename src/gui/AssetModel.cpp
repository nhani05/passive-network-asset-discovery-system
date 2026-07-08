#include "pnad/gui/AssetModel.hpp"

#include "pnad/constants/CliConstants.hpp"

#include <sqlite3.h>
#include <algorithm>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace asset_discovery::gui {
namespace {

QString formatRelativeTime(const QString& timestampStr);
qint64 parseTimestampSortValue(const QString& timestampStr);

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
    item.displayName = dto.value("displayName").toString();
    item.vendor = dto.value("vendor").toString();
    item.osHint = dto.value("osHint", dto.value("os")).toString();
    item.deviceType = dto.value("deviceType").toString();
    item.modelHint = dto.value("modelHint").toString();

    const QString firstSeenRaw = dto.value("firstSeen").toString();
    const QString lastSeenRaw = dto.value("lastSeen").toString();
    item.firstSeenSortValue = parseTimestampSortValue(firstSeenRaw);
    item.lastSeenSortValue = parseTimestampSortValue(lastSeenRaw);
    item.firstSeen = formatRelativeTime(firstSeenRaw);
    item.lastSeen = formatRelativeTime(lastSeenRaw);
    item.discoverySources = variantStringList(dto.value("discoverySources"));
    return item;
}

qint64 parseTimestampSortValue(const QString& timestampStr)
{
    const QString trimmed = timestampStr.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }

    const qsizetype dot = trimmed.indexOf('.');
    bool secondsOk = false;
    const qint64 seconds = (dot >= 0 ? trimmed.left(dot) : trimmed).toLongLong(&secondsOk);
    if (secondsOk) {
        qint64 micros = 0;
        if (dot >= 0) {
            QString microText = trimmed.mid(dot + 1);
            if (microText.size() > 6) {
                microText = microText.left(6);
            }
            while (microText.size() < 6) {
                microText.append('0');
            }
            bool microsOk = false;
            micros = microText.toLongLong(&microsOk);
            if (!microsOk) {
                micros = 0;
            }
        }
        return seconds * 1000000 + micros;
    }

    const auto formatted = QDateTime::fromString(trimmed, "yyyy-MM-dd HH:mm:ss");
    if (formatted.isValid()) {
        return formatted.toSecsSinceEpoch() * 1000000;
    }
    return 0;
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
    map.insert("displayName", asset.displayName.isEmpty() ? "-" : asset.displayName);
    map.insert("vendor", asset.vendor.isEmpty() ? "-" : asset.vendor);
    map.insert("osHint", asset.osHint.isEmpty() ? "-" : asset.osHint);
    map.insert("deviceType", asset.deviceType.isEmpty() ? "-" : asset.deviceType);
    map.insert("modelHint", asset.modelHint.isEmpty() ? "-" : asset.modelHint);
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
    object.insert("display_name", asset.displayName);
    object.insert("vendor", asset.vendor);
    object.insert("os_hint", asset.osHint);
    object.insert("device_type", asset.deviceType);
    object.insert("model_hint", asset.modelHint);
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
    case DisplayNameRole:
        return asset.displayName.isEmpty() ? "-" : asset.displayName;
    case VendorRole:
        return asset.vendor.isEmpty() ? "-" : asset.vendor;
    case OsHintRole:
        return asset.osHint.isEmpty() ? "-" : asset.osHint;
    case DeviceTypeRole:
        return asset.deviceType.isEmpty() ? "-" : asset.deviceType;
    case ModelHintRole:
        return asset.modelHint.isEmpty() ? "-" : asset.modelHint;
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
    roles[DisplayNameRole] = "displayName";
    roles[VendorRole] = "vendor";
    roles[OsHintRole] = "osHint";
    roles[DeviceTypeRole] = "deviceType";
    roles[ModelHintRole] = "modelHint";
    roles[FirstSeenRole] = "firstSeen";
    roles[LastSeenRole] = "lastSeen";
    roles[SourcesRole] = "discoverySources";
    return roles;
}

QString AssetModel::sortColumn() const
{
    switch (sortColumn_) {
    case SortColumn::Mac:
        return "macAddress";
    case SortColumn::FirstSeen:
        return "firstSeen";
    case SortColumn::LastSeen:
        return "lastSeen";
    case SortColumn::None:
        return "";
    }
    return "";
}

bool AssetModel::sortAscending() const
{
    return sortAscending_;
}

void AssetModel::reloadFromDatabase(const QString& dbPath)
{
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.toUtf8().constData(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        qWarning() << "Failed to open SQLite database for AssetModel:" << dbPath;
        return;
    }

    const char* sql = "SELECT mac_address, ip_addresses, hostname, display_name, vendor, os_hint, device_type, "
                      "model_hint, first_seen, last_seen, discovery_sources FROM assets ORDER BY mac_address;";
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
        const char* displayName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        item.displayName = displayName ? QString::fromUtf8(displayName) : "";
        const char* vendor = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        item.vendor = vendor ? QString::fromUtf8(vendor) : "";
        const char* osHint = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        item.osHint = osHint ? QString::fromUtf8(osHint) : "";
        const char* deviceType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        item.deviceType = deviceType ? QString::fromUtf8(deviceType) : "";
        const char* modelHint = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        item.modelHint = modelHint ? QString::fromUtf8(modelHint) : "";

        const QString firstSeenRaw = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)));
        const QString lastSeenRaw = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9)));
        item.firstSeenSortValue = parseTimestampSortValue(firstSeenRaw);
        item.lastSeenSortValue = parseTimestampSortValue(lastSeenRaw);
        item.firstSeen = formatRelativeTime(firstSeenRaw);
        item.lastSeen = formatRelativeTime(lastSeenRaw);
        item.discoverySources = parseJsonStringArray(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10)));

        newAssets.push_back(item);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    sortAssets(newAssets);
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

    sortAssets(nextAssets);
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
            if (sortColumn_ != SortColumn::None) {
                assets_[row] = item;
                applyCurrentSort();
                return;
            }
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
    if (sortColumn_ != SortColumn::None) {
        applyCurrentSort();
        return;
    }
    emit assetsChanged();
}

void AssetModel::sortByColumn(const QString& column)
{
    const QString normalized = column.trimmed();
    SortColumn nextColumn = SortColumn::None;
    if (normalized == "macAddress" || normalized == "mac") {
        nextColumn = SortColumn::Mac;
    } else if (normalized == "firstSeen" || normalized == "first_seen") {
        nextColumn = SortColumn::FirstSeen;
    } else if (normalized == "lastSeen" || normalized == "last_seen") {
        nextColumn = SortColumn::LastSeen;
    } else {
        return;
    }

    if (nextColumn == SortColumn::Mac) {
        sortAscending_ = true;
    } else if (sortColumn_ == nextColumn) {
        sortAscending_ = !sortAscending_;
    } else {
        sortAscending_ = false;
    }
    sortColumn_ = nextColumn;
    emit sortChanged();
    applyCurrentSort();
}

int AssetModel::rowForMac(const QString& macAddress) const
{
    const QString normalizedMac = macAddress.toLower();
    for (int row = 0; row < assets_.size(); ++row) {
        if (assets_[row].macAddress.toLower() == normalizedMac) {
            return row;
        }
    }
    return -1;
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
    const QFileInfo fileInfo(path);
    const QDir parentDir(fileInfo.absoluteDir());
    if (!parentDir.exists() && !QDir().mkpath(parentDir.absolutePath())) {
        qWarning() << "Failed to create export directory:" << parentDir.absolutePath();
        return false;
    }
    const QFileInfo parentInfo(parentDir.absolutePath());
    if (!parentInfo.exists() || !parentInfo.isDir() || !parentInfo.isWritable()) {
        qWarning() << "Export directory is not writable:" << parentDir.absolutePath();
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Failed to open export file:" << path << file.errorString();
        return false;
    }

    const QString normalized = format.trimmed().toLower();
    if (normalized == QString::fromLatin1(constants::cli::OutputJson)) {
        QJsonArray rows;
        for (const auto& asset : assets_) {
            rows.push_back(assetToJson(asset));
        }
        return file.write(QJsonDocument(rows).toJson(QJsonDocument::Indented)) >= 0;
    }

    QTextStream stream(&file);
    if (normalized == QString::fromLatin1(constants::cli::OutputCsv)) {
        stream << "ip,mac,hostname,display_name,vendor,os_hint,device_type,model_hint,first_seen,last_seen,protocols\n";
        for (const auto& asset : assets_) {
            stream << csvCell(asset.ipAddresses.join("; ")) << ","
                   << csvCell(asset.macAddress) << ","
                   << csvCell(asset.hostname) << ","
                   << csvCell(asset.displayName) << ","
                   << csvCell(asset.vendor) << ","
                   << csvCell(asset.osHint) << ","
                   << csvCell(asset.deviceType) << ","
                   << csvCell(asset.modelHint) << ","
                   << csvCell(asset.firstSeen) << ","
                   << csvCell(asset.lastSeen) << ","
                   << csvCell(asset.discoverySources.join("; ")) << "\n";
        }
        return stream.status() == QTextStream::Ok;
    }

    for (const auto& asset : assets_) {
        stream << asset.macAddress << " | " << asset.ipAddresses.join(", ")
               << " | " << asset.displayName << " | " << asset.vendor << " | "
               << asset.osHint << " | " << asset.deviceType << " | "
               << asset.modelHint << " | " << asset.discoverySources.join(", ") << "\n";
    }
    return stream.status() == QTextStream::Ok;
}

void AssetModel::sortAssets(QVector<AssetItem>& assets) const
{
    if (sortColumn_ == SortColumn::None) {
        return;
    }

    const auto compareText = [](const QString& left, const QString& right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    };
    const bool ascending = sortAscending_;
    const SortColumn column = sortColumn_;

    std::stable_sort(assets.begin(), assets.end(), [&](const AssetItem& left, const AssetItem& right) {
        switch (column) {
        case SortColumn::Mac:
            return compareText(left.macAddress, right.macAddress);
        case SortColumn::FirstSeen:
            if (left.firstSeenSortValue == right.firstSeenSortValue) {
                return compareText(left.macAddress, right.macAddress);
            }
            return ascending
                ? left.firstSeenSortValue < right.firstSeenSortValue
                : left.firstSeenSortValue > right.firstSeenSortValue;
        case SortColumn::LastSeen:
            if (left.lastSeenSortValue == right.lastSeenSortValue) {
                return compareText(left.macAddress, right.macAddress);
            }
            return ascending
                ? left.lastSeenSortValue < right.lastSeenSortValue
                : left.lastSeenSortValue > right.lastSeenSortValue;
        case SortColumn::None:
            return false;
        }
        return false;
    });
}

void AssetModel::applyCurrentSort()
{
    beginResetModel();
    sortAssets(assets_);
    endResetModel();
    emit assetsChanged();
}

} // namespace asset_discovery::gui
