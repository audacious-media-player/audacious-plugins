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
    required property int entry
    required property var dismiss
    required property var restorePreviousView

    property var details: ({})
    property var fieldValues: ({})
    property var originalValues: ({})
    property int valueRevision: 0
    property bool saveFailed: false
    property bool sessionClosed: false

    readonly property bool hasError:
        (root.details.error || "").length > 0
    readonly property bool dirty: {
        root.valueRevision
        var keys = Object.keys(root.originalValues)
        for (var index = 0; index < keys.length; ++index) {
            var key = keys[index]
            if (String(root.fieldValues[key])
                    !== String(root.originalValues[key]))
                return true
        }
        return false
    }

    objectName: "mobileSongInfoPage"
    title: details.title || qsTr("Song Info")

    function copyValues(values) {
        var copy = {}
        var keys = Object.keys(values)
        for (var index = 0; index < keys.length; ++index)
            copy[keys[index]] = values[keys[index]]
        return copy
    }

    function normalizedValue(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function initializeValues() {
        var values = {}
        var fields = root.details.fields || []
        for (var index = 0; index < fields.length; ++index) {
            if (fields[index].type === "field")
                values[String(fields[index].id)] =
                    root.normalizedValue(fields[index].value)
        }
        root.originalValues = root.copyValues(values)
        root.fieldValues = values
        root.valueRevision++
    }

    function valueFor(id) {
        root.valueRevision
        return root.normalizedValue(root.fieldValues[String(id)])
    }

    function setFieldValue(id, value) {
        if (root.valueFor(id) === root.normalizedValue(value))
            return

        var values = root.copyValues(root.fieldValues)
        values[String(id)] = root.normalizedValue(value)
        root.fieldValues = values
        root.valueRevision++
        root.saveFailed = false
    }

    function revertValues() {
        root.fieldValues = root.copyValues(root.originalValues)
        root.valueRevision++
        root.saveFailed = false
    }

    function closeSession() {
        if (root.sessionClosed || root.details.session === undefined)
            return
        root.sessionClosed = true
        root.backend.closeSongInfo(root.details.session)
    }

    Component.onCompleted: {
        root.details = root.backend.openSongInfo(root.entry)
        root.initializeValues()
    }

    Component.onDestruction: {
        root.closeSession()
        root.restorePreviousView()
    }

    onBackRequested: function(event) {
        event.accepted = true
        root.dismiss()
    }

    footer: Controls.ToolBar {
        visible: root.details.canWrite === true && !root.hasError
        leftPadding: Kirigami.Units.largeSpacing
        rightPadding: Kirigami.Units.largeSpacing
        topPadding: Kirigami.Units.largeSpacing * 2
        bottomPadding: Kirigami.Units.largeSpacing * 2
        implicitHeight: visible ? footerLayout.implicitHeight
                                  + topPadding + bottomPadding : 0

        contentItem: RowLayout {
            id: footerLayout

            Controls.Button {
                text: qsTr("Revert")
                enabled: root.dirty
                onClicked: root.revertValues()
            }

            Item { Layout.fillWidth: true }

            Controls.Button {
                text: qsTr("Save")
                icon.name: "document-save"
                highlighted: true
                enabled: root.dirty
                onClicked: {
                    if (root.backend.saveSongInfo(root.details.session,
                                                  root.fieldValues)) {
                        root.closeSession()
                        root.dismiss()
                    } else {
                        root.saveFailed = true
                    }
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
            visible: root.hasError
            text: root.details.error || ""
            type: Kirigami.MessageType.Error
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.saveFailed
            text: qsTr("Unable to write tags to this file.")
            type: Kirigami.MessageType.Error
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: Object.keys(root.details).length > 0
                     && root.details.canWrite === false
                     && !root.hasError
            text: qsTr("Tags cannot be edited for this file.")
            type: Kirigami.MessageType.Information
        }

        Repeater {
            id: fieldRepeater

            readonly property var page: contents.page

            model: root.details.fields || []

            delegate: ColumnLayout {
                id: fieldDelegate

                required property var modelData
                readonly property var page: parent.page

                Layout.fillWidth: true
                Layout.topMargin: modelData.type === "section"
                                  ? Kirigami.Units.smallSpacing : 0
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Heading {
                    Layout.fillWidth: true
                    visible: fieldDelegate.modelData.type === "section"
                    text: fieldDelegate.modelData.label || ""
                    level: 2
                }

                Controls.Label {
                    Layout.fillWidth: true
                    visible: fieldDelegate.modelData.type === "field"
                    text: fieldDelegate.modelData.label || ""
                    wrapMode: Text.Wrap
                    font.bold: fieldDelegate.modelData.editable === true
                }

                Controls.TextField {
                    Layout.fillWidth: true
                    visible: fieldDelegate.modelData.type === "field"
                             && fieldDelegate.modelData.editable === true
                             && fieldDelegate.modelData.multiline !== true
                    text: fieldDelegate.page.valueFor(
                              fieldDelegate.modelData.id)
                    inputMethodHints: fieldDelegate.modelData.numeric === true
                                      ? Qt.ImhDigitsOnly : Qt.ImhNone
                    onTextEdited: fieldDelegate.page.setFieldValue(
                                      fieldDelegate.modelData.id, text)
                }

                Controls.ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 6
                    visible: fieldDelegate.modelData.type === "field"
                             && fieldDelegate.modelData.editable === true
                             && fieldDelegate.modelData.multiline === true

                    Controls.TextArea {
                        text: fieldDelegate.page.valueFor(
                                  fieldDelegate.modelData.id)
                        wrapMode: TextEdit.Wrap
                        onTextEdited: fieldDelegate.page.setFieldValue(
                                          fieldDelegate.modelData.id, text)
                    }
                }

                Controls.Label {
                    Layout.fillWidth: true
                    visible: fieldDelegate.modelData.type === "field"
                             && fieldDelegate.modelData.editable !== true
                    text: fieldDelegate.page.valueFor(
                              fieldDelegate.modelData.id).length > 0
                          ? fieldDelegate.page.valueFor(
                                fieldDelegate.modelData.id) : "—"
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                    color: fieldDelegate.page.valueFor(
                               fieldDelegate.modelData.id).length > 0
                           ? Kirigami.Theme.textColor
                           : Kirigami.Theme.disabledTextColor
                }

                Kirigami.Separator {
                    Layout.fillWidth: true
                    visible: fieldDelegate.modelData.type === "field"
                }
            }
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: Kirigami.Units.largeSpacing
        }
    }
}
