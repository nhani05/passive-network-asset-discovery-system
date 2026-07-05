#pragma once

#include "pnad/capture/NetworkInterface.hpp"

#include <QAbstractListModel>
#include <QVariantMap>

namespace asset_discovery::gui {

class InterfaceModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum InterfaceRoles {
        SystemNameRole = Qt::UserRole + 1,
        DisplayNameRole,
        MacAddressRole,
        AddressesRole,
        IsUpRole,
        IsRunningRole,
        IsLoopbackRole,
        IsVirtualRole,
        PcapAvailableRole,
        PcapDiagnosticRole,
        AfPacketAvailableRole,
        AfPacketDiagnosticRole,
        CaptureAllowedRole,
        PermissionDiagnosticRole,
        RefreshedAtRole,
    };

    explicit InterfaceModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE int findBySystemName(const QString& systemName) const;
    Q_INVOKABLE QString systemNameAt(int row) const;
    Q_INVOKABLE QVariantMap get(int row) const;

private:
    QVariantMap toMap(const capture::NetworkInterfaceInfo& interfaceInfo) const;

    std::vector<capture::NetworkInterfaceInfo> interfaces_;
};

} // namespace asset_discovery::gui
