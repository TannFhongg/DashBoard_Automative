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
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    app.setApplicationName("AutomotiveDashboard");
    app.setOrganizationName("AutomotiveDashboard");

    // ── Tạo Backend Controller ──
    DashboardController controller;

    // ── Tự động detect cổng serial trên Linux ──
    // Thường là /dev/ttyUSB0 hoặc /dev/ttyACM0
    QStringList ports = controller.availablePorts();
    if (!ports.isEmpty()) {
        controller.setPortName("/dev/" + ports.first());
    } else {
        controller.setPortName("/dev/ttyUSB0");  // default
    }

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