// SPDX-License-Identifier: Apache-2.0
// SPIKE-01 throwaway Quick3D scene (issue #3, case E).
//
// Quick3D renders through its own renderer inside the scene graph, so this scene
// checks that the baseline capture still contains the final composed result.

import QtQuick
import QtQuick3D

Item {
    id: root

    property int frameCounter: 0
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
            id: camera
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

            Model {
                source: "#Cylinder"
                position: Qt.vector3d(-190, 0, 0)
                scale: Qt.vector3d(0.7, 0.9, 0.7)
                materials: PrincipledMaterial {
                    baseColor: "#3fb950"
                    metalness: 0.1
                    roughness: 0.6
                }
            }
        }
    }

    Rectangle {
        x: 24
        y: 24
        width: 320
        height: 48
        radius: 8
        color: "#0d1117"
        opacity: 0.85

        Text {
            anchors.centerIn: parent
            text: "HyRemote SPIKE-01 :: Quick3D  frame " + root.frameCounter
            color: "#e6edf3"
            font.pixelSize: 18
        }
    }

    NumberAnimation on angle {
        from: 0
        to: Math.PI * 2
        duration: 2400
        loops: Animation.Infinite
        running: true
    }

    Timer {
        interval: 16
        repeat: true
        running: true
        onTriggered: root.frameCounter++
    }
}
