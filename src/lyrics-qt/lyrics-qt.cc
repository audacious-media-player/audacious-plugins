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
#include <QTimer>

#include <libaudcore/hook.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "../lyrics-common/lyrics.h"
#include "../lyrics-common/preferences.h"

class TextEdit : public QTextEdit
{
public:
    TextEdit (QWidget * parent = nullptr) : QTextEdit (parent) {}

protected:
    void contextMenuEvent (QContextMenuEvent * event) override;
};

struct TimedLyricLine
{
    int timestamp_ms;
    String text;
};

static std::vector<TimedLyricLine> timed_lyrics;

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

    bool init () override;
    void * get_qt_widget () override;
};

EXPORT LyricsQt aud_plugin_instance;

const PluginPreferences LyricsQt::prefs = {{widgets}};

FileProvider file_provider;
LrcLibProvider lrclib_provider;
LyricsOVHProvider lyrics_ovh_provider;

LyricsState g_state;

static QTextEdit * textedit;

bool LyricsQt::init ()
{
    aud_config_set_defaults (CFG_SECTION, defaults);
    return true;
}

static void highlight_lyrics (int current_time_ms)
{
    if (!textedit)
        return;

    if (!aud_get_bool (CFG_SECTION, "sync_lyrics"))
        return;

    textedit->document ()->clear ();

    std::vector<TimedLyricLine> lines_to_display;

    for (size_t i = 0; i < timed_lyrics.size (); ++i)
    {
        if (timed_lyrics[i].timestamp_ms >= current_time_ms)
        {
            size_t start_index = (i > 1) ? i - 2 : 0;
            size_t end_index = std::min (i + 2, timed_lyrics.size () - 1);

            size_t line_count = 0;

            for (size_t j = start_index; j <= end_index && line_count < 4; ++j)
            {
                lines_to_display.push_back (timed_lyrics[j]);
                line_count++;
            }
            break;
        }
    }

    QTextCursor cursor (textedit->document ());

    for (size_t i = 0; i < lines_to_display.size(); ++i)
    {
        const TimedLyricLine & line = lines_to_display[i];
        QTextCharFormat format;

        if (i == 1)
        {
            format.setFontPointSize(16);
            format.setForeground (Qt::white);
        }
        else
        {
            format.setForeground (Qt::white);
        }

        cursor.setCharFormat (format);
        cursor.insertText(QString ((const char *) line.text));
        cursor.insertHtml("<br>");
    }
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
        cursor.insertHtml(QString("<br><i>") + QString (artist) +
                          QString("</i>"));

    timed_lyrics.clear ();
    std::istringstream iss (lyrics);
    std::string line;

    // Default offset is 0
    int global_offset = 0;

    // Matches standard time tags: [mm:ss.xx]
    std::regex time_re(R"(\[\s*(\d+)\s*:\s*(\d+(?:\.\d+)?)\s*\])");
    // Matches the offset ID tag: [offset:+1000] or [offset:-500]
    std::regex offset_re(R"(\[\s*offset\s*:\s*([+-]?\d+)\s*\])",
                         std::regex_constants::icase);

    while (std::getline (iss, line))
    {
        // Sanitize the line: remove leading/trailing spaces and carriage return
        line.erase (0, line.find_first_not_of(" \t\r"));
        line.erase (line.find_last_not_of(" \t\r") + 1);

        if (line.empty ())
            continue;

        // 1. Check for the global offset tag
        std::smatch offset_match;
        if (std::regex_search (line, offset_match, offset_re))
        {
            global_offset = std::stoi (offset_match[1].str ());
            continue;
        }

        // 2. Extract MULTIPLE time tags from a single line
        std::sregex_iterator it (line.begin (), line.end (), time_re);
        std::sregex_iterator end;

        std::vector<int> timestamps;
        size_t last_pos = 0;

        for (; it != end; ++it)
        {
            int minutes = std::stoi(it->str (1));
            float seconds = std::stof(it->str (2));
            int timestamp_ms =
                static_cast<int> ((minutes * 60 + seconds) * 1000);

            timestamps.push_back (timestamp_ms);
            last_pos =
                it->position () + it->length (); // Track where the tag ends
        }

        // 3. If timestamps were found, attach the remaining text to all of them
        if (!timestamps.empty ())
        {
            // The text is whatever exists after the very last time tag
            std::string text = line.substr (last_pos);
            text.erase(0,
                       text.find_first_not_of (
                           " \t")); // Trim leading spaces from the lyric text

            for (int ts : timestamps)
            {
                TimedLyricLine timed_line;
                timed_line.timestamp_ms = ts;
                timed_line.text = String (text.c_str ());

                timed_lyrics.push_back (timed_line);
            }
        }
    }

    // 4. Apply offsets and sort chronologically
    if (!timed_lyrics.empty ())
    {
        for (auto & t_line : timed_lyrics)
        {
            // A "+" offset means lyrics appear sooner, so we SUBTRACT it from
            // the timestamp A "-" offset means lyrics appear later, subtracting
            // a negative ADDS it.
            t_line.timestamp_ms -= global_offset;
        }

        // Because multi-timestamp tags (e.g. repeated chorus lines) push
        // timestamps out of order, we must sort the vector so your
        // 'highlight_lyrics' binary/linear search functions correctly.
        std::sort (timed_lyrics.begin (), timed_lyrics.end (),
                  [](const TimedLyricLine & a, const TimedLyricLine & b) {
                      return a.timestamp_ms < b.timestamp_ms;
                  });
    }
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
            QByteArray raw_data = str.toUtf8 ();
            output = String (raw_data.constData ());
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
    timed_lyrics.clear ();
}

static void update_lyrics_display ()
{
    int current_time_ms = aud_drct_get_time();
    highlight_lyrics(current_time_ms);
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


    QTimer * timer = new QTimer(textedit);
    timer->setInterval(100);
    QObject::connect(timer, &QTimer::timeout, update_lyrics_display);
    timer->start();

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
