
import QtQuick 2.15

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

    // ── Smooth giá trị RPM ──
    Behavior on smoothedValue {
        NumberAnimation {
            id:           smoothValueAnim
            duration:     200
            easing.type:  Easing.OutCubic
        }
    }

    onValueChanged: {
        var clamped = Math.max(minValue, Math.min(maxValue, value));
        // Tăng ga: nhanh hơn (150ms), Thả ga: chậm hơn (400ms)
        smoothValueAnim.duration = (clamped > smoothedValue) ? 150 : 400;
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
            ctx.font             = "bold " + Math.round(r * 0.09) + "px monospace";
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
            ctx.font      = "bold " + Math.round(r * 0.065) + "px monospace";
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
    // KIM ĐỒNG HỒ RPM — vẽ bằng Canvas
    // Dùng Canvas thay Shape để tránh PathClose issue (Qt 6.5)
    // Bất đối xứng: tăng nhanh (SpringAnimation), giảm chậm (SmoothedAnimation)
    // ─────────────────────────────────────────
    Canvas {
        id: needleCanvas
        anchors.fill: parent
        z: 5

        // Góc kim hiển thị (được animate)
        property real displayAngle: root.needleAngle

        // Behavior duy nhất — duration thay đổi theo chiều qua needleAnim
        Behavior on displayAngle {
            NumberAnimation {
                id:       needleAnim
                duration: 180           // mặc định: tăng ga nhanh
                easing.type: Easing.OutQuart
            }
        }

        // Khi target angle thay đổi: điều chỉnh duration theo chiều tăng/giảm
        onDisplayAngleChanged: requestPaint()

        Connections {
            target: root
            function onNeedleAngleChanged() {
                var increasing = (root.needleAngle > needleCanvas.displayAngle);
                // Tăng ga → nhanh (180ms), Thả ga → chậm hơn (380ms, quán tính)
                needleAnim.duration    = increasing ? 180 : 380;
                needleAnim.easing.type = increasing ? Easing.OutQuart : Easing.OutCubic;
                needleCanvas.displayAngle = root.needleAngle;
            }
        }

        // Vẽ lại khi redline đổi màu
        property bool redlineState: root.isRedline
        onRedlineStateChanged: requestPaint()

        onPaint: {
                    var ctx  = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    var cx   = width  / 2;
                    var cy   = height / 2;
                    var rad  = displayAngle * Math.PI / 180;

                    // ── Cân chỉnh tỷ lệ giống hệt Speedometer ──
                    var len  = Math.min(width, height) * 0.38; // Chiều dài thân kim
                    var tail = Math.min(width, height) * 0.08; // Chiều dài đuôi kim
                    var w    = 3.5; // Nửa độ rộng gốc kim

                    ctx.save();
                    ctx.translate(cx, cy);
                    ctx.rotate(rad);

                    // 1. Vẽ thân kim (tam giác)
                    ctx.beginPath();
                    ctx.moveTo(0, -len);
                    ctx.lineTo( w,  0);
                    ctx.lineTo(-w,  0);
                    ctx.closePath();
                    ctx.fillStyle = root.isRedline ? "#FF2020" : "#FF7020";
                    ctx.fill();

                    // 2. Vẽ đuôi kim (đối trọng)
                    ctx.beginPath();
                    ctx.moveTo( w,  0);
                    ctx.lineTo(-w,  0);
                    ctx.lineTo( 0,  tail);
                    ctx.closePath();
                    ctx.fillStyle = "#991010";
                    ctx.fill();

                    ctx.restore();
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
