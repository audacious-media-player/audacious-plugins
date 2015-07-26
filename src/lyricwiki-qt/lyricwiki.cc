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

#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

#define AUD_PLUGIN_QT_ONLY
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

class LyricWikiQt : public GeneralPlugin {
public:
    static constexpr PluginInfo info = {
        N_("LyricWiki Plugin"),
        PACKAGE
    };

    constexpr LyricWikiQt() : GeneralPlugin (info, false) {}
    void * get_qt_widget ();
};

EXPORT LyricWikiQt aud_plugin_instance;

/*
 * Suppress libxml warnings, because lyricwiki does not generate anything near
 * valid HTML.
 */
static void libxml_error_handler(void *ctx, const char *msg, ...)
{
}

/* g_free() returned text */
static char *scrape_lyrics_from_lyricwiki_edit_page(const char *buf, int64_t len)
{
    xmlDocPtr doc;
    char *ret = nullptr;

    /*
     * temporarily set our error-handling functor to our suppression function,
     * but we have to set it back because other components of Audacious depend
     * on libxml and we don't want to step on their code paths.
     *
     * unfortunately, libxml is anti-social and provides us with no way to get
     * the previous error functor, so we just have to set it back to default after
     * parsing and hope for the best.
     */
    xmlSetGenericErrorFunc(nullptr, libxml_error_handler);
    doc = htmlReadMemory(buf, (int) len, nullptr, "utf-8", (HTML_PARSE_RECOVER | HTML_PARSE_NONET));
    xmlSetGenericErrorFunc(nullptr, nullptr);

    if (doc != nullptr)
    {
        xmlXPathContextPtr xpath_ctx = nullptr;
        xmlXPathObjectPtr xpath_obj = nullptr;
        xmlNodePtr node = nullptr;

        xpath_ctx = xmlXPathNewContext(doc);
        if (xpath_ctx == nullptr)
            goto give_up;

        xpath_obj = xmlXPathEvalExpression((xmlChar *) "//*[@id=\"wpTextbox1\"]", xpath_ctx);
        if (xpath_obj == nullptr)
            goto give_up;

        if (!xpath_obj->nodesetval->nodeMax)
            goto give_up;

        node = xpath_obj->nodesetval->nodeTab[0];
give_up:
        if (xpath_obj != nullptr)
            xmlXPathFreeObject(xpath_obj);

        if (xpath_ctx != nullptr)
            xmlXPathFreeContext(xpath_ctx);

        if (node != nullptr)
        {
            xmlChar *lyric = xmlNodeGetContent(node);

            if (lyric != nullptr)
            {
                GMatchInfo *match_info;
                GRegex *reg;

                reg = g_regex_new
                 ("<(lyrics?)>[[:space:]]*(.*?)[[:space:]]*</\\1>",
                 (GRegexCompileFlags) (G_REGEX_MULTILINE | G_REGEX_DOTALL),
                 (GRegexMatchFlags) 0, nullptr);
                g_regex_match(reg, (char *) lyric, G_REGEX_MATCH_NEWLINE_ANY, &match_info);

                ret = g_match_info_fetch(match_info, 2);
                if (!g_utf8_collate(ret, "<!-- PUT LYRICS HERE (and delete this entire line) -->"))
                {
                    g_free(ret);
                    ret = g_strdup(_("No lyrics available"));
                }

                g_regex_unref(reg);
            }

            xmlFree(lyric);
        }

        xmlFreeDoc(doc);
    }

    return ret;
}

static String scrape_uri_from_lyricwiki_search_result(const char *buf, int64_t len)
{
    xmlDocPtr doc;
    String uri;

    /*
     * workaround buggy lyricwiki search output where it cuts the lyrics
     * halfway through the UTF-8 symbol resulting in invalid XML.
     */
    GRegex *reg;

    reg = g_regex_new ("<(lyrics?)>.*</\\1>", (GRegexCompileFlags)
     (G_REGEX_MULTILINE | G_REGEX_DOTALL | G_REGEX_UNGREEDY),
     (GRegexMatchFlags) 0, nullptr);
    char *newbuf = g_regex_replace_literal(reg, buf, len, 0, "", G_REGEX_MATCH_NEWLINE_ANY, nullptr);
    g_regex_unref(reg);

    /*
     * temporarily set our error-handling functor to our suppression function,
     * but we have to set it back because other components of Audacious depend
     * on libxml and we don't want to step on their code paths.
     *
     * unfortunately, libxml is anti-social and provides us with no way to get
     * the previous error functor, so we just have to set it back to default after
     * parsing and hope for the best.
     */
    xmlSetGenericErrorFunc(nullptr, libxml_error_handler);
    doc = xmlParseMemory(newbuf, strlen(newbuf));
    xmlSetGenericErrorFunc(nullptr, nullptr);

    if (doc != nullptr)
    {
        xmlNodePtr root, cur;

        root = xmlDocGetRootElement(doc);

        for (cur = root->xmlChildrenNode; cur; cur = cur->next)
        {
            if (xmlStrEqual(cur->name, (xmlChar *) "url"))
            {
                xmlChar *lyric;
                char *basename;

                lyric = xmlNodeGetContent(cur);
                basename = g_path_get_basename((char *) lyric);

                uri = String (str_printf ("http://lyrics.wikia.com/index.php?"
                 "action=edit&title=%s", basename));

                g_free(basename);
                xmlFree(lyric);
            }
        }

        xmlFreeDoc(doc);
    }

    g_free(newbuf);

    return uri;
}

static void update_lyrics_window(const char *title, const char *artist, const char *lyrics);

static void get_lyrics_step_3(const char *uri, const Index<char> &buf, void*)
{
    if (!state.uri || strcmp(state.uri, uri))
        return;

    if (!buf.len())
    {
        update_lyrics_window (_("Error"), nullptr,
         str_printf (_("Unable to fetch %s"), uri));
        return;
    }

    char *lyrics = scrape_lyrics_from_lyricwiki_edit_page(buf.begin(), buf.len());

    if (!lyrics)
    {
        update_lyrics_window (_("Error"), nullptr,
         str_printf (_("Unable to parse %s"), uri));
        return;
    }

    update_lyrics_window(state.title, state.artist, lyrics);

    g_free(lyrics);
}

static void get_lyrics_step_2(const char *uri1, const Index<char> &buf, void*)
{
    if (!state.uri || strcmp(state.uri, uri1))
        return;

    if (!buf.len())
    {
        update_lyrics_window (_("Error"), nullptr,
         str_printf (_("Unable to fetch %s"), uri1));
        return;
    }

    String uri = scrape_uri_from_lyricwiki_search_result(buf.begin(), buf.len());

    if (!uri)
    {
        update_lyrics_window (_("Error"), nullptr,
         str_printf (_("Unable to parse %s"), uri1));
        return;
    }

    state.uri = uri;

    update_lyrics_window(state.title, state.artist, _("Looking for lyrics ..."));
    vfs_async_file_get_contents(uri, get_lyrics_step_3, nullptr);
}

static void get_lyrics_step_1(void)
{
    if(!state.artist || !state.title)
    {
        update_lyrics_window(_("Error"), nullptr, _("Missing song metadata"));
        return;
    }

    StringBuf title_buf = str_encode_percent (state.title);
    StringBuf artist_buf = str_encode_percent (state.artist);

    state.uri = String (str_printf ("http://lyrics.wikia.com/api.php?"
     "action=lyrics&artist=%s&song=%s&fmt=xml", (const char *) artist_buf,
     (const char *) title_buf));

    update_lyrics_window(state.title, state.artist, _("Connecting to lyrics.wikia.com ..."));
    vfs_async_file_get_contents(state.uri, get_lyrics_step_2, nullptr);
}

static QTextEdit * textedit;

static void update_lyrics_window(const char *title, const char *artist, const char *lyrics)
{
    QTextDocument doc;
    QTextCursor cursor (& doc);

    if (! textedit)
        return;

    cursor.insertHtml (QString ("<big><b>") + QString (title) + QString ("<b></big>"));

    if (artist != nullptr)
    {
        cursor.insertHtml (QString ("<br><i><b>") + QString (artist) + QString ("</b></i>"));
    }

    cursor.insertHtml ("<br><br>");
    cursor.insertText (lyrics);

    AUDDBG ("update document <%p>\n", textedit);

    textedit->setDocument (doc.clone ());
}

static void lyricwiki_playback_began(void)
{
    /* FIXME: cancel previous VFS requests (not possible with current API) */

    state.filename = aud_drct_get_filename();

    Tuple tuple = aud_drct_get_tuple();
    state.title = tuple.get_str(Tuple::Title);
    state.artist = tuple.get_str(Tuple::Artist);

    state.uri = String ();

    get_lyrics_step_1();
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
    textedit = new QTextEdit;
    textedit->setReadOnly (true);

    hook_associate ("tuple change", (HookFunction) lyricwiki_playback_began, nullptr);
    hook_associate ("playback ready", (HookFunction) lyricwiki_playback_began, nullptr);

    if (aud_drct_get_ready ())
        lyricwiki_playback_began ();

    QObject::connect (textedit, &QObject::destroyed, lw_cleanup);

    return textedit;
}
