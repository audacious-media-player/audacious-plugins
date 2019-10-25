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

#include <glib.h>
#include <string.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QMenu>
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
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/vfs_async.h>
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>

typedef struct {
    String filename; /* of song file */
    String title, artist;
    String lyrics;

    enum Source {
        None,
        LyricWiki
    } source;
} LyricsState;

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
    static constexpr PluginInfo info = {
        N_("LyricWiki Plugin"),
        PACKAGE,
        nullptr, // about
        nullptr, // prefs
        PluginQtOnly
    };

    constexpr LyricWikiQt () : GeneralPlugin (info, false) {}
    void * get_qt_widget ();
};

EXPORT LyricWikiQt aud_plugin_instance;

static void update_lyrics_window (const char * title, const char * artist, const char * lyrics);

// LyricProvider encapsulates an entire strategy for fetching lyrics,
// for example from LyricWiki or local storage.
class LyricProvider {
public:
    virtual bool match (LyricsState state) = 0;
    virtual void fetch (LyricsState state) = 0;
    virtual String edit_uri (LyricsState state) = 0;
};

// FileProvider provides a strategy for fetching and saving lyrics
// in local files.
class FileProvider : public LyricProvider {
public:
    FileProvider() {};

    bool match (LyricsState state);
    void fetch (LyricsState state);
    String edit_uri (LyricsState state) { return String (); }

private:
    String local_uri_for_entry (LyricsState state);
};

static FileProvider file_provider;

String FileProvider::local_uri_for_entry (LyricsState state)
{
    // FIXME: implement support for local lyrics database for
    // streamed files
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
    update_lyrics_window (state.artist, state.title, state.lyrics);
    g_state = state;
}

bool FileProvider::match (LyricsState state)
{
    String path = local_uri_for_entry (state);
    if (! path)
        return false;

    AUDINFO("Checking for local lyric file: '%s'\n", (const char *) path);

    bool exists = VFSFile::test_file(path, VFS_IS_REGULAR);
    if (exists)
        fetch (state);

    return exists;
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

    VFSConsumer handle_edit_page = [=] (const char *, const Index<char> & buf, void *) {
        if (! buf.len ())
        {
            update_lyrics_window (_("Error"), nullptr,
             str_printf (_("Unable to fetch %s"), (const char *) uri));
            return;
        }

        LyricsState new_state = scrape_edit_page (state, buf.begin (), buf.len ());

        if (! new_state.lyrics)
        {
            update_lyrics_window (_("No Lyrics Found"), nullptr,
             str_printf (_("Artist: %s\nTitle: %s"), (const char *) state.artist, (const char *) state.title));
            return;
        }

        update_lyrics_window (new_state.title, new_state.artist, new_state.lyrics);
        g_state = new_state;
    };

    vfs_async_file_get_contents (uri, handle_edit_page, nullptr);
}

bool LyricWikiProvider::match (LyricsState state)
{
    String uri = match_uri (state);

    VFSConsumer handle_match_api = [=] (const char *, const Index<char> & buf, void *) {
        if (! buf.len ())
        {
            update_lyrics_window (_("Error"), nullptr,
             str_printf (_("Unable to fetch %s"), (const char *) uri));
            return;
        }

        LyricsState new_state = scrape_match_api (buf.begin (), buf.len ());
        if (! new_state.artist || ! new_state.title)
        {
            update_lyrics_window (_("Error"), nullptr,
             str_printf (_("Unable to fetch %s"), (const char *) uri));
            return;
        }

        fetch (new_state);
    };

    vfs_async_file_get_contents (uri, handle_match_api, nullptr);
    update_lyrics_window (state.title, state.artist, _("Looking for lyrics ..."));

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

    return result;
}

static QTextEdit * textedit;

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

    if (! g_state.artist || ! g_state.title)
    {
        update_lyrics_window (_("Error"), nullptr, _("Missing title and/or artist."));
        return;
    }

    if (! file_provider.match (g_state))
        lyricwiki_provider.match (g_state);
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
    QAction * edit = menu->addAction (_("Edit lyrics ..."));
    QObject::connect (edit, & QAction::triggered, [] () {
        QUrl url = QUrl ((const char *) lyricwiki_provider.edit_uri (g_state));
        QDesktopServices::openUrl (url);
    });
    menu->exec (event->globalPos ());
    menu->deleteLater ();
}
