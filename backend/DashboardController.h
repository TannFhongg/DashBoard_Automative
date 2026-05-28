
#pragma once

#include <QObject>
#include <QString>
#include <QSettings>
#include <QTimer>
#include "SerialWorker.h"

class DashboardController : public QObject
{
    Q_OBJECT

    //  Q_PROPERTY đọc và nhận tb khi thay đổi
    Q_PROPERTY(int     speed    READ speed    NOTIFY speedChanged)
    Q_PROPERTY(int     rpm      READ rpm      NOTIFY rpmChanged)
    Q_PROPERTY(QString gear     READ gear     NOTIFY gearChanged)
    Q_PROPERTY(double  trip     READ trip     NOTIFY tripChanged)
    Q_PROPERTY(double  odo      READ odo      NOTIFY odoChanged)
    Q_PROPERTY(bool    connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString portName READ portName  WRITE setPortName NOTIFY portNameChanged)
    Q_PROPERTY(int     serialFps READ serialFps NOTIFY serialFpsChanged)

public:
    explicit DashboardController(QObject *parent = nullptr);
    ~DashboardController();

    // Getters
    int     speed()     const { return m_speed; }
    int     rpm()       const { return m_rpm; }
    QString gear()      const { return m_gear; }
    double  trip()      const { return m_trip; }
    double  odo()       const { return m_odo; }
    bool    connected() const { return m_connected; }
    QString portName()  const { return m_portName; }
    int     serialFps() const { return m_serialFps; }

    // Setter cho portName
    void setPortName(const QString &name);

public slots:
    // Gọi từ QML
    Q_INVOKABLE void connectSerial();
    Q_INVOKABLE void disconnectSerial();
    Q_INVOKABLE void resetTrip();
    Q_INVOKABLE QStringList availablePorts();

signals:
    void speedChanged(int speed);
    void rpmChanged(int rpm);
    void gearChanged(const QString &gear);
    void tripChanged(double trip);
    void odoChanged(double odo);
    void connectedChanged(bool connected);
    void portNameChanged(const QString &name);
    void serialFpsChanged(int fps);
    void errorOccurred(const QString &message);

private slots:
    //  Slots nhận từ SerialManager ( trên Main Thread qua Queued)
    void onFrameReceived(const QString &frame);
    void onPortOpened(const QString &portName);
    void onPortClosed();
    void onSerialStats(int fps);

private:
    //  Parse frame UART: "S120,R4500,GD,T15.5"
    void parseFrame(const QString &frame);

    // Lưu/Load ODO qua QSettings
    void saveOdo();
    void loadOdo();

    //  Thành viên dữ liệu
    int     m_speed     = 0;
    int     m_rpm       = 0;
    QString m_gear      = "P";
    double  m_trip      = 0.0;
    double  m_odo       = 0.0;
    bool    m_connected = false;
    int     m_serialFps = 0;
    QString m_portName;

    //  Qt Objects
    SerialManager *m_serialManager = nullptr;   // Worker Thread I/O
    QSettings     *m_settings      = nullptr;

    //  Lưu ODO định kỳ mỗi 5 giây
    QTimer *m_saveTimer = nullptr;
};