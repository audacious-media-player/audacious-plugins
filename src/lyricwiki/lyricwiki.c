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

/*
 * TODO:
 *   - do the VFS operations in another thread
 *   - why don't we have a url encoding function?
 *     ... or do we and I'm just smoking crack?
 *   - no idea what this code does if lyricwiki is down - probably crashes!
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

#include "config.h"
#include "urlencode.h"

/*
 * Suppress libxml warnings, because lyricwiki does not generate anything near
 * valid HTML.
 */
void
libxml_error_handler(void *ctx, const char *msg, ...)
{

}

/* Oh, this is going to be fun... */
gchar *
scrape_lyrics_from_lyricwiki_edit_page(const gchar *buf, gsize len)
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

				reg = g_regex_new("<(lyrics?)>(.*)</\\1>", (G_REGEX_MULTILINE | G_REGEX_DOTALL | G_REGEX_UNGREEDY), 0, NULL);
				g_regex_match(reg, (gchar *) lyric, G_REGEX_MATCH_NEWLINE_ANY, &match_info);

				ret = g_match_info_fetch(match_info, 2);
				if (!g_utf8_collate(ret, "\n<!-- PUT LYRICS HERE (and delete this entire line) -->\n"))
				{
					g_free(ret);
					ret = NULL;
				}

				g_regex_unref(reg);
			}

			xmlFree(lyric);
		}

		xmlFreeDoc(doc);
	}

	return ret;
}

gchar *
scrape_uri_from_lyricwiki_search_result(const gchar *buf, gsize len)
{
	xmlDocPtr doc;
	gchar *uri = NULL;

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
	doc = xmlParseMemory(buf, (int) len);
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

				uri = g_strdup_printf("http://lyrics.wikia.com/index.php?action=edit&title=%s", basename);
				g_free(basename);
				xmlFree(lyric);
			}
		}

		xmlFreeDoc(doc);
	}

	return uri;
}

gboolean
check_current_track(Tuple *tu)
{
	gboolean ret = TRUE;
	gint playlist, pos, i;
	gint fields[] = {FIELD_ARTIST, FIELD_TITLE};
	Tuple *cu = NULL;

	if (tu == NULL)
		return FALSE;

	playlist = aud_playlist_get_playing();
	pos = aud_playlist_get_position(playlist);
	cu = aud_playlist_entry_get_tuple(playlist, pos, FALSE);

	if (cu == NULL)
		return FALSE;

	for (i = 0; i < sizeof(fields)/sizeof(gint); i++)
	{
		gchar * string1 = tuple_get_str (tu, fields[i], NULL);
		gchar * string2 = tuple_get_str (cu, fields[i], NULL);

		if (string1 == NULL && string2 == NULL)
			continue;

		if (string1 == NULL || string2 == NULL ||
			strcmp(string1, string2) != 0)
		{
		    ret = FALSE;
			str_unref (string1);
			str_unref (string2);
		    break;
		}

		str_unref (string1);
		str_unref (string2);
	}

	tuple_unref(cu);
	return ret;
}

void update_lyrics_window(const Tuple *tu, const gchar *lyrics);

gboolean
get_lyrics_step_3(gchar *buf, gint64 len, Tuple *tu)
{
	gchar *lyrics = NULL;

	if (buf != NULL)
	{
		lyrics = scrape_lyrics_from_lyricwiki_edit_page(buf, len);
		g_free(buf);
	}

	if (check_current_track(tu))
		update_lyrics_window(tu, lyrics);

	tuple_unref (tu);

	if (lyrics != NULL)
		g_free(lyrics);

	return buf == NULL ? FALSE : TRUE;
}

gboolean
get_lyrics_step_2(gchar *buf, gint64 len, Tuple *tu)
{
	gchar *uri;

	uri = scrape_uri_from_lyricwiki_search_result(buf, len);
	if (uri == NULL)
	{
		if (check_current_track(tu))
			update_lyrics_window(tu, NULL);

		goto CLEANUP;
	}

	if (check_current_track(tu))
	{
		update_lyrics_window(tu, _("\nLooking for lyrics..."));
		vfs_async_file_get_contents(uri, (VFSConsumer) get_lyrics_step_3, tu);
	}
	else
	{
		g_free(uri);
		goto CLEANUP;
	}

	g_free(buf);

	return TRUE;

CLEANUP:
	g_free(buf);
	tuple_unref (tu);
	return FALSE;
}

void
get_lyrics_step_1(const Tuple *tu)
{
	gchar *uri;
	gchar *artist, *title;

	gchar * artist0 = tuple_get_str (tu, FIELD_ARTIST, NULL);
	gchar * title0 = tuple_get_str (tu, FIELD_TITLE, NULL);
	artist = lyricwiki_url_encode (artist0);
	title = lyricwiki_url_encode (title0);
	str_unref (artist0);
	str_unref (title0);

	uri = g_strdup_printf("http://lyrics.wikia.com/api.php?action=lyrics&artist=%s&song=%s&fmt=xml", artist, title);

	g_free(artist);
	g_free(title);

	update_lyrics_window(tu, _("\nConnecting to lyrics.wikia.com..."));
	vfs_async_file_get_contents(uri, (VFSConsumer) get_lyrics_step_2, (Tuple *) tu);

	g_free(uri);
}

GtkWidget *scrollview, *vbox;
GtkWidget *textview;
GtkTextBuffer *textbuffer;

GtkWidget *
build_widget(void)
{
	textview = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview), 4);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(textview), 4);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD);
	textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	scrollview = gtk_scrolled_window_new(NULL, NULL);
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

void
update_lyrics_window(const Tuple *tu, const gchar *lyrics)
{
	GtkTextIter iter;
	const gchar *real_lyrics;

	if (textbuffer == NULL)
		return;

	gtk_text_buffer_set_text(GTK_TEXT_BUFFER(textbuffer), "", -1);

	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(textbuffer), &iter);

	gchar * title = tuple_get_str (tu, FIELD_TITLE, NULL);
	gchar * artist = tuple_get_str (tu, FIELD_ARTIST, NULL);

	if (! title)
		title = tuple_get_str (tu, FIELD_FILE_NAME, NULL);

	gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(textbuffer), &iter,
			title, strlen(title), "weight_bold", "size_x_large", NULL);

	gtk_text_buffer_insert(GTK_TEXT_BUFFER(textbuffer), &iter, "\n", 1);

	if (artist != NULL)
	{
		gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(textbuffer),
				&iter, artist, strlen(artist), "style_italic", NULL);

		gtk_text_buffer_insert(GTK_TEXT_BUFFER(textbuffer), &iter, "\n", 1);
	}

	real_lyrics = lyrics != NULL ? lyrics : _("\nNo lyrics were found.");

	gtk_text_buffer_insert(GTK_TEXT_BUFFER(textbuffer), &iter, real_lyrics, strlen(real_lyrics));

	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(textbuffer), &iter);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(textview), &iter, 0, TRUE, 0, 0);

	str_unref (title);
	str_unref (artist);
}

void
lyricwiki_playback_began(void)
{
	gint playlist, pos;

	if (!aud_drct_get_playing())
		return;

	playlist = aud_playlist_get_playing();
	pos = aud_playlist_get_position(playlist);
	Tuple * tu = aud_playlist_entry_get_tuple (playlist, pos, FALSE);

	get_lyrics_step_1(tu);
}

static gboolean init (void)
{
	hook_associate("title change", (HookFunction) lyricwiki_playback_began, NULL);
	hook_associate("playback ready", (HookFunction) lyricwiki_playback_began, NULL);

	build_widget();

	lyricwiki_playback_began();
	return TRUE;
}

static void
cleanup(void)
{
	hook_dissociate("title change", (HookFunction) lyricwiki_playback_began);
	hook_dissociate("playback ready", (HookFunction) lyricwiki_playback_began);

	if (vbox)
		gtk_widget_destroy (vbox);
	textbuffer = NULL;
}

static gpointer
get_widget(void)
{
	if (! vbox)
		build_widget ();
	return vbox;
}

AUD_GENERAL_PLUGIN
(
	.name = N_("LyricWiki Plugin"),
	.domain = PACKAGE,
	.init = init,
	.cleanup = cleanup,
	.get_widget = get_widget,
)
