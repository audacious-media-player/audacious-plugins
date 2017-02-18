/*
 * Copyright (c) 2010, 2014 William Pitcock <nenolod@dereferenced.org>
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
    String uri; /* URI we are trying to retrieve */
} LyricsState;

static LyricsState state;

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

/*
 * Suppress libxml warnings, because lyricwiki does not generate anything near
 * valid HTML.
 */
static void libxml_error_handler (void * ctx, const char * msg, ...)
{
}

static CharPtr scrape_lyrics_from_lyricwiki_edit_page (const char * buf, int64_t len)
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

                g_regex_unref (reg);
            }

            xmlFree (lyric);
        }

        xmlFreeDoc (doc);
    }

    return ret;
}

static String scrape_uri_from_lyricwiki_search_result (const char * buf, int64_t len)
{
    xmlDocPtr doc;
    String uri;

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
            if (xmlStrEqual(cur->name, (xmlChar *) "url"))
            {
                auto lyric = (char *) xmlNodeGetContent (cur);

                // If the lyrics are unknown, LyricWiki returns a broken link
                // to the edit page.  Extract the song ID (artist:title) from
                // the URI and recreate a working link.
                char * title = strstr (lyric, "title=");
                if (title)
                {
                    title += 6;

                    // Strip trailing "&action=edit"
                    char * amp = strchr (title, '&');
                    if (amp)
                        * amp = 0;

                    // Spaces get replaced with plus signs for some reason.
                    str_replace_char (title, '+', ' ');

                    // LyricWiki interprets UTF-8 as ISO-8859-1, then "converts"
                    // it to UTF-8 again.  Worse, it will sometimes corrupt only
                    // the song title in this way while leaving the artist name
                    // intact.  So we have to percent-decode the URI, split the
                    // two strings apart, repair them separately, and then
                    // rebuild the URI.
                    auto strings = str_list_to_index (str_decode_percent (title), ":");
                    for (String & s : strings)
                    {
                        StringBuf orig_utf8 = str_convert (s, -1, "UTF-8", "ISO-8859-1");
                        if (orig_utf8 && g_utf8_validate (orig_utf8, -1, nullptr))
                            s = String (orig_utf8);
                    }

                    uri = String (str_printf ("http://lyrics.wikia.com/index.php?"
                     "action=edit&title=%s", (const char *) str_encode_percent
                     (index_to_str_list (strings, ":"))));
                }
                else
                {
                    // Convert normal lyrics link to edit page link
                    char * slash = strrchr (lyric, '/');
                    if (slash && ! strstr (slash, "lyrics.wikia.com"))
                        uri = String (str_printf ("http://lyrics.wikia.com/index.php?"
                         "action=edit&title=%s", slash + 1));
                    else
                        uri = String ("N/A");
                }

                xmlFree ((xmlChar *) lyric);
            }
        }

        xmlFreeDoc (doc);
    }

    return uri;
}

static void update_lyrics_window (const char * title, const char * artist, const char * lyrics);

static void get_lyrics_step_3 (const char * uri, const Index<char> & buf, void *)
{
    if (! state.uri || strcmp (state.uri, uri))
        return;

    if (! buf.len ())
    {
        update_lyrics_window (_("Error"), nullptr,
         str_printf (_("Unable to fetch %s"), uri));
        return;
    }

    CharPtr lyrics = scrape_lyrics_from_lyricwiki_edit_page (buf.begin (), buf.len ());

    if (! lyrics)
    {
        update_lyrics_window (_("Error"), nullptr,
         str_printf (_("Unable to parse %s"), uri));
        return;
    }

    update_lyrics_window (state.title, state.artist, lyrics);
}

static void get_lyrics_step_2 (const char * uri1, const Index<char> & buf, void *)
{
    if (! state.uri || strcmp (state.uri, uri1))
        return;

    if (! buf.len ())
    {
        update_lyrics_window (_("Error"), nullptr,
         str_printf (_("Unable to fetch %s"), uri1));
        return;
    }

    String uri = scrape_uri_from_lyricwiki_search_result (buf.begin (), buf.len ());

    if (! uri)
    {
        update_lyrics_window (_("Error"), nullptr,
         str_printf (_("Unable to parse %s"), uri1));
        return;
    }
    else if (uri == String ("N/A"))
    {
        update_lyrics_window (state.title, state.artist,
         _("No lyrics available"));
        return;
    }

    state.uri = uri;

    update_lyrics_window (state.title, state.artist, _("Looking for lyrics ..."));
    vfs_async_file_get_contents (uri, get_lyrics_step_3, nullptr);
}

static void get_lyrics_step_1 ()
{
    if (! state.artist || ! state.title)
    {
        update_lyrics_window (_("Error"), nullptr, _("Missing song metadata"));
        return;
    }

    StringBuf title_buf = str_encode_percent (state.title);
    StringBuf artist_buf = str_encode_percent (state.artist);

    state.uri = String (str_printf ("http://lyrics.wikia.com/api.php?"
     "action=lyrics&artist=%s&song=%s&fmt=xml", (const char *) artist_buf,
     (const char *) title_buf));

    update_lyrics_window (state.title, state.artist, _("Connecting to lyrics.wikia.com ..."));
    vfs_async_file_get_contents (state.uri, get_lyrics_step_2, nullptr);
}

static QTextEdit * textedit;

static void update_lyrics_window (const char * title, const char * artist, const char * lyrics)
{
    QTextDocument doc;
    QTextCursor cursor (& doc);

    if (! textedit)
        return;

    cursor.insertHtml (QString ("<big><b>") + QString (title) + QString ("</b></big>"));

    if (artist)
    {
        cursor.insertHtml (QString ("<br><i>") + QString (artist) + QString ("</i>"));
    }

    cursor.insertHtml ("<br><br>");
    cursor.insertText (lyrics);

    textedit->setDocument (doc.clone ());
}

static void lyricwiki_playback_began ()
{
    /* FIXME: cancel previous VFS requests (not possible with current API) */

    state.filename = aud_drct_get_filename ();

    Tuple tuple = aud_drct_get_tuple ();
    state.title = tuple.get_str (Tuple::Title);
    state.artist = tuple.get_str (Tuple::Artist);

    state.uri = String ();

    get_lyrics_step_1 ();
}

static void lw_cleanup (QObject * object = nullptr)
{
    state.filename = String ();
    state.title = String ();
    state.artist = String ();
    state.uri = String ();

    hook_dissociate ("tuple change", (HookFunction) lyricwiki_playback_began);
    hook_dissociate ("playback ready", (HookFunction) lyricwiki_playback_began);

    textedit = nullptr;
}

void * LyricWikiQt::get_qt_widget ()
{
    textedit = new TextEdit;
    textedit->setReadOnly (true);

    hook_associate ("tuple change", (HookFunction) lyricwiki_playback_began, nullptr);
    hook_associate ("playback ready", (HookFunction) lyricwiki_playback_began, nullptr);

    if (aud_drct_get_ready ())
        lyricwiki_playback_began ();

    QObject::connect (textedit, & QObject::destroyed, lw_cleanup);

    return textedit;
}

void TextEdit::contextMenuEvent (QContextMenuEvent * event)
{
    if (! state.uri)
        return QTextEdit::contextMenuEvent (event);

    QMenu * menu = createStandardContextMenu ();
    menu->addSeparator ();
    QAction * edit = menu->addAction (_("Edit lyrics ..."));
    QObject::connect (edit, & QAction::triggered, [] () {
        QDesktopServices::openUrl (QUrl ((const char *) state.uri));
    });
    menu->exec (event->globalPos ());
    delete menu;
}
