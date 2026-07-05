#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <cstdint>

namespace asset_discovery::gui {

struct AnalysisSessionItem {
    std::int64_t id = 0;
    QString mode;
    QString source;
    QString startTime;
    QString endTime;
    QString status;
    int assetCount = 0;
    int eventCount = 0;
    QString errorSummary;
    QString storageContext;
};

class AnalysisSessionModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int completedSessionCount READ completedSessionCount NOTIFY sessionsChanged)
    Q_PROPERTY(QString latestSessionLabel READ latestSessionLabel NOTIFY sessionsChanged)
    Q_PROPERTY(int latestAssetCount READ latestAssetCount NOTIFY sessionsChanged)
    Q_PROPERTY(int latestEventCount READ latestEventCount NOTIFY sessionsChanged)

public:
    enum SessionRoles {
        IdRole = Qt::UserRole + 1,
        ModeRole,
        SourceRole,
        StartTimeRole,
        EndTimeRole,
        StatusRole,
        AssetCountRole,
        EventCountRole,
        ErrorSummaryRole,
        StorageContextRole
    };

    explicit AnalysisSessionModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString lastError() const { return lastError_; }
    int completedSessionCount() const;

    Q_INVOKABLE void reloadFromDatabase(const QString& dbPath);
    Q_INVOKABLE QString latestSourceForMode(const QString& mode) const;
    Q_INVOKABLE QVariantMap latestSession() const;
    Q_INVOKABLE int rowCountForQml() const { return rowCount(); }
    Q_INVOKABLE bool exportSummaryToFile(const QString& path, const QString& format) const;

    QString latestSessionLabel() const;
    int latestAssetCount() const;
    int latestEventCount() const;

signals:
    void lastErrorChanged();
    void sessionsChanged();

private:
    QVector<AnalysisSessionItem> sessions_;
    QString lastError_;
};

} // namespace asset_discovery::gui
