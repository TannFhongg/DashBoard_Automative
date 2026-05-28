/**
 * @file    SerialWorker.h
 * @brief   Serial Worker chạy trên QThread riêng biệt
 *
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  VẤN ĐỀ CẦN GIẢI QUYẾT                                         │
 * │                                                                  │
 * │  Nếu đọc QSerialPort trực tiếp trên Main Thread:                │
 * │    → UI bị block khi UART busy hoặc dữ liệu đến liên tục 50FPS  │
 * │    → Gây giật lag animation của kim đồng hồ                     │
 * │                                                                  │
 * │  Giải pháp: Worker Pattern                                       │
 * │    SerialWorker (QObject) chạy trên QThread riêng               │
 * │    Giao tiếp với DashboardController qua Qt Signals/Slots       │
 * │    (thread-safe, không cần mutex thủ công)                      │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * Luồng hoạt động:
 *
 *   Main Thread                   Worker Thread
 *   ──────────────                ──────────────────────────────
 *   DashboardController           SerialWorker
 *        │                             │
 *        │──openPort(name)────────────▶│  mở QSerialPort
 *        │                             │  readyRead → onDataReady()
 *        │◀──frameReceived(str)────────│  emit khi có frame hoàn chỉnh
 *        │◀──errorOccurred(str)────────│  emit khi lỗi
 *        │──closePort()───────────────▶│  đóng port
 *
 * Kỹ thuật chính:
 *   - QObject::moveToThread(): di chuyển worker sang thread mới
 *   - Queued Connection: signal cross-thread tự động thread-safe
 *   - QSerialPort tạo trong Worker thread (không share giữa threads)
 */

#pragma once

#include <QObject>
#include <QThread>
#include <QByteArray>
#include <QString>
#include <QtSerialPort/QSerialPort>

// ══════════════════════════════════════════════════════════════════
// CLASS: SerialWorker
// Đối tượng này sẽ được moveToThread() → chạy trên Worker Thread
// Toàn bộ I/O serial xảy ra trên thread này, không đụng Main Thread
// ══════════════════════════════════════════════════════════════════
class SerialWorker : public QObject
{
    Q_OBJECT

public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker();

    // Trạng thái kết nối (có thể đọc từ bất kỳ thread nào nhờ Qt::QueuedConnection)
    bool isConnected() const { return m_connected; }

public slots:
    /**
     * @brief Mở cổng serial — phải được gọi qua signal/slot
     *        để đảm bảo thực thi trên đúng Worker Thread
     * @param portName  Tên cổng, ví dụ "/dev/ttyUSB0"
     * @param baudRate  Tốc độ baud, mặc định 115200
     */
    void openPort(const QString &portName, int baudRate = 115200);

    /**
     * @brief Đóng cổng serial — gọi trước khi quit thread
     */
    void closePort();

private slots:
    /**
     * @brief Slot nội bộ: kích hoạt bởi QSerialPort::readyRead
     *        Chạy trên Worker Thread → không block UI
     */
    void onDataReady();

    /**
     * @brief Slot xử lý lỗi serial
     */
    void onSerialError(QSerialPort::SerialPortError error);

signals:
    /**
     * @brief Phát ra khi parse được 1 frame hoàn chỉnh kết thúc bằng '\n'
     *        Kết nối Queued → DashboardController nhận trên Main Thread
     * @param frame  Chuỗi frame không có '\n', ví dụ "S120,R4500,GD,T15.5"
     */
    void frameReceived(const QString &frame);

    /**
     * @brief Phát ra khi kết nối thành công
     * @param portName  Tên cổng đã mở
     */
    void portOpened(const QString &portName);

    /**
     * @brief Phát ra khi đóng cổng (chủ động hoặc do lỗi)
     */
    void portClosed();

    /**
     * @brief Phát ra khi có lỗi
     * @param message  Mô tả lỗi
     */
    void errorOccurred(const QString &message);

    /**
     * @brief Thống kê: số frame nhận được mỗi giây (dùng cho status bar)
     * @param fps  Frame Per Second thực tế
     */
    void statsUpdated(int fps);

private:
    QSerialPort *m_serial   = nullptr;   // Tạo và sử dụng trên Worker Thread
    QByteArray   m_rxBuffer;             // Buffer tích lũy bytes chưa đủ frame
    bool         m_connected = false;

    // Thống kê FPS
    int      m_frameCount   = 0;
    qint64   m_lastStatTime = 0;

    // Xử lý buffer: tách frame từ byte stream
    void processBuffer();

    // Tính và phát stats FPS
    void updateStats();
};


// ══════════════════════════════════════════════════════════════════
// CLASS: SerialManager
// Lớp bọc (wrapper) tiện lợi:
//   - Tạo và quản lý QThread + SerialWorker
//   - Expose interface gọn gàng cho DashboardController
//   - Tự động cleanup khi destroy
// ══════════════════════════════════════════════════════════════════
class SerialManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager();

    bool isRunning() const;

public slots:
    /**
     * @brief Khởi động Worker Thread và mở cổng serial
     *        Thread-safe: có thể gọi từ Main Thread
     */
    void start(const QString &portName, int baudRate = 115200);

    /**
     * @brief Dừng Worker Thread và đóng cổng
     *        Thread-safe: có thể gọi từ Main Thread
     */
    void stop();

signals:
    // ── Forward signals từ SerialWorker lên DashboardController ──
    void frameReceived(const QString &frame);
    void portOpened(const QString &portName);
    void portClosed();
    void errorOccurred(const QString &message);
    void statsUpdated(int fps);

    // ── Internal signals để giao tiếp với Worker (qua Queued Connection) ──
    void requestOpen(const QString &portName, int baudRate);
    void requestClose();

private:
    QThread      *m_thread = nullptr;
    SerialWorker *m_worker = nullptr;
};