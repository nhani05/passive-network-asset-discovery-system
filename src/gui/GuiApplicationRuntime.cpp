#include "pnad/gui/GuiApplicationRuntime.hpp"

#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "pnad/gui/AnalysisSessionModel.hpp"
#include "pnad/gui/AssetModel.hpp"
#include "pnad/gui/CaptureController.hpp"
#include "pnad/gui/CaptureServiceFacade.hpp"
#include "pnad/gui/EventModel.hpp"
#include "pnad/gui/HealthDiagnosticsModel.hpp"
#include "pnad/gui/InterfaceModel.hpp"

namespace asset_discovery::gui {

GuiApplicationRuntime::GuiApplicationRuntime(QObject* parent)
    : QObject(parent)
{
    captureController_ = std::make_unique<CaptureController>(this);
    captureServiceFacade_ = std::make_unique<CaptureServiceFacade>(captureController_.get(), this);
    assetModel_ = std::make_unique<AssetModel>(this);
    eventModel_ = std::make_unique<EventModel>(this);
    interfaceModel_ = std::make_unique<InterfaceModel>(this);
    analysisSessionModel_ = std::make_unique<AnalysisSessionModel>(this);
    healthDiagnosticsModel_ = std::make_unique<HealthDiagnosticsModel>(this);

    refreshTimer_.setInterval(1000);
    refreshTimer_.setTimerType(Qt::CoarseTimer);
}

GuiApplicationRuntime::~GuiApplicationRuntime() = default;

void GuiApplicationRuntime::initialize(QQmlApplicationEngine& engine)
{
    registerContextProperties(engine);
    connectRefreshMechanism();
    loadInitialModels();
}

void GuiApplicationRuntime::reloadModelsFromDatabase()
{
    if (!captureController_->isRunning()) {
        return;
    }

    const QString dbPath = captureController_->sqlitePath();
    const QFileInfo dbInfo(dbPath);
    if (dbInfo.exists()) {
        const QDateTime currentDbModified = dbInfo.lastModified();
        if (lastDbModified_.isValid() && currentDbModified <= lastDbModified_) {
            return;
        }
        lastDbModified_ = currentDbModified;
    }

    assetModel_->reloadFromDatabase(dbPath);
    eventModel_->reloadFromDatabase(dbPath);
    analysisSessionModel_->reloadFromDatabase(dbPath);
}

CaptureController* GuiApplicationRuntime::captureController() const
{
    return captureController_.get();
}

AssetModel* GuiApplicationRuntime::assetModel() const
{
    return assetModel_.get();
}

EventModel* GuiApplicationRuntime::eventModel() const
{
    return eventModel_.get();
}

InterfaceModel* GuiApplicationRuntime::interfaceModel() const
{
    return interfaceModel_.get();
}

AnalysisSessionModel* GuiApplicationRuntime::analysisSessionModel() const
{
    return analysisSessionModel_.get();
}

HealthDiagnosticsModel* GuiApplicationRuntime::healthDiagnosticsModel() const
{
    return healthDiagnosticsModel_.get();
}

void GuiApplicationRuntime::connectRefreshMechanism()
{
    QObject::connect(captureController_.get(), &CaptureController::isRunningChanged,
                     this, [this]() {
                         if (captureController_->isRunning()) {
                             refreshTimer_.start();
                             reloadModelsFromDatabase();
                         } else {
                             refreshTimer_.stop();
                         }
                     }, Qt::QueuedConnection);

    QObject::connect(&refreshTimer_, &QTimer::timeout,
                     this, &GuiApplicationRuntime::reloadModelsFromDatabase,
                     Qt::QueuedConnection);
}

void GuiApplicationRuntime::registerContextProperties(QQmlApplicationEngine& engine)
{
    engine.rootContext()->setContextProperty("captureController", captureController_.get());
    engine.rootContext()->setContextProperty("captureServiceFacade", captureServiceFacade_.get());
    engine.rootContext()->setContextProperty("assetModel", assetModel_.get());
    engine.rootContext()->setContextProperty("eventModel", eventModel_.get());
    engine.rootContext()->setContextProperty("interfaceModel", interfaceModel_.get());
    engine.rootContext()->setContextProperty("analysisSessionModel", analysisSessionModel_.get());
    engine.rootContext()->setContextProperty("healthDiagnosticsModel", healthDiagnosticsModel_.get());
}

void GuiApplicationRuntime::loadInitialModels()
{
    assetModel_->reloadFromDatabase(captureController_->sqlitePath());
    eventModel_->reloadFromDatabase(captureController_->sqlitePath());
    analysisSessionModel_->reloadFromDatabase(captureController_->sqlitePath());
}

} // namespace asset_discovery::gui
