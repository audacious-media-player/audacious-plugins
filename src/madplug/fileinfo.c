/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
 *
 * Portions derived from xmms-mad:
 * Copyright (C) 2001-2002 Sam Clegg - See COPYING
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include "plugin.h"
#include "input.h"

#include "mp3.xpm"

#include <math.h>
#include <audacious/util.h>
#include <gtk/gtk.h>

/* yaz */
#include <langinfo.h>

#ifndef NOGUI
static GtkWidget *window = 0;
static GtkWidget *filename_entry, *id3_frame;
static GtkWidget *title_entry, *artist_entry, *album_entry;
static GtkWidget *year_entry, *tracknum_entry, *comment_entry;
static GtkWidget *genre_combo;
static GtkWidget *mpeg_level, *mpeg_bitrate, *mpeg_samplerate,
    *mpeg_frames, *mpeg_duration, *mpeg_flags;
static GtkWidget *mpeg_fileinfo, *mpeg_replaygain, *mpeg_replaygain2,
    *mpeg_replaygain3, *mpeg_replaygain4, *mp3gain1, *mp3gain2;
#endif                          /* !NOGUI */

static GList *genre_list = 0;
static struct mad_info_t info;
struct id3_frame *id3_frame_new(const char *str);

#ifndef NOGUI
static void
update_id3_frame(struct id3_tag *tag, const char *frame_name,
                 const char *data)
{
    int res;
    struct id3_frame *frame;
    union id3_field *field;
    id3_ucs4_t *ucs4;

    if (data == NULL)
        return;

    /* printf ("updating id3: %s: %s\n", frame_name, data); */

    /*
     * An empty string removes the frame altogether.
     */
    if (strlen(data) == 0) {
        while ((frame = id3_tag_findframe(tag, frame_name, 0)))
            id3_tag_detachframe(tag, frame);
        return;
    }

    frame = id3_tag_findframe(tag, frame_name, 0);
    if (!frame) {
        frame = id3_frame_new(frame_name);
        id3_tag_attachframe(tag, frame);
    }

    // setup ucs4 string
    if(audmad_config.sjis) {
        ucs4 = id3_latin1_ucs4duplicate((id3_latin1_t *) data);
    }
    else {
        ucs4 = id3_utf8_ucs4duplicate((id3_utf8_t *) data);
    }

    // set encoding
    field = id3_frame_field(frame, 0);
    id3_field_settextencoding(field, audmad_config.sjis ? ID3_FIELD_TEXTENCODING_ISO_8859_1 :
			      ID3_FIELD_TEXTENCODING_UTF_8);
        
    // setup genre code
    if (!strcmp(frame_name, ID3_FRAME_GENRE)) {
        char *tmp;
        int index = id3_genre_number(ucs4);
        g_free(ucs4);
        tmp = g_strdup_printf("%d", index);
        ucs4 = id3_latin1_ucs4duplicate((unsigned char *) tmp);
    }

    // write string
    if (!strcmp(frame_name, ID3_FRAME_COMMENT)) {
        field = id3_frame_field(frame, 3);
        field->type = ID3_FIELD_TYPE_STRINGFULL;
        res = id3_field_setfullstring(field, ucs4);
    }
    else {
        field = id3_frame_field(frame, 1);
        field->type = ID3_FIELD_TYPE_STRINGLIST;
        res = id3_field_setstrings(field, 1, &ucs4);
    }

    if (res != 0)
        g_print("error setting id3 field: %s\n", frame_name);
}

static void close_window(GtkWidget * w, gpointer data)
{
    input_term(&info);
    gtk_widget_destroy(window);
}

static void save_cb(GtkWidget * w, gpointer data)
{
    gchar *text, *sjis;
    struct id3_file *id3file;
    struct id3_tag *id3tag;
    char *encoding;

    if (info.remote)
        return;

    /* read tag from file */
    id3file = id3_file_open(info.filename, ID3_FILE_MODE_READWRITE);
    if (!id3file) {
        id3tag = id3_tag_new();
        id3_tag_clearframes(id3tag);
        id3tag->options |= ID3_TAG_OPTION_ID3V1;
        if (!id3file) {
            xmms_show_message("File Info", "Couldn't open file!", "Ok",
                              FALSE, NULL, NULL);
            return;
        }
    }
    id3tag = id3_file_tag(id3file);
    id3_tag_options(id3tag, ID3_TAG_OPTION_ID3V1, ~0);    /* enables id3v1 */
//    id3_tag_options(id3tag, ID3_TAG_OPTION_ID3V1, 0);    /* disables id3v1 */
    if (!id3tag) {
        id3tag = id3_tag_new();
    }

    encoding = audmad_config.sjis ? "SJIS" : "UTF-8";

    text = gtk_editable_get_chars(GTK_EDITABLE(title_entry), 0, -1);
    sjis = g_convert(text, strlen(text), encoding, "UTF-8", NULL, NULL, NULL);
    update_id3_frame(id3tag, ID3_FRAME_TITLE, sjis);
    free(text);
    free(sjis);

    text = gtk_editable_get_chars(GTK_EDITABLE(artist_entry), 0, -1);
    sjis = g_convert(text, strlen(text), encoding, "UTF-8", NULL, NULL, NULL);
    update_id3_frame(id3tag, ID3_FRAME_ARTIST, sjis);
    free(text);
    free(sjis);

    text = gtk_editable_get_chars(GTK_EDITABLE(album_entry), 0, -1);
    sjis =
        g_convert(text, strlen(text), encoding, "UTF-8", NULL, NULL, NULL);
    update_id3_frame(id3tag, ID3_FRAME_ALBUM, sjis);
    free(text);
    free(sjis);

    text = gtk_editable_get_chars(GTK_EDITABLE(year_entry), 0, -1);
    sjis =
        g_convert(text, strlen(text), encoding, "UTF-8", NULL, NULL, NULL);
    update_id3_frame(id3tag, ID3_FRAME_YEAR, sjis);
    free(text);
    free(sjis);

    text = gtk_editable_get_chars(GTK_EDITABLE(comment_entry), 0, -1);
    sjis =
        g_convert(text, strlen(text), encoding, "UTF-8", NULL, NULL, NULL);
    update_id3_frame(id3tag, ID3_FRAME_COMMENT, sjis);
    free(text);
    free(sjis);

    text = gtk_editable_get_chars(GTK_EDITABLE(tracknum_entry), 0, -1);
    sjis =
        g_convert(text, strlen(text), encoding, "UTF-8", NULL, NULL, NULL);
    update_id3_frame(id3tag, ID3_FRAME_TRACK, sjis);
    free(text);
    free(sjis);

    text = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(genre_combo)->entry),
                               0, -1);
    sjis = g_convert(text, strlen(text), encoding, "UTF-8", NULL, NULL, NULL);
    update_id3_frame(id3tag, ID3_FRAME_GENRE, sjis);
    free(text);
    free(sjis);

    if (id3_file_update(id3file) != 0) {
        xmms_show_message("File Info", "Couldn't write tag!", "Ok", FALSE,
                          NULL, NULL);
    }
    id3_file_close(id3file);
//    gtk_widget_destroy(window);
}

#if 0
static void remove_id3_cb(GtkWidget * w, gpointer data)
{
    struct id3_file *id3file;
    struct id3_tag *id3tag;

    id3file = id3_file_open(info.filename, ID3_FILE_MODE_READWRITE);

    if (id3file == NULL)
        return;

    id3tag = id3_file_tag(id3file);

    if (id3tag == NULL)
    {
        id3_file_close(id3file);
        return;
    }

    id3_tag_clearframes(id3tag);
    id3_file_update(id3file);
    id3_file_close(id3file);

    gtk_entry_set_text(GTK_ENTRY(title_entry), "");
    gtk_entry_set_text(GTK_ENTRY(artist_entry), "");
    gtk_entry_set_text(GTK_ENTRY(album_entry), "");
    gtk_entry_set_text(GTK_ENTRY(comment_entry), "");
    gtk_entry_set_text(GTK_ENTRY(year_entry), "");
    gtk_entry_set_text(GTK_ENTRY(tracknum_entry), "");
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(genre_combo)->entry), "");
    gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);
}
#endif

static void remove_id3_cb(GtkWidget * w, gpointer data)
{
    struct id3_file *id3file;
    struct id3_tag *id3tag;

    /* read tag from file */
    id3file = id3_file_open(info.filename, ID3_FILE_MODE_READWRITE);
    if (!id3file)
        return;

    id3tag = id3_file_tag(id3file);
    if(id3tag) {
        /* since current libid3tag cannot delete tag completely, just add a dummy. */
        const char *dummy = "";
        update_id3_frame(id3tag, ID3_FRAME_TITLE, dummy);
        update_id3_frame(id3tag, ID3_FRAME_ARTIST, dummy);
        update_id3_frame(id3tag, ID3_FRAME_ALBUM, dummy);
        update_id3_frame(id3tag, ID3_FRAME_YEAR, dummy);
        update_id3_frame(id3tag, ID3_FRAME_TRACK, dummy);
        update_id3_frame(id3tag, ID3_FRAME_GENRE, "Other");
        update_id3_frame(id3tag, ID3_FRAME_COMMENT, dummy);

        if (id3_file_update(id3file) != 0) {
            xmms_show_message("File Info", "Couldn't write tag!", "OK", FALSE,
                              NULL, NULL);
        }
    }

    id3_file_close(id3file);

    gtk_entry_set_text(GTK_ENTRY(title_entry), "");
    gtk_entry_set_text(GTK_ENTRY(artist_entry), "");
    gtk_entry_set_text(GTK_ENTRY(album_entry), "");
    gtk_entry_set_text(GTK_ENTRY(comment_entry), "");
    gtk_entry_set_text(GTK_ENTRY(year_entry), "");
    gtk_entry_set_text(GTK_ENTRY(tracknum_entry), "");
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(genre_combo)->entry), "");
    gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);
    
//    gtk_widget_destroy(window);
}

#if 0
void create_window()
{
    GtkWidget *vbox, *hbox, *left_vbox, *table;
    GtkWidget *mpeg_frame, *mpeg_box;
    GtkWidget *label, *filename_hbox;
    GtkWidget *bbox, *save, *remove_id3, *cancel;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
    gtk_signal_connect(GTK_OBJECT(window), "destroy",
                       GTK_SIGNAL_FUNC(close_window), &window);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    filename_hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), filename_hbox, FALSE, TRUE, 0);

    label = gtk_label_new("Filename:");
    gtk_box_pack_start(GTK_BOX(filename_hbox), label, FALSE, TRUE, 0);
    filename_entry = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(filename_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(filename_hbox), filename_entry, TRUE, TRUE,
                       0);

    hbox = gtk_hbox_new(FALSE, 10);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    left_vbox = gtk_vbox_new(FALSE, 10);
    gtk_box_pack_start(GTK_BOX(hbox), left_vbox, FALSE, FALSE, 0);

    id3_frame = gtk_frame_new("ID3 Tag:");
    gtk_box_pack_start(GTK_BOX(left_vbox), id3_frame, FALSE, FALSE, 0);

    table = gtk_table_new(5, 5, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 5);
    gtk_container_add(GTK_CONTAINER(id3_frame), table);

    label = gtk_label_new("Title:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL,
                     GTK_FILL, 5, 5);

    title_entry = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(table), title_entry, 1, 4, 0, 1,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Artist:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL,
                     GTK_FILL, 5, 5);

    artist_entry = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(table), artist_entry, 1, 4, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Album:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL,
                     GTK_FILL, 5, 5);

    album_entry = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(table), album_entry, 1, 4, 2, 3,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Comment:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL,
                     GTK_FILL, 5, 5);

    comment_entry = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(table), comment_entry, 1, 4, 3, 4,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Year:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5, GTK_FILL,
                     GTK_FILL, 5, 5);

    year_entry = gtk_entry_new();
    gtk_widget_set_usize(year_entry, 40, -1);
    gtk_table_attach(GTK_TABLE(table), year_entry, 1, 2, 4, 5,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Track number:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 4, 5, GTK_FILL,
                     GTK_FILL, 5, 5);

    tracknum_entry = gtk_entry_new();
    gtk_widget_set_usize(tracknum_entry, 40, -1);
    gtk_table_attach(GTK_TABLE(table), tracknum_entry, 3, 4, 4, 5,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Genre:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6, GTK_FILL,
                     GTK_FILL, 5, 5);

    genre_combo = gtk_combo_new();
    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(genre_combo)->entry),
                           FALSE);
    if (!genre_list) {
        int i = 0;
        const id3_ucs4_t *ucs4 = id3_genre_index(i);
        while (ucs4) {
            genre_list =
                g_list_append(genre_list, id3_ucs4_latin1duplicate(ucs4));
            i++;
            ucs4 = id3_genre_index(i);
        }
    }
    gtk_combo_set_popdown_strings(GTK_COMBO(genre_combo), genre_list);

    gtk_table_attach(GTK_TABLE(table), genre_combo, 1, 4, 5, 6,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(left_vbox), bbox, FALSE, FALSE, 0);

    save = gtk_button_new_with_label("Save");
    gtk_signal_connect(GTK_OBJECT(save), "clicked",
                       GTK_SIGNAL_FUNC(save_cb), NULL);
    GTK_WIDGET_SET_FLAGS(save, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(bbox), save, TRUE, TRUE, 0);
    gtk_widget_grab_default(save);

    remove_id3 = gtk_button_new_with_label("Remove ID3");
    gtk_signal_connect(GTK_OBJECT(remove_id3), "clicked",
                       GTK_SIGNAL_FUNC(remove_id3_cb), NULL);
    GTK_WIDGET_SET_FLAGS(remove_id3, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(bbox), remove_id3, TRUE, TRUE, 0);

    cancel = gtk_button_new_with_label("Cancel");
    gtk_signal_connect_object(GTK_OBJECT(cancel), "clicked",
                              GTK_SIGNAL_FUNC(gtk_widget_destroy),
                              GTK_OBJECT(window));
    GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);

    mpeg_frame = gtk_frame_new("MPEG Info:");
    gtk_box_pack_start(GTK_BOX(hbox), mpeg_frame, FALSE, FALSE, 0);

    mpeg_box = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(mpeg_frame), mpeg_box);
    gtk_container_set_border_width(GTK_CONTAINER(mpeg_box), 10);
    gtk_box_set_spacing(GTK_BOX(mpeg_box), 0);

    mpeg_level = gtk_label_new("");
    gtk_widget_set_usize(mpeg_level, 120, -2);
    gtk_misc_set_alignment(GTK_MISC(mpeg_level), 0, 0);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_level, FALSE, FALSE, 0);

    mpeg_bitrate = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_bitrate), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_bitrate), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_bitrate, FALSE, FALSE, 0);

    mpeg_samplerate = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_samplerate), 0, 0);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_samplerate, FALSE, FALSE,
                       0);

    mpeg_flags = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_flags), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_flags), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_flags, FALSE, FALSE, 0);

    mpeg_frames = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_frames), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_frames), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_frames, FALSE, FALSE, 0);

    mpeg_duration = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_duration), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_duration), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_duration, FALSE, FALSE, 0);

    mpeg_replaygain = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_replaygain), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_replaygain), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_replaygain, FALSE, FALSE,
                       0);
    mpeg_replaygain2 = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_replaygain2), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_replaygain2), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_replaygain2, FALSE, FALSE,
                       0);
    mpeg_replaygain3 = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_replaygain3), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_replaygain3), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_replaygain3, FALSE, FALSE,
                       0);
    mpeg_replaygain4 = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_replaygain4), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_replaygain4), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_replaygain4, FALSE, FALSE,
                       0);
    mp3gain1 = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mp3gain1), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mp3gain1), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mp3gain1, FALSE, FALSE, 0);
    mp3gain2 = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mp3gain2), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mp3gain2), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mp3gain2, FALSE, FALSE, 0);

    mpeg_fileinfo = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_fileinfo), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_fileinfo), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_fileinfo, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
}
#endif

static void
change_buttons(GtkWidget * object)
{
    gtk_widget_set_sensitive(GTK_WIDGET(object), TRUE);
}

void create_window()
{
    GtkWidget *vbox, *hbox, *left_vbox, *table;
    GtkWidget *mpeg_frame, *mpeg_box;
    GtkWidget *label, *filename_hbox;
    GtkWidget *bbox, *save, *remove_id3, *cancel;
    GtkWidget *pixmapwid;
    GdkPixbuf *pixbuf;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(window),
			     GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(G_OBJECT(window), "destroy",
                       G_CALLBACK(close_window), &window);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    filename_hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), filename_hbox, FALSE, TRUE, 0);

    pixbuf = gdk_pixbuf_new_from_xpm_data((const gchar **)
                                          gnome_mime_audio_xpm);
    pixmapwid = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    gtk_misc_set_alignment(GTK_MISC(pixmapwid), 0, 0);
    gtk_box_pack_start(GTK_BOX(filename_hbox), pixmapwid, FALSE, FALSE,
                       0);

    label = gtk_label_new("<b>Name:</b>");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(filename_hbox), label, FALSE, TRUE, 0);
    filename_entry = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(filename_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(filename_hbox), filename_entry, TRUE, TRUE,
                       0);

    hbox = gtk_hbox_new(FALSE, 10);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    left_vbox = gtk_table_new(2, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), left_vbox, FALSE, FALSE, 0);

    mpeg_frame = gtk_frame_new(_(" MPEG Info "));
    gtk_table_attach(GTK_TABLE(left_vbox), mpeg_frame, 0, 2, 0, 1,
	GTK_FILL, GTK_FILL, 5, 5);

    mpeg_box = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(mpeg_frame), mpeg_box);
    gtk_container_set_border_width(GTK_CONTAINER(mpeg_box), 10);
    gtk_box_set_spacing(GTK_BOX(mpeg_box), 0);

    mpeg_level = gtk_label_new("");
    gtk_widget_set_usize(mpeg_level, 120, -2);
    gtk_misc_set_alignment(GTK_MISC(mpeg_level), 0, 0);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_level, FALSE, FALSE, 0);

    mpeg_bitrate = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_bitrate), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_bitrate), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_bitrate, FALSE, FALSE, 0);

    mpeg_samplerate = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_samplerate), 0, 0);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_samplerate, FALSE, FALSE,
                       0);

    mpeg_flags = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_flags), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_flags), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_flags, FALSE, FALSE, 0);

    mpeg_frames = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_frames), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_frames), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_frames, FALSE, FALSE, 0);

    mpeg_duration = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_duration), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_duration), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_duration, FALSE, FALSE, 0);

    mpeg_replaygain = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_replaygain), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_replaygain), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_replaygain, FALSE, FALSE,
                       0);
    mpeg_replaygain2 = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_replaygain2), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_replaygain2), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_replaygain2, FALSE, FALSE,
                       0);
    mpeg_replaygain3 = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_replaygain3), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_replaygain3), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_replaygain3, FALSE, FALSE,
                       0);
    mpeg_replaygain4 = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_replaygain4), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_replaygain4), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_replaygain4, FALSE, FALSE,
                       0);
    mp3gain1 = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mp3gain1), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mp3gain1), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mp3gain1, FALSE, FALSE, 0);
    mp3gain2 = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mp3gain2), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mp3gain2), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mp3gain2, FALSE, FALSE, 0);

    mpeg_fileinfo = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(mpeg_fileinfo), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mpeg_fileinfo), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(mpeg_box), mpeg_fileinfo, FALSE, FALSE, 0);

    id3_frame = gtk_frame_new(_(" ID3 Tag "));
    gtk_table_attach(GTK_TABLE(left_vbox), id3_frame, 2, 4, 0, 1,
	GTK_FILL, GTK_FILL, 0, 5);

    table = gtk_table_new(5, 5, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 5);
    gtk_container_add(GTK_CONTAINER(id3_frame), table);

    label = gtk_label_new("Title:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL,
                     GTK_FILL, 5, 5);

    title_entry = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(table), title_entry, 1, 4, 0, 1,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Artist:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL,
                     GTK_FILL, 5, 5);

    artist_entry = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(table), artist_entry, 1, 4, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Album:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL,
                     GTK_FILL, 5, 5);

    album_entry = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(table), album_entry, 1, 4, 2, 3,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Comment:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL,
                     GTK_FILL, 5, 5);

    comment_entry = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(table), comment_entry, 1, 4, 3, 4,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Year:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5, GTK_FILL,
                     GTK_FILL, 5, 5);

    year_entry = gtk_entry_new();
    gtk_widget_set_usize(year_entry, 40, -1);
    gtk_table_attach(GTK_TABLE(table), year_entry, 1, 2, 4, 5,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Track number:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 4, 5, GTK_FILL,
                     GTK_FILL, 5, 5);

    tracknum_entry = gtk_entry_new();
    gtk_widget_set_usize(tracknum_entry, 40, -1);
    gtk_table_attach(GTK_TABLE(table), tracknum_entry, 3, 4, 4, 5,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    label = gtk_label_new("Genre:");
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6, GTK_FILL,
                     GTK_FILL, 5, 5);

    genre_combo = gtk_combo_new();
    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(genre_combo)->entry),
                           FALSE);
    if (!genre_list) {
        int i = 0;
        const id3_ucs4_t *ucs4 = id3_genre_index(i);
        while (ucs4) {
            genre_list =
                g_list_append(genre_list, id3_ucs4_utf8duplicate(ucs4));
            i++;
            ucs4 = id3_genre_index(i);
        }
    }
    gtk_combo_set_popdown_strings(GTK_COMBO(genre_combo), genre_list);

    gtk_table_attach(GTK_TABLE(table), genre_combo, 1, 4, 5, 6,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

    bbox = gtk_hbutton_box_new();
    gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_SPREAD);

    save = gtk_button_new_from_stock(GTK_STOCK_SAVE);
    g_signal_connect(G_OBJECT(save), "clicked",
                       G_CALLBACK(save_cb), NULL);
    gtk_box_pack_start(GTK_BOX(bbox), save, TRUE, TRUE, 0);
    gtk_widget_grab_default(save);

    remove_id3 = gtk_button_new_from_stock(GTK_STOCK_DELETE);
    g_signal_connect(G_OBJECT(remove_id3), "clicked",
                       G_CALLBACK(remove_id3_cb), save);
    gtk_box_pack_start(GTK_BOX(bbox), remove_id3, TRUE, TRUE, 0);

    g_signal_connect_swapped(G_OBJECT(title_entry), "changed",
                             G_CALLBACK(change_buttons), save);
    g_signal_connect_swapped(G_OBJECT(artist_entry), "changed",
                             G_CALLBACK(change_buttons), save);
    g_signal_connect_swapped(G_OBJECT(album_entry), "changed",
                             G_CALLBACK(change_buttons), save);
    g_signal_connect_swapped(G_OBJECT(year_entry), "changed",
                             G_CALLBACK(change_buttons), save);
    g_signal_connect_swapped(G_OBJECT(comment_entry), "changed",
                             G_CALLBACK(change_buttons), save);
    g_signal_connect_swapped(G_OBJECT(tracknum_entry), "changed",
                             G_CALLBACK(change_buttons), save);
        
    g_signal_connect_swapped(G_OBJECT(GTK_COMBO(genre_combo)->entry), "changed",
                                 G_CALLBACK(change_buttons), save);


    gtk_table_attach(GTK_TABLE(table), bbox, 0, 5, 6, 7, GTK_FILL, 0,
                     0, 8);

    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
    gtk_table_attach(GTK_TABLE(left_vbox), bbox, 0, 4, 1, 2, GTK_FILL,
                         0, 0, 8);

    cancel = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked",
                             G_CALLBACK(gtk_widget_destroy),
                             G_OBJECT(window));
    GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);

    gtk_widget_show_all(window);
}

static void id3_frame_to_entry(char *framename, GtkEntry * entry)
{
    gtk_entry_set_text(entry, "");

    if (info.tag) {
        gchar *text = input_id3_get_string(info.tag, framename);
        if (text) {
            gtk_entry_set_text(entry, text);
            g_free(text);
        }
    }
}
#endif                          /* !NOGUI */

void audmad_get_file_info(char *filename)
{
#ifndef NOGUI
    gchar *title;
    gchar message[128];
    static char const *const layer_str[3] = { "I", "II", "III" };
    static char const *const mode_str[4] = {
        ("single channel"), ("dual channel"), "joint stereo", "stereo"
    };
    char *utf_filename;

#ifdef DEBUG
    g_message("f: audmad_get_file_info: %s\n", filename);
#endif
    input_init(&info, filename);

    if (!strncasecmp("http://", filename, strlen("http://")) ||
	!strncasecmp("https://", filename, strlen("https://"))) {
        info.remote = TRUE;
	return; //tentative
    }

    utf_filename = str_to_utf8(filename);
    create_window();

    input_get_info(&info, info.remote ? TRUE : audmad_config.fast_play_time_calc);

    title = g_strdup_printf("File Info - %s", g_basename(filename));
    gtk_window_set_title(GTK_WINDOW(window), title);
    g_free(title);

    gtk_entry_set_text(GTK_ENTRY(filename_entry), utf_filename);
    gtk_editable_set_position(GTK_EDITABLE(filename_entry), -1);

    free(utf_filename);

    id3_frame_to_entry(ID3_FRAME_ARTIST, GTK_ENTRY(artist_entry));
    id3_frame_to_entry(ID3_FRAME_TITLE, GTK_ENTRY(title_entry));
    id3_frame_to_entry(ID3_FRAME_ALBUM, GTK_ENTRY(album_entry));

// year
// id3_frame_to_entry (ID3_FRAME_YEAR, GTK_ENTRY (year_entry));
// to set year entry, we have to do manually because TYER is still used equally to TDRC.
    gtk_entry_set_text(GTK_ENTRY(year_entry), "");
    if (info.tag) {
        gchar *text = NULL;
        text = input_id3_get_string(info.tag, "TDRC");
        if (!text)
            text = input_id3_get_string(info.tag, "TYER");
        if (text) {
            gtk_entry_set_text(GTK_ENTRY(year_entry), text);
            g_free(text);
        }
    }

    id3_frame_to_entry(ID3_FRAME_TRACK, GTK_ENTRY(tracknum_entry));
    id3_frame_to_entry(ID3_FRAME_COMMENT, GTK_ENTRY(comment_entry));
    snprintf(message, 127, "Layer %s", layer_str[info.mpeg_layer - 1]);
    gtk_label_set_text(GTK_LABEL(mpeg_level), message);
    if (info.vbr) {
        snprintf(message, 127, "VBR (avg. %d kbps)", info.bitrate / 1000);
    }
    else {
        snprintf(message, 127, "%d kbps", info.bitrate / 1000);
    }
    gtk_label_set_text(GTK_LABEL(mpeg_bitrate), message);
    snprintf(message, 127, "%d Hz", info.freq);
    gtk_label_set_text(GTK_LABEL(mpeg_samplerate), message);
    if (info.frames != -1) {
        snprintf(message, 127, "%d frames", info.frames);
        gtk_label_set_text(GTK_LABEL(mpeg_frames), message);
    }
    else {
        gtk_label_set_text(GTK_LABEL(mpeg_frames), "");
    }
    gtk_label_set_text(GTK_LABEL(mpeg_flags), mode_str[info.mode]);
    snprintf(message, 127, "%ld  seconds",
             mad_timer_count(info.duration, MAD_UNITS_SECONDS));
    gtk_label_set_text(GTK_LABEL(mpeg_duration), message);

    if (info.replaygain_album_str != NULL) {
        snprintf(message, 127, "RG_album=%4s (x%4.2f)",
                 info.replaygain_album_str, info.replaygain_album_scale);
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain), "");

    if (info.replaygain_track_str != NULL) {
        snprintf(message, 127, "RG_track=%4s (x%4.2f)",
                 info.replaygain_track_str, info.replaygain_track_scale);
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain2), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain2), "");

    if (info.replaygain_album_peak_str != NULL) {
        snprintf(message, 127, "Peak album=%4s (%+5.3fdBFS)",
                 info.replaygain_album_peak_str,
                 20 * log10(info.replaygain_album_peak));
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain3), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain3), "");

    if (info.replaygain_track_peak_str != NULL) {
        snprintf(message, 127, "Peak track=%4s (%+5.3fdBFS)",
                 info.replaygain_track_peak_str,
                 20 * log10(info.replaygain_track_peak));
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain4), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mpeg_replaygain3), "");

    if (info.mp3gain_undo_str != NULL) {
        snprintf(message, 127, "mp3gain undo=%4s (%+5.3fdB)",
                 info.mp3gain_undo_str, info.mp3gain_undo);
        gtk_label_set_text(GTK_LABEL(mp3gain1), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mp3gain1), "");

    if (info.mp3gain_minmax_str != NULL) {
        snprintf(message, 127, "mp3gain minmax=%4s (max-min=%+6.3fdB)",
                 info.mp3gain_minmax_str, info.mp3gain_minmax);
        gtk_label_set_text(GTK_LABEL(mp3gain2), message);
    }
    else
        gtk_label_set_text(GTK_LABEL(mp3gain2), "");

    gtk_label_set_text(GTK_LABEL(mpeg_fileinfo), "");


    /* work out the index of the genre in the list */
    {
        const id3_ucs4_t *string;
        struct id3_frame *frame;
        union id3_field *field;
        frame = id3_tag_findframe(info.tag, ID3_FRAME_GENRE, 0);
        if (frame) {
            field = id3_frame_field(frame, 1);
            string = id3_field_getstrings(field, 0);
            string = id3_genre_name(string);
            if (string)
                gtk_list_select_item(GTK_LIST
                                     (GTK_COMBO(genre_combo)->list),
                                     id3_genre_number(string));
        }
    }

    gtk_widget_set_sensitive(id3_frame, TRUE);

#endif                          /* !NOGUI */
}
