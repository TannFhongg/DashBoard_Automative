/**
 * @file    SerialWorker.cpp
 * @brief   Implementation của SerialWorker và SerialManager
 *
 * ═══════════════════════════════════════════════════════════════
 * NGUYÊN LÝ HOẠT ĐỘNG CỦA WORKER PATTERN TRONG QT
 * ═══════════════════════════════════════════════════════════════
 *
 * Bước 1 – Tạo objects:
 *   QThread      *thread = new QThread(parent);
 *   SerialWorker *worker = new SerialWorker();   // KHÔNG có parent
 *
 * Bước 2 – Di chuyển worker sang thread mới:
 *   worker->moveToThread(thread);
 *   Sau lệnh này, tất cả slot của worker CHẠY TRÊN thread mới,
 *   không phải Main Thread nữa.
 *
 * Bước 3 – Kết nối signals (Queued Connection tự động):
 *   connect(thread, &QThread::started, worker, &SerialWorker::onThreadStarted);
 *   Khi thread A emit signal đến object ở thread B → Qt tự động
 *   dùng Qt::QueuedConnection → thread-safe, không cần mutex.
 *
 * Bước 4 – Cleanup đúng cách:
 *   worker->closePort();
 *   thread->quit();      // Dừng event loop của thread
 *   thread->wait();      // Chờ thread kết thúc hoàn toàn
 *   delete worker;       // Worker KHÔNG có parent → phải xóa thủ công
 *
 * ═══════════════════════════════════════════════════════════════
 * TẠI SAO KHÔNG DÙNG QSerialPort Ở MAIN THREAD?
 * ═══════════════════════════════════════════════════════════════
 * - QSerialPort::readyRead gọi slot đồng bộ trên thread sở hữu nó
 * - Nếu ở Main Thread: Qt event loop bị chiếm → animation bị skip
 * - 50FPS = signal cứ 20ms → đủ để làm UI stuttering
 * - Worker Thread có event loop riêng → Main Thread hoàn toàn tự do
 */

#include "SerialWorker.h"
#include <QSerialPortInfo>
#include <QDateTime>
#include <QDebug>

// ══════════════════════════════════════════════════════════════════
// SerialWorker — IMPLEMENTATION
// ══════════════════════════════════════════════════════════════════

SerialWorker::SerialWorker(QObject *parent)
    : QObject(parent)
{
    // QUAN TRỌNG: KHÔNG tạo QSerialPort ở đây.
    // Constructor có thể chạy trên Main Thread (trước moveToThread).
    // QSerialPort phải được tạo trên cùng thread sẽ sử dụng nó.
    // → Tạo trong openPort(), được gọi sau moveToThread()
    qDebug() << "[SerialWorker] Created on thread:" << QThread::currentThreadId();
}

SerialWorker::~SerialWorker()
{
    closePort();
    qDebug() << "[SerialWorker] Destroyed";
}

// ──────────────────────────────────────────────────────────────────
// MỞ CỔNG SERIAL
// Hàm này chạy trên Worker Thread nhờ Queued Connection
// ──────────────────────────────────────────────────────────────────
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

    // Tạo QSerialPort trên Worker Thread → đúng affinity
    // Tạo QSerialPort trên Worker Thread → đúng affinity
    m_serial = new QSerialPort(this);

    // Cấu hình thông số UART
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    // ── 1. MỞ CỔNG TRƯỚC ──
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

    // ── 2. NẾU THÀNH CÔNG, MỚI CONNECT SIGNALS ──
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

// ──────────────────────────────────────────────────────────────────
// ĐÓNG CỔNG SERIAL
// ──────────────────────────────────────────────────────────────────
void SerialWorker::closePort()
{
    if (m_serial) {
        if (m_serial->isOpen()) {
            m_serial->close();
            qDebug() << "[SerialWorker] Port closed";
        }
        // Không delete ở đây nếu đang trong event loop của Worker Thread
        // Qt sẽ xóa qua parent-child hoặc deleteLater
        m_serial->deleteLater();
        m_serial = nullptr;
    }

    m_connected = false;
    m_rxBuffer.clear();
    emit portClosed();
}

// ──────────────────────────────────────────────────────────────────
// SLOT: DỮ LIỆU ĐẾN TỪ SERIAL PORT
// Chạy trên Worker Thread → không block UI
// ──────────────────────────────────────────────────────────────────
void SerialWorker::onDataReady()
{
    if (!m_serial) return;

    // Đọc toàn bộ bytes có sẵn trong hardware buffer
    // readAll() không block — chỉ lấy bytes đang có
    m_rxBuffer.append(m_serial->readAll());

    // Xử lý buffer để tách các frame hoàn chỉnh
    processBuffer();

    // Cập nhật thống kê FPS
    updateStats();
}

// ──────────────────────────────────────────────────────────────────
// XỬ LÝ BUFFER: TÁCH FRAME TỪ BYTE STREAM
//
// Vấn đề: UART không đảm bảo 1 lần readAll() = 1 frame hoàn chỉnh.
// Có thể nhận được:
//   - Nửa frame:  "S120,R450"
//   - 1.5 frame:  "S120,R4500,GD,T15.5\nS121,R46"
//   - 3 frame:    "S120,...\nS121,...\nS122,...\n"
//
// Giải pháp: Buffer tích lũy, tách khi gặp '\n'
// ──────────────────────────────────────────────────────────────────
void SerialWorker::processBuffer()
{
    // Lặp cho đến khi hết frame hoàn chỉnh trong buffer
    while (true) {
        // Tìm ký tự kết thúc frame '\n'
        int newlinePos = m_rxBuffer.indexOf('\n');
        if (newlinePos < 0) {
            // Chưa có frame đầy đủ → đợi thêm dữ liệu
            break;
        }

        // Tách frame (không bao gồm '\n')
        QByteArray frameBytes = m_rxBuffer.left(newlinePos);

        // Xóa frame vừa xử lý + '\n' khỏi buffer
        m_rxBuffer.remove(0, newlinePos + 1);

        // Loại bỏ whitespace, '\r' (CR+LF compatibility)
        QString frame = QString::fromLatin1(frameBytes).trimmed();

        if (frame.isEmpty()) continue;

        // Kiểm tra sanity: frame hợp lệ phải bắt đầu bằng 'S'
        if (!frame.startsWith('S')) {
            qWarning() << "[SerialWorker] Malformed frame (skip):" << frame;
            continue;
        }

        // ── Emit signal → Queued Connection → Main Thread nhận ──
        // Qt tự động marshal dữ liệu qua event queue, thread-safe
        emit frameReceived(frame);
        m_frameCount++;
    }

    // ── Bảo vệ tràn buffer ──
    // Nếu buffer > 512 bytes mà không có '\n' → dữ liệu rác
    // Giữ lại 64 bytes cuối phòng trường hợp frame đang dở
    if (m_rxBuffer.size() > 512) {
        qWarning() << "[SerialWorker] Buffer overflow ("
                   << m_rxBuffer.size() << "bytes), truncating";
        m_rxBuffer = m_rxBuffer.right(64);
    }
}

// ──────────────────────────────────────────────────────────────────
// CẬP NHẬT THỐNG KÊ FPS
// Tính số frame/giây thực tế → hiển thị trên status bar
// ──────────────────────────────────────────────────────────────────
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

// ──────────────────────────────────────────────────────────────────
// XỬ LÝ LỖI SERIAL
// ──────────────────────────────────────────────────────────────────
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

    // Lỗi nghiêm trọng → đóng port, thông báo cho Manager
    if (isFatal) {
        closePort();
    }
}


// ══════════════════════════════════════════════════════════════════
// SerialManager — IMPLEMENTATION
// Quản lý vòng đời của QThread + SerialWorker
// ══════════════════════════════════════════════════════════════════

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

// ──────────────────────────────────────────────────────────────────
// KHỞI ĐỘNG: TẠO THREAD VÀ MỞ PORT
//
// Chuỗi khởi tạo chuẩn Qt Worker Pattern:
//
//   1. new SerialWorker()      — KHÔNG có parent (bắt buộc)
//   2. moveToThread(thread)    — chuyển affinity
//   3. connect signals         — Queued connection tự động
//   4. thread->start()         — khởi động event loop của thread
//   5. emit requestOpen(...)   — Worker nhận và mở port trên thread của nó
// ──────────────────────────────────────────────────────────────────
void SerialManager::start(const QString &portName, int baudRate)
{
    // Nếu đang chạy → stop trước
    if (isRunning()) {
        qDebug() << "[SerialManager] Already running, restarting...";
        stop();
    }

    // Bước 1: Tạo Worker KHÔNG có parent
    //         (Worker có parent thì không moveToThread được)
    m_worker = new SerialWorker();
    m_thread = new QThread(this);  // thread có parent = this (auto cleanup)

    // Bước 2: Di chuyển worker sang thread mới
    m_worker->moveToThread(m_thread);
    qDebug() << "[SerialManager] Worker moved to thread:" << m_thread;

    // ── Bước 3a: Kết nối lifecycle ──

    // Khi thread start → không cần làm gì ngay (openPort gọi sau)
    // Khi thread finish → tự xóa worker
    connect(m_thread, &QThread::finished,
            m_worker, &QObject::deleteLater);

    // ── Bước 3b: Kết nối internal signals để điều khiển Worker ──
    // requestOpen/requestClose chạy trên Main Thread
    // Worker nhận qua Queued Connection → thực thi trên Worker Thread
    connect(this,     &SerialManager::requestOpen,
            m_worker, &SerialWorker::openPort,
            Qt::QueuedConnection);

    connect(this,     &SerialManager::requestClose,
            m_worker, &SerialWorker::closePort,
            Qt::QueuedConnection);

    // ── Bước 3c: Forward signals từ Worker → ngoài ──
    // frameReceived: Worker Thread → Main Thread (Queued tự động)
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

    // Bước 4: Khởi động event loop của thread
    m_thread->start();
    qDebug() << "[SerialManager] Thread started";

    // Bước 5: Yêu cầu Worker mở cổng (thực thi trên Worker Thread)
    emit requestOpen(portName, baudRate);
}

// ──────────────────────────────────────────────────────────────────
// DỪNG: ĐÓNG PORT VÀ CLEANUP THREAD
//
// Thứ tự quan trọng:
//   1. Gửi requestClose → Worker đóng serial (trên Worker Thread)
//   2. thread->quit()   → Dừng event loop của thread
//   3. thread->wait()   → Chờ thread kết thúc hoàn toàn
//                         (BLOCKING, nhưng cần thiết để tránh crash)
//   4. m_thread = nullptr (worker bị xóa qua deleteLater ở step 3a)
// ──────────────────────────────────────────────────────────────────
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

    // m_thread có parent = this → Qt tự xóa
    // m_worker đã được deleteLater khi thread finish
    m_thread = nullptr;
    m_worker = nullptr;

    qDebug() << "[SerialManager] Stopped cleanly";
}