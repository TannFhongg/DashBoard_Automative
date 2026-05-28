

#include "SerialWorker.h"
#include <QSerialPortInfo>
#include <QDateTime>
#include <QDebug>

// trien khia

SerialWorker::SerialWorker(QObject *parent)
    : QObject(parent)
{
    qDebug() << "[SerialWorker] Created on thread:" << QThread::currentThreadId();
}

SerialWorker::~SerialWorker()
{
    closePort();
    qDebug() << "[SerialWorker] Destroyed";
}

// MỞ CỔNG SERIAL

void SerialWorker::openPort(const QString &portName, int baudRate)
{
    qDebug() << "[SerialWorker] openPort() on thread:" << QThread::currentThreadId();

    // Dọn dẹp kết nối cũ nếu có
    if (m_serial) {
        if (m_serial->isOpen()) m_serial->close();
        delete m_serial;
        m_serial = nullptr;
    }
    m_rxBuffer.clear();

    // Tạo QSerialPort trên Worker Thread
    // Tạo QSerialPort trên Worker Thread
    m_serial = new QSerialPort(this);

    // Cấu hình thông số UART
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    // 1. MỞ CỔNG
    if (!m_serial->open(QIODevice::ReadOnly)) {
        QString err = QString("[SerialWorker] Cannot open %1: %2")
        .arg(portName)
            .arg(m_serial->errorString()); // Lúc này m_serial chắc chắn không bị null
        qWarning() << err;
        emit errorOccurred(err);

        delete m_serial;
        m_serial = nullptr;
        return;
    }

    // 2. NẾU THÀNH CÔNG THI CONNECT SIGNALS
    connect(m_serial, &QSerialPort::readyRead,
            this,     &SerialWorker::onDataReady,
            Qt::DirectConnection);

    connect(m_serial, &QSerialPort::errorOccurred,
            this,     &SerialWorker::onSerialError,
            Qt::DirectConnection);

    m_connected    = true;
    // ... (Giữ nguyên các đoạn code m_lastStatTime = ... bên dưới)
    m_lastStatTime = QDateTime::currentMSecsSinceEpoch();
    m_frameCount   = 0;

    qDebug() << "[SerialWorker] Port opened:" << portName << "@" << baudRate;
    emit portOpened(portName);
}

void SerialWorker::closePort()
{
    if (m_serial) {
        if (m_serial->isOpen()) {
            m_serial->close();
            qDebug() << "[SerialWorker] Port closed";
        }

        m_serial->deleteLater();
        m_serial = nullptr;
    }

    m_connected = false;
    m_rxBuffer.clear();
    emit portClosed();
}

// SLOT: DỮ LIỆU ĐẾN TỪ SERIAL PORT

void SerialWorker::onDataReady()
{
    if (!m_serial) return;

    // Đọc toàn bộ byte có sẵn trong hardware buffer

    m_rxBuffer.append(m_serial->readAll());

    // Xử lý buffer để tách các frame hoàn chỉnh
    processBuffer();

    // Cập nhật thống kê FPS
    updateStats();
}


void SerialWorker::processBuffer()
{
    // Lặp cho đến khi hết frame hoàn chỉnh trong buffer
    while (true) {

        int newlinePos = m_rxBuffer.indexOf('\n');
        if (newlinePos < 0) {
            // Chưa có frame đầy đủ → đợi thêm dữ liệu
            break;
        }

        // Tách frame (không bao gồm '\n')
        QByteArray frameBytes = m_rxBuffer.left(newlinePos);

        // Xóa frame vừa xử lý + '\n' khỏi buffer
        m_rxBuffer.remove(0, newlinePos + 1);

        // Loại bỏ khoang trang
        QString frame = QString::fromLatin1(frameBytes).trimmed();

        if (frame.isEmpty()) continue;

        // frame hợp lệ phải bắt đầu bằng 'S'
        if (!frame.startsWith('S')) {
            qWarning() << "[SerialWorker] Malformed frame (skip):" << frame;
            continue;
        }

        // Emit signal ->  Queued Connection ->  Main Thread nhận

        emit frameReceived(frame);
        m_frameCount++;
    }

    //  Bảo vệ tràn buffer
    // Nếu buffer > 512 bytes mà không có '\n' thi do la dữ liệu rác
    // Giữ lại 64 bytes cuối phòng trường hợp frame đang dở
    if (m_rxBuffer.size() > 512) {
        qWarning() << "[SerialWorker] Buffer overflow ("
                   << m_rxBuffer.size() << "bytes), truncating";
        m_rxBuffer = m_rxBuffer.right(64);
    }
}


// CẬP NHẬT THỐNG KÊ FPS =Tính số frame/giây thực tế

void SerialWorker::updateStats()
{
    qint64 now     = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = now - m_lastStatTime;

    // Cập nhật mỗi 1 giây
    if (elapsed >= 1000) {
        int fps = static_cast<int>((m_frameCount * 1000.0) / elapsed);
        emit statsUpdated(fps);

        m_frameCount   = 0;
        m_lastStatTime = now;

        qDebug() << "[SerialWorker] Throughput:" << fps << "fps";
    }
}
// xu li cac loi serial
void SerialWorker::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;

    QString msg;
    bool isFatal = false;

    switch (error) {
    case QSerialPort::DeviceNotFoundError:
        msg     = "Device not found";
        isFatal = true;
        break;
    case QSerialPort::PermissionError:
        msg     = "Permission denied. Try: sudo chmod a+rw /dev/ttyUSB0";
        isFatal = true;
        break;
    case QSerialPort::ResourceError:
        // Thiết bị bị rút ra đột ngột
        msg     = "Device disconnected (ResourceError)";
        isFatal = true;
        break;
    case QSerialPort::TimeoutError:
        // Không fatal, chỉ log
        msg     = "Read timeout";
        isFatal = false;
        break;
    default:
        msg = QString("Serial error [code %1]: %2")
                  .arg(static_cast<int>(error))
                  .arg(m_serial ? m_serial->errorString() : "unknown");
        isFatal = false;
        break;
    }

    qWarning() << "[SerialWorker]" << msg;
    emit errorOccurred(msg);

    if (isFatal) {
        closePort();
    }
}



SerialManager::SerialManager(QObject *parent)
    : QObject(parent)
{
    qDebug() << "[SerialManager] Created";
}

SerialManager::~SerialManager()
{
    stop();
}

bool SerialManager::isRunning() const
{
    return m_thread && m_thread->isRunning();
}

void SerialManager::start(const QString &portName, int baudRate)
{
    // Nếu đang chạy - > stop trước
    if (isRunning()) {
        qDebug() << "[SerialManager] Already running, restarting...";
        stop();
    }

    // Tạo Worker KHÔNG có parent
    //         có parent thì không moveToThread được
    m_worker = new SerialWorker();
    m_thread = new QThread(this);  // thread có parent = this (auto cleanup)

    // Di chuyển worker sang thread mới
    m_worker->moveToThread(m_thread);
    qDebug() << "[SerialManager] Worker moved to thread:" << m_thread;

    // Kết nối lifecycle

    connect(m_thread, &QThread::finished,
            m_worker, &QObject::deleteLater);

    //  Kết nối internal signals để điều khiển Worker

    connect(this,     &SerialManager::requestOpen,
            m_worker, &SerialWorker::openPort,
            Qt::QueuedConnection);

    connect(this,     &SerialManager::requestClose,
            m_worker, &SerialWorker::closePort,
            Qt::QueuedConnection);

    //  Forward signals từ Worker

    connect(m_worker, &SerialWorker::frameReceived,
            this,     &SerialManager::frameReceived,
            Qt::QueuedConnection);

    connect(m_worker, &SerialWorker::portOpened,
            this,     &SerialManager::portOpened,
            Qt::QueuedConnection);

    connect(m_worker, &SerialWorker::portClosed,
            this,     &SerialManager::portClosed,
            Qt::QueuedConnection);

    connect(m_worker, &SerialWorker::errorOccurred,
            this,     &SerialManager::errorOccurred,
            Qt::QueuedConnection);

    connect(m_worker, &SerialWorker::statsUpdated,
            this,     &SerialManager::statsUpdated,
            Qt::QueuedConnection);

    // Khởi động event loop của thread
    m_thread->start();
    qDebug() << "[SerialManager] Thread started";

    // Yêu cầu Worker mở cổng (thực thi trên Worker Thread)
    emit requestOpen(portName, baudRate);
}


//  ĐÓNG PORT , CLEANUP THREAD
void SerialManager::stop()
{
    if (!m_thread) return;

    qDebug() << "[SerialManager] Stopping...";

    // Yêu cầu worker đóng port trước
    emit requestClose();

    // Dừng event loop của thread
    m_thread->quit();

    // Chờ tối đa 3 giây
    if (!m_thread->wait(3000)) {
        qWarning() << "[SerialManager] Thread did not stop in time, forcing terminate";
        m_thread->terminate();
        m_thread->wait(1000);
    }


    m_thread = nullptr;
    m_worker = nullptr;

    qDebug() << "[SerialManager] Stopped cleanly";
}