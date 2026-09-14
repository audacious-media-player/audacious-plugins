/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as Controls
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kirigami.primitives 2.11 as KirigamiPrimitives

// The C++ interface plugin injects "player" as a context property.
// qmllint disable unqualified
Kirigami.Page {
    id: root

    required property var backend
    required property var showPlaying

    objectName: "mobilePlaylistPage"
    title: root.backend.playlistTitle.length > 0
           ? root.backend.playlistTitle
           : qsTr("Playlist")
    padding: 0

    function metadataDetails(artist, album) {
        var details = []
        if (artist.length > 0)
            details.push(artist)
        if (album.length > 0)
            details.push(album)
        return details.join(" · ")
    }

    actions: [
        Kirigami.Action {
            text: qsTr("Add files")
            icon.name: "list-add"
            onTriggered: root.backend.addFiles()
        },
        Kirigami.Action {
            text: qsTr("New playlist")
            icon.name: "tab-new"
            onTriggered: root.backend.newPlaylist()
        }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing

            Controls.ComboBox {
                Layout.fillWidth: true
                model: root.backend.playlistNames
                currentIndex: root.backend.activePlaylistIndex
                Accessible.name: qsTr("Playlist")
                onActivated: function(index) {
                    root.backend.activatePlaylist(index)
                }
            }

            Controls.ToolButton {
                text: qsTr("Add files")
                icon.name: "list-add"
                display: Controls.AbstractButton.IconOnly
                onClicked: root.backend.addFiles()
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: text
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: playlistView

                anchors.fill: parent
                clip: true
                model: root.backend.playlistModel
                currentIndex: -1
                boundsBehavior: Flickable.StopAtBounds
                bottomMargin: miniPlayer.visible
                              ? miniPlayer.height
                                + Kirigami.Units.largeSpacing * 2
                              : 0

                delegate: Controls.ItemDelegate {
                    id: rowDelegate

                    required property int index
                    required property string title
                    required property string artist
                    required property string album
                    required property string duration
                    required property bool playing
                    required property int queuePosition

                    readonly property var controller: player

                    width: ListView.view.width
                    height: Math.max(implicitHeight,
                                     Kirigami.Units.gridUnit * 4)
                    highlighted: playing
                    onClicked: controller.playEntry(index)

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: rowDelegate.playing
                                    ? (rowDelegate.controller.paused
                                       ? "media-playback-pause"
                                       : "media-playback-start")
                                    : "audio-x-generic"
                            implicitWidth: Kirigami.Units.iconSizes.medium
                            implicitHeight: implicitWidth
                            color: rowDelegate.playing
                                   ? Kirigami.Theme.highlightColor
                                   : Kirigami.Theme.textColor
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Controls.Label {
                                Layout.fillWidth: true
                                text: rowDelegate.title
                                font.bold: rowDelegate.playing
                                elide: Text.ElideRight
                            }

                            Controls.Label {
                                Layout.fillWidth: true
                                text: root.metadataDetails(rowDelegate.artist,
                                                           rowDelegate.album)
                                color: Kirigami.Theme.disabledTextColor
                                elide: Text.ElideRight
                                visible: text.length > 0
                            }
                        }

                        Controls.Label {
                            text: rowDelegate.queuePosition > 0
                                  ? qsTr("Q%1").arg(
                                        rowDelegate.queuePosition)
                                  : rowDelegate.duration
                            color: rowDelegate.queuePosition > 0
                                   ? Kirigami.Theme.highlightColor
                                   : Kirigami.Theme.disabledTextColor
                        }

                        Controls.ToolButton {
                            text: qsTr("Track actions")
                            icon.name: "overflow-menu"
                            display: Controls.AbstractButton.IconOnly
                            onClicked: trackMenu.open()
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.text: text

                            Controls.Menu {
                                id: trackMenu

                                y: parent.height

                                Controls.MenuItem {
                                    text: rowDelegate.queuePosition > 0
                                          ? qsTr("Remove from queue")
                                          : qsTr("Add to queue")
                                    icon.name: rowDelegate.queuePosition > 0
                                               ? "list-remove"
                                               : "list-add"
                                    onTriggered: rowDelegate.controller.toggleQueued(
                                                     rowDelegate.index)
                                }

                                Controls.MenuItem {
                                    text: qsTr("Remove from playlist")
                                    icon.name: "edit-delete"
                                    onTriggered: rowDelegate.controller.removeEntry(
                                                     rowDelegate.index)
                                }
                            }
                        }
                    }
                }

                Controls.ScrollIndicator.vertical: Controls.ScrollIndicator {}
            }

            KirigamiPrimitives.ShadowedRectangle {
                id: miniPlayer

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Kirigami.Units.largeSpacing
                z: 2
                height: Kirigami.Units.gridUnit * 4
                visible: root.backend.playing
                         || root.backend.title.length > 0
                color: Kirigami.Theme.backgroundColor
                radius: Kirigami.Units.largeSpacing

                border.width: 1
                border.color: Qt.rgba(Kirigami.Theme.textColor.r,
                                      Kirigami.Theme.textColor.g,
                                      Kirigami.Theme.textColor.b, 0.18)
                shadow.size: Kirigami.Units.largeSpacing
                shadow.color: Qt.rgba(0, 0, 0, 0.35)
                shadow.yOffset: 2

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.showPlaying()

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Controls.AbstractButton {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumWidth: 0
                            text: qsTr("Now playing")
                            Accessible.name: text
                            background: Item {}
                            onClicked: root.showPlaying()

                            contentItem: RowLayout {
                                spacing: Kirigami.Units.smallSpacing

                                Rectangle {
                                    Layout.preferredWidth: parent.height
                                    Layout.preferredHeight: width
                                    radius: Kirigami.Units.smallSpacing
                                    color: Kirigami.Theme.alternateBackgroundColor

                                    Kirigami.Icon {
                                        anchors.centerIn: parent
                                        width: parent.width / 2
                                        height: width
                                        source: "media-optical-audio"
                                        color: Kirigami.Theme.disabledTextColor
                                        visible: miniCover.status !== Image.Ready
                                    }

                                    Image {
                                        id: miniCover

                                        anchors.fill: parent
                                        source: root.backend.albumArt
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        cache: false
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 0

                                    Controls.Label {
                                        Layout.fillWidth: true
                                        text: root.backend.title.length > 0
                                              ? root.backend.title
                                              : qsTr("Nothing playing")
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Controls.Label {
                                        Layout.fillWidth: true
                                        text: root.metadataDetails(
                                                  root.backend.artist,
                                                  root.backend.album)
                                        color: Kirigami.Theme.disabledTextColor
                                        elide: Text.ElideRight
                                        visible: text.length > 0
                                    }
                                }
                            }
                        }

                        Controls.ToolButton {
                            text: qsTr("Previous")
                            icon.name: "media-skip-backward"
                            display: Controls.AbstractButton.IconOnly
                            onClicked: root.backend.previous()
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.text: text
                        }

                        Controls.ToolButton {
                            text: root.backend.playing && !root.backend.paused
                                  ? qsTr("Pause") : qsTr("Play")
                            icon.name: root.backend.playing && !root.backend.paused
                                       ? "media-playback-pause"
                                       : "media-playback-start"
                            display: Controls.AbstractButton.IconOnly
                            onClicked: root.backend.playPause()
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.text: text
                        }

                        Controls.ToolButton {
                            text: qsTr("Next")
                            icon.name: "media-skip-forward"
                            display: Controls.AbstractButton.IconOnly
                            onClicked: root.backend.next()
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.text: text
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                visible: playlistView.count === 0

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignHCenter
                    source: "folder-music-symbolic"
                    implicitWidth: Kirigami.Units.iconSizes.huge
                    implicitHeight: implicitWidth
                }

                Kirigami.Heading {
                    Layout.alignment: Qt.AlignHCenter
                    level: 3
                    text: qsTr("This playlist is empty")
                }

                Controls.Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Add music")
                    icon.name: "list-add"
                    onClicked: root.backend.addFiles()
                }
            }
        }
    }
}
