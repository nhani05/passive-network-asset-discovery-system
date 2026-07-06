#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace asset_discovery::gui {

struct LogItem {
    QString timestamp;
    QString severity;
    QString source;
    QString message;
};

class LogModel final : public QAbstractListModel {
    Q_OBJECT
public:
    enum LogRoles {
        TimestampRole = Qt::UserRole + 1,
        SeverityRole,
        SourceRole,
        MessageRole
    };

    explicit LogModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void loadLogDtos(const QVariantList& logs);
    Q_INVOKABLE void appendLogDto(const QVariantMap& log);
    Q_INVOKABLE void clear();

private:
    QVector<LogItem> logs_;
};

} // namespace asset_discovery::gui
