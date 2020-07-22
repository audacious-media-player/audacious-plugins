// Copyright (c) 2019 Ariadne Conill <ariadne@dereferenced.org>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// This software is provided 'as is' and without any warranty, express or
// implied.  In no event shall the authors be liable for any damages arising
// from the use of this software.

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/preferences.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/runtime.h>
#include <libaudcore/index.h>
#include <libaudcore/playlist.h>

#include <libaudqt/treeview.h>

#include <QWidget>
#include <QTabWidget>
#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "shoutcast-model.h"
#include "shoutcast-widget.h"
#include "icecast-widget.h"
#include "ihr-widget.h"

class StreamTunerWidget : public QTabWidget {
public:
     StreamTunerWidget(QWidget * parent = nullptr);

private:
     ShoutcastTunerWidget *m_shoutcast_tuner;
     IcecastListingWidget *m_icecast_tuner;
     IHRTunerWidget *m_ihr_tuner;
};

StreamTunerWidget::StreamTunerWidget(QWidget *parent) :
    QTabWidget(parent)
{
    setDocumentMode (true);
    setTabPosition (QTabWidget::TabPosition::South);

    m_shoutcast_tuner = new ShoutcastTunerWidget (this);
    m_icecast_tuner = new IcecastListingWidget (this);
    m_ihr_tuner = new IHRTunerWidget (this);

    addTab (m_shoutcast_tuner, _("Shoutcast"));
    addTab (m_icecast_tuner, _("Icecast"));
    addTab (m_ihr_tuner, "iHeartRadio");

    tabBar ()->setAutoHide (false);
}

// plugin glue

class StreamTunerPlugin : public GeneralPlugin {
public:
    static constexpr PluginInfo info = {
        N_("Stream Tuner"),
        PACKAGE,
        nullptr,
        nullptr,
        PluginQtOnly
    };

    constexpr StreamTunerPlugin () : GeneralPlugin (info, false) { }

    void * get_qt_widget ();
};

EXPORT StreamTunerPlugin aud_plugin_instance;

void * StreamTunerPlugin::get_qt_widget ()
{
    return new StreamTunerWidget ();
}
