// SPDX-License-Identifier: Apache-2.0
// Async capture spike Quick3D scene (issue #16).
//
// Same structure as AsyncScene.qml: a Quick3D viewport plus 2D QML content in the
// same root item, so the composition and fidelity checks can tell whether the
// asynchronous capture reproduces the composed result.

import QtQuick
import QtQuick3D

Item {
    id: root

    property int tick: 0
    property bool frozen: false
    property real angle: 0
    property real degrees: angle * 57.29577951

    View3D {
        id: view
        anchors.fill: parent

        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: "#101820"
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        PerspectiveCamera {
            position: Qt.vector3d(0, 140, 460)
            eulerRotation.x: -14
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(-35, -40, 0)
            brightness: 1.1
        }

        Model {
            source: "#Sphere"
            scale: Qt.vector3d(1.5, 1.5, 1.5)
            eulerRotation.y: root.degrees
            materials: PrincipledMaterial {
                baseColor: "#e0592a"
                metalness: 0.25
                roughness: 0.35
            }
        }

        Node {
            eulerRotation.y: -root.degrees

            Model {
                source: "#Cube"
                position: Qt.vector3d(190, 0, 0)
                scale: Qt.vector3d(0.9, 0.9, 0.9)
                materials: PrincipledMaterial {
                    baseColor: "#4a90d9"
                    metalness: 0.15
                    roughness: 0.5
                }
            }
        }
    }

    Rectangle {
        objectName: "tickPatch"
        x: 8
        y: 8
        width: 48
        height: 48
        color: Qt.rgba((root.tick & 0xff) / 255.0, ((root.tick >> 8) & 0xff) / 255.0, 1.0, 1.0)
    }

    Rectangle {
        objectName: "staticBlock"
        x: 80
        y: 500
        width: 160
        height: 80
        color: "#3fb950"
    }

    NumberAnimation on angle {
        from: 0
        to: Math.PI * 2
        duration: 2400
        loops: Animation.Infinite
        running: !root.frozen
    }
}
