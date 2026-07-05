#include "pnad/gui/InterfaceModel.hpp"

#include <QStringList>

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
    case AfPacketAvailableRole:
        return interfaceInfo.afPacketAvailable;
    case AfPacketDiagnosticRole:
        return QString::fromStdString(interfaceInfo.afPacketDiagnostic);
    case CaptureAllowedRole:
        return interfaceInfo.captureAllowed;
    case PermissionDiagnosticRole:
        return QString::fromStdString(interfaceInfo.permissionDiagnostic);
    case RefreshedAtRole:
        return QString::fromStdString(interfaceInfo.refreshedAt);
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
    roles[AfPacketAvailableRole] = "afPacketAvailable";
    roles[AfPacketDiagnosticRole] = "afPacketDiagnostic";
    roles[CaptureAllowedRole] = "captureAllowed";
    roles[PermissionDiagnosticRole] = "permissionDiagnostic";
    roles[RefreshedAtRole] = "refreshedAt";
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
    map.insert("afPacketAvailable", interfaceInfo.afPacketAvailable);
    map.insert("afPacketDiagnostic", QString::fromStdString(interfaceInfo.afPacketDiagnostic));
    map.insert("captureAllowed", interfaceInfo.captureAllowed);
    map.insert("permissionDiagnostic", QString::fromStdString(interfaceInfo.permissionDiagnostic));
    map.insert("refreshedAt", QString::fromStdString(interfaceInfo.refreshedAt));
    return map;
}

} // namespace asset_discovery::gui
