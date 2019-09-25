/*
 * Copyright (c) 2019 TANAKA Hidemune
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

#include <libaudqt/libaudqt.h>

class Lyric_japanQt : public GeneralPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Lyric_japan Plugin"),
        PACKAGE,
        nullptr, // about
        nullptr, // prefs
        PluginQtOnly
    };

    constexpr Lyric_japanQt () : GeneralPlugin (info, false) {}

    void * get_qt_widget ();
};

EXPORT Lyric_japanQt aud_plugin_instance;

typedef struct {
    String filename; /* of song file */
    String title, artist;
    String uri; /* URI we are trying to retrieve */
} LyricsState;

static LyricsState state;

/*
 * Suppress libxml warnings, because Lyric_japan does not generate anything near
 * valid HTML.
 */
static void libxml_error_handler (void * ctx, const char * msg, ...)
{
}

static CharPtr scrape_lyrics_from_lyric_japan_edit_page (const char * buf, int64_t len)
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

static void update_lyrics_window (const char * title, const char * artist,
 const char * lyrics, bool edit_enabled);

static void get_lyrics_step_1 ()
{
    if (! state.artist || ! state.title)
    {
        update_lyrics_window (_("Error"), nullptr, _("Missing song metadata"), false);
        return;
    }

    StringBuf title_buf = str_encode_percent (state.title);
    StringBuf artist_buf = str_encode_percent (state.artist);

    state.uri = String (str_printf ("x-www-browser https://search.yahoo.co.jp/search?p=%s+%s+歌詞&ei=UTF-8", (const char *) artist_buf,
     (const char *) title_buf));
    system(state.uri);
}

static QTextEdit * textedit;

static void lyric_japan_playback_began ()
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

    hook_dissociate ("tuple change", (HookFunction) lyric_japan_playback_began);
    hook_dissociate ("playback ready", (HookFunction) lyric_japan_playback_began);

    textedit = nullptr;
}

void * Lyric_japanQt::get_qt_widget ()
{
    textedit = new QTextEdit;
    textedit->setReadOnly (true);

#ifdef Q_OS_MAC  // Mac-specific font tweaks
    textedit->document ()->setDefaultFont (QApplication::font ("QTipLabel"));
#endif

    hook_associate ("tuple change", (HookFunction) lyric_japan_playback_began, nullptr);
    hook_associate ("playback ready", (HookFunction) lyric_japan_playback_began, nullptr);

    if (aud_drct_get_ready ())
        lyric_japan_playback_began ();

    QObject::connect (textedit, & QObject::destroyed, lw_cleanup);

    return textedit;
}
