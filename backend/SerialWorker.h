
#pragma once

#include <QObject>
#include <QThread>
#include <QByteArray>
#include <QString>
#include <QtSerialPort/QSerialPort>


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
      Mở cổng serial hải được gọi qua signal/slot để đảm bảo thực thi trên đúng Worker Thread
      portName  Tên cổng
      baudRate  Tốc độ baud 115200
     */
    void openPort(const QString &portName, int baudRate = 115200);

    /** Đóng cổng serial — gọi trước khi quit thread
     */
    void closePort();

private slots:
    /**Slot nội bộ: kích hoạt bởi QSerialPort::readyRead Chạy trên Worker Thread
     */
    void onDataReady();

    /**Slot xử lý lỗi serial
     */
    void onSerialError(QSerialPort::SerialPortError error);

signals:
    /**
  Phát ra khi parse được 1 frame hoàn chỉnh kết thúc bằng '\n'
     */
    void frameReceived(const QString &frame);

    /**
      Phát ra khi kết nối thành công
      portName  Tên cổng đã mở
     */
    void portOpened(const QString &portName);

    /**
    Phát ra khi đóng cổng
     */
    void portClosed();

    /**
     Phát ra khi có lỗi
     message  Mô tả lỗi
     */
    void errorOccurred(const QString &message);

    /**
    Thống kêsố frame nhận được mỗi giây (dùng cho status bar)
    fps  Frame Per Second thực tế
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



//   - Tạo và quản lý QThread + SerialWorker

class SerialManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager();

    bool isRunning() const;

public slots:
    /**
     Khởi động Worker Thread và mở cổng serial
     Thread-safe: có thể gọi từ Main Thread
     */
    void start(const QString &portName, int baudRate = 115200);

    /**
     Dừng Worker Thread và đóng cổng
     Thread-safe: có thể gọi từ Main Thread
     */
    void stop();

signals:
    // Forward signals từ SerialWorker lên DashboardController
    void frameReceived(const QString &frame);
    void portOpened(const QString &portName);
    void portClosed();
    void errorOccurred(const QString &message);
    void statsUpdated(int fps);

    //  Internal signals để giao tiếp với Worker (qua Queued Connection)
    void requestOpen(const QString &portName, int baudRate);
    void requestClose();

private:
    QThread      *m_thread = nullptr;
    SerialWorker *m_worker = nullptr;
};