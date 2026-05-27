/**
 * @file    Speedometer.qml
 * @brief   Đồng hồ tốc độ dạng vòng tròn với kim quét
 *
 * Kỹ thuật:
 *   - Canvas API để vẽ arc, gradient, tick marks
 *   - SmoothedAnimation: mô phỏng quán tính cơ học
 *     easing.type: Easing.OutElastic → kim dao động nhẹ khi dừng
 *   - Behavior on value: tự động animate bất kỳ thay đổi nào
 */

import QtQuick 2.15
import QtQuick.Shapes 1.15

Item {
    id: root

    // ── API của component ──
    property int   value:    0        // Giá trị hiện tại (km/h)
    property int   maxValue: 240      // Giá trị tối đa
    property int   minValue: 0
    property real  startAngle: -220   // Góc bắt đầu (độ, từ 12 giờ)
    property real  endAngle:   40     // Góc kết thúc
    property color arcColor:   "#00D4FF"
    property color needleColor: "#FF4040"
    property string label:  "km/h"

    // ── Giá trị được smooth để tạo animation ──
    property real smoothedValue: 0

    // ── Animation: mô phỏng quán tính đồng hồ cơ ──
    Behavior on smoothedValue {
        SmoothedAnimation {
            velocity:    80      // tốc độ thay đổi tối đa (units/sec)
            duration:    250     // ms để đạt target
            easing.type: Easing.OutQuart
        }
    }

    // Cập nhật smoothedValue khi value thay đổi
    onValueChanged: {
        smoothedValue = Math.max(minValue, Math.min(maxValue, value));
    }

    // ── Tính góc kim từ giá trị ──
    readonly property real needleAngle: {
        var fraction = (smoothedValue - minValue) / (maxValue - minValue);
        return startAngle + fraction * (endAngle - startAngle);
    }

    // ── Canvas để vẽ các vòng cung và scale ──
    Canvas {
        id: gaugeCanvas
        anchors.fill: parent

        // Vẽ lại khi smoothedValue thay đổi
        onSmoothValueChanged: requestPaint()
        property real smoothValue: root.smoothedValue

        onPaint: {
            var ctx = getContext("2d");
            var cx  = width  / 2;
            var cy  = height / 2;
            var r   = Math.min(width, height) / 2 - 8;

            ctx.clearRect(0, 0, width, height);

            var startRad = (root.startAngle - 90) * Math.PI / 180;
            var endRad   = (root.endAngle   - 90) * Math.PI / 180;

            // ── 1. Background track (vòng xám) ──
            ctx.beginPath();
            ctx.arc(cx, cy, r - 4, startRad, endRad, false);
            ctx.strokeStyle = "#1a2a3a";
            ctx.lineWidth   = 14;
            ctx.lineCap     = "round";
            ctx.stroke();

            // ── 2. Active arc (màu theo giá trị) ──
            var valueFrac  = (smoothValue - root.minValue) / (root.maxValue - root.minValue);
            var activeEnd  = startRad + valueFrac * (endRad - startRad);
            var grad = ctx.createLinearGradient(0, 0, width, height);
            grad.addColorStop(0.0, "#005FA3");
            grad.addColorStop(0.5, "#00D4FF");
            grad.addColorStop(1.0, "#00FFB0");

            ctx.beginPath();
            ctx.arc(cx, cy, r - 4, startRad, activeEnd, false);
            ctx.strokeStyle = grad;
            ctx.lineWidth   = 14;
            ctx.lineCap     = "round";
            ctx.stroke();

            // ── 3. Tick marks (vạch chia) ──
            var majorTicks = 12;  // 0, 20, 40 ... 240
            var minorTicks = 48;  // chia nhỏ hơn

            // Minor ticks
            ctx.strokeStyle = "#334455";
            ctx.lineWidth   = 1.5;
            for (var i = 0; i <= minorTicks; i++) {
                var ang = startRad + (i / minorTicks) * (endRad - startRad);
                var innerR = r - 22;
                var outerR = r - 14;
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(ang) * innerR, cy + Math.sin(ang) * innerR);
                ctx.lineTo(cx + Math.cos(ang) * outerR, cy + Math.sin(ang) * outerR);
                ctx.stroke();
            }

            // Major ticks + số
            ctx.strokeStyle = "#7799AA";
            ctx.lineWidth   = 2.5;
            ctx.fillStyle   = "#AACCDD";
            ctx.font        = "bold " + Math.round(r * 0.09) + "px 'Courier New'";
            ctx.textAlign   = "center";
            ctx.textBaseline = "middle";

            for (var j = 0; j <= majorTicks; j++) {
                var mAng    = startRad + (j / majorTicks) * (endRad - startRad);
                var mInnerR = r - 26;
                var mOuterR = r - 10;
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(mAng) * mInnerR, cy + Math.sin(mAng) * mInnerR);
                ctx.lineTo(cx + Math.cos(mAng) * mOuterR, cy + Math.sin(mAng) * mOuterR);
                ctx.stroke();

                // Số
                var labelVal = Math.round(root.minValue + (j / majorTicks) * (root.maxValue - root.minValue));
                var labelR   = r - 38;
                ctx.fillText(
                    labelVal.toString(),
                    cx + Math.cos(mAng) * labelR,
                    cy + Math.sin(mAng) * labelR
                );
            }
        }
    }

    // ── Kim đồng hồ - dùng Shape để bo đẹp ──
    Item {
        id: needleContainer
        anchors.centerIn: parent
        width:  parent.width
        height: parent.height

        // Rotation được animate qua needleAngle
        rotation: root.needleAngle
        transformOrigin: Item.Center

        // Thêm Behavior trực tiếp trên rotation cho hiệu ứng quán tính
        Behavior on rotation {
            SpringAnimation {
                spring:   2.0    // độ cứng lò xo
                damping:  0.3    // hệ số giảm chấn (0=dao động mãi, 1=không dao động)
                epsilon:  0.25   // ngưỡng dừng
                velocity: 200
            }
        }

        // Thân kim: hình thang nhọn
        Shape {
            anchors.centerIn: parent
            ShapePath {
                fillColor:   root.needleColor
                strokeColor: "transparent"
                PathMove { x: 0;  y: -gaugeCanvas.height * 0.38 }  // Đầu kim
                PathLine { x:  3; y: 0 }
                PathLine { x: -3; y: 0 }
                PathClose {}
            }
        }

        // Đuôi kim (counterweight)
        Shape {
            anchors.centerIn: parent
            ShapePath {
                fillColor:   "#AA2020"
                strokeColor: "transparent"
                PathMove { x:  2; y: 0 }
                PathLine { x: -2; y: 0 }
                PathLine { x:  0; y: gaugeCanvas.height * 0.08 }
                PathClose {}
            }
        }
    }

    // ── Hub trung tâm ──
    Rectangle {
        anchors.centerIn: parent
        width:  16; height: 16
        radius: 8
        color:  "#C0C0D0"
        border.color: "#607080"
        border.width: 2
    }

    // ── Giá trị số hiển thị ──
    Text {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom:           parent.bottom
            bottomMargin:     parent.height * 0.22
        }
        text:  Math.round(root.smoothedValue).toString()
        color: "#E0F4FF"
        font {
            family:    "Courier New"
            bold:      true
            pixelSize: parent.height * 0.12
        }
    }

    // ── Label đơn vị ──
    Text {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom:           parent.bottom
            bottomMargin:     parent.height * 0.14
        }
        text:  root.label
        color: "#607080"
        font {
            family:    "Courier New"
            pixelSize: parent.height * 0.05
        }
    }
}
