/**
 * @file    Tachometer.qml
 * @brief   Đồng hồ vòng tua máy (RPM) chuyên biệt
 *
 * Khác biệt so với Speedometer:
 *   - Có vùng màu phân tầng: Xanh → Vàng → Đỏ (Redline)
 *   - Vạch số hiển thị ×1000 rpm (0, 1, 2 ... 8)
 *   - Kim màu cam, đuôi kim màu đỏ rực
 *   - Redline zone từ 6500 RPM trở lên nhấp nháy cảnh báo
 *   - SpringAnimation mạnh hơn → mô phỏng kim RPM
 *     phản ứng nhanh khi tăng ga, rơi chậm khi thả ga
 *
 * Vùng màu arc:
 *   0    → 3000  : Xanh lá   (idle / economy)
 *   3000 → 5500  : Vàng cam  (normal operation)
 *   5500 → 6500  : Cam đậm   (performance)
 *   6500 → 8000  : Đỏ        (REDLINE — cảnh báo)
 */

import QtQuick 2.15
import QtQuick.Shapes 1.15

Item {
    id: root

    // ── Public API ──
    property int   value:       0        // RPM hiện tại
    property int   maxValue:    8000
    property int   minValue:    0
    property real  startAngle:  -220     // Góc bắt đầu
    property real  endAngle:    40       // Góc kết thúc
    property int   redlineRpm:  6500     // Ngưỡng Redline

    // ── Giá trị smooth để animation mượt ──
    property real smoothedValue: 0

    // ── Trạng thái Redline ──
    readonly property bool isRedline: value >= redlineRpm

    // ── Animation kim: SpringAnimation phản ứng nhanh/chậm bất đối xứng ──
    // Tăng nhanh (gas in): spring cao, damping thấp → kim vọt lên
    // Giảm chậm (gas out): SmoothedAnimation → kim rơi tự nhiên
    Behavior on smoothedValue {
        SmoothedAnimation {
            // Tốc độ thay đổi tối đa: 4000 rpm/s (tăng) vs 2000 rpm/s (giảm)
            // SmoothedAnimation velocity xử lý đều 2 chiều
            // Để bất đối xứng thật sự cần dùng NumberAnimation + onValueChanged
            velocity:    3500
            duration:    180
            easing.type: Easing.OutCubic
        }
    }

    onValueChanged: {
        var clamped = Math.max(minValue, Math.min(maxValue, value));
        // Tăng nhanh hơn giảm: khi tăng dùng spring, giảm dùng smooth
        if (clamped > smoothedValue) {
            // Tăng ga: animation nhanh hơn
            rpmSpring.enabled   = true;
            smoothAnim.enabled  = false;
        } else {
            // Thả ga: animation chậm hơn, có inertia
            rpmSpring.enabled   = false;
            smoothAnim.enabled  = true;
        }
        smoothedValue = clamped;
    }

    // ── Tính góc kim ──
    readonly property real needleAngle: {
        var frac = (smoothedValue - minValue) / (maxValue - minValue);
        return startAngle + frac * (endAngle - startAngle);
    }

    // ─────────────────────────────────────────
    // CANVAS: Vẽ các vòng cung màu phân tầng
    // ─────────────────────────────────────────
    Canvas {
        id: gaugeCanvas
        anchors.fill: parent

        property real smoothVal: root.smoothedValue
        onSmoothValChanged: requestPaint()

        // Vẽ lại khi redline blink
        property bool redlineBlink: false
        onRedlineBlinkChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            var cx  = width  / 2;
            var cy  = height / 2;
            var r   = Math.min(width, height) / 2 - 8;

            ctx.clearRect(0, 0, width, height);

            var sRad = (root.startAngle - 90) * Math.PI / 180;
            var eRad = (root.endAngle   - 90) * Math.PI / 180;
            var totalAngle = eRad - sRad;

            // ── Hàm tiện ích: tính góc từ RPM ──
            function rpmToRad(rpm) {
                var frac = (rpm - root.minValue) / (root.maxValue - root.minValue);
                return sRad + frac * totalAngle;
            }

            // ─────────────────────────────────
            // 1. Background track
            // ─────────────────────────────────
            ctx.beginPath();
            ctx.arc(cx, cy, r - 4, sRad, eRad, false);
            ctx.strokeStyle = "#0D1A26";
            ctx.lineWidth   = 16;
            ctx.lineCap     = "round";
            ctx.stroke();

            // ─────────────────────────────────
            // 2. Vùng màu phân tầng (luôn hiển thị)
            // ─────────────────────────────────
            var zones = [
                // [fromRpm, toRpm, color]
                [0,    3000, "#004422"],   // Idle - xanh đậm
                [3000, 5500, "#445500"],   // Normal - olive
                [5500, 6500, "#663300"],   // Performance - nâu cam
                [6500, 8000, "#550000"],   // Redline - đỏ đậm
            ];
            ctx.lineWidth = 16;
            ctx.lineCap   = "butt";
            for (var z = 0; z < zones.length; z++) {
                ctx.beginPath();
                ctx.arc(cx, cy, r - 4, rpmToRad(zones[z][0]), rpmToRad(zones[z][1]), false);
                ctx.strokeStyle = zones[z][2];
                ctx.stroke();
            }

            // ─────────────────────────────────
            // 3. Active arc (theo giá trị thực tế)
            // ─────────────────────────────────
            var curVal = smoothVal;

            // Segment 1: 0 → min(curVal, 3000) — Xanh lá
            if (curVal > 0) {
                var seg1End = Math.min(curVal, 3000);
                ctx.beginPath();
                ctx.arc(cx, cy, r - 4, rpmToRad(0), rpmToRad(seg1End), false);
                ctx.strokeStyle = "#00E676";
                ctx.lineWidth   = 16;
                ctx.lineCap     = "round";
                ctx.stroke();
            }

            // Segment 2: 3000 → min(curVal, 5500) — Vàng
            if (curVal > 3000) {
                var seg2End = Math.min(curVal, 5500);
                ctx.beginPath();
                ctx.arc(cx, cy, r - 4, rpmToRad(3000), rpmToRad(seg2End), false);
                ctx.strokeStyle = "#FFD700";
                ctx.lineWidth   = 16;
                ctx.lineCap     = "round";
                ctx.stroke();
            }

            // Segment 3: 5500 → min(curVal, 6500) — Cam
            if (curVal > 5500) {
                var seg3End = Math.min(curVal, 6500);
                ctx.beginPath();
                ctx.arc(cx, cy, r - 4, rpmToRad(5500), rpmToRad(seg3End), false);
                ctx.strokeStyle = "#FF8C00";
                ctx.lineWidth   = 16;
                ctx.lineCap     = "round";
                ctx.stroke();
            }

            // Segment 4: 6500 → curVal — Đỏ (Redline, có blink)
            if (curVal > 6500) {
                var redAlpha = redlineBlink ? 1.0 : 0.55;
                ctx.beginPath();
                ctx.arc(cx, cy, r - 4, rpmToRad(6500), rpmToRad(curVal), false);
                ctx.strokeStyle = "rgba(255, 30, 30, " + redAlpha + ")";
                ctx.lineWidth   = 18;  // Dày hơn để nổi bật
                ctx.lineCap     = "round";
                ctx.stroke();
            }

            // ─────────────────────────────────
            // 4. Minor tick marks
            // ─────────────────────────────────
            var minorCount = 40;  // 40 vạch nhỏ
            ctx.lineWidth   = 1.5;
            for (var i = 0; i <= minorCount; i++) {
                var ang  = sRad + (i / minorCount) * totalAngle;
                var rpm  = root.minValue + (i / minorCount) * (root.maxValue - root.minValue);
                // Vạch trong vùng redline tô đỏ mờ
                ctx.strokeStyle = (rpm >= root.redlineRpm) ? "#551111" : "#1E3040";
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(ang) * (r - 24), cy + Math.sin(ang) * (r - 24));
                ctx.lineTo(cx + Math.cos(ang) * (r - 14), cy + Math.sin(ang) * (r - 14));
                ctx.stroke();
            }

            // ─────────────────────────────────
            // 5. Major tick marks + nhãn ×1000
            // ─────────────────────────────────
            var majorCount = 8;  // 0, 1, 2 ... 8 (×1000 rpm)
            ctx.lineWidth        = 3;
            ctx.font             = "bold " + Math.round(r * 0.09) + "px 'Courier New'";
            ctx.textAlign        = "center";
            ctx.textBaseline     = "middle";

            for (var j = 0; j <= majorCount; j++) {
                var mRpm = root.minValue + (j / majorCount) * (root.maxValue - root.minValue);
                var mAng = rpmToRad(mRpm);
                var isRed = (mRpm >= root.redlineRpm);

                ctx.strokeStyle = isRed ? "#993333" : "#6688AA";
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(mAng) * (r - 28), cy + Math.sin(mAng) * (r - 28));
                ctx.lineTo(cx + Math.cos(mAng) * (r - 10), cy + Math.sin(mAng) * (r - 10));
                ctx.stroke();

                // Số ×1000
                var labelR = r - 42;
                ctx.fillStyle = isRed ? "#AA3333" : "#778899";
                ctx.fillText(
                    j.toString(),
                    cx + Math.cos(mAng) * labelR,
                    cy + Math.sin(mAng) * labelR
                );
            }

            // ─────────────────────────────────
            // 6. Nhãn "REDLINE" trên vùng đỏ
            // ─────────────────────────────────
            var redStartAng = rpmToRad(root.redlineRpm);
            var redMidAng   = rpmToRad((root.redlineRpm + root.maxValue) / 2);
            var labelRR     = r - 58;
            ctx.font      = "bold " + Math.round(r * 0.065) + "px 'Courier New'";
            ctx.fillStyle = redlineBlink ? "#FF4040" : "#772222";
            ctx.fillText(
                "RED",
                cx + Math.cos(redMidAng) * labelRR,
                cy + Math.sin(redMidAng) * labelRR
            );
        }
    }

    // ── Redline blink timer ──
    Timer {
        id: redlineTimer
        interval: 200
        repeat:   true
        running:  root.isRedline
        onTriggered: gaugeCanvas.redlineBlink = !gaugeCanvas.redlineBlink
        onRunningChanged: {
            if (!running) {
                gaugeCanvas.redlineBlink = false;
                gaugeCanvas.requestPaint();
            }
        }
    }

    // ─────────────────────────────────────────
    // KIM ĐỒNG HỒ RPM
    // ─────────────────────────────────────────
    Item {
        id: needleContainer
        anchors.centerIn: parent
        width:  parent.width
        height: parent.height

        rotation:        root.needleAngle
        transformOrigin: Item.Center

        // ── Behavior tăng (SpringAnimation) ──
        SpringAnimation on rotation {
            id:      rpmSpring
            enabled: false
            spring:  4.5      // Cứng hơn Speedometer → phản ứng nhanh
            damping: 0.25     // Ít giảm chấn → hơi dao động khi tới đỉnh
            epsilon: 0.5
        }

        // ── Behavior giảm (SmoothedAnimation) ──
        SmoothedAnimation on rotation {
            id:       smoothAnim
            enabled:  true
            velocity: 1500     // rơi chậm hơn tốc độ tăng
            duration: 350
            easing.type: Easing.OutQuad
        }

        // Thân kim
        Shape {
            anchors.centerIn: parent
            ShapePath {
                fillColor:   root.isRedline ? "#FF2020" : "#FF7020"
                strokeColor: "transparent"
                PathMove { x:  0; y: -gaugeCanvas.height * 0.37 }
                PathLine { x:  3.5; y: 10 }
                PathLine { x: -3.5; y: 10 }
                PathClose {}

                Behavior on fillColor {
                    ColorAnimation { duration: 100 }
                }
            }
        }

        // Đuôi kim (counterweight — hình thang ngắn)
        Shape {
            anchors.centerIn: parent
            ShapePath {
                fillColor:   "#991010"
                strokeColor: "transparent"
                PathMove { x:  3; y: 10 }
                PathLine { x: -3; y: 10 }
                PathLine { x: -1.5; y: gaugeCanvas.height * 0.09 }
                PathLine { x:  1.5; y: gaugeCanvas.height * 0.09 }
                PathClose {}
            }
        }
    }

    // ── Hub trung tâm: viền đỏ khi redline ──
    Rectangle {
        anchors.centerIn: parent
        width:  18; height: 18
        radius: 9
        color:  root.isRedline ? "#440000" : "#1A1A2A"
        border {
            color: root.isRedline ? "#FF2020" : "#556677"
            width: 2.5
        }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        // Chấm trung tâm
        Rectangle {
            anchors.centerIn: parent
            width:  6; height: 6
            radius: 3
            color:  root.isRedline ? "#FF4040" : "#8899AA"
            Behavior on color { ColorAnimation { duration: 150 } }
        }
    }

    // ── Giá trị RPM số (hiển thị ÷ 1000) ──
    Column {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom:           parent.bottom
            bottomMargin:     parent.height * 0.20
        }
        spacing: 0

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: {
                // Hiển thị 4 chữ số với leading zero
                var r = Math.round(root.smoothedValue);
                return r.toString().padStart(4, "0");
            }
            color: root.isRedline ? "#FF4040" : "#FFAA60"
            font {
                family:    "Courier New"
                bold:      true
                pixelSize: parent.parent.height * 0.11
            }
            Behavior on color { ColorAnimation { duration: 150 } }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text:  "rpm"
            color: root.isRedline ? "#882020" : "#5A3A1A"
            font {
                family:    "Courier New"
                pixelSize: parent.parent.height * 0.05
                letterSpacing: 1
            }
            Behavior on color { ColorAnimation { duration: 150 } }
        }
    }

    // ── Cảnh báo REDLINE ──
    Text {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom:           parent.bottom
            bottomMargin:     parent.height * 0.12
        }
        visible:  root.isRedline && gaugeCanvas.redlineBlink
        text:     "⚠ REDLINE"
        color:    "#FF2020"
        font {
            family:    "Courier New"
            bold:      true
            pixelSize: parent.height * 0.055
            letterSpacing: 2
        }
    }
}
