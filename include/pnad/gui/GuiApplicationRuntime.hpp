#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <memory>

class QQmlApplicationEngine;

namespace asset_discovery::gui {

class CaptureController;
class CaptureServiceFacade;
class AssetModel;
class EventModel;
class InterfaceModel;
class AnalysisSessionModel;
class HealthDiagnosticsModel;

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
    EventModel* eventModel() const;
    InterfaceModel* interfaceModel() const;
    AnalysisSessionModel* analysisSessionModel() const;
    HealthDiagnosticsModel* healthDiagnosticsModel() const;

private:
    void connectRefreshMechanism();
    void registerContextProperties(QQmlApplicationEngine& engine);
    void loadInitialModels();

    std::unique_ptr<CaptureController> captureController_;
    std::unique_ptr<CaptureServiceFacade> captureServiceFacade_;
    std::unique_ptr<AssetModel> assetModel_;
    std::unique_ptr<EventModel> eventModel_;
    std::unique_ptr<InterfaceModel> interfaceModel_;
    std::unique_ptr<AnalysisSessionModel> analysisSessionModel_;
    std::unique_ptr<HealthDiagnosticsModel> healthDiagnosticsModel_;

    QTimer refreshTimer_;
    QDateTime lastDbModified_;
};

} // namespace asset_discovery::gui
