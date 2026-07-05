#include "pnad/gui/CaptureServiceFacade.hpp"
#include "pnad/gui/CaptureController.hpp"

namespace asset_discovery::gui {

CaptureServiceFacade::CaptureServiceFacade(CaptureController* controller, QObject* parent)
    : QObject(parent), controller_(controller)
{
}

QString CaptureServiceFacade::sqlitePath() const
{
    return controller_ ? controller_->sqlitePath() : QString();
}

bool CaptureServiceFacade::isRunning() const
{
    return controller_ ? controller_->isRunning() : false;
}

QString CaptureServiceFacade::serviceStatus() const
{
    if (!controller_) {
        return QStringLiteral("stopped");
    }

    return controller_->isRunning() ? QStringLiteral("running") : QStringLiteral("stopped");
}

QString CaptureServiceFacade::backendExecutablePath() const
{
    return QStringLiteral("asset-discovery-backend");
}

void CaptureServiceFacade::startCapture()
{
    if (controller_) {
        controller_->startCapture();
    }
}

void CaptureServiceFacade::stopCapture()
{
    if (controller_) {
        controller_->stopCapture();
    }
}

} // namespace asset_discovery::gui
