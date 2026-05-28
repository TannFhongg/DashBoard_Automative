
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include "backend/DashboardController.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setApplicationName("AutomotiveDashboard");
    app.setOrganizationName("AutomotiveDashboard");

    //  Tạo Backend Controller
    DashboardController controller;

    // ttyUSB* = USB-to-UART converter (CP2102, CH340, FTDI)
    QString autoPort;
    const QStringList ports = controller.availablePorts();
    for (const QString &p : ports) {
        if (p.startsWith("ttyUSB") || p.startsWith("ttyACM")) {
            autoPort = "/dev/" + p;
            break;
        }
    }
    if (autoPort.isEmpty()) {
        autoPort = "/dev/ttyUSB0";  //
        qWarning() << "[main] No ESP32 port found. Will try" << autoPort;
        qWarning() << "[main] Tip: plug in ESP32, then run: ls /dev/ttyUSB*";
    }
    controller.setPortName(autoPort);
    qDebug() << "[main] Using port:" << autoPort;

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("dashboard", &controller);

    // Load file QML chính
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML";
        return -1;
    }

    return app.exec();
}