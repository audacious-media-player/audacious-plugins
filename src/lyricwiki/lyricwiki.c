/*
 * Copyright (c) 2010 William Pitcock <nenolod@dereferenced.org>
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

#include <stdio.h>
#include <glib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <audacious/plugins.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/vfs_async.h>

/* all strings in this struct are pooled */
typedef struct {
	char *filename; /* of song file */
	char *title, *artist;
	char *uri; /* URI we are trying to retrieve */
} LyricsState;

static LyricsState state;

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
	gchar *ret = NULL;

	/*
	 * temporarily set our error-handling functor to our suppression function,
	 * but we have to set it back because other components of Audacious depend
	 * on libxml and we don't want to step on their code paths.
	 *
	 * unfortunately, libxml is anti-social and provides us with no way to get
	 * the previous error functor, so we just have to set it back to default after
	 * parsing and hope for the best.
	 */
	xmlSetGenericErrorFunc(NULL, libxml_error_handler);
	doc = htmlReadMemory(buf, (int) len, NULL, "utf-8", (HTML_PARSE_RECOVER | HTML_PARSE_NONET));
	xmlSetGenericErrorFunc(NULL, NULL);

	if (doc != NULL)
	{
		xmlXPathContextPtr xpath_ctx = NULL;
		xmlXPathObjectPtr xpath_obj = NULL;
		xmlNodePtr node = NULL;

		xpath_ctx = xmlXPathNewContext(doc);
		if (xpath_ctx == NULL)
			goto give_up;

		xpath_obj = xmlXPathEvalExpression((xmlChar *) "//*[@id=\"wpTextbox1\"]", xpath_ctx);
		if (xpath_obj == NULL)
			goto give_up;

		if (!xpath_obj->nodesetval->nodeMax)
			goto give_up;

		node = xpath_obj->nodesetval->nodeTab[0];
give_up:
		if (xpath_obj != NULL)
			xmlXPathFreeObject(xpath_obj);

		if (xpath_ctx != NULL)
			xmlXPathFreeContext(xpath_ctx);

		if (node != NULL)
		{
			xmlChar *lyric = xmlNodeGetContent(node);

			if (lyric != NULL)
			{
				GMatchInfo *match_info;
				GRegex *reg;

				reg = g_regex_new("<(lyrics?)>[[:space:]]*(.*?)[[:space:]]*</\\1>", (G_REGEX_MULTILINE | G_REGEX_DOTALL), 0, NULL);
				g_regex_match(reg, (gchar *) lyric, G_REGEX_MATCH_NEWLINE_ANY, &match_info);

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

/* str_unref() returned string */
static char *scrape_uri_from_lyricwiki_search_result(const char *buf, int64_t len)
{
	xmlDocPtr doc;
	gchar *uri = NULL;

	/*
	 * workaround buggy lyricwiki search output where it cuts the lyrics
	 * halfway through the UTF-8 symbol resulting in invalid XML.
	 */
	GRegex *reg;

	reg = g_regex_new("<(lyrics?)>.*</\\1>", (G_REGEX_MULTILINE | G_REGEX_DOTALL | G_REGEX_UNGREEDY), 0, NULL);
	gchar *newbuf = g_regex_replace_literal(reg, buf, len, 0, "", G_REGEX_MATCH_NEWLINE_ANY, NULL);
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
	xmlSetGenericErrorFunc(NULL, libxml_error_handler);
	doc = xmlParseMemory(newbuf, strlen(newbuf));
	xmlSetGenericErrorFunc(NULL, NULL);

	if (doc != NULL)
	{
		xmlNodePtr root, cur;

		root = xmlDocGetRootElement(doc);

		for (cur = root->xmlChildrenNode; cur; cur = cur->next)
		{
			if (xmlStrEqual(cur->name, (xmlChar *) "url"))
			{
				xmlChar *lyric;
				gchar *basename;

				lyric = xmlNodeGetContent(cur);
				basename = g_path_get_basename((gchar *) lyric);

				uri = str_printf("http://lyrics.wikia.com/index.php?action=edit"
				 "&title=%s", basename);

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

static bool_t get_lyrics_step_3(void *buf, int64_t len, void *requri)
{
	if (!state.uri || strcmp(state.uri, requri))
	{
		g_free(buf);
		str_unref(requri);
		return FALSE;
	}
	str_unref(requri);

	if(!len)
	{
		SPRINTF(error, _("Unable to fetch %s"), state.uri);
		update_lyrics_window(_("Error"), NULL, error);
		g_free(buf);
		return FALSE;
	}

	char *lyrics = scrape_lyrics_from_lyricwiki_edit_page(buf, len);

	if(!lyrics)
	{
		SPRINTF(error, _("Unable to parse %s"), state.uri);
		update_lyrics_window(_("Error"), NULL, error);
		g_free(buf);
		return FALSE;
	}

	update_lyrics_window(state.title, state.artist, lyrics);

	g_free(lyrics);
	return TRUE;
}

static bool_t get_lyrics_step_2(void *buf, int64_t len, void *requri)
{
	if (!state.uri || strcmp(state.uri, requri))
	{
		g_free(buf);
		str_unref(requri);
		return FALSE;
	}
	str_unref(requri);

	if(!len)
	{
		SPRINTF(error, _("Unable to fetch %s"), state.uri);
		update_lyrics_window(_("Error"), NULL, error);
		g_free(buf);
		return FALSE;
	}

	char *uri = scrape_uri_from_lyricwiki_search_result(buf, len);

	if(!uri)
	{
		SPRINTF(error, _("Unable to parse %s"), state.uri);
		update_lyrics_window(_("Error"), NULL, error);
		g_free(buf);
		return FALSE;
	}

	str_unref(state.uri);
	state.uri = uri;

	update_lyrics_window(state.title, state.artist, _("Looking for lyrics ..."));
	vfs_async_file_get_contents(uri, get_lyrics_step_3, str_ref(state.uri));

	g_free(buf);
	return TRUE;
}

static void get_lyrics_step_1(void)
{
	if(!state.artist || !state.title)
	{
		update_lyrics_window(_("Error"), NULL, _("Missing song metadata"));
		return;
	}

	char title_buf[strlen(state.title) * 3 + 1];
	char artist_buf[strlen(state.artist) * 3 + 1];
	str_encode_percent(state.title, -1, title_buf);
	str_encode_percent(state.artist, -1, artist_buf);

	str_unref(state.uri);
	state.uri = str_printf("http://lyrics.wikia.com/api.php?action=lyrics&"
	 "artist=%s&song=%s&fmt=xml", artist_buf, title_buf);

	update_lyrics_window(state.title, state.artist, _("Connecting to lyrics.wikia.com ..."));
	vfs_async_file_get_contents(state.uri, get_lyrics_step_2, str_ref(state.uri));
}

static GtkWidget *scrollview, *vbox;
static GtkWidget *textview;
static GtkTextBuffer *textbuffer;

static GtkWidget *build_widget(void)
{
	textview = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview), 4);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(textview), 4);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD);
	textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	scrollview = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollview), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollview), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

	gtk_container_add(GTK_CONTAINER(scrollview), textview);

	gtk_box_pack_start(GTK_BOX(vbox), scrollview, TRUE, TRUE, 0);

	gtk_widget_show(textview);
	gtk_widget_show(scrollview);
	gtk_widget_show(vbox);

	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(textbuffer), "weight_bold", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(textbuffer), "size_x_large", "scale", PANGO_SCALE_X_LARGE, NULL);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(textbuffer), "style_italic", "style", PANGO_STYLE_ITALIC, NULL);

	g_signal_connect (vbox, "destroy", (GCallback) gtk_widget_destroyed, & vbox);
	return vbox;
}

static void update_lyrics_window(const char *title, const char *artist, const char *lyrics)
{
	GtkTextIter iter;

	if (textbuffer == NULL)
		return;

	gtk_text_buffer_set_text(GTK_TEXT_BUFFER(textbuffer), "", -1);

	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(textbuffer), &iter);

	gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(textbuffer), &iter,
			title, -1, "weight_bold", "size_x_large", NULL);

	if (artist != NULL)
	{
		gtk_text_buffer_insert(GTK_TEXT_BUFFER(textbuffer), &iter, "\n", -1);
		gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(textbuffer),
				&iter, artist, -1, "style_italic", NULL);
	}

	gtk_text_buffer_insert(GTK_TEXT_BUFFER(textbuffer), &iter, "\n\n", -1);
	gtk_text_buffer_insert(GTK_TEXT_BUFFER(textbuffer), &iter, lyrics, -1);

	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(textbuffer), &iter);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(textview), &iter, 0, TRUE, 0, 0);
}

static void lyricwiki_playback_began(void)
{
	if (!aud_drct_get_playing())
		return;

	/* FIXME: cancel previous VFS requests (not possible with current API) */
	str_unref(state.filename);
	str_unref(state.title);
	str_unref(state.artist);
	str_unref(state.uri);

	int playlist = aud_playlist_get_playing();
	int pos = aud_playlist_get_position(playlist);

	state.filename = aud_playlist_entry_get_filename(playlist, pos);
	aud_playlist_entry_describe(playlist, pos, &state.title, &state.artist, NULL, FALSE);
	state.uri = NULL;

	get_lyrics_step_1();
}

static void cleanup (void)
{
	str_unref (state.filename);
	str_unref (state.title);
	str_unref (state.artist);
	str_unref (state.uri);
	state.filename = NULL;
	state.title = NULL;
	state.artist = NULL;
	state.uri = NULL;

	hook_dissociate ("title change", (HookFunction) lyricwiki_playback_began);
	hook_dissociate ("playback ready", (HookFunction) lyricwiki_playback_began);

	textbuffer = NULL;
}

static void * get_widget (void)
{
	build_widget ();

	hook_associate ("title change", (HookFunction) lyricwiki_playback_began, NULL);
	hook_associate ("playback ready", (HookFunction) lyricwiki_playback_began, NULL);

	lyricwiki_playback_began ();

	g_signal_connect (vbox, "destroy", cleanup, NULL);

	return vbox;
}


AUD_GENERAL_PLUGIN
(
	.name = N_("LyricWiki Plugin"),
	.domain = PACKAGE,
	.get_widget = get_widget
)
