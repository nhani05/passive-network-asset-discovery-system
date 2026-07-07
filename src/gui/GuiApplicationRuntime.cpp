#include "pnad/gui/GuiApplicationRuntime.hpp"

#include "pnad/constants/GuiConstants.hpp"

#include <QFileInfo>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QVariantMap>

#include <exception>

#include "pnad/gui/AssetModel.hpp"
#include "pnad/gui/CaptureController.hpp"
#include "pnad/gui/InterfaceModel.hpp"
#include "pnad/gui/LogModel.hpp"
#include "pnad/storage/SQLiteWriter.hpp"

namespace asset_discovery::gui {

GuiApplicationRuntime::GuiApplicationRuntime(QObject* parent)
    : QObject(parent)
{
    captureController_ = std::make_unique<CaptureController>(this);
    assetModel_ = std::make_unique<AssetModel>(this);
    interfaceModel_ = std::make_unique<InterfaceModel>(this);
    logModel_ = std::make_unique<LogModel>(this);

    refreshTimer_.setInterval(constants::gui::RefreshIntervalMs);
    refreshTimer_.setTimerType(Qt::CoarseTimer);
}

GuiApplicationRuntime::~GuiApplicationRuntime() = default;

void GuiApplicationRuntime::initialize(QQmlApplicationEngine& engine)
{
    registerContextProperties(engine);
    connectRefreshMechanism();
    // clearApplicationData();
    loadInitialModels();
}

void GuiApplicationRuntime::reloadModelsFromDatabase()
{
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
}

CaptureController* GuiApplicationRuntime::captureController() const
{
    return captureController_.get();
}

AssetModel* GuiApplicationRuntime::assetModel() const
{
    return assetModel_.get();
}

InterfaceModel* GuiApplicationRuntime::interfaceModel() const
{
    return interfaceModel_.get();
}

LogModel* GuiApplicationRuntime::logModel() const
{
    return logModel_.get();
}

void GuiApplicationRuntime::connectRefreshMechanism()
{
    QObject::connect(captureController_.get(), &CaptureController::eventLogMessage,
                     this, [this](const QString& timestamp, const QString& severity, const QString& source, const QString& message) {
                         QVariantMap log;
                         log.insert("timestamp", timestamp);
                         log.insert("severity", severity);
                         log.insert("source", source);
                         log.insert("message", message);
                         logModel_->appendLogDto(log);
                     }, Qt::QueuedConnection);

    QObject::connect(captureController_.get(), &CaptureController::assetDiscovered,
                     this, [this](const QVariantMap& asset, bool) {
                         assetModel_->applyAssetDto(asset);
                     }, Qt::QueuedConnection);

    QObject::connect(captureController_.get(), &CaptureController::isRunningChanged,
                     this, [this]() {
                         if (captureController_->isRunning()) {
                             reloadModelsFromDatabase();
                             refreshTimer_.start();
                         } else {
                             refreshTimer_.stop();
                         }
                     }, Qt::QueuedConnection);

    QObject::connect(captureController_.get(), &CaptureController::captureFinished,
                     this, &GuiApplicationRuntime::reloadModelsFromDatabase,
                     Qt::QueuedConnection);

    QObject::connect(&refreshTimer_, &QTimer::timeout,
                     this, &GuiApplicationRuntime::reloadModelsFromDatabase,
                     Qt::QueuedConnection);
}

void GuiApplicationRuntime::registerContextProperties(QQmlApplicationEngine& engine)
{
    engine.rootContext()->setContextProperty("captureController", captureController_.get());
    engine.rootContext()->setContextProperty("assetModel", assetModel_.get());
    engine.rootContext()->setContextProperty("interfaceModel", interfaceModel_.get());
    engine.rootContext()->setContextProperty("logModel", logModel_.get());
}

void GuiApplicationRuntime::loadInitialModels()
{
    assetModel_->reloadFromDatabase(captureController_->sqlitePath());
}

void GuiApplicationRuntime::clearApplicationData()
{
    try {
        storage::SQLiteWriter writer(captureController_->sqlitePath().toStdString());
        if (const auto error = writer.clearApplicationData(); error.has_value()) {
            qWarning() << "Failed to clear SQLite application data:" << QString::fromStdString(*error);
        }
    } catch (const std::exception& error) {
        qWarning() << "Failed to clear SQLite application data:" << error.what();
    }
}

} // namespace asset_discovery::gui
