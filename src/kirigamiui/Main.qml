/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

import QtQuick 2.15
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

    function showMobilePage(source, objectName, properties) {
        root.visible = true

        for (var index = 1; index < root.pageStack.depth; ++index) {
            var page = root.pageStack.get(index)
            if (page.objectName === objectName) {
                root.pageStack.currentIndex = index
                return
            }
        }

        var pageProperties = properties || {}
        pageProperties.backend = root.backend
        root.pageStack.push(source, pageProperties)
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
                            "mobilePreferencesPage",
                            {"showPlugins": function() {
                                root.showPluginsPage()
                            }})
    }

    function showPluginsPage() {
        root.showMobilePage(Qt.resolvedUrl("PluginsPage.qml"),
                            "mobilePluginsPage",
                            {"showPlugin": function(basename) {
                                root.showPluginPreferencesPage(basename)
                            }})
    }

    function showPluginPreferencesPage(basename) {
        root.pageStack.push(Qt.resolvedUrl("PluginPreferencesPage.qml"),
                            {"backend": root.backend,
                             "basename": basename,
                             "dismiss": function() {
                                 root.pageStack.pop()
                             }})
    }

    function showAboutPage() {
        root.showMobilePage(Qt.resolvedUrl("AboutPage.qml"),
                            "mobileAboutPage")
    }

    function showPlayingPage() {
        root.showMobilePage(Qt.resolvedUrl("PlayingPage.qml"),
                            "mobilePlayingPage",
                            {"dismiss": function() {
                                root.pageStack.pop()
                            }})
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
            root.hideMobilePage("mobilePluginPreferencesPage")
            root.hideMobilePage("mobilePluginsPage")
            root.hideMobilePage("mobilePreferencesPage")
        }

        function onAboutRequested() {
            root.showAboutPage()
        }

        function onAboutDismissed() {
            root.hideMobilePage("mobileAboutPage")
        }
    }

    pageStack.initialPage: PlaylistPage {
        backend: root.backend
        showPlaying: function() {
            root.showPlayingPage()
        }
    }
}
