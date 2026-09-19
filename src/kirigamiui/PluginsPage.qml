/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as Controls
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami

// The delegate reaches custom properties on its owning ListView.
// qmllint disable missing-property
Kirigami.Page {
    id: root

    required property var backend
    required property var showPlugin

    objectName: "mobilePluginsPage"
    title: qsTr("Plugins")
    padding: 0

    ListView {
        id: pluginList

        readonly property var controller: root.backend
        readonly property var openPlugin: root.showPlugin

        anchors.fill: parent
        clip: true
        model: root.backend.pluginModel
        boundsBehavior: Flickable.StopAtBounds

        section.property: "category"
        section.criteria: ViewSection.FullString
        section.delegate: Kirigami.ListSectionHeader {
            required property string section
            width: ListView.view.width
            text: section
        }

        delegate: Controls.ItemDelegate {
            id: pluginDelegate

            required property int index
            required property string pluginName
            required property string basename
            required property bool pluginEnabled
            required property bool hasPreferences
            required property bool hasAbout

            readonly property bool hasDetails: hasPreferences || hasAbout

            width: ListView.view.width
            height: Math.max(implicitHeight, Kirigami.Units.gridUnit * 3)
            onClicked: {
                if (pluginDelegate.pluginEnabled && pluginDelegate.hasDetails)
                    pluginDelegate.ListView.view.openPlugin(
                                pluginDelegate.basename)
            }

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Controls.Label {
                        Layout.fillWidth: true
                        text: pluginDelegate.pluginName
                        elide: Text.ElideRight
                    }

                    Controls.Label {
                        Layout.fillWidth: true
                        visible: pluginDelegate.pluginEnabled
                                 && pluginDelegate.hasDetails
                        text: pluginDelegate.hasPreferences
                              ? (pluginDelegate.hasAbout
                                 ? qsTr("Settings and information")
                                 : qsTr("Settings"))
                              : qsTr("Information")
                        color: Kirigami.Theme.disabledTextColor
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                    }
                }

                Kirigami.Icon {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: width
                    source: "go-next"
                    visible: pluginDelegate.pluginEnabled
                             && pluginDelegate.hasDetails
                }

                Controls.Switch {
                    Accessible.name: qsTr("Enable %1").arg(pluginDelegate.pluginName)
                    checked: pluginDelegate.pluginEnabled
                    onToggled: pluginDelegate.ListView.view.controller.setPluginEnabled(
                                   pluginDelegate.index, checked)
                }
            }
        }

        Controls.ScrollIndicator.vertical: Controls.ScrollIndicator {}
    }
}
// qmllint enable missing-property
