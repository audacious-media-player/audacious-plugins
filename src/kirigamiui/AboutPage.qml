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
    property int selectedDocument: 0

    objectName: "mobileAboutPage"
    title: qsTr("About Audacious")

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 25
            Layout.preferredHeight: Kirigami.Units.gridUnit * 5
            source: "qrc:/about-logo.svg"
            fillMode: Image.PreserveAspectFit
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
            text: root.backend.copyrightText
            wrapMode: Text.Wrap
        }

        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: Text.AlignHCenter
            text: "<a href=\"https://audacious-media-player.org\">" +
                  "https://audacious-media-player.org</a>"
            textFormat: Text.RichText
            onLinkActivated: function(link) {
                Qt.openUrlExternally(link)
            }
        }

        Controls.ButtonGroup {
            id: documentButtonGroup
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 0

            Controls.Button {
                Layout.fillWidth: true
                text: qsTr("Credits")
                checkable: true
                checked: true
                Controls.ButtonGroup.group: documentButtonGroup
                onClicked: root.selectedDocument = 0
            }

            Controls.Button {
                Layout.fillWidth: true
                text: qsTr("License")
                checkable: true
                Controls.ButtonGroup.group: documentButtonGroup
                onClicked: root.selectedDocument = 1
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.selectedDocument

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: creditsText.height
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds

                Controls.Label {
                    id: creditsText

                    width: parent.width
                    leftPadding: Kirigami.Units.largeSpacing
                    rightPadding: Kirigami.Units.largeSpacing
                    topPadding: Kirigami.Units.smallSpacing
                    bottomPadding: Kirigami.Units.largeSpacing
                    text: root.backend.creditsText
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: licenseText.height
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds

                Controls.Label {
                    id: licenseText

                    width: parent.width
                    leftPadding: Kirigami.Units.largeSpacing
                    rightPadding: Kirigami.Units.largeSpacing
                    topPadding: Kirigami.Units.smallSpacing
                    bottomPadding: Kirigami.Units.largeSpacing
                    text: root.backend.licenseText
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
