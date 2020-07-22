/*
 * Copyright (c) 2010, 2014, 2019 Ariadne Conill <ariadne@dereferenced.org>
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

#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

#define AUD_GLIB_INTEGRATION
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/preferences.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/vfs_async.h>
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>

struct LyricsState {
    String filename; /* of song file */
    String title, artist;
    String lyrics;

    enum Source {
        None,
        Local,
        LyricWiki,
        LyricsOVH
    } source = None;

    bool error = false;
};

static LyricsState g_state;

class TextEdit : public QTextEdit
{
public:
    TextEdit (QWidget * parent = nullptr) : QTextEdit (parent) {}

protected:
    void contextMenuEvent (QContextMenuEvent * event);
};

class LyricWikiQt : public GeneralPlugin
{
public:
    static const char * const defaults[];
    static const PluginPreferences prefs;
    static const PreferencesWidget widgets[];
    static constexpr PluginInfo info = {
        N_("Lyrics"),
        PACKAGE,
        nullptr, // about
        & prefs,
        PluginQtOnly
    };

    constexpr LyricWikiQt () : GeneralPlugin (info, false) {}

    bool init ();
    void * get_qt_widget ();
};

EXPORT LyricWikiQt aud_plugin_instance;

const char * const LyricWikiQt::defaults[] = {
    "remote-source", "lyricwiki",
    "enable-file-provider", "TRUE",
    "enable-cache", "TRUE",
    "split-title-on-chars", "FALSE",
    "split-on-chars", "-",
    "truncate-fields-on-chars", "FALSE",
    "truncate-on-chars", "|",
    nullptr
};

static const ComboItem remote_sources[] = {
    ComboItem(N_("Nowhere"), "nowhere"),
    ComboItem(N_("LyricWiki"), "lyricwiki"),
    ComboItem(N_("lyrics.ovh"), "lyrics.ovh")
};

static const PreferencesWidget truncate_elements[] = {
    WidgetLabel(N_("<small>Artist is truncated at the start, Title -- at the end</small>")),
    WidgetEntry(N_("Chars to truncate on:"), WidgetString ("lyricwiki", "truncate-on-chars"))
};

static const PreferencesWidget split_elements[] = {
    WidgetLabel(N_("<small>Chars are ORed in RegExp, surrounded by whitespace</small>")),
    WidgetEntry(N_("Chars to split on:"), WidgetString ("lyricwiki", "split-on-chars")),
    WidgetCheck(N_("Further truncate those on chars"),
        WidgetBool ("lyricwiki", "truncate-fields-on-chars")),
    WidgetTable({{truncate_elements}}, WIDGET_CHILD)
};

const PreferencesWidget LyricWikiQt::widgets[] = {
    WidgetLabel(N_("<b>General</b>")),
    WidgetCheck(N_("Split title into artist and title on chars"),
        WidgetBool ("lyricwiki", "split-title-on-chars")),
    WidgetTable({{split_elements}}, WIDGET_CHILD),
    WidgetLabel(N_("<b>Internet Sources</b>")),
    WidgetCombo(N_("Fetch lyrics from:"),
        WidgetString ("lyricwiki", "remote-source"),
        {{remote_sources}}),
    WidgetCheck(N_("Store fetched lyrics in local cache"),
        WidgetBool ("lyricwiki", "enable-cache")),
    WidgetLabel(N_("<b>Local Storage</b>")),
    WidgetCheck(N_("Load lyric files (.lrc) from local storage"),
        WidgetBool ("lyricwiki", "enable-file-provider"))
};

const PluginPreferences LyricWikiQt::prefs = {{widgets}};

bool LyricWikiQt::init ()
{
    aud_config_set_defaults ("lyricwiki", defaults);

    return true;
}

static void update_lyrics_window (const char * title, const char * artist, const char * lyrics);
static void update_lyrics_window_message (LyricsState state, const char * message);
static void update_lyrics_window_error (const char * message);
static void update_lyrics_window_notfound (LyricsState state);

// LyricProvider encapsulates an entire strategy for fetching lyrics,
// for example from LyricWiki or local storage.
class LyricProvider {
public:
    virtual bool match (LyricsState state) = 0;
    virtual void fetch (LyricsState state) = 0;
    virtual String edit_uri (LyricsState state) = 0;
};

// FileProvider provides a strategy for fetching and saving lyrics
// in local files.  It also manages the local lyrics cache.
class FileProvider : public LyricProvider {
public:
    FileProvider() {};

    bool match (LyricsState state);
    void fetch (LyricsState state);
    String edit_uri (LyricsState state) { return String (); }
    void save (LyricsState state);
    void cache (LyricsState state);
    void cache_fetch (LyricsState state);

private:
    String local_uri_for_entry (LyricsState state);
    String cache_uri_for_entry (LyricsState state);
};

static FileProvider file_provider;

static void persist_state (LyricsState state)
{
    g_state = state;
    g_state.error = false;

    if (g_state.source == LyricsState::Source::Local || ! aud_get_bool("lyricwiki", "enable-cache"))
        return;

    file_provider.cache (state);
}

void FileProvider::cache (LyricsState state)
{
    auto uri = cache_uri_for_entry (state);
    bool exists = VFSFile::test_file (uri, VFS_IS_REGULAR);
    if (exists)
        return;

    AUDINFO ("Add to cache: %s\n", (const char *) uri);
    VFSFile::write_file (uri, state.lyrics, strlen (state.lyrics));
}

#ifdef S_IRGRP
#define DIRMODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#else
#define DIRMODE (S_IRWXU)
#endif

String FileProvider::cache_uri_for_entry (LyricsState state)
{
    auto user_dir = aud_get_path (AudPath::UserDir);
    StringBuf base_path = filename_build ({user_dir, "lyrics"});
    StringBuf artist_path = filename_build ({base_path, state.artist});

    if (g_mkdir_with_parents (artist_path, DIRMODE) < 0)
        AUDERR ("Failed to create %s: %s\n", (const char *) artist_path, strerror (errno));

    StringBuf title_path = str_concat({filename_build({artist_path, state.title}), ".lrc"});

    return String (filename_to_uri (title_path));
}

String FileProvider::local_uri_for_entry (LyricsState state)
{
    if (strcmp (uri_get_scheme (state.filename), "file"))
        return String ();

    // it's a local file: convert our URI to a local path
    StringBuf filename = uri_to_filename (state.filename);

    // strip off the extension
    char * ext = strrchr((char *) filename, '.');
    if (! ext)
        return String ();
    *ext = '\0';

    // combine the mangled filename and '.lrc' extension
    return String (filename_to_uri (str_concat ({filename, ".lrc"})));
}

void FileProvider::fetch (LyricsState state)
{
    String path = local_uri_for_entry (state);
    if (! path)
        return;

    auto data = VFSFile::read_file (path, VFS_APPEND_NULL);
    if (! data.len())
        return;

    state.lyrics = String (data.begin ());
    state.source = LyricsState::Source::Local;

    update_lyrics_window (state.title, state.artist, state.lyrics);
    persist_state (state);
}

void FileProvider::cache_fetch (LyricsState state)
{
    String path = cache_uri_for_entry (state);
    if (! path)
        return;

    auto data = VFSFile::read_file (path, VFS_APPEND_NULL);
    if (! data.len())
        return;

    state.lyrics = String (data.begin ());
    state.source = LyricsState::Source::Local;

    update_lyrics_window (state.title, state.artist, state.lyrics);
    persist_state (state);
}

bool FileProvider::match (LyricsState state)
{
    String path = local_uri_for_entry (state);
    if (! path)
        return false;

    AUDINFO("Checking for local lyric file: '%s'\n", (const char *) path);

    bool exists = VFSFile::test_file(path, VFS_IS_REGULAR);
    if (exists)
    {
        fetch (state);
        return true;
    }

    path = cache_uri_for_entry (state);
    if (! path)
        return false;

    AUDINFO("Checking for cache lyric file: '%s'\n", (const char *) path);

    exists = VFSFile::test_file(path, VFS_IS_REGULAR);
    if (exists)
        cache_fetch (state);

    return exists;
}

void FileProvider::save (LyricsState state)
{
    if (! state.lyrics)
        return;

    String path = local_uri_for_entry (state);
    if (! path)
        return;

    AUDINFO("Saving lyrics to local file: '%s'\n", (const char *) path);

    VFSFile::write_file (path, state.lyrics, strlen (state.lyrics));
}

// LyricWikiProvider provides a strategy for fetching lyrics from
// LyricWiki.
class LyricWikiProvider : public LyricProvider {
public:
    LyricWikiProvider() {};

    bool match (LyricsState state);
    void fetch (LyricsState state);
    String edit_uri (LyricsState state);

private:
    LyricsState scrape_edit_page(LyricsState state, const char * buf, int64_t len);
    LyricsState scrape_match_api(const char * buf, int64_t len);
    String match_uri (LyricsState state);
};

static LyricWikiProvider lyricwiki_provider;

String LyricWikiProvider::edit_uri (LyricsState state)
{
    StringBuf title_buf, artist_buf;

    title_buf = str_copy (state.title);
    str_replace_char (title_buf, ' ', '_');
    title_buf = str_encode_percent (title_buf, -1);

    artist_buf = str_copy (state.artist);
    str_replace_char (artist_buf, ' ', '_');
    artist_buf = str_encode_percent (artist_buf, -1);

    return String (str_printf ("https://lyrics.fandom.com/index.php?action=edit&title=%s:%s",
     (const char *) artist_buf, (const char *) title_buf));
}

String LyricWikiProvider::match_uri (LyricsState state)
{
    StringBuf title_buf, artist_buf;

    title_buf = str_copy (state.title);
    title_buf = str_encode_percent (title_buf, -1);

    artist_buf = str_copy (state.artist);
    artist_buf = str_encode_percent (artist_buf, -1);

    return String (str_printf ("https://lyrics.fandom.com/api.php?action=lyrics&artist=%s&title=%s&fmt=xml",
     (const char *) artist_buf, (const char *) title_buf));
}

void LyricWikiProvider::fetch (LyricsState state)
{
    String uri = edit_uri (state);

    auto handle_edit_page = [=] (const char *, const Index<char> & buf) {
        if (! buf.len ())
        {
            update_lyrics_window_error (str_printf (_("Unable to fetch %s"), (const char *) uri));
            return;
        }

        LyricsState new_state = scrape_edit_page (state, buf.begin (), buf.len ());

        if (! new_state.lyrics)
        {
            update_lyrics_window_notfound (new_state);
            return;
        }

        // we have to inject our original artist and title back because otherwise
        // the lyrics will cache wrong.
        new_state.artist = g_state.artist;
        new_state.title = g_state.title;

        update_lyrics_window (new_state.title, new_state.artist, new_state.lyrics);
        persist_state (new_state);
    };

    vfs_async_file_get_contents (uri, handle_edit_page);
}

bool LyricWikiProvider::match (LyricsState state)
{
    String uri = match_uri (state);

    auto handle_match_api = [=] (const char *, const Index<char> & buf) {
        if (! buf.len ())
        {
            update_lyrics_window_error (str_printf (_("Unable to fetch %s"), (const char *) uri));
            return;
        }

        LyricsState new_state = scrape_match_api (buf.begin (), buf.len ());
        if (! new_state.artist || ! new_state.title)
        {
            update_lyrics_window_error (str_printf (_("Unable to fetch %s"), (const char *) uri));
            return;
        }

        fetch (new_state);
    };

    vfs_async_file_get_contents (uri, handle_match_api);
    update_lyrics_window_message (state, _("Looking for lyrics ..."));

    return true;
}

/*
 * Suppress libxml warnings, because lyricwiki does not generate anything near
 * valid HTML.
 */
static void libxml_error_handler (void * ctx, const char * msg, ...)
{
}

LyricsState LyricWikiProvider::scrape_edit_page (LyricsState state, const char * buf, int64_t len)
{
    xmlDocPtr doc;
    CharPtr ret;

    /*
     * temporarily set our error-handling functor to our suppression function,
     * but we have to set it back because other components of Audacious depend
     * on libxml and we don't want to step on their code paths.
     *
     * unfortunately, libxml is anti-social and provides us with no way to get
     * the previous error functor, so we just have to set it back to default after
     * parsing and hope for the best.
     */
    xmlSetGenericErrorFunc (nullptr, libxml_error_handler);
    doc = htmlReadMemory (buf, (int) len, nullptr, "utf-8", (HTML_PARSE_RECOVER | HTML_PARSE_NONET));
    xmlSetGenericErrorFunc (nullptr, nullptr);

    if (doc)
    {
        xmlXPathContextPtr xpath_ctx = nullptr;
        xmlXPathObjectPtr xpath_obj = nullptr;
        xmlNodePtr node = nullptr;

        xpath_ctx = xmlXPathNewContext (doc);
        if (! xpath_ctx)
            goto give_up;

        xpath_obj = xmlXPathEvalExpression ((xmlChar *) "//*[@id=\"wpTextbox1\"]", xpath_ctx);
        if (! xpath_obj)
            goto give_up;

        if (! xpath_obj->nodesetval->nodeMax)
            goto give_up;

        node = xpath_obj->nodesetval->nodeTab[0];
give_up:
        if (xpath_obj)
            xmlXPathFreeObject (xpath_obj);

        if (xpath_ctx)
            xmlXPathFreeContext (xpath_ctx);

        if (node)
        {
            xmlChar * lyric = xmlNodeGetContent (node);

            if (lyric)
            {
                GMatchInfo * match_info;
                GRegex * reg;

                reg = g_regex_new
                 ("<(lyrics?)>[[:space:]]*(.*?)[[:space:]]*</\\1>",
                 (GRegexCompileFlags) (G_REGEX_MULTILINE | G_REGEX_DOTALL),
                 (GRegexMatchFlags) 0, nullptr);
                g_regex_match (reg, (char *) lyric, G_REGEX_MATCH_NEWLINE_ANY, & match_info);

                ret.capture (g_match_info_fetch (match_info, 2));
                if (! strcmp_nocase (ret, "<!-- PUT LYRICS HERE (and delete this entire line) -->"))
                    ret.capture (g_strdup (_("No lyrics available")));

                g_match_info_free (match_info);
                g_regex_unref (reg);
            }

            xmlFree (lyric);
        }

        xmlFreeDoc (doc);
    }

    LyricsState new_state;

    new_state.filename = state.filename;
    new_state.artist = state.artist;
    new_state.title = state.title;
    new_state.lyrics = String (ret);
    new_state.source = LyricsState::Source::LyricWiki;

    return new_state;
}

LyricsState LyricWikiProvider::scrape_match_api (const char * buf, int64_t len)
{
    xmlDocPtr doc;
    LyricsState result;

    /*
     * workaround buggy lyricwiki search output where it cuts the lyrics
     * halfway through the UTF-8 symbol resulting in invalid XML.
     */
    GRegex * reg;

    reg = g_regex_new ("<(lyrics?)>.*</\\1>", (GRegexCompileFlags)
     (G_REGEX_MULTILINE | G_REGEX_DOTALL | G_REGEX_UNGREEDY),
     (GRegexMatchFlags) 0, nullptr);
    CharPtr newbuf (g_regex_replace_literal (reg, buf, len, 0, "", G_REGEX_MATCH_NEWLINE_ANY, nullptr));
    g_regex_unref (reg);

    /*
     * temporarily set our error-handling functor to our suppression function,
     * but we have to set it back because other components of Audacious depend
     * on libxml and we don't want to step on their code paths.
     *
     * unfortunately, libxml is anti-social and provides us with no way to get
     * the previous error functor, so we just have to set it back to default after
     * parsing and hope for the best.
     */
    xmlSetGenericErrorFunc (nullptr, libxml_error_handler);
    doc = xmlParseMemory (newbuf, strlen (newbuf));
    xmlSetGenericErrorFunc (nullptr, nullptr);

    if (doc != nullptr)
    {
        xmlNodePtr root, cur;

        root = xmlDocGetRootElement(doc);

        for (cur = root->xmlChildrenNode; cur; cur = cur->next)
        {
            xmlChar * content = xmlNodeGetContent (cur);

            if (xmlStrEqual(cur->name, (xmlChar *) "artist"))
                result.artist = String ((const char *) xmlNodeGetContent (cur));
            else if (xmlStrEqual(cur->name, (xmlChar *) "song"))
                result.title = String ((const char *) xmlNodeGetContent (cur));

            if (content)
                xmlFree (content);
        }

        xmlFreeDoc (doc);
    }

    // FIXME: don't use g_state
    result.filename = g_state.filename;

    return result;
}

// LyricsOVHProvider provides a strategy for fetching lyrics using the
// lyrics.ovh search engine.
class LyricsOVHProvider : public LyricProvider {
public:
    LyricsOVHProvider() {};

    bool match (LyricsState state);
    void fetch (LyricsState state);
    String edit_uri (LyricsState state) { return String (); }
};

bool LyricsOVHProvider::match (LyricsState state)
{
    fetch (state);
    return true;
}

void LyricsOVHProvider::fetch (LyricsState state)
{
    auto handle_result_cb = [=] (const char *filename, const Index<char> & buf) {
        if (! buf.len ())
        {
            update_lyrics_window_error(str_printf(_("Unable to fetch %s"), filename));
            return;
        }

        QByteArray json = QByteArray (buf.begin (), buf.len ());
        QJsonDocument doc = QJsonDocument::fromJson (json);

        if (doc.isNull () || ! doc.isObject ())
        {
            update_lyrics_window_error(str_printf(_("Unable to parse %s"), filename));
            return;
        }

        LyricsState new_state = g_state;
        new_state.lyrics = String ();

        auto obj = doc.object ();
        if (obj.contains ("lyrics"))
        {
            auto str = obj["lyrics"].toString();
            if (! str.isNull ())
            {
                auto raw_data = str.toLocal8Bit();
                new_state.lyrics = String (raw_data.data ());
            }
        }
        else
        {
            update_lyrics_window_notfound (new_state);
            return;
        }

        new_state.source = LyricsState::Source::LyricsOVH;

        update_lyrics_window (new_state.title, new_state.artist, new_state.lyrics);
        persist_state (new_state);
    };

    auto artist = str_copy (state.artist);
    artist = str_encode_percent (state.artist, -1);

    auto title = str_copy (state.title);
    title = str_encode_percent (state.title, -1);

    auto uri = str_concat({"https://api.lyrics.ovh/v1/", artist, "/", title});

    vfs_async_file_get_contents(uri, handle_result_cb);
    update_lyrics_window_message (state, _("Looking for lyrics ..."));
}

static LyricsOVHProvider lyrics_ovh_provider;

static LyricProvider * remote_source ()
{
    auto source = aud_get_str ("lyricwiki", "remote-source");

    if (! strcmp (source, "lyricwiki"))
        return &lyricwiki_provider;

    if (! strcmp (source, "lyrics.ovh"))
        return &lyrics_ovh_provider;

    return nullptr;
}

static QTextEdit * textedit;

static void update_lyrics_window_message (LyricsState state, const char * message)
{
    update_lyrics_window (state.title, state.artist, message);
}

static void update_lyrics_window_error (const char * message)
{
    update_lyrics_window (_("Error"), nullptr, message);
    g_state.error = true;
}

static void update_lyrics_window_notfound (LyricsState state)
{
    update_lyrics_window (state.title, state.artist, _("Lyrics could not be found."));
    g_state.error = true;
}

static void update_lyrics_window (const char * title, const char * artist, const char * lyrics)
{
    if (! textedit)
        return;

    textedit->document ()->clear ();

    QTextCursor cursor (textedit->document ());
    cursor.insertHtml (QString ("<big><b>") + QString (title) + QString ("</b></big>"));

    if (artist)
    {
        cursor.insertHtml (QString ("<br><i>") + QString (artist) + QString ("</i>"));
    }

    cursor.insertHtml ("<br><br>");
    cursor.insertText (lyrics);
}

static void lyricwiki_playback_began ()
{
    /* FIXME: cancel previous VFS requests (not possible with current API) */

    g_state.filename = aud_drct_get_filename ();

    Tuple tuple = aud_drct_get_tuple ();
    g_state.title = tuple.get_str (Tuple::Title);
    g_state.artist = tuple.get_str (Tuple::Artist);

    if (aud_get_bool ("lyricwiki", "split-title-on-chars"))
    {
        QString artist = QString (g_state.artist);
        QString title = QString (g_state.title);

        QRegularExpression qre;
        qre.setPattern (QString ("^(.*)\\s+[") + aud_get_str ("lyricwiki", "split-on-chars") + "]\\s+(.*)$");

        QRegularExpressionMatch qrematch = qre.match (title);

        if (qrematch.hasMatch ())
        {
            artist = qrematch.captured (1);
            title  = qrematch.captured (2);

            if (aud_get_bool ("lyricwiki", "truncate-fields-on-chars"))
            {
                qre.setPattern (QString ("^.*\\s+[") + aud_get_str ("lyricwiki", "truncate-on-chars") + "]\\s+");
                artist.remove (qre);

                qre.setPattern (QString ("\\s+[") + aud_get_str ("lyricwiki", "truncate-on-chars") + "]\\s+.*$");
                title.remove (qre);
            }

            g_state.artist = String ();
            g_state.title  = String ();
            g_state.artist = String (artist.toUtf8 ());
            g_state.title  = String (title.toUtf8 ());
        }
    }

    if (! aud_get_bool ("lyricwiki", "enable-file-provider") || ! file_provider.match (g_state))
    {
        if (! g_state.artist || ! g_state.title)
        {
            update_lyrics_window_error (_("Missing title and/or artist."));
            return;
        }

        auto rsrc = remote_source ();
        if (rsrc)
            rsrc->match (g_state);
    }
}

static void lw_cleanup (QObject * object = nullptr)
{
    g_state.filename = String ();
    g_state.title = String ();
    g_state.artist = String ();

    hook_dissociate ("tuple change", (HookFunction) lyricwiki_playback_began);
    hook_dissociate ("playback ready", (HookFunction) lyricwiki_playback_began);

    textedit = nullptr;
}

void * LyricWikiQt::get_qt_widget ()
{
    textedit = new TextEdit;
    textedit->setReadOnly (true);

#ifdef Q_OS_MAC  // Mac-specific font tweaks
    textedit->document ()->setDefaultFont (QApplication::font ("QTipLabel"));
#endif

    hook_associate ("tuple change", (HookFunction) lyricwiki_playback_began, nullptr);
    hook_associate ("playback ready", (HookFunction) lyricwiki_playback_began, nullptr);

    if (aud_drct_get_ready ())
        lyricwiki_playback_began ();

    QObject::connect (textedit, & QObject::destroyed, lw_cleanup);

    return textedit;
}

void TextEdit::contextMenuEvent (QContextMenuEvent * event)
{
    if (! g_state.artist || ! g_state.title)
        return QTextEdit::contextMenuEvent (event);

    QMenu * menu = createStandardContextMenu ();
    menu->addSeparator ();

    QAction * edit = menu->addAction (_("Edit Lyrics ..."));
    QObject::connect (edit, & QAction::triggered, [] () {
        QUrl url = QUrl ((const char *) lyricwiki_provider.edit_uri (g_state));
        QDesktopServices::openUrl (url);
    });

    if (g_state.lyrics && g_state.source != LyricsState::Source::Local && ! g_state.error)
    {
        QAction * save = menu->addAction (_("Save Locally"));
        QObject::connect (save, & QAction::triggered, [] () {
            file_provider.save (g_state);
        });
    }

    if (g_state.source == LyricsState::Source::Local || g_state.error)
    {
        QAction * refresh = menu->addAction (_("Refresh"));
        QObject::connect (refresh, & QAction::triggered, [] () {
            auto rsrc = remote_source ();
            if (rsrc)
                rsrc->match (g_state);
        });
    }

    menu->exec (event->globalPos ());
    menu->deleteLater ();
}
