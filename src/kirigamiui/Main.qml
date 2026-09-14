/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as Controls
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami

// The C++ interface plugin injects "player" as a context property.
// qmllint disable unqualified
Kirigami.ApplicationWindow {
    id: root

    readonly property var backend: player

    width: 400
    height: 760
    minimumWidth: 320
    minimumHeight: 540
    visible: true
    title: root.backend.title.length > 0
           ? root.backend.title + " — Audacious"
           : qsTr("Audacious")

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

    function showMobilePage(source, objectName) {
        root.visible = true

        for (var index = 1; index < root.pageStack.depth; ++index) {
            var page = root.pageStack.get(index)
            if (page.objectName === objectName) {
                root.pageStack.currentIndex = index
                return
            }
        }

        root.pageStack.push(source, {"backend": root.backend})
    }

    function hideMobilePage(objectName) {
        for (var index = root.pageStack.depth - 1; index > 0; --index) {
            var page = root.pageStack.get(index)
            if (page.objectName === objectName)
                root.pageStack.removePage(page)
        }
    }

    function showPreferencesPage() {
        root.showMobilePage(Qt.resolvedUrl("PreferencesPage.qml"),
                            "mobilePreferencesPage")
    }

    function showAboutPage() {
        root.showMobilePage(Qt.resolvedUrl("AboutPage.qml"),
                            "mobileAboutPage")
    }

    function showPlaylistPage() {
        root.showMobilePage(Qt.resolvedUrl("PlaylistPage.qml"),
                            "mobilePlaylistPage")
    }

    onClosing: function(close) {
        close.accepted = false
        root.backend.requestQuit()
    }

    Connections {
        target: root.backend

        function onPreferencesRequested() {
            root.showPreferencesPage()
        }

        function onPreferencesDismissed() {
            root.hideMobilePage("mobilePreferencesPage")
        }

        function onAboutRequested() {
            root.showAboutPage()
        }

        function onAboutDismissed() {
            root.hideMobilePage("mobileAboutPage")
        }
    }

    pageStack.initialPage: Kirigami.Page {
        id: playerPage
        title: qsTr("Playing")
        padding: 0
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

        background: Item {
            id: playingBackdrop

            readonly property color baseColor: Kirigami.Theme.backgroundColor
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
                         ? (playingBackdrop.baseLuminance < 0.5 ? 0.44 : 0.60)
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
                    id: drawerButton

                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Kirigami.Units.gridUnit * 2.5
                    implicitHeight: implicitWidth
                    opacity: pressed ? 1 : 0.9
                    text: qsTr("Menu")
                    icon.name: "application-menu"
                    display: Controls.AbstractButton.IconOnly
                    background: Item {}
                    contentItem: Item {
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: 1
                            anchors.verticalCenterOffset: 2
                            width: Kirigami.Units.iconSizes.medium
                            height: width
                            source: drawerButton.icon.name
                            color: Qt.rgba(0, 0, 0, 0.75)
                        }

                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: Kirigami.Units.iconSizes.medium
                            height: width
                            source: drawerButton.icon.name
                            color: "white"
                        }
                    }
                    onClicked: navigationMenu.open()
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: text
                }

                Item { Layout.fillWidth: true }

                Controls.ToolButton {
                    id: moreButton

                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Kirigami.Units.gridUnit * 2.5
                    implicitHeight: implicitWidth
                    opacity: pressed ? 1 : 0.9
                    text: qsTr("More options")
                    icon.name: "overflow-menu"
                    display: Controls.AbstractButton.IconOnly
                    background: Item {}
                    contentItem: Item {
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: 1
                            anchors.verticalCenterOffset: 2
                            width: Kirigami.Units.iconSizes.medium
                            height: width
                            source: moreButton.icon.name
                            color: Qt.rgba(0, 0, 0, 0.75)
                        }

                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: Kirigami.Units.iconSizes.medium
                            height: width
                            source: moreButton.icon.name
                            color: "white"
                        }
                    }
                    onClicked: playingMenu.open()
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: text
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(
                                            playerPage.width
                                            - Kirigami.Units.largeSpacing * 2,
                                            playerPage.height * 0.5)
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
                    spacing: Kirigami.Units.largeSpacing

                    Item { Layout.fillWidth: true }

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

                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Controls.ToolButton {
                        text: qsTr("Shuffle")
                        icon.name: "media-playlist-shuffle"
                        display: Controls.AbstractButton.IconOnly
                        checkable: true
                        checked: root.backend.shuffle
                        onToggled: root.backend.setShuffle(checked)
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: text
                    }

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

                    Controls.ToolButton {
                        text: qsTr("Repeat")
                        icon.name: "media-playlist-repeat"
                        display: Controls.AbstractButton.IconOnly
                        checkable: true
                        checked: root.backend.repeat
                        onToggled: root.backend.setRepeat(checked)
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: text
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Controls.Button {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.rightMargin: Kirigami.Units.largeSpacing
                Layout.topMargin: Kirigami.Units.smallSpacing
                Layout.bottomMargin: Kirigami.Units.largeSpacing
                text: root.backend.playlistTitle.length > 0
                      ? qsTr("View %1").arg(root.backend.playlistTitle)
                      : qsTr("View playlist")
                icon.name: "view-media-playlist"
                onClicked: root.showPlaylistPage()
            }
        }

        Controls.Menu {
            id: navigationMenu

            x: Kirigami.Units.largeSpacing
            y: drawerButton.mapToItem(playerPage, 0, drawerButton.height).y
               + Kirigami.Units.smallSpacing

            Controls.MenuItem {
                text: qsTr("Playlist")
                icon.name: "view-media-playlist"
                onTriggered: root.showPlaylistPage()
            }

            Controls.MenuItem {
                text: qsTr("New playlist")
                icon.name: "tab-new"
                onTriggered: root.backend.newPlaylist()
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                text: qsTr("Preferences")
                icon.name: "configure"
                onTriggered: root.backend.showPreferences()
            }

            Controls.MenuItem {
                text: qsTr("About Audacious")
                icon.name: "help-about"
                onTriggered: root.backend.showAbout()
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                text: qsTr("Quit")
                icon.name: "application-exit"
                onTriggered: root.backend.requestQuit()
            }
        }

        Controls.Menu {
            id: playingMenu

            x: playerPage.width - width - Kirigami.Units.largeSpacing
            y: moreButton.mapToItem(playerPage, 0, moreButton.height).y
               + Kirigami.Units.smallSpacing

            Controls.MenuItem {
                text: qsTr("Open files")
                icon.name: "document-open"
                onTriggered: root.backend.openFiles()
            }

            Controls.MenuItem {
                text: qsTr("Add files")
                icon.name: "list-add"
                onTriggered: root.backend.addFiles()
            }

            Controls.MenuItem {
                text: qsTr("Stop playback")
                icon.name: "media-playback-stop"
                enabled: root.backend.playing
                onTriggered: root.backend.stop()
            }
        }
    }
}
