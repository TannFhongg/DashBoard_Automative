/**
 * @file    main.qml
 * @brief   Giao diện HMI Dashboard chính
 *
 * Layout:
 *   ┌─────────────────────────────────────┐
 *   │  [SPEED] odo/trip  [GEAR]  [RPM]   │
 *   │                                     │
 *   │    Speedometer    Tachometer        │
 *   │                                     │
 *   │      [Status bar / Serial port]     │
 *   └─────────────────────────────────────┘
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Window {
    id: root
    visible:     true
    width:       1024
    height:      600
    title:       "Automotive Dashboard"
    color:       "#070D14"   // Nền rất tối, màu cockpit ban đêm

    // ── Kết nối serial khi start ──
    Component.onCompleted: {
        dashboard.connectSerial()
    }

    // ── Nhận thông báo lỗi serial ──
    Connections {
        target: dashboard
        function onErrorOccurred(message) {
            errorBanner.text    = message
            errorBanner.visible = true
            errorHideTimer.restart()
        }
    }

    // ─────────────────────────────────────────
    // BACKGROUND: lưới kỹ thuật số
    // ─────────────────────────────────────────
    Canvas {
        id: bgCanvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            // Lưới mờ
            ctx.strokeStyle = "#0A1A28";
            ctx.lineWidth   = 0.5;
            var step = 40;
            for (var x = 0; x < width; x += step) {
                ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke();
            }
            for (var y = 0; y < height; y += step) {
                ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke();
            }

            // Vignette (tối góc)
            var grad = ctx.createRadialGradient(width/2, height/2, height*0.3,
                                                width/2, height/2, height*0.8);
            grad.addColorStop(0.0, "rgba(0,0,0,0)");
            grad.addColorStop(1.0, "rgba(0,0,0,0.6)");
            ctx.fillStyle = grad;
            ctx.fillRect(0, 0, width, height);
        }
    }

    // ─────────────────────────────────────────
    // LAYOUT CHÍNH
    // ─────────────────────────────────────────
    ColumnLayout {
        anchors {
            fill:    parent
            margins: 16
        }
        spacing: 8

        // ── Hàng 1: Header ──
        RowLayout {
            Layout.fillWidth: true
            height: 36
            spacing: 16

            // Logo / Tên hệ thống
            Text {
                text:  "◈ DASHBOARD SIM"
                color: "#004466"
                font { family: "Courier New"; bold: true; pixelSize: 13; letterSpacing: 3 }
            }

            Item { Layout.fillWidth: true }

            // ODO display
            Column {
                spacing: 1
                Text {
                    text:  "ODO"
                    color: "#334455"
                    font { family: "Courier New"; pixelSize: 9; letterSpacing: 2 }
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text:  dashboard.odo.toFixed(1) + " km"
                    color: "#8899AA"
                    font { family: "Courier New"; bold: true; pixelSize: 14 }
                }
            }

            // Separator
            Rectangle { width: 1; height: 24; color: "#0A2030" }

            // Trip display + Reset button
            Column {
                spacing: 1
                Text {
                    text:  "TRIP"
                    color: "#334455"
                    font { family: "Courier New"; pixelSize: 9; letterSpacing: 2 }
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Row {
                    spacing: 8
                    Text {
                        text:  dashboard.trip.toFixed(1) + " km"
                        color: "#00AACC"
                        font { family: "Courier New"; bold: true; pixelSize: 14 }
                    }
                    Text {
                        text:     "[RST]"
                        color:    resetMouse.containsMouse ? "#FF6040" : "#223040"
                        font { family: "Courier New"; pixelSize: 11 }
                        MouseArea {
                            id: resetMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked:    dashboard.resetTrip()
                            cursorShape:  Qt.PointingHandCursor
                        }
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Status indicator
            Row {
                spacing: 6
                Rectangle {
                    width:  8; height: 8; radius: 4
                    color:  dashboard.connected ? "#00E676" : "#FF3030"
                    // Nháy khi chưa kết nối
                    SequentialAnimation on opacity {
                        running: !dashboard.connected
                        loops:   Animation.Infinite
                        NumberAnimation { to: 0.2; duration: 600 }
                        NumberAnimation { to: 1.0; duration: 600 }
                    }
                }
                Text {
                    text:  dashboard.connected ? "CONNECTED" : "NO SIGNAL"
                    color: dashboard.connected ? "#00C853" : "#AA3020"
                    font { family: "Courier New"; pixelSize: 11; letterSpacing: 1 }
                }
            }
        }

        // ── Divider ──
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color:  "#0A2030"
        }

        // ── Hàng 2: Gauge chính ──
        RowLayout {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            spacing: 20

            // Speedometer
            Item {
                Layout.fillHeight: true
                Layout.preferredWidth: root.width * 0.38

                // Label
                Text {
                    text:  "VELOCITY"
                    color: "#004466"
                    font { family: "Courier New"; pixelSize: 10; letterSpacing: 3 }
                    anchors { top: parent.top; horizontalCenter: parent.horizontalCenter }
                }

                Speedometer {
                    id: speedGauge
                    anchors {
                        fill:        parent
                        topMargin:   18
                        bottomMargin: 0
                    }
                    value:      dashboard.speed
                    maxValue:   240
                    arcColor:   "#00D4FF"
                    label:      "km/h"
                }
            }

            // ── Center panel ──
            ColumnLayout {
                Layout.fillHeight: true
                Layout.preferredWidth: root.width * 0.24
                spacing: 12

                // PRND Gear Display
                GearDisplay {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120 //parent.height * 0.30
                    currentGear: dashboard.gear
                }

                // Divider
                Rectangle {
                    Layout.fillWidth: true
                    height: 1; color: "#0A2030"
                }

                // Số hiển thị lớn: Speed + RPM
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8

                    // Speed digital readout
                    Rectangle {
                        Layout.fillWidth:  true
                        Layout.fillHeight: true
                        color:    "#050B12"
                        radius:   8
                        border.color: "#0A2030"
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                text:  "SPEED"
                                color: "#223040"
                                font { family: "Courier New"; pixelSize: 9; letterSpacing: 3 }
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                text:  dashboard.speed.toString().padStart(3, "0")
                                color: "#00D4FF"
                                font { family: "Courier New"; bold: true; pixelSize: 36 }

                                // Đỏ khi overspeed
                                Behavior on color { ColorAnimation { duration: 300 } }
                                Component.onCompleted: {
                                    root.Connections
                                    {
                                        target: dashboard
                                        function onSpeedChanged(s) {
                                            speedReadout.color = s > 200 ? "#FF3030" :
                                                                 s > 120 ? "#FFD700" : "#00D4FF"
                                        }
                                    }
                                }
                                id: speedReadout
                            }
                            Text {
                                text:  "km/h"
                                color: "#334455"
                                font { family: "Courier New"; pixelSize: 10 }
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }

                    // RPM digital readout
                    Rectangle {
                        Layout.fillWidth:  true
                        Layout.fillHeight: true
                        color:    "#050B12"
                        radius:   8
                        border.color: "#0A2030"
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                text:  "ENGINE"
                                color: "#223040"
                                font { family: "Courier New"; pixelSize: 9; letterSpacing: 3 }
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Text {
                                id: rpmReadout
                                text:  dashboard.rpm.toString().padStart(4, "0")
                                color: dashboard.rpm > 6500 ? "#FF3030" :
                                       dashboard.rpm > 5000 ? "#FFD700" : "#FF8040"
                                font { family: "Courier New"; bold: true; pixelSize: 36 }
                                Behavior on color { ColorAnimation { duration: 200 } }
                            }
                            Text {
                                text:  "rpm"
                                color: "#334455"
                                font { family: "Courier New"; pixelSize: 10 }
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }
                }
            }

            // Tachometer (component chuyên biệt)
            Item {
                Layout.fillHeight: true
                Layout.preferredWidth: root.width * 0.38

                Text {
                    text:  "ENGINE RPM"
                    color: "#330F00"
                    font { family: "Courier New"; pixelSize: 10; letterSpacing: 3 }
                    anchors { top: parent.top; horizontalCenter: parent.horizontalCenter }
                }

                Tachometer {
                    id: rpmGauge
                    anchors {
                        fill:        parent
                        topMargin:   18
                        bottomMargin: 0
                    }
                    value:      dashboard.rpm
                    maxValue:   8000
                    redlineRpm: 6500
                }
            }
        }

        // ── Hàng 3: Status bar ──
        Rectangle {
            Layout.fillWidth: true
            height: 28
            color:  "#050A10"
            radius: 4
            border.color: "#0A1820"

            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }

                Text {
                    text:  "PORT: " + dashboard.portName
                    color: "#1A3040"
                    font { family: "Courier New"; pixelSize: 10 }
                }

                Item { Layout.fillWidth: true }

                // FPS / throughput indicator
                Text {
                    id: fpsText
                    text:  dashboard.serialFps + " FPS  |  UART 115200"
                    color: dashboard.serialFps >= 45 ? "#1A3040" :
                           dashboard.serialFps >= 20 ? "#3A3010" : "#3A1010"
                    font { family: "Courier New"; pixelSize: 10 }
                    Behavior on color { ColorAnimation { duration: 500 } }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: {
                        var now = new Date()
                        return Qt.formatTime(now, "hh:mm:ss")
                    }
                    color: "#1A3040"
                    font { family: "Courier New"; pixelSize: 10 }
                    Timer {
                        interval: 1000; running: true; repeat: true
                        onTriggered: parent.text = Qt.formatTime(new Date(), "hh:mm:ss")
                    }
                }
            }
        }
    }

    // ── Error banner ──
    Rectangle {
        id: errorBanner
        anchors {
            top:              parent.top
            horizontalCenter: parent.horizontalCenter
            topMargin:        8
        }
        width:   400; height: 32
        radius:  6
        color:   "#3A0000"
        border.color: "#AA0000"
        visible: false

        property string text: ""

        Text {
            anchors.centerIn: parent
            text:  errorBanner.text
            color: "#FF6060"
            font { family: "Courier New"; pixelSize: 12 }
        }

        Timer {
            id: errorHideTimer
            interval: 5000
            onTriggered: errorBanner.visible = false
        }
    }
}
