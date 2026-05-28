/**
 * @file    GearDisplay.qml
 * @brief   Hiển thị trạng thái hộp số PRND
 *
 * Màu sắc theo tiêu chuẩn automotive:
 *   P → Trắng  (xe đứng yên, an toàn)
 *   R → Đỏ    (cảnh báo, lùi xe)
 *   N → Vàng  (trung tính, cẩn thận)
 *   D → Xanh  (sẵn sàng tiến)
 */

import QtQuick 2.15
import QtQuick.Effects

Item {
    id: root
    property string currentGear: "P"

    // ── Model dữ liệu PRND ──
    readonly property var gearData: ({
        "P": { color: "#E8E8F0", glow: "#AAAACC", label: "PARK"    },
        "R": { color: "#FF3030", glow: "#FF0000", label: "REVERSE"  },
        "N": { color: "#FFD700", glow: "#FFB000", label: "NEUTRAL"  },
        "D": { color: "#00E676", glow: "#00C853", label: "DRIVE"    }
    })

    Row {
        anchors.centerIn: parent
        spacing: root.width * 0.04

        Repeater {
            model: ["P", "R", "N", "D"]

            delegate: Item {
                            id: gearItem
                            property string gearLetter: modelData
                            property bool   isActive:   (gearLetter === root.currentGear)

                            // 1. Đổi tên từ 'data' thành 'gearConfig' để tránh trùng từ khóa hệ thống
                            property var    gearConfig: root.gearData[gearLetter]

                            width:  root.width  * 0.3
                            height: root.height

                            // ── Background glow effect khi active ──
                            Rectangle {
                                id: glowRect
                                anchors.centerIn: parent

                                // 2. Thay toàn bộ 'parent' thành 'gearItem'
                                width:  gearItem.isActive ? gearItem.width  + 8 : gearItem.width
                                height: gearItem.isActive ? gearItem.height + 8 : gearItem.height
                                radius: 10
                                color:  "transparent"
                                border {
                                    color: gearItem.isActive ? gearItem.gearConfig.glow : "transparent"
                                    width: gearItem.isActive ? 2 : 0
                                }

                                Behavior on border.width { NumberAnimation { duration: 200 } }
                                Behavior on width        { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                            }

                            // ── Main tile ──
                            Rectangle {
                                id: tile
                                anchors.fill: parent
                                radius: 8

                                color: gearItem.isActive ? gearItem.gearConfig.color : "#0D1520"

                                border {
                                    color: gearItem.isActive ? gearItem.gearConfig.glow : "#1E3040"
                                    width: 1.5
                                }

                                gradient: gearItem.isActive ? activeGrad : inactiveGrad

                                Gradient {
                                    id: activeGrad
                                    orientation: Gradient.Vertical
                                    // Gọi trực tiếp id gearItem thay vì gọi parent.parent.data...
                                    GradientStop { position: 0.0; color: Qt.lighter(gearItem.gearConfig.color, 1.4) }
                                    GradientStop { position: 1.0; color: gearItem.gearConfig.color }
                                }
                                Gradient {
                                    id: inactiveGrad
                                    orientation: Gradient.Vertical
                                    GradientStop { position: 0.0; color: "#111B28" }
                                    GradientStop { position: 1.0; color: "#0A1018" }
                                }

                                Behavior on color {
                                    ColorAnimation { duration: 300; easing.type: Easing.OutCubic }
                                }

                                // ── Ký tự Gear to ──
                                Text {
                                    id: gearText
                                    anchors {
                                        horizontalCenter: parent.horizontalCenter
                                        top:              parent.top
                                        topMargin:        parent.height * 0.12
                                    }
                                    text:  gearItem.gearLetter
                                    color: gearItem.isActive ? "#0A0A0F" : "#2A4060"
                                    font {
                                        family:    "Courier New"
                                        bold:      true
                                        pixelSize: parent.height * 0.45
                                    }
                                    Behavior on color { ColorAnimation { duration: 250 } }
                                }

                                // ── Label nhỏ bên dưới ──
                                Text {
                                    anchors {
                                        horizontalCenter: parent.horizontalCenter
                                        bottom:           parent.bottom
                                        bottomMargin:     parent.height * 0.08
                                    }
                                    text:     gearItem.gearConfig.label
                                    color:    gearItem.isActive ? "#1A1A2A" : "#1A2A3A"
                                    font {
                                        family:    "Courier New"
                                        pixelSize: parent.height * 0.10
                                        letterSpacing: 1
                                    }
                                    Behavior on color { ColorAnimation { duration: 250 } }
                                }

                                // ── Scale animation khi chuyển số ──
                                scale: gearItem.isActive ? 1.05 : 1.0
                                Behavior on scale {
                                    SpringAnimation { spring: 5; damping: 0.5 }
                                }
                            }
                        }
        }
    }
}
