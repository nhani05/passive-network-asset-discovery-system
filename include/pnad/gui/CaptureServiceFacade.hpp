#pragma once

#include <QObject>
#include <QString>

namespace asset_discovery::gui {

class CaptureController;

class CaptureServiceFacade : public QObject
{
    Q_OBJECT
public:
    explicit CaptureServiceFacade(CaptureController* controller, QObject* parent = nullptr);

    QString sqlitePath() const;
    bool isRunning() const;
    QString serviceStatus() const;
    QString backendExecutablePath() const;
    void startCapture();
    void stopCapture();

private:
    CaptureController* controller_;
};

} // namespace asset_discovery::gui
