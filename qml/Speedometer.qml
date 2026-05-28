/**
 * @file    Speedometer.qml
 * @brief   Đồng hồ tốc độ - Qt 6.5 compatible
 *
 * Fix: Thay Shape+PathClose bằng Canvas để vẽ kim
 * PathClose không available trong Qt Quick Shapes 1.15 standalone
 */

import QtQuick 2.15

Item {
    id: root

    property int   value:      0
    property int   maxValue:   240
    property int   minValue:   0
    property real  startAngle: -220
    property real  endAngle:   40
    property color arcColor:   "#00D4FF"
    property color needleColor: "#FF4040"
    property string label:     "km/h"

    // ── Giá trị smooth ──
    property real smoothedValue: 0

    Behavior on smoothedValue {
        SmoothedAnimation {
            velocity:    80
            duration:    250
            easing.type: Easing.OutQuart
        }
    }

    onValueChanged: {
        smoothedValue = Math.max(minValue, Math.min(maxValue, value));
    }

    readonly property real needleAngle: {
        var fraction = (smoothedValue - minValue) / (maxValue - minValue);
        return startAngle + fraction * (endAngle - startAngle);
    }

    // ── Canvas vẽ arc + tick marks ──
    Canvas {
        id: gaugeCanvas
        anchors.fill: parent

        property real smoothVal: root.smoothedValue
        onSmoothValChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            var cx  = width  / 2;
            var cy  = height / 2;
            var r   = Math.min(width, height) / 2 - 8;

            ctx.clearRect(0, 0, width, height);

            var startRad = (root.startAngle - 90) * Math.PI / 180;
            var endRad   = (root.endAngle   - 90) * Math.PI / 180;

            // 1. Background track
            ctx.beginPath();
            ctx.arc(cx, cy, r - 4, startRad, endRad, false);
            ctx.strokeStyle = "#1a2a3a";
            ctx.lineWidth   = 14;
            ctx.lineCap     = "round";
            ctx.stroke();

            // 2. Active arc gradient
            var valueFrac = (smoothVal - root.minValue) / (root.maxValue - root.minValue);
            var activeEnd = startRad + valueFrac * (endRad - startRad);
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

            // 3. Minor ticks
            var minorTicks = 48;
            ctx.strokeStyle = "#334455";
            ctx.lineWidth   = 1.5;
            for (var i = 0; i <= minorTicks; i++) {
                var ang    = startRad + (i / minorTicks) * (endRad - startRad);
                var innerR = r - 22;
                var outerR = r - 14;
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(ang) * innerR, cy + Math.sin(ang) * innerR);
                ctx.lineTo(cx + Math.cos(ang) * outerR, cy + Math.sin(ang) * outerR);
                ctx.stroke();
            }

            // 4. Major ticks + labels
            var majorTicks = 12;
            ctx.strokeStyle  = "#7799AA";
            ctx.lineWidth    = 2.5;
            ctx.fillStyle    = "#AACCDD";
            ctx.font         = "bold " + Math.round(r * 0.09) + "px 'Courier New'";
            ctx.textAlign    = "center";
            ctx.textBaseline = "middle";

            for (var j = 0; j <= majorTicks; j++) {
                var mAng    = startRad + (j / majorTicks) * (endRad - startRad);
                var mInnerR = r - 26;
                var mOuterR = r - 10;
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(mAng) * mInnerR, cy + Math.sin(mAng) * mInnerR);
                ctx.lineTo(cx + Math.cos(mAng) * mOuterR, cy + Math.sin(mAng) * mOuterR);
                ctx.stroke();

                var labelVal = Math.round(root.minValue + (j / majorTicks) * (root.maxValue - root.minValue));
                var labelR   = r - 38;
                ctx.fillText(labelVal.toString(),
                    cx + Math.cos(mAng) * labelR,
                    cy + Math.sin(mAng) * labelR);
            }
        }
    }

    // ── Canvas vẽ kim (thay Shape để tránh PathClose issue) ──
    Canvas {
        id: needleCanvas
        anchors.fill: parent

        // Chỉ vẽ lại khi góc thay đổi
        property real angle: root.needleAngle
        onAngleChanged: requestPaint()

        // SpringAnimation trên property angle
        Behavior on angle {
            SpringAnimation {
                spring:   2.0
                damping:  0.3
                epsilon:  0.25
                velocity: 200
            }
        }

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            var cx  = width  / 2;
            var cy  = height / 2;
            var rad = (angle - 90) * Math.PI / 180;
            var len = Math.min(width, height) * 0.38;  // chiều dài kim
            var tail = Math.min(width, height) * 0.08; // đuôi kim
            var w   = 3.5;  // nửa chiều rộng gốc kim

            ctx.save();
            ctx.translate(cx, cy);
            ctx.rotate(rad);

            // Thân kim (tam giác nhọn)
            ctx.beginPath();
            ctx.moveTo(0, -len);   // đầu kim
            ctx.lineTo( w,  0);
            ctx.lineTo(-w,  0);
            ctx.closePath();
            ctx.fillStyle = root.needleColor;
            ctx.fill();

            // Đuôi kim (counterweight)
            ctx.beginPath();
            ctx.moveTo( w,  0);
            ctx.lineTo(-w,  0);
            ctx.lineTo( 0,  tail);
            ctx.closePath();
            ctx.fillStyle = "#AA2020";
            ctx.fill();

            ctx.restore();
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
        z: 10
    }

    // ── Giá trị số ──
    Text {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom:           parent.bottom
            bottomMargin:     parent.height * 0.22
        }
        text:  Math.round(root.smoothedValue).toString()
        color: "#E0F4FF"
        font { family: "Courier New"; bold: true; pixelSize: parent.height * 0.12 }
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
        font { family: "Courier New"; pixelSize: parent.height * 0.05 }
    }
}
