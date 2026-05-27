/**
 * @file    DashboardController.cpp
 * @brief   Implementation của Qt Backend Controller
 *
 * Luồng dữ liệu:
 *   ESP32 → UART → QSerialPort → onSerialDataReady()
 *   → parseFrame() → emit signals → QML bindings tự cập nhật
 */

#include "DashboardController.h"
#include <QSerialPortInfo>
#include <QDebug>
#include <QRegularExpression>

// ─────────────────────────────────────────────
// CONSTRUCTOR / DESTRUCTOR
// ─────────────────────────────────────────────
DashboardController::DashboardController(QObject *parent)
    : QObject(parent)
{
    // ── Khởi tạo QSettings ──
    // Trên Linux: lưu tại ~/.config/AutomotiveDashboard/dashboard.ini
    m_settings = new QSettings("AutomotiveDashboard", "dashboard", this);

    // ── Load ODO đã lưu từ lần trước ──
    loadOdo();

    // ── Khởi tạo Serial Port ──
    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead,
            this, &DashboardController::onSerialDataReady);
    connect(m_serial, &QSerialPort::errorOccurred,
            this, &DashboardController::onSerialError);

    // ── Timer lưu ODO định kỳ (mỗi 5 giây) ──
    // Tránh ghi file quá thường xuyên gây wear trên storage
    m_saveTimer = new QTimer(this);
    m_saveTimer->setInterval(5000);
    connect(m_saveTimer, &QTimer::timeout, this, &DashboardController::saveOdo);
    m_saveTimer->start();

    qDebug() << "[DashboardController] Initialized. ODO loaded:" << m_odo << "km";
}

DashboardController::~DashboardController()
{
    // Lưu ODO cuối cùng khi thoát ứng dụng
    saveOdo();
    if (m_serial->isOpen()) {
        m_serial->close();
    }
}

// ─────────────────────────────────────────────
// KẾT NỐI SERIAL
// ─────────────────────────────────────────────
void DashboardController::connectSerial()
{
    if (m_serial->isOpen()) {
        qDebug() << "[Serial] Already open";
        return;
    }

    m_serial->setPortName(m_portName);
    m_serial->setBaudRate(QSerialPort::Baud115200);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serial->open(QIODevice::ReadWrite)) {
        m_connected = true;
        emit connectedChanged(true);
        qDebug() << "[Serial] Connected to" << m_portName << "@ 115200 baud";
    } else {
        QString err = QString("Cannot open %1: %2")
        .arg(m_portName)
            .arg(m_serial->errorString());
        qWarning() << "[Serial] Error:" << err;
        emit errorOccurred(err);
    }
}

void DashboardController::disconnectSerial()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        m_connected = false;
        emit connectedChanged(false);
        qDebug() << "[Serial] Disconnected";
    }
}

// ─────────────────────────────────────────────
// SLOT: NHẬN DỮ LIỆU UART
//
// Kỹ thuật: Tích lũy dữ liệu vào buffer cho đến khi
// gặp ký tự '\n' mới xử lý → tránh mất frame do
// TCP-like segmentation của UART
// ─────────────────────────────────────────────
void DashboardController::onSerialDataReady()
{
    m_rxBuffer.append(m_serial->readAll());

    // Xử lý tất cả frame hoàn chỉnh trong buffer
    while (true) {
        int newlinePos = m_rxBuffer.indexOf('\n');
        if (newlinePos < 0) break;  // Chưa có frame hoàn chỉnh

        // Tách ra 1 frame hoàn chỉnh
        QByteArray frameBytes = m_rxBuffer.left(newlinePos);
        m_rxBuffer.remove(0, newlinePos + 1);  // Xóa frame đã xử lý

        QString frame = QString::fromLatin1(frameBytes).trimmed();
        if (!frame.isEmpty()) {
            parseFrame(frame);
        }
    }

    // Phòng tràn buffer (> 1KB là bất thường)
    if (m_rxBuffer.size() > 1024) {
        qWarning() << "[Serial] Buffer overflow, clearing";
        m_rxBuffer.clear();
    }
}

// ─────────────────────────────────────────────
// PARSE FRAME UART
//
// Format: "S120,R4500,GD,T15.5"
//
// Phương pháp: Dùng QRegularExpression để extract
// từng field, an toàn hơn sscanf với C++ string
// ─────────────────────────────────────────────
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

    // ── Chỉ emit signal nếu giá trị thực sự thay đổi ──
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

    // Trip: cập nhật liên tục (không cần kiểm tra thay đổi)
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
// LƯU / ĐỌC ODO BẰNG QSETTINGS
//
// Chiến lược: Lưu ODO_base = tổng ODO khi reset trip
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

// ─────────────────────────────────────────────
// RESET TRIP (Gọi từ QML)
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
// TIỆN ÍCH
// ─────────────────────────────────────────────
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
    }
    return ports;
}

void DashboardController::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;

    QString msg = QString("Serial error [%1]: %2")
                      .arg(error)
                      .arg(m_serial->errorString());
    qWarning() << msg;

    if (error == QSerialPort::ResourceError) {
        // Thiết bị bị rút ra
        disconnectSerial();
        emit errorOccurred("Device disconnected unexpectedly");
    } else {
        emit errorOccurred(msg);
    }
}