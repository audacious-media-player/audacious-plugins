/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as Controls
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami

Kirigami.ScrollablePage {
    id: root

    required property var backend

    objectName: "mobileAboutPage"
    title: qsTr("About Audacious")

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: Kirigami.Units.iconSizes.huge
            implicitHeight: implicitWidth
            source: "audacious"
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            level: 1
            text: qsTr("Audacious %1").arg(root.backend.applicationVersion)
            wrapMode: Text.Wrap
        }

        Controls.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("A fast and lightweight audio player")
            wrapMode: Text.Wrap
        }

        Controls.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            color: Kirigami.Theme.disabledTextColor
            text: qsTr("This mobile interface is built with Kirigami.")
            wrapMode: Text.Wrap
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        Controls.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: root.backend.copyrightText
            wrapMode: Text.Wrap
        }

        Controls.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Audacious is free software distributed under the BSD 2-Clause License.")
            wrapMode: Text.Wrap
        }

        Controls.Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Visit the Audacious website")
            icon.name: "internet-web-browser"
            onClicked: Qt.openUrlExternally("https://audacious-media-player.org")
        }

        Controls.Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Source code and issue tracker")
            icon.name: "code-context"
            onClicked: Qt.openUrlExternally(
                           "https://github.com/audacious-media-player/audacious")
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: Kirigami.Units.largeSpacing
        }
    }
}
