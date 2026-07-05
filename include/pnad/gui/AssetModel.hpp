#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>

namespace asset_discovery::gui {

struct AssetItem {
    QString macAddress;
    QStringList ipAddresses;
    QString hostname;
    QString firstSeen;
    QString lastSeen;
    QStringList discoverySources;
    QString vendor;
    QString deviceType;
    QString os;
    QString rawObservedMetadata;
    QString status;
    QString risk;
    QString sourceSummary;
    double firstSeenEpoch = 0;
    double lastSeenEpoch = 0;
};

struct AssetTimelineItem {
    QString time;
    double sortKey = 0;
    QString type;
    QString severity;
    QString ipAddress;
    QString macAddress;
    QString message;
};

class AssetModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int activeAssetCount READ activeAssetCount NOTIFY assetsChanged)
    Q_PROPERTY(int newAssetsTodayCount READ newAssetsTodayCount NOTIFY assetsChanged)
    Q_PROPERTY(int highRiskAssetCount READ highRiskAssetCount NOTIFY assetsChanged)
    Q_PROPERTY(QString newestAssetLabel READ newestAssetLabel NOTIFY assetsChanged)
public:
    enum AssetRoles {
        MacRole = Qt::UserRole + 1,
        IpsRole,
        HostnameRole,
        FirstSeenRole,
        LastSeenRole,
        SourcesRole,
        VendorRole,
        DeviceTypeRole,
        OsRole,
        RawMetadataRole,
        StatusRole,
        RiskRole,
        SourceSummaryRole,
        LastSeenSortRole
    };

    explicit AssetModel(QObject* parent = nullptr);

    // QAbstractItemModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Reload from SQLite database
    Q_INVOKABLE void reloadFromDatabase(const QString& dbPath);
    Q_INVOKABLE bool matchesSearch(int row, const QString& query) const;
    Q_INVOKABLE bool matchesFilters(
        int row,
        const QString& query,
        const QString& statusFilter,
        const QString& riskFilter,
        const QString& sourceFilter) const;
    Q_INVOKABLE QString networkGroupForRow(int row) const;
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QVariantMap assetForIdentity(const QString& identity) const;
    Q_INVOKABLE int rowForIdentity(const QString& identity) const;
    Q_INVOKABLE QVariantList timelineForAsset(const QString& identity) const;
    Q_INVOKABLE void sortByLastSeenDescending();
    Q_INVOKABLE void sortByMacAddress();
    Q_INVOKABLE bool exportToFile(const QString& path, const QString& format) const;

    int activeAssetCount() const;
    int newAssetsTodayCount() const;
    int highRiskAssetCount() const;
    QString newestAssetLabel() const;

signals:
    void assetsChanged();

private:
    QVector<AssetItem> assets_;
    QVector<AssetTimelineItem> timeline_;
};

} // namespace asset_discovery::gui
