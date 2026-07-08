#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace asset_discovery::gui {

struct AssetItem {
    QString macAddress;
    QStringList ipAddresses;
    QString hostname;
    QString displayName;
    QString vendor;
    QString osHint;
    QString deviceType;
    QString modelHint;
    QString firstSeen;
    QString lastSeen;
    qint64 firstSeenSortValue = 0;
    qint64 lastSeenSortValue = 0;
    QStringList discoverySources;
};

class AssetModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString sortColumn READ sortColumn NOTIFY sortChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)
public:
    enum class SortColumn {
        None,
        Mac,
        FirstSeen,
        LastSeen
    };

    enum AssetRoles {
        MacRole = Qt::UserRole + 1,
        IpsRole,
        HostnameRole,
        DisplayNameRole,
        VendorRole,
        OsHintRole,
        DeviceTypeRole,
        ModelHintRole,
        FirstSeenRole,
        LastSeenRole,
        SourcesRole
    };

    explicit AssetModel(QObject* parent = nullptr);

    // QAbstractItemModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString sortColumn() const;
    bool sortAscending() const;

    // Reload from SQLite database
    Q_INVOKABLE void reloadFromDatabase(const QString& dbPath);
    Q_INVOKABLE void loadAssetDtos(const QVariantList& assets);
    Q_INVOKABLE void applyAssetDto(const QVariantMap& asset);
    Q_INVOKABLE void sortByColumn(const QString& column);
    Q_INVOKABLE int rowForMac(const QString& macAddress) const;
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE bool exportToFile(const QString& path, const QString& format) const;

signals:
    void assetsChanged();
    void sortChanged();

private:
    void sortAssets(QVector<AssetItem>& assets) const;
    void applyCurrentSort();

    QVector<AssetItem> assets_;
    SortColumn sortColumn_ = SortColumn::None;
    bool sortAscending_ = true;
};

} // namespace asset_discovery::gui
