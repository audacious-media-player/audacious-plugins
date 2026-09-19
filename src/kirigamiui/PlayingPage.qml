/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as Controls
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami

Kirigami.Page {
    id: root

    required property var backend
    required property var dismiss

    objectName: "mobilePlayingPage"
    title: qsTr("Playing")
    padding: 0
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

    function formatTime(milliseconds) {
        var total = Math.max(0, Math.floor(milliseconds / 1000))
        var seconds = total % 60
        var minutes = Math.floor(total / 60) % 60
        var hours = Math.floor(total / 3600)
        if (hours > 0)
            return hours + ":" + (minutes < 10 ? "0" : "") + minutes
                    + ":" + (seconds < 10 ? "0" : "") + seconds
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    background: Item {
        id: playingBackdrop

        readonly property color baseColor: Kirigami.Theme.backgroundColor
        readonly property color color: baseColor
        readonly property real baseLuminance:
            baseColor.r * 0.2126 + baseColor.g * 0.7152
            + baseColor.b * 0.0722

        Rectangle {
            anchors.fill: parent
            color: playingBackdrop.baseColor
        }

        Image {
            id: backdropImage

            anchors.fill: parent
            source: root.backend.albumArtBackground
            fillMode: Image.Stretch
            smooth: true
            mipmap: true
            asynchronous: true
            cache: false
            opacity: status === Image.Ready ? 0.72 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: 350
                    easing.type: Easing.InOutQuad
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            color: playingBackdrop.baseColor
            opacity: backdropImage.status === Image.Ready
                     ? (playingBackdrop.baseLuminance < 0.5 ? 0.44 : 0.52)
                     : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: 350
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 3
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            spacing: 0

            Controls.ToolButton {
                id: dismissButton

                Layout.alignment: Qt.AlignVCenter
                implicitWidth: Kirigami.Units.gridUnit * 2.5
                implicitHeight: implicitWidth
                opacity: pressed ? 1 : 0.9
                text: qsTr("Back to playlist")
            icon.name: "go-down"
                display: Controls.AbstractButton.IconOnly
                background: Item {}
                contentItem: Item {
                    Kirigami.Icon {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: 1
                        anchors.verticalCenterOffset: 2
                        width: Kirigami.Units.iconSizes.medium
                        height: width
                        source: dismissButton.icon.name
                        color: Qt.rgba(0, 0, 0, 0.75)
                    }

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: Kirigami.Units.iconSizes.medium
                        height: width
                        source: dismissButton.icon.name
                        color: "white"
                    }
                }
            onClicked: root.dismiss()
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: text
            }

            Item { Layout.fillWidth: true }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(
                                    root.width
                                        - Kirigami.Units.largeSpacing * 2,
                                    root.height * 0.5)
            Layout.minimumHeight: Kirigami.Units.gridUnit * 10

            Rectangle {
                id: artFrame
                anchors.centerIn: parent
                width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2,
                                parent.height - Kirigami.Units.largeSpacing * 2)
                height: width
                radius: Kirigami.Units.largeSpacing * 1.5
                color: Kirigami.Theme.alternateBackgroundColor

                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: parent.width / 3
                    height: width
                    source: "media-optical-audio"
                    color: Kirigami.Theme.disabledTextColor
                    visible: cover.status !== Image.Ready
                }

                Image {
                    id: cover

                    anchors.fill: parent
                    source: root.backend.albumArt
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: false
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Heading {
                Layout.fillWidth: true
                level: 2
                text: root.backend.title.length > 0 ? root.backend.title : qsTr("Nothing playing")
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Controls.Label {
                Layout.fillWidth: true
                text: root.backend.artist
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                visible: text.length > 0
            }

            Controls.Label {
                Layout.fillWidth: true
                text: root.backend.album
                horizontalAlignment: Text.AlignHCenter
                color: Kirigami.Theme.disabledTextColor
                elide: Text.ElideRight
                visible: text.length > 0
            }

            Controls.Slider {
                id: seekSlider
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, root.backend.duration)
                enabled: root.backend.ready && root.backend.duration > 0
                onPressedChanged: {
                    if (!pressed)
                        root.backend.seek(value)
                }

                Binding on value {
                    value: root.backend.position
                    when: !seekSlider.pressed
                    restoreMode: Binding.RestoreNone
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Controls.Label {
                    text: root.formatTime(seekSlider.pressed
                                          ? seekSlider.value
                                          : root.backend.position)
                    font.features: {"tnum": 1}
                }
                Item { Layout.fillWidth: true }
                Controls.Label {
                    text: root.formatTime(root.backend.duration)
                    font.features: {"tnum": 1}
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                Item { Layout.fillWidth: true }

                Controls.ToolButton {
                    text: qsTr("Shuffle")
                    icon.name: "media-playlist-shuffle"

                    // I am not sure why the icon size needs to be set for the shuffle/repeat
                    // actions but the other tool buttons are fine.
                    icon.width: Kirigami.Units.iconSizes.medium
                    icon.height: Kirigami.Units.iconSizes.medium

                    display: Controls.AbstractButton.IconOnly
                    checkable: true
                    checked: root.backend.shuffle
                    onToggled: root.backend.setShuffle(checked)
                    implicitWidth: Kirigami.Units.gridUnit * 3
                    implicitHeight: implicitWidth
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: text
                }

                Controls.ToolButton {
                    text: qsTr("Previous")
                    icon.name: "media-skip-backward"
                    display: Controls.AbstractButton.IconOnly
                    onClicked: root.backend.previous()
                    implicitWidth: Kirigami.Units.gridUnit * 3
                    implicitHeight: implicitWidth
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: text
                }

                Controls.RoundButton {
                    text: root.backend.playing && !root.backend.paused ? qsTr("Pause") : qsTr("Play")
                    icon.name: root.backend.playing && !root.backend.paused
                               ? "media-playback-pause"
                               : "media-playback-start"
                    display: Controls.AbstractButton.IconOnly
                    onClicked: root.backend.playPause()
                    implicitWidth: Kirigami.Units.gridUnit * 4
                    implicitHeight: implicitWidth
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: text
                }

                Controls.ToolButton {
                    text: qsTr("Next")
                    icon.name: "media-skip-forward"
                    display: Controls.AbstractButton.IconOnly
                    onClicked: root.backend.next()
                    implicitWidth: Kirigami.Units.gridUnit * 3
                    implicitHeight: implicitWidth
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: text
                }

                Controls.ToolButton {
                    text: qsTr("Repeat")
                    icon.name: "media-playlist-repeat"

                    // I am not sure why the icon size needs to be set for the shuffle/repeat
                    // actions but the other tool buttons are fine.
                    icon.width: Kirigami.Units.iconSizes.medium
                    icon.height: Kirigami.Units.iconSizes.medium

                    display: Controls.AbstractButton.IconOnly
                    checkable: true
                    checked: root.backend.repeat
                    onToggled: root.backend.setRepeat(checked)
                    implicitWidth: Kirigami.Units.gridUnit * 3
                    implicitHeight: implicitWidth
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: text
                }

                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true

                Kirigami.Icon {
                    source: "audio-volume-medium"
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: implicitWidth
                }

                Controls.Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: root.backend.volume
                    onMoved: root.backend.setVolume(value)
                    Accessible.name: qsTr("Volume")
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.largeSpacing
        }
    }

}
