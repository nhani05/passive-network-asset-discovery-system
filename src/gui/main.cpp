#include <QApplication>
#include <QQmlApplicationEngine>
#include "pnad/gui/GuiApplicationRuntime.hpp"

int main(int argc, char* argv[])
{
    // Enable High DPI scaling if supported (standard Qt5/Qt6 practice)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication app(argc, argv);

    asset_discovery::gui::GuiApplicationRuntime guiRuntime;
    QQmlApplicationEngine engine;
    guiRuntime.initialize(engine);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject* obj, const QUrl& objUrl) {
                         if (!obj && url == objUrl) {
                             QCoreApplication::exit(-1);
                         }
                     }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
