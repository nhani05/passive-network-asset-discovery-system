#include "pnad/gui/HealthDiagnosticsModel.hpp"

#include "pnad/capture/NetworkInterface.hpp"
#include "pnad/storage/SQLiteWriter.hpp"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace asset_discovery::gui {
namespace {

HealthDiagnosticItem makeItem(
    QString category,
    QString state,
    QString detail,
    QString remediation = {})
{
    HealthDiagnosticItem item;
    item.category = std::move(category);
    item.state = std::move(state);
    item.detail = std::move(detail);
    item.remediation = std::move(remediation);
    return item;
}

QString renderBackendDetail(
    const capture::NetworkInterfaceInfo* interfaceInfo,
    const QString& backendPolicy)
{
    if (interfaceInfo == nullptr) {
        return "Select a network interface in Capture Center to inspect capture backend readiness.";
    }
    QStringList available;
    if (interfaceInfo->pcapAvailable) {
        available << "pcap";
    }
    if (interfaceInfo->afPacketAvailable) {
        available << "af-packet";
    }
    return "Policy " + backendPolicy + "; available: "
        + (available.isEmpty() ? QString("none") : available.join(", "));
}

} // namespace

HealthDiagnosticsModel::HealthDiagnosticsModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int HealthDiagnosticsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return items_.size();
}

QVariant HealthDiagnosticsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) {
        return QVariant();
    }

    const auto& item = items_[index.row()];
    switch (role) {
    case CategoryRole:
        return item.category;
    case StateRole:
        return item.state;
    case DetailRole:
        return item.detail;
    case RemediationRole:
        return item.remediation;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> HealthDiagnosticsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[CategoryRole] = "category";
    roles[StateRole] = "state";
    roles[DetailRole] = "detail";
    roles[RemediationRole] = "remediation";
    return roles;
}

void HealthDiagnosticsModel::refresh(
    const QString& sqlitePath,
    const QString& interfaceName,
    const QString& backendPolicy,
    const QString& lastError,
    const QString& statusText,
    const QString& recentFailureSummary,
    const QString& runtimeLogPath)
{
    QVector<HealthDiagnosticItem> nextItems;

    const auto interfaces = capture::listNetworkInterfaces();
    const auto selected = std::find_if(
        interfaces.begin(),
        interfaces.end(),
        [&](const auto& item) {
            return QString::fromStdString(item.systemName) == interfaceName;
        });
    const capture::NetworkInterfaceInfo* selectedInterface =
        selected == interfaces.end() ? nullptr : &(*selected);

    if (selectedInterface == nullptr) {
        nextItems.push_back(makeItem(
            "Selected interface",
            "Warning",
            "No live interface selected.",
            "Open Capture Center and choose a network interface."));
        nextItems.push_back(makeItem(
            "Capture permission",
            "Warning",
            "No live interface selected.",
            "Open Capture Center and choose a network interface before starting Live Capture."));
    } else if (selectedInterface->captureAllowed) {
        nextItems.push_back(makeItem(
            "Selected interface",
            "OK",
            QString::fromStdString(selectedInterface->systemName) + " is available for Live Capture."));
        nextItems.push_back(makeItem(
            "Capture permission",
            "OK",
            QString::fromStdString(selectedInterface->systemName) + " is ready for Live Capture."));
    } else {
        nextItems.push_back(makeItem(
            "Selected interface",
            selectedInterface->isUp ? "Warning" : "Error",
            QString::fromStdString(selectedInterface->systemName) + (selectedInterface->isUp ? " is visible but not fully ready." : " is not running."),
            "Refresh interfaces or choose another network interface."));
        nextItems.push_back(makeItem(
            "Capture permission",
            "Error",
            QString::fromStdString(selectedInterface->permissionDiagnostic),
            "Choose another interface or update local capture permissions."));
    }

    const bool backendReady = selectedInterface != nullptr
        && (selectedInterface->pcapAvailable || selectedInterface->afPacketAvailable);
    nextItems.push_back(makeItem(
        "Backend availability",
        backendReady ? "OK" : "Warning",
        renderBackendDetail(selectedInterface, backendPolicy),
        backendReady ? QString() : QString("Install or enable a supported packet capture backend.")));

    bool databaseReady = false;
    QString databaseDetail;
    const QFileInfo databaseInfo(sqlitePath);
    const QDir databaseDir(databaseInfo.absoluteDir());
    if (sqlitePath.trimmed().isEmpty()) {
        databaseDetail = "Local database is not configured.";
    } else if (!databaseDir.exists()
        || !databaseDir.isReadable()
        || !QFileInfo(databaseDir.absolutePath()).isWritable()) {
        databaseDetail = "Database folder is not writable: " + databaseDir.absolutePath();
    } else {
        try {
            storage::SQLiteWriter writer(sqlitePath.toStdString());
            int count = 0;
            const auto countError = writer.countAssets(count);
            if (countError.has_value()) {
                databaseDetail = QString::fromStdString(*countError);
            } else {
                databaseReady = true;
                databaseDetail = sqlitePath;
            }
        } catch (const std::exception& exception) {
            databaseDetail = QString::fromStdString(exception.what());
        }
    }
    nextItems.push_back(makeItem(
        "Local database",
        databaseReady ? "OK" : "Error",
        databaseDetail,
        databaseReady ? QString() : QString("Choose a writable local database location in Preferences.")));

    nextItems.push_back(makeItem(
        "Runtime status",
        lastError.trimmed().isEmpty() ? "OK" : "Error",
        lastError.trimmed().isEmpty()
            ? (recentFailureSummary.trimmed().isEmpty() ? statusText : recentFailureSummary)
            : lastError,
        lastError.trimmed().isEmpty() ? QString() : QString("Review the latest worker failure and rerun the workflow.")));

    nextItems.push_back(makeItem(
        "Parser",
        "OK",
        "Built-in ARP, DHCP, DNS, NetBIOS, SSDP, and TCP parser plugins are available through the desktop engine."));

    nextItems.push_back(makeItem(
        "Parser queue",
        "Warning",
        "Queue depth is not exposed by the current capture runtime.",
        "Use session completion and runtime errors as the current readiness signals."));

    nextItems.push_back(makeItem(
        "Dropped counters",
        "Warning",
        "Dropped packet and dropped event counters are not exposed by the current capture runtime.",
        "Counters are shown as unavailable instead of estimated."));

    nextItems.push_back(makeItem(
        "Resource usage",
        "Warning",
        "CPU and memory usage are not exposed by the current desktop diagnostics model.",
        "Use operating-system monitoring for detailed process resource usage."));

    const QString backend = QString::fromLocal8Bit(qgetenv("QT_QUICK_BACKEND"));
    nextItems.push_back(makeItem(
        "Rendering",
        backend == "software" ? "Warning" : "OK",
        backend == "software"
            ? QString("Qt Quick software rendering fallback is active.")
            : QString("Qt Quick rendering backend is available."),
        backend == "software"
            ? QString("Software rendering is supported, but hardware acceleration may improve large views.")
            : QString()));

    nextItems.push_back(makeItem(
        "Runtime log",
        runtimeLogPath.trimmed().isEmpty() ? "Warning" : "OK",
        runtimeLogPath.trimmed().isEmpty() ? QString("Runtime failure log is not configured.") : runtimeLogPath));

    beginResetModel();
    items_ = std::move(nextItems);
    endResetModel();
}

} // namespace asset_discovery::gui
