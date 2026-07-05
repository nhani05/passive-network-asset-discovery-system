#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace asset_discovery::gui {

struct HealthDiagnosticItem {
    QString category;
    QString state;
    QString detail;
    QString remediation;
};

class HealthDiagnosticsModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum HealthRoles {
        CategoryRole = Qt::UserRole + 1,
        StateRole,
        DetailRole,
        RemediationRole
    };

    explicit HealthDiagnosticsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh(
        const QString& sqlitePath,
        const QString& interfaceName,
        const QString& backendPolicy,
        const QString& lastError,
        const QString& statusText,
        const QString& recentFailureSummary,
        const QString& runtimeLogPath);

private:
    QVector<HealthDiagnosticItem> items_;
};

} // namespace asset_discovery::gui
