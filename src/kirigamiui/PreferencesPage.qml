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

    objectName: "mobilePreferencesPage"
    title: qsTr("Preferences")

    Kirigami.FormLayout {
        wideMode: false

        Kirigami.InlineMessage {
            Kirigami.FormData.isSection: true
            Layout.fillWidth: true
            visible: true
            text: qsTr("Changes are saved automatically.")
            type: Kirigami.MessageType.Information
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: qsTr("Playback")
        }

        Controls.CheckBox {
            id: resumePlayback
            text: qsTr("Resume playback on startup")
            checked: root.backend.boolSetting("resume_playback_on_startup")
            onToggled: root.backend.setBoolSetting("resume_playback_on_startup",
                                                   checked)
        }

        Controls.CheckBox {
            text: qsTr("Pause instead of resuming immediately")
            enabled: resumePlayback.checked
            checked: root.backend.boolSetting("always_resume_paused")
            onToggled: root.backend.setBoolSetting("always_resume_paused", checked)
        }

        Controls.CheckBox {
            text: qsTr("Advance when the current song is deleted")
            checked: root.backend.boolSetting("advance_on_delete")
            onToggled: root.backend.setBoolSetting("advance_on_delete", checked)
        }

        Controls.CheckBox {
            text: qsTr("Add folders recursively")
            checked: root.backend.boolSetting("recurse_folders")
            onToggled: root.backend.setBoolSetting("recurse_folders", checked)
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: qsTr("Playlist")
        }

        Controls.CheckBox {
            text: qsTr("Clear the playlist when opening files")
            checked: root.backend.boolSetting("clear_playlist")
            onToggled: root.backend.setBoolSetting("clear_playlist", checked)
        }

        Controls.CheckBox {
            text: qsTr("Open files in a temporary playlist")
            checked: root.backend.boolSetting("open_to_temporary")
            onToggled: root.backend.setBoolSetting("open_to_temporary", checked)
        }

        Controls.CheckBox {
            text: qsTr("Skip confirmation when removing a playlist")
            checked: root.backend.boolSetting("no_confirm_playlist_delete")
            onToggled: root.backend.setBoolSetting("no_confirm_playlist_delete",
                                                   checked)
        }

        Controls.CheckBox {
            text: qsTr("Guess missing metadata from the file path")
            checked: root.backend.boolSetting("metadata_fallbacks")
            onToggled: root.backend.setBoolSetting("metadata_fallbacks", checked)
        }

        Controls.CheckBox {
            text: qsTr("Load song metadata only when played")
            checked: root.backend.boolSetting("metadata_on_play")
            onToggled: root.backend.setBoolSetting("metadata_on_play", checked)
        }

        Controls.CheckBox {
            text: qsTr("Show hours separately in track times")
            checked: root.backend.boolSetting("show_hours")
            onToggled: root.backend.setBoolSetting("show_hours", checked)
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: qsTr("Audio")
        }

        Controls.CheckBox {
            id: replayGain
            text: qsTr("Enable ReplayGain")
            checked: root.backend.boolSetting("enable_replay_gain")
            onToggled: root.backend.setBoolSetting("enable_replay_gain", checked)
        }

        Controls.ComboBox {
            Layout.fillWidth: true
            Kirigami.FormData.label: qsTr("ReplayGain mode:")
            enabled: replayGain.checked
            model: [qsTr("Track"), qsTr("Album"), qsTr("Based on shuffle")]
            currentIndex: root.backend.intSetting("replay_gain_mode")
            onActivated: function(index) {
                root.backend.setIntSetting("replay_gain_mode", index)
            }
        }

        Controls.CheckBox {
            text: qsTr("Prevent clipping")
            enabled: replayGain.checked
            checked: root.backend.boolSetting("enable_clipping_prevention")
            onToggled: root.backend.setBoolSetting("enable_clipping_prevention",
                                                   checked)
        }

        Controls.CheckBox {
            text: qsTr("Use software volume control")
            checked: root.backend.boolSetting("software_volume_control")
            onToggled: root.backend.setBoolSetting("software_volume_control",
                                                   checked)
        }

        Controls.CheckBox {
            text: qsTr("Soft clipping")
            checked: root.backend.boolSetting("soft_clipping")
            onToggled: root.backend.setBoolSetting("soft_clipping", checked)
        }

        Controls.SpinBox {
            Layout.fillWidth: true
            Kirigami.FormData.label: qsTr("Output buffer (ms):")
            from: 100
            to: 10000
            stepSize: 100
            editable: true
            value: root.backend.intSetting("output_buffer_size")
            onValueModified: root.backend.setIntSetting("output_buffer_size",
                                                        value)
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: qsTr("Network")
        }

        Controls.SpinBox {
            Layout.fillWidth: true
            Kirigami.FormData.label: qsTr("Network buffer (KiB):")
            from: 16
            to: 1024
            stepSize: 16
            editable: true
            value: root.backend.intSetting("net_buffer_kb")
            onValueModified: root.backend.setIntSetting("net_buffer_kb", value)
        }

        Item {
            Kirigami.FormData.isSection: true
            implicitHeight: Kirigami.Units.largeSpacing
        }
    }
}
