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
    QString firstSeen;
    QString lastSeen;
    QStringList discoverySources;
};

class AssetModel final : public QAbstractListModel {
    Q_OBJECT
public:
    enum AssetRoles {
        MacRole = Qt::UserRole + 1,
        IpsRole,
        HostnameRole,
        FirstSeenRole,
        LastSeenRole,
        SourcesRole
    };

    explicit AssetModel(QObject* parent = nullptr);

    // QAbstractItemModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Reload from SQLite database
    Q_INVOKABLE void reloadFromDatabase(const QString& dbPath);
    Q_INVOKABLE void loadAssetDtos(const QVariantList& assets);
    Q_INVOKABLE void applyAssetDto(const QVariantMap& asset);
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE bool exportToFile(const QString& path, const QString& format) const;

signals:
    void assetsChanged();

private:
    QVector<AssetItem> assets_;
};

} // namespace asset_discovery::gui
