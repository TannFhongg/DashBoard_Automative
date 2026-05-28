

#include "DashboardController.h"
#include <QSerialPortInfo>
#include <QDebug>
#include <QRegularExpression>

DashboardController::DashboardController(QObject *parent)
    : QObject(parent)
{
    //  Khởi tạo QSettings

    m_settings = new QSettings("AutomotiveDashboard", "dashboard", this);

    //  Load ODO đã lưu từ lần trước
    loadOdo();

    // Khởi tạo SerialManager (Worker Thread Pattern)
    m_serialManager = new SerialManager(this);

    // Kết nối signals từ SerialManager -> slots của Controller
    // Tất cả đều là Queued Connection ddeens thread-safe, nhận trên Main Thread
    connect(m_serialManager, &SerialManager::frameReceived,
            this,            &DashboardController::onFrameReceived);

    connect(m_serialManager, &SerialManager::portOpened,
            this,            &DashboardController::onPortOpened);

    connect(m_serialManager, &SerialManager::portClosed,
            this,            &DashboardController::onPortClosed);

    connect(m_serialManager, &SerialManager::statsUpdated,
            this,            &DashboardController::onSerialStats);

    connect(m_serialManager, &SerialManager::errorOccurred,
            this,            &DashboardController::errorOccurred);   // Chuyen tiep thẳng ra QML

    //  Timer lưu ODO định kỳ
    m_saveTimer = new QTimer(this);
    m_saveTimer->setInterval(5000);
    connect(m_saveTimer, &QTimer::timeout, this, &DashboardController::saveOdo);
    m_saveTimer->start();

    qDebug() << "[DashboardController] Initialized. ODO loaded:" << m_odo << "km";
}

DashboardController::~DashboardController()
{
    saveOdo();
    m_serialManager->stop();   // Dừng worker thread
}


// Connect / ngat SERIAL (Gọi từ QML)

void DashboardController::connectSerial()
{
    if (m_portName.isEmpty()) {
        emit errorOccurred("Port name is not set");
        return;
    }
    qDebug() << "[Controller] Starting SerialManager on" << m_portName;
    m_serialManager->start(m_portName, 115200);
}

void DashboardController::disconnectSerial()
{
    m_serialManager->stop();
}


// SLOTS: NHẬN TỪ SerialManager tất cả chạy trên Main Thread (Queued Connection)

void DashboardController::onPortOpened(const QString &portName)
{
    m_connected = true;
    emit connectedChanged(true);
    qDebug() << "[Controller] Port opened:" << portName;
}

void DashboardController::onPortClosed()
{
    m_connected = false;
    emit connectedChanged(false);
    // Reset giá trị về 0 khi mất kết nối
    if (m_speed != 0) { m_speed = 0; emit speedChanged(0); }
    if (m_rpm   != 0) { m_rpm   = 0; emit rpmChanged(0);   }
    qDebug() << "[Controller] Port closed";
}

void DashboardController::onFrameReceived(const QString &frame)
{
    // Frame đã được tách bởi SerialWorker
    parseFrame(frame);
}

void DashboardController::onSerialStats(int fps)
{
    if (m_serialFps != fps) {
        m_serialFps = fps;
        emit serialFpsChanged(fps);
    }
}

// PARSE FRAME UART
// Format: "S120,R4500,GD,T15.5"

void DashboardController::parseFrame(const QString &frame)
{
    // Pattern: S<int>,R<int>,G<char>,T<float>
    static QRegularExpression re(
        R"(S(\d+),R(\d+),G([PRND]),T([\d.]+))"
        );

    QRegularExpressionMatch match = re.match(frame);
    if (!match.hasMatch()) {
        qWarning() << "[Parse] Invalid frame:" << frame;
        return;
    }

    bool ok;
    int newSpeed = match.captured(1).toInt(&ok);
    if (!ok) return;

    int newRpm = match.captured(2).toInt(&ok);
    if (!ok) return;

    QString newGear = match.captured(3);

    double newTrip = match.captured(4).toDouble(&ok);
    if (!ok) return;

    //  Chỉ emit signal nếu giá trị thực sự thay đổi
    // Tối ưu: tránh QML re-render không cần thiết
    if (m_speed != newSpeed) {
        m_speed = newSpeed;
        emit speedChanged(m_speed);
    }

    if (m_rpm != newRpm) {
        m_rpm = newRpm;
        emit rpmChanged(m_rpm);
    }

    if (m_gear != newGear) {
        m_gear = newGear;
        emit gearChanged(m_gear);
    }

    // Trip: cập nhật liên tục
    if (qAbs(m_trip - newTrip) > 0.01) {
        m_trip = newTrip;
        emit tripChanged(m_trip);

        // Cập nhật ODO = ODO_base + trip hiện tại
        double newOdo = m_settings->value("odo_base", 0.0).toDouble() + m_trip;
        if (qAbs(m_odo - newOdo) > 0.01) {
            m_odo = newOdo;
            emit odoChanged(m_odo);
        }
    }
}

// ─────────────────────────────────────────────
// LƯU / ĐỌC ODO  QSETTINGS
//
//Lưu ODO_base = tổng ODO khi reset trip
// ODO hiển thị = ODO_base + trip hiện tại
// Khi trip reset: ODO_base += trip → trip = 0
// ─────────────────────────────────────────────
void DashboardController::saveOdo()
{
    m_settings->setValue("odo_km", m_odo);
    m_settings->setValue("odo_base", m_odo - m_trip);
    m_settings->sync();  // Flush xuống disk ngay lập tức
    qDebug() << "[Settings] ODO saved:" << m_odo << "km";
}

void DashboardController::loadOdo()
{
    m_odo = m_settings->value("odo_km", 0.0).toDouble();
    qDebug() << "[Settings] ODO loaded:" << m_odo << "km";
    emit odoChanged(m_odo);
}

// RESET TRIP

void DashboardController::resetTrip()
{
    // Gộp trip hiện tại vào ODO_base
    double currentBase = m_settings->value("odo_base", 0.0).toDouble();
    m_settings->setValue("odo_base", currentBase + m_trip);

    m_trip = 0.0;
    emit tripChanged(m_trip);
    saveOdo();
    qDebug() << "[Trip] Reset. New ODO base:" << (currentBase + m_trip);
}


void DashboardController::setPortName(const QString &name)
{
    if (m_portName != name) {
        m_portName = name;
        emit portNameChanged(name);
    }
}

QStringList DashboardController::availablePorts()
{
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ports << info.portName();
        qDebug() << "  Found port:" << info.portName() << "-" << info.description();
    }
    return ports;
}