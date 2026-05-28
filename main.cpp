/**
 * @file    main.cpp
 * @brief   Entry point Qt Application
 *
 * Đăng ký DashboardController vào QML context,
 * sau đó load giao diện main.qml
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include "backend/DashboardController.h"

int main(int argc, char *argv[])
{
    // High-DPI support
    //QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    app.setApplicationName("AutomotiveDashboard");
    app.setOrganizationName("AutomotiveDashboard");

    // ── Tạo Backend Controller ──
    DashboardController controller;

    // ── Tự động detect cổng serial ESP32 trên Linux ──
    // ttyUSB* = USB-to-UART converter (CP2102, CH340, FTDI)
    // ttyACM* = USB CDC device (một số board ESP32-S3)
    // BỎ QUA ttyS* = cổng serial ảo của kernel, không phải ESP32
    QString autoPort;
    const QStringList ports = controller.availablePorts();
    for (const QString &p : ports) {
        if (p.startsWith("ttyUSB") || p.startsWith("ttyACM")) {
            autoPort = "/dev/" + p;
            break;
        }
    }
    if (autoPort.isEmpty()) {
        autoPort = "/dev/ttyUSB0";  // fallback mặc định
        qWarning() << "[main] No ESP32 port found. Will try" << autoPort;
        qWarning() << "[main] Tip: plug in ESP32, then run: ls /dev/ttyUSB*";
    }
    controller.setPortName(autoPort);
    qDebug() << "[main] Using port:" << autoPort;

    // ── QML Engine ──
    QQmlApplicationEngine engine;

    // Đăng ký controller vào QML context với tên "dashboard"
    // QML có thể dùng: dashboard.speed, dashboard.rpm, v.v.
    engine.rootContext()->setContextProperty("dashboard", &controller);

    // Load file QML chính
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML";
        return -1;
    }

    return app.exec();
}