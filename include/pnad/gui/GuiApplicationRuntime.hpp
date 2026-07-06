#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <memory>

class QQmlApplicationEngine;

namespace asset_discovery::gui {

class CaptureController;
class AssetModel;
class InterfaceModel;
class LogModel;

class GuiApplicationRuntime : public QObject
{
    Q_OBJECT

public:
    explicit GuiApplicationRuntime(QObject* parent = nullptr);
    ~GuiApplicationRuntime() override;

    void initialize(QQmlApplicationEngine& engine);
    void reloadModelsFromDatabase();

    CaptureController* captureController() const;
    AssetModel* assetModel() const;
    InterfaceModel* interfaceModel() const;
    LogModel* logModel() const;

private:
    void connectRefreshMechanism();
    void registerContextProperties(QQmlApplicationEngine& engine);
    void loadInitialModels();
    void clearApplicationData();

    std::unique_ptr<CaptureController> captureController_;
    std::unique_ptr<AssetModel> assetModel_;
    std::unique_ptr<InterfaceModel> interfaceModel_;
    std::unique_ptr<LogModel> logModel_;

    QTimer refreshTimer_;
    QDateTime lastDbModified_;
};

} // namespace asset_discovery::gui
