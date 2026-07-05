#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace asset_discovery::gui {

struct EventItem {
    int id;
    QString eventTime;
    QString eventType;
    QString severity;
    QString ipAddress;
    QString macAddress;
    QString oldIp;
    QString newIp;
    QString oldMac;
    QString newMac;
    QString hostname;
    QString protocol;
    QString interface;
    QString message;
    QString rawMetadata;
};

class EventModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int highSeverityCount READ highSeverityCount NOTIFY eventsChanged)
    Q_PROPERTY(int warningSeverityCount READ warningSeverityCount NOTIFY eventsChanged)
    Q_PROPERTY(int unresolvedCount READ unresolvedCount NOTIFY eventsChanged)
public:
    enum EventRoles {
        IdRole = Qt::UserRole + 1,
        EventTimeRole,
        EventTypeRole,
        SeverityRole,
        IpAddressRole,
        MacAddressRole,
        OldIpRole,
        NewIpRole,
        OldMacRole,
        NewMacRole,
        HostnameRole,
        ProtocolRole,
        InterfaceRole,
        MessageRole,
        RawMetadataRole,
        EventLabelRole,
        AssetLabelRole,
        ResolvedRole
    };

    explicit EventModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void reloadFromDatabase(const QString& dbPath);
    Q_INVOKABLE bool matchesFilter(int row, const QString& query, const QString& severityFilter) const;
    Q_INVOKABLE QString relatedAssetQuery(int row) const;
    Q_INVOKABLE QString readableEventType(int row) const;
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE bool exportEvidenceToFile(int row, const QString& path, const QString& format) const;
    Q_INVOKABLE bool exportToFile(const QString& path, const QString& format) const;

    int highSeverityCount() const;
    int warningSeverityCount() const;
    int unresolvedCount() const;

signals:
    void eventsChanged();

private:
    QVector<EventItem> events_;
};

} // namespace asset_discovery::gui
