/*
 * Copyright (c) 2010-2019 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <QApplication>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QTextEdit>

#include <libaudcore/hook.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>

#include "../lyrics-common/lyrics.h"
#include "../lyrics-common/preferences.h"

class TextEdit : public QTextEdit
{
public:
    TextEdit (QWidget * parent = nullptr) : QTextEdit (parent) {}

protected:
    void contextMenuEvent (QContextMenuEvent * event);
};

class LyricsQt : public GeneralPlugin
{
public:
    static const PluginPreferences prefs;
    static constexpr PluginInfo info = {
        N_("Lyrics"),
        PACKAGE,
        nullptr, // about
        & prefs,
        PluginQtOnly
    };

    constexpr LyricsQt () : GeneralPlugin (info, false) {}

    bool init ();
    void * get_qt_widget ();
};

EXPORT LyricsQt aud_plugin_instance;

const PluginPreferences LyricsQt::prefs = {{widgets}};

FileProvider file_provider;
ChartLyricsProvider chart_lyrics_provider;
LrcLibProvider lrclib_provider;
LyricsOVHProvider lyrics_ovh_provider;

LyricsState g_state;

static QTextEdit * textedit;

bool LyricsQt::init ()
{
    aud_config_set_defaults (CFG_SECTION, defaults);
    return true;
}

void update_lyrics_window (const char * title, const char * artist, const char * lyrics)
{
    if (! textedit)
        return;

    textedit->document ()->clear ();

    QTextCursor cursor (textedit->document ());
    cursor.insertHtml (QString ("<big><b>") + QString (title) + QString ("</b></big>"));

    if (artist)
        cursor.insertHtml (QString ("<br><i>") + QString (artist) + QString ("</i>"));

    cursor.insertHtml ("<br><br>");
    cursor.insertText (lyrics);
}

bool try_parse_json (const Index<char> & buf, const char * key, String & output)
{
    QByteArray json = QByteArray (buf.begin (), buf.len ());
    QJsonDocument doc = QJsonDocument::fromJson (json);

    if (doc.isNull () || ! doc.isObject ())
        return false;

    QJsonObject obj = doc.object ();
    if (obj.contains (key))
    {
        QString str = obj[key].toString ();
        if (! str.isNull ())
        {
            QByteArray raw_data = str.toLocal8Bit ();
            output = String (raw_data.data ());
        }
    }

    return true;
}

static void lyrics_cleanup (QObject * object = nullptr)
{
    g_state.filename = String ();
    g_state.title = String ();
    g_state.artist = String ();
    g_state.lyrics = String ();

    hook_dissociate ("tuple change", (HookFunction) lyrics_playback_began);
    hook_dissociate ("playback ready", (HookFunction) lyrics_playback_began);

    textedit = nullptr;
}

void * LyricsQt::get_qt_widget ()
{
    textedit = new TextEdit;
    textedit->setReadOnly (true);

#ifdef Q_OS_MAC  // Mac-specific font tweaks
    textedit->document ()->setDefaultFont (QApplication::font ("QTipLabel"));
#endif

    hook_associate ("tuple change", (HookFunction) lyrics_playback_began, nullptr);
    hook_associate ("playback ready", (HookFunction) lyrics_playback_began, nullptr);

    if (aud_drct_get_ready ())
        lyrics_playback_began ();

    QObject::connect (textedit, & QObject::destroyed, lyrics_cleanup);

    return textedit;
}

void TextEdit::contextMenuEvent (QContextMenuEvent * event)
{
    if (! g_state.artist || ! g_state.title)
        return QTextEdit::contextMenuEvent (event);

    LyricProvider * remote_provider = remote_source ();

    QMenu * menu = createStandardContextMenu ();
    menu->addSeparator ();

    if (g_state.lyrics && g_state.source != LyricsState::Source::Local && ! g_state.error)
    {
        if (remote_provider)
        {
            String edit_uri = remote_provider->edit_uri (g_state);

            if (edit_uri && edit_uri[0])
            {
                QAction * edit = menu->addAction (_("Edit Lyrics ..."));
                QObject::connect (edit, & QAction::triggered, [edit_uri] () {
                    QDesktopServices::openUrl (QUrl ((const char *) edit_uri));
                });
            }
        }

        QAction * save = menu->addAction (_("Save Locally"));
        QObject::connect (save, & QAction::triggered, [] () {
            file_provider.save (g_state);
        });
    }

    if (g_state.source == LyricsState::Source::Local || g_state.error)
    {
        QAction * refresh = menu->addAction (_("Refresh"));
        QObject::connect (refresh, & QAction::triggered, [remote_provider] () {
            if (remote_provider)
                remote_provider->match (g_state);
        });
    }

    menu->exec (event->globalPos ());
    menu->deleteLater ();
}
