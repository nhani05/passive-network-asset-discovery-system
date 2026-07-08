#include "pnad/gui/InterfaceModel.hpp"

#include <QStringList>
#include <utility>

namespace asset_discovery::gui {
namespace {

QStringList formatAddresses(const capture::NetworkInterfaceInfo& interfaceInfo)
{
    QStringList addresses;
    for (const auto& address : interfaceInfo.addresses) {
        QString value = QString::fromStdString(address.address);
        if (address.prefixLength != 0) {
            value += "/" + QString::number(address.prefixLength);
        }
        addresses.append(value);
    }
    return addresses;
}

} // namespace

InterfaceModel::InterfaceModel(QObject* parent)
    : QAbstractListModel(parent)
{
    refresh();
}

QString InterfaceModel::readinessFor(const capture::NetworkInterfaceInfo& interfaceInfo)
{
    if (interfaceInfo.captureAllowed) {
        return "ready";
    }
    if (!interfaceInfo.isUp) {
        return "down";
    }
    if (interfaceInfo.isLoopback) {
        return "loopback";
    }
    if (interfaceInfo.isVirtual) {
        return "virtual";
    }
    if (!interfaceInfo.pcapAvailable) {
        return "backend unavailable";
    }
    return "permission required";
}

QString InterfaceModel::readinessDiagnosticFor(const capture::NetworkInterfaceInfo& interfaceInfo)
{
    if (interfaceInfo.captureAllowed) {
        if (interfaceInfo.isVirtual) {
            return "virtual interface is capture-capable but not recommended for network discovery";
        }
        return "ready for live capture";
    }
    if (!interfaceInfo.permissionDiagnostic.empty()) {
        return QString::fromStdString(interfaceInfo.permissionDiagnostic);
    }
    return readinessFor(interfaceInfo);
}

QString InterfaceModel::displayLabelFor(const capture::NetworkInterfaceInfo& interfaceInfo)
{
    QStringList parts;
    parts.append(QString::fromStdString(interfaceInfo.systemName));
    const auto addresses = formatAddresses(interfaceInfo);
    if (!addresses.isEmpty()) {
        parts.append(addresses.join(", "));
    }
    parts.append(readinessFor(interfaceInfo));
    return parts.join(" - ");
}

int InterfaceModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(interfaces_.size());
}

QVariant InterfaceModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto& interfaceInfo = interfaces_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case SystemNameRole:
        return QString::fromStdString(interfaceInfo.systemName);
    case DisplayNameRole:
        return QString::fromStdString(interfaceInfo.displayName);
    case MacAddressRole:
        return interfaceInfo.macAddress.empty() ? "-" : QString::fromStdString(interfaceInfo.macAddress);
    case AddressesRole:
        return formatAddresses(interfaceInfo);
    case IsUpRole:
        return interfaceInfo.isUp;
    case IsRunningRole:
        return interfaceInfo.isRunning;
    case IsLoopbackRole:
        return interfaceInfo.isLoopback;
    case IsVirtualRole:
        return interfaceInfo.isVirtual;
    case PcapAvailableRole:
        return interfaceInfo.pcapAvailable;
    case PcapDiagnosticRole:
        return QString::fromStdString(interfaceInfo.pcapDiagnostic);
    case CaptureAllowedRole:
        return interfaceInfo.captureAllowed;
    case PermissionDiagnosticRole:
        return QString::fromStdString(interfaceInfo.permissionDiagnostic);
    case RefreshedAtRole:
        return QString::fromStdString(interfaceInfo.refreshedAt);
    case DisplayLabelRole:
        return displayLabelFor(interfaceInfo);
    case ReadinessRole:
        return readinessFor(interfaceInfo);
    case ReadinessDiagnosticRole:
        return readinessDiagnosticFor(interfaceInfo);
    default:
        return {};
    }
}

QHash<int, QByteArray> InterfaceModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SystemNameRole] = "systemName";
    roles[DisplayNameRole] = "displayName";
    roles[MacAddressRole] = "macAddress";
    roles[AddressesRole] = "addresses";
    roles[IsUpRole] = "isUp";
    roles[IsRunningRole] = "isRunning";
    roles[IsLoopbackRole] = "isLoopback";
    roles[IsVirtualRole] = "isVirtual";
    roles[PcapAvailableRole] = "pcapAvailable";
    roles[PcapDiagnosticRole] = "pcapDiagnostic";
    roles[CaptureAllowedRole] = "captureAllowed";
    roles[PermissionDiagnosticRole] = "permissionDiagnostic";
    roles[RefreshedAtRole] = "refreshedAt";
    roles[DisplayLabelRole] = "displayLabel";
    roles[ReadinessRole] = "readiness";
    roles[ReadinessDiagnosticRole] = "readinessDiagnostic";
    return roles;
}

void InterfaceModel::refresh()
{
    beginResetModel();
    interfaces_ = capture::listNetworkInterfaces();
    endResetModel();
}

int InterfaceModel::findBySystemName(const QString& systemName) const
{
    for (std::size_t index = 0; index < interfaces_.size(); ++index) {
        if (interfaces_[index].systemName == systemName.toStdString()) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

QString InterfaceModel::systemNameAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }
    return QString::fromStdString(interfaces_[static_cast<std::size_t>(row)].systemName);
}

int InterfaceModel::firstCaptureAllowedRow() const
{
    for (std::size_t index = 0; index < interfaces_.size(); ++index) {
        const auto& interfaceInfo = interfaces_[index];
        if (interfaceInfo.captureAllowed && !interfaceInfo.isLoopback) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

QString InterfaceModel::preferredSystemName(const QString& currentSystemName, bool preserveCurrentSelection) const
{
    const int currentRow = findBySystemName(currentSystemName);
    if (preserveCurrentSelection && currentRow >= 0) {
        return currentSystemName;
    }
    if (currentRow >= 0) {
        const auto& current = interfaces_[static_cast<std::size_t>(currentRow)];
        if (current.captureAllowed && !current.isLoopback) {
            return currentSystemName;
        }
    }

    const int readyRow = firstCaptureAllowedRow();
    if (readyRow >= 0) {
        return systemNameAt(readyRow);
    }
    return currentRow >= 0 ? currentSystemName : QString();
}

QVariantMap InterfaceModel::get(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }
    return toMap(interfaces_[static_cast<std::size_t>(row)]);
}

QVariantMap InterfaceModel::toMap(const capture::NetworkInterfaceInfo& interfaceInfo) const
{
    QVariantMap map;
    map.insert("systemName", QString::fromStdString(interfaceInfo.systemName));
    map.insert("displayName", QString::fromStdString(interfaceInfo.displayName));
    map.insert("macAddress", interfaceInfo.macAddress.empty() ? "-" : QString::fromStdString(interfaceInfo.macAddress));
    map.insert("addresses", formatAddresses(interfaceInfo));
    map.insert("isUp", interfaceInfo.isUp);
    map.insert("isRunning", interfaceInfo.isRunning);
    map.insert("isLoopback", interfaceInfo.isLoopback);
    map.insert("isVirtual", interfaceInfo.isVirtual);
    map.insert("pcapAvailable", interfaceInfo.pcapAvailable);
    map.insert("pcapDiagnostic", QString::fromStdString(interfaceInfo.pcapDiagnostic));
    map.insert("captureAllowed", interfaceInfo.captureAllowed);
    map.insert("permissionDiagnostic", QString::fromStdString(interfaceInfo.permissionDiagnostic));
    map.insert("refreshedAt", QString::fromStdString(interfaceInfo.refreshedAt));
    map.insert("displayLabel", displayLabelFor(interfaceInfo));
    map.insert("readiness", readinessFor(interfaceInfo));
    map.insert("readinessDiagnostic", readinessDiagnosticFor(interfaceInfo));
    return map;
}

void InterfaceModel::setInterfacesForTesting(std::vector<capture::NetworkInterfaceInfo> interfaces)
{
    capture::sortNetworkInterfacesForDisplay(interfaces);
    beginResetModel();
    interfaces_ = std::move(interfaces);
    endResetModel();
}

} // namespace asset_discovery::gui
