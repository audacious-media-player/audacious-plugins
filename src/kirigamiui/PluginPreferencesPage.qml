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
    required property string basename
    required property var dismiss

    property var details: ({})
    property var preferences: []
    property var preferenceValues: ({})
    property int preferenceRevision: 0
    property bool sessionClosed: false

    objectName: "mobilePluginPreferencesPage"
    title: details.name || qsTr("Plugin")

    function reloadValues() {
        root.preferenceValues = root.backend.pluginPreferenceValues();
        root.preferenceRevision++;
    }

    function valueFor(id) {
        root.preferenceRevision;
        return root.preferenceValues[id];
    }

    function preferenceEnabled(preference) {
        root.preferenceRevision;
        var dependencies = preference.dependencies || [];
        for (var index = 0; index < dependencies.length; ++index) {
            if (!root.preferenceValues[dependencies[index]])
                return false;
        }
        return true;
    }

    function updatePreference(id, value) {
        root.backend.setPluginPreference(id, value);
        root.reloadValues();
    }

    function componentForType(type) {
        switch (type) {
        case "heading": return headingComponent;
        case "label": return labelComponent;
        case "button": return buttonComponent;
        case "check": return checkComponent;
        case "radio": return radioComponent;
        case "integer": return integerComponent;
        case "double": return doubleComponent;
        case "string":
        case "file":
        case "font": return stringComponent;
        case "combo": return comboComponent;
        case "separator": return separatorComponent;
        case "unsupported": return unsupportedComponent;
        default: return null;
        }
    }

    function closeSession(apply) {
        if (root.sessionClosed)
            return;
        root.sessionClosed = true;
        root.backend.closePluginPreferences(root.basename, apply);
    }

    Component.onCompleted: {
        root.details = root.backend.openPluginPreferences(root.basename);
        root.preferences = root.details.preferences || [];
        root.reloadValues();
    }

    Component.onDestruction: root.closeSession(false)

    footer: Controls.ToolBar {
        visible: root.details.requiresApply === true
        implicitHeight: visible ? footerLayout.implicitHeight
                                  + Kirigami.Units.smallSpacing * 2 : 0

        RowLayout {
            id: footerLayout
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing

            Item {
                Layout.fillWidth: true
            }

            Controls.Button {
                text: qsTr("Cancel")
                onClicked: {
                    root.closeSession(false);
                    root.dismiss();
                }
            }

            Controls.Button {
                text: qsTr("Set")
                highlighted: true
                onClicked: {
                    root.closeSession(true);
                    root.dismiss();
                }
            }
        }
    }

    ColumnLayout {
        id: contents

        readonly property var page: root

        width: parent.width
        spacing: Kirigami.Units.smallSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: Object.keys(root.details).length === 0
            text: qsTr("This plugin is no longer available.")
            type: Kirigami.MessageType.Error
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            visible: root.details.hasPreferences === true
            text: qsTr("Settings")
            level: 2
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.details.nativePreferences === true
            text: qsTr("This plugin uses native desktop settings.")
            type: Kirigami.MessageType.Information
        }

        Controls.Button {
            Layout.fillWidth: true
            visible: root.details.nativePreferences === true
            text: qsTr("Open native settings")
            icon.name: "preferences-system"
            onClicked: {
                root.closeSession(false);
                root.backend.openNativePluginPreferences(root.basename);
                root.dismiss();
            }
        }

        Repeater {
            id: preferenceRepeater

            readonly property var page: contents.page

            model: root.preferences

            delegate: Loader {
                id: preferenceLoader

                required property var modelData
                readonly property var preference: modelData
                readonly property var page: parent.page

                Layout.fillWidth: true
                Layout.leftMargin: (preference.indent || 0)
                                   * Kirigami.Units.largeSpacing
                enabled: preferenceLoader.page.preferenceEnabled(preference)

                sourceComponent: preferenceLoader.page.componentForType(
                                     preference.type)

                Binding {
                    target: preferenceLoader.item
                    property: "preference"
                    value: preferenceLoader.preference
                    when: preferenceLoader.item !== null
                }

                Binding {
                    target: preferenceLoader.item
                    property: "page"
                    value: preferenceLoader.page
                    when: preferenceLoader.item !== null
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            visible: root.details.about && root.details.about.length > 0
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            visible: root.details.about && root.details.about.length > 0
            text: qsTr("About")
            level: 2
        }

        Controls.Label {
            Layout.fillWidth: true
            visible: root.details.about && root.details.about.length > 0
            text: root.details.about || ""
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: Kirigami.Units.largeSpacing
        }
    }

    Component {
        id: headingComponent

        Kirigami.Heading {
            id: preferenceHeading

            property var preference: ({})
            property var page: null

            width: parent.width
            text: preferenceHeading.preference.label || ""
            level: 3
            wrapMode: Text.Wrap
        }
    }

    Component {
        id: labelComponent

        Controls.Label {
            id: preferenceLabel

            property var preference: ({})
            property var page: null

            width: parent.width
            text: preferenceLabel.preference.label || ""
            textFormat: Text.RichText
            wrapMode: Text.Wrap
        }
    }

    Component {
        id: buttonComponent

        Controls.Button {
            id: preferenceButton

            property var preference: ({})
            property var page: null

            text: preferenceButton.preference.label || ""
            icon.name: preferenceButton.preference.icon || ""
            onClicked: {
                preferenceButton.page.backend.activatePluginPreference(
                            preferenceButton.preference.id);
                preferenceButton.page.reloadValues();
            }
        }
    }

    Component {
        id: checkComponent

        Controls.CheckBox {
            id: preferenceCheck

            property var preference: ({})
            property var page: null

            width: parent.width
            text: preferenceCheck.preference.label || ""
            checked: !!preferenceCheck.page.valueFor(preferenceCheck.preference.id)
            onToggled: preferenceCheck.page.updatePreference(
                           preferenceCheck.preference.id, checked)
        }
    }

    Component {
        id: radioComponent

        Controls.RadioButton {
            id: preferenceRadio

            property var preference: ({})
            property var page: null

            width: parent.width
            text: preferenceRadio.preference.label || ""
            checked: !!preferenceRadio.page.valueFor(preferenceRadio.preference.id)
            onClicked: preferenceRadio.page.updatePreference(
                           preferenceRadio.preference.id, true)
        }
    }

    Component {
        id: integerComponent

        RowLayout {
            id: integerRow

            property var preference: ({})
            property var page: null

            width: parent.width

            Controls.Label {
                Layout.fillWidth: true
                text: integerRow.preference.label || ""
                wrapMode: Text.Wrap
            }

            Controls.SpinBox {
                from: Math.round(integerRow.preference.minimum)
                to: Math.round(integerRow.preference.maximum)
                stepSize: Math.max(1, Math.round(integerRow.preference.step))
                editable: true
                value: Number(integerRow.page.valueFor(integerRow.preference.id))
                onValueModified: integerRow.page.updatePreference(
                                     integerRow.preference.id, value)
            }

            Controls.Label {
                text: integerRow.preference.suffix || ""
                visible: text.length > 0
            }
        }
    }

    Component {
        id: doubleComponent

        RowLayout {
            id: doubleRow

            property var preference: ({})
            property var page: null
            width: parent.width

            readonly property int decimals: Math.max(0, -Math.floor(Math.log(
                                                                        doubleRow.preference.step)
                                                                    / Math.LN10 + 0.01))
            readonly property int valueScale: Math.pow(10, decimals)

            Controls.Label {
                Layout.fillWidth: true
                text: doubleRow.preference.label || ""
                wrapMode: Text.Wrap
            }

            Controls.SpinBox {
                from: Math.round(doubleRow.preference.minimum * doubleRow.valueScale)
                to: Math.round(doubleRow.preference.maximum * doubleRow.valueScale)
                stepSize: Math.max(1, Math.round(doubleRow.preference.step
                                                 * doubleRow.valueScale))
                editable: true
                value: Math.round(Number(doubleRow.page.valueFor(
                                             doubleRow.preference.id))
                                  * doubleRow.valueScale)
                textFromValue: function (value, locale) {
                    return Number(value / doubleRow.valueScale).toLocaleString(locale,
                                                                               'f', doubleRow.decimals);
                }
                valueFromText: function (text, locale) {
                    return Math.round(Number.fromLocaleString(locale, text)
                                      * doubleRow.valueScale);
                }
                onValueModified: doubleRow.page.updatePreference(
                                     doubleRow.preference.id,
                                     value / doubleRow.valueScale)
            }

            Controls.Label {
                text: doubleRow.preference.suffix || ""
                visible: text.length > 0
            }
        }
    }

    Component {
        id: stringComponent

        ColumnLayout {
            id: stringColumn

            property var preference: ({})
            property var page: null

            width: parent.width
            spacing: Kirigami.Units.smallSpacing

            Controls.Label {
                Layout.fillWidth: true
                text: stringColumn.preference.label || ""
                visible: text.length > 0
                wrapMode: Text.Wrap
            }

            Controls.TextField {
                Layout.fillWidth: true
                text: String(stringColumn.page.valueFor(
                                 stringColumn.preference.id) || "")
                echoMode: stringColumn.preference.password ? TextInput.Password :
                                                             TextInput.Normal
                onEditingFinished: stringColumn.page.updatePreference(
                                       stringColumn.preference.id, text)
            }
        }
    }

    Component {
        id: comboComponent

        ColumnLayout {
            id: comboColumn

            property var preference: ({})
            property var page: null

            width: parent.width
            spacing: Kirigami.Units.smallSpacing

            Controls.Label {
                Layout.fillWidth: true
                text: comboColumn.preference.label || ""
                visible: text.length > 0
                wrapMode: Text.Wrap
            }

            Controls.ComboBox {
                Layout.fillWidth: true
                model: comboColumn.preference.choices || []
                textRole: "label"
                currentIndex: Number(comboColumn.page.valueFor(
                                         comboColumn.preference.id))
                onActivated: function (index) {
                    comboColumn.page.updatePreference(comboColumn.preference.id,
                                                      index);
                }
            }
        }
    }

    Component {
        id: separatorComponent

        Kirigami.Separator {
            property var preference: ({})
            property var page: null

            width: parent.width
        }
    }

    Component {
        id: unsupportedComponent

        Kirigami.InlineMessage {
            id: unsupportedMessage

            property var preference: ({})
            property var page: null

            width: parent.width
            visible: true
            text: unsupportedMessage.preference.label || ""
            type: Kirigami.MessageType.Information
        }
    }
}
