/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2009  Audacious development team.
 *
 *  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include <inttypes.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/debug.h>
#include <audacious/drct.h>
#include <audacious/gtk-compat.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>

#include "actions-mainwin.h"
#include "config.h"
#include "dnd.h"
#include "skins_cfg.h"
#include "ui_dock.h"
#include "ui_manager.h"
#include "ui_playlist.h"
#include "ui_skinned_button.h"
#include "ui_skinned_playlist.h"
#include "ui_skinned_playlist_slider.h"
#include "ui_skinned_textbox.h"
#include "ui_skinned_window.h"
#include "util.h"

#include "images/audacious_playlist.xpm"

gint active_playlist;
gchar * active_title;
glong active_length;
GtkWidget * playlistwin, * playlistwin_list;

static GMutex *resize_mutex = NULL;

static GtkWidget *playlistwin_shade, *playlistwin_close;
static GtkWidget *playlistwin_shaded_shade, *playlistwin_shaded_close;

static GtkWidget *playlistwin_slider;
static GtkWidget *playlistwin_time_min, *playlistwin_time_sec;
static GtkWidget *playlistwin_info, *playlistwin_sinfo;
static GtkWidget *playlistwin_srew, *playlistwin_splay;
static GtkWidget *playlistwin_spause, *playlistwin_sstop;
static GtkWidget *playlistwin_sfwd, *playlistwin_seject;
static GtkWidget *playlistwin_sscroll_up, *playlistwin_sscroll_down;

static void playlistwin_select_search_cbt_cb(GtkWidget *called_cbt,
                                             gpointer other_cbt);
static gboolean playlistwin_select_search_kp_cb(GtkWidget *entry,
                                                GdkEventKey *event,
                                                gpointer searchdlg_win);

static gboolean playlistwin_resizing = FALSE;
static gint playlistwin_resize_x, playlistwin_resize_y;
static int drop_position;
static gboolean song_changed;

gboolean
playlistwin_is_shaded(void)
{
    return config.playlist_shaded;
}

gint
playlistwin_get_width(void)
{
    config.playlist_width /= PLAYLISTWIN_WIDTH_SNAP;
    config.playlist_width *= PLAYLISTWIN_WIDTH_SNAP;
    return config.playlist_width;
}

gint
playlistwin_get_height_unshaded(void)
{
    config.playlist_height /= PLAYLISTWIN_HEIGHT_SNAP;
    config.playlist_height *= PLAYLISTWIN_HEIGHT_SNAP;
    return config.playlist_height;
}

gint
playlistwin_get_height_shaded(void)
{
    return PLAYLISTWIN_SHADED_HEIGHT;
}

gint
playlistwin_get_height(void)
{
    if (playlistwin_is_shaded())
        return playlistwin_get_height_shaded();
    else
        return playlistwin_get_height_unshaded();
}

static void playlistwin_update_info (void)
{
    gchar *text, *sel_text, *tot_text;
    gint64 selection, total;

    total = aud_playlist_get_total_length (active_playlist, TRUE) / 1000;
    selection = aud_playlist_get_selected_length (active_playlist, TRUE) / 1000;

    if (selection >= 3600)
        sel_text = g_strdup_printf ("%" PRId64 ":%02" PRId64 ":%02" PRId64,
        selection / 3600, selection / 60 % 60, selection % 60);
    else
        sel_text = g_strdup_printf ("%" PRId64 ":%02" PRId64,
        selection / 60, selection % 60);

    if (total >= 3600)
        tot_text = g_strdup_printf ("%" PRId64 ":%02" PRId64 ":%02" PRId64,
        total / 3600, total / 60 % 60, total % 60);
    else
        tot_text = g_strdup_printf ("%" PRId64 ":%02" PRId64,
        total / 60, total % 60);

    text = g_strconcat(sel_text, "/", tot_text, NULL);
    textbox_set_text (playlistwin_info, text);
    g_free(text);
    g_free(tot_text);
    g_free(sel_text);
}

static void update_rollup_text (void)
{
    gint playlist = aud_playlist_get_active ();
    gint entry = aud_playlist_get_position (playlist);
    gchar scratch[512];

    scratch[0] = 0;

    if (entry > -1)
    {
        gint length = aud_playlist_entry_get_length (playlist, entry, FALSE);

        if (aud_cfg->show_numbers_in_pl)
            snprintf (scratch, sizeof scratch, "%d. ", 1 + entry);

        gchar * title = aud_playlist_entry_get_title (playlist, entry, FALSE);
        snprintf (scratch + strlen (scratch), sizeof scratch - strlen (scratch),
         "%s", title);
        g_free (title);

        if (length > 0)
            snprintf (scratch + strlen (scratch), sizeof scratch - strlen
             (scratch), " (%d:%02d)", length / 60000, length / 1000 % 60);
    }

    textbox_set_text (playlistwin_sinfo, scratch);
}

static void real_update (void)
{
    ui_skinned_playlist_update (playlistwin_list);
    playlistwin_update_info ();
    update_rollup_text ();
}

void playlistwin_update (void)
{
    if (! aud_playlist_update_pending ())
        real_update ();
}

static void
playlistwin_set_geometry_hints(gboolean shaded)
{
    GdkGeometry geometry;

    geometry.min_width = PLAYLISTWIN_MIN_WIDTH;
    geometry.width_inc = PLAYLISTWIN_WIDTH_SNAP;
    geometry.max_width = 65535;

    if (shaded)
    {
        geometry.min_height = PLAYLISTWIN_SHADED_HEIGHT;
        geometry.height_inc = 0;
        geometry.max_height = PLAYLISTWIN_SHADED_HEIGHT;
    }
    else
    {
        geometry.min_height = PLAYLISTWIN_MIN_HEIGHT;
        geometry.height_inc = PLAYLISTWIN_HEIGHT_SNAP;
        geometry.max_height = 65535;
    }

    gtk_window_set_geometry_hints ((GtkWindow *) playlistwin, NULL, & geometry,
     GDK_HINT_MIN_SIZE | GDK_HINT_RESIZE_INC | GDK_HINT_MAX_SIZE);
}

void
playlistwin_set_sinfo_font(gchar *font)
{
    gchar *tmp = NULL, *tmp2 = NULL, *tmp3 = NULL;

    g_return_if_fail(font);
    AUDDBG("Attempt to set font \"%s\"\n", font);

    tmp = g_strdup(font);
    g_return_if_fail(tmp);

    tmp3 = strrchr(tmp, ' ');
    if (tmp3 != NULL)
        * tmp3 = '\0';

    tmp2 = g_strdup_printf("%s 5", tmp);
    g_return_if_fail(tmp2);

    textbox_set_font (playlistwin_sinfo, config.mainwin_use_bitmapfont ? NULL :
     tmp2);

    g_free(tmp);
    g_free(tmp2);
}

void
playlistwin_set_sinfo_scroll(gboolean scroll)
{
    if(playlistwin_is_shaded())
        textbox_set_scroll (playlistwin_sinfo, config.autoscroll);
    else
        textbox_set_scroll (playlistwin_sinfo, FALSE);
}

static void
playlistwin_set_shade_menu(gboolean shaded)
{
    GtkAction *action = gtk_action_group_get_action(
      toggleaction_group_others , "roll up playlist editor" );
    gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(action) , shaded );
    playlistwin_update ();
}

void
playlistwin_shade_toggle(void)
{
    playlistwin_set_shade_menu(!config.playlist_shaded);
}

static gboolean
playlistwin_release(GtkWidget * widget,
                    GdkEventButton * event,
                    gpointer callback_data)
{
    playlistwin_resizing = FALSE;
    return FALSE;
}

static void playlistwin_scroll (gboolean up)
{
    gint rows, first, focused;

    ui_skinned_playlist_row_info (playlistwin_list, & rows, & first, & focused);
    ui_skinned_playlist_scroll_to (playlistwin_list, first + (up ? -1 : 1) *
     rows / 3);
}

static void playlistwin_scroll_up_pushed (void)
{
    playlistwin_scroll (TRUE);
}

static void playlistwin_scroll_down_pushed (void)
{
    playlistwin_scroll (FALSE);
}

static void
playlistwin_select_all(void)
{
    aud_playlist_select_all (active_playlist, 1);
}

static void
playlistwin_select_none(void)
{
    aud_playlist_select_all (active_playlist, 0);
}

static void copy_selected_to_new (gint playlist)
{
    gint entries = aud_playlist_entry_count (playlist);
    gint new = aud_playlist_count ();
    struct index * copy = index_new ();
    gint entry;

    aud_playlist_insert (new);

    for (entry = 0; entry < entries; entry ++)
    {
        if (aud_playlist_entry_get_selected (playlist, entry))
            index_append (copy, aud_playlist_entry_get_filename (playlist, entry));
    }

    aud_playlist_entry_insert_batch (new, 0, copy, NULL, FALSE);
    aud_playlist_set_active (new);
}

static void
playlistwin_select_search(void)
{
    GtkWidget *searchdlg_win, *searchdlg_table;
    GtkWidget *searchdlg_hbox, *searchdlg_logo, *searchdlg_helptext;
    GtkWidget *searchdlg_entry_title, *searchdlg_label_title;
    GtkWidget *searchdlg_entry_album, *searchdlg_label_album;
    GtkWidget *searchdlg_entry_file_name, *searchdlg_label_file_name;
    GtkWidget *searchdlg_entry_performer, *searchdlg_label_performer;
    GtkWidget *searchdlg_checkbt_clearprevsel;
    GtkWidget *searchdlg_checkbt_newplaylist;
    GtkWidget *searchdlg_checkbt_autoenqueue;
    gint result;

    /* create dialog */
    searchdlg_win = gtk_dialog_new_with_buttons(
      _("Search entries in active playlist") , GTK_WINDOW(mainwin) ,
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT ,
      GTK_STOCK_CANCEL , GTK_RESPONSE_REJECT , GTK_STOCK_OK , GTK_RESPONSE_ACCEPT , NULL );
    gtk_window_set_position(GTK_WINDOW(searchdlg_win), GTK_WIN_POS_CENTER);

    /* help text and logo */
    searchdlg_hbox = gtk_hbox_new( FALSE , 4 );
    searchdlg_logo = gtk_image_new_from_stock( GTK_STOCK_FIND , GTK_ICON_SIZE_DIALOG );
    searchdlg_helptext = gtk_label_new( _("Select entries in playlist by filling one or more "
      "fields. Fields use regular expressions syntax, case-insensitive. If you don't know how "
      "regular expressions work, simply insert a literal portion of what you're searching for.") );
    gtk_label_set_line_wrap( GTK_LABEL(searchdlg_helptext) , TRUE );
    gtk_box_pack_start( GTK_BOX(searchdlg_hbox) , searchdlg_logo , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(searchdlg_hbox) , searchdlg_helptext , FALSE , FALSE , 0 );

    /* title */
    searchdlg_label_title = gtk_label_new( _("Title: ") );
    searchdlg_entry_title = gtk_entry_new();
    gtk_misc_set_alignment( GTK_MISC(searchdlg_label_title) , 0 , 0.5 );
    g_signal_connect( G_OBJECT(searchdlg_entry_title) , "key-press-event" ,
      G_CALLBACK(playlistwin_select_search_kp_cb) , searchdlg_win );

    /* album */
    searchdlg_label_album= gtk_label_new( _("Album: ") );
    searchdlg_entry_album= gtk_entry_new();
    gtk_misc_set_alignment( GTK_MISC(searchdlg_label_album) , 0 , 0.5 );
    g_signal_connect( G_OBJECT(searchdlg_entry_album) , "key-press-event" ,
      G_CALLBACK(playlistwin_select_search_kp_cb) , searchdlg_win );

    /* artist */
    searchdlg_label_performer = gtk_label_new( _("Artist: ") );
    searchdlg_entry_performer = gtk_entry_new();
    gtk_misc_set_alignment( GTK_MISC(searchdlg_label_performer) , 0 , 0.5 );
    g_signal_connect( G_OBJECT(searchdlg_entry_performer) , "key-press-event" ,
      G_CALLBACK(playlistwin_select_search_kp_cb) , searchdlg_win );

    /* file name */
    searchdlg_label_file_name = gtk_label_new( _("Filename: ") );
    searchdlg_entry_file_name = gtk_entry_new();
    gtk_misc_set_alignment( GTK_MISC(searchdlg_label_file_name) , 0 , 0.5 );
    g_signal_connect( G_OBJECT(searchdlg_entry_file_name) , "key-press-event" ,
      G_CALLBACK(playlistwin_select_search_kp_cb) , searchdlg_win );

    /* some options that control behaviour */
    searchdlg_checkbt_clearprevsel = gtk_check_button_new_with_label(
      _("Clear previous selection before searching") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(searchdlg_checkbt_clearprevsel) , TRUE );
    searchdlg_checkbt_autoenqueue = gtk_check_button_new_with_label(
      _("Automatically toggle queue for matching entries") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(searchdlg_checkbt_autoenqueue) , FALSE );
    searchdlg_checkbt_newplaylist = gtk_check_button_new_with_label(
      _("Create a new playlist with matching entries") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(searchdlg_checkbt_newplaylist) , FALSE );
    g_signal_connect( G_OBJECT(searchdlg_checkbt_autoenqueue) , "clicked" ,
      G_CALLBACK(playlistwin_select_search_cbt_cb) , searchdlg_checkbt_newplaylist );
    g_signal_connect( G_OBJECT(searchdlg_checkbt_newplaylist) , "clicked" ,
      G_CALLBACK(playlistwin_select_search_cbt_cb) , searchdlg_checkbt_autoenqueue );

    /* place fields in searchdlg_table */
    searchdlg_table = gtk_table_new( 8 , 2 , FALSE );
    gtk_table_set_row_spacing( GTK_TABLE(searchdlg_table) , 0 , 8 );
    gtk_table_set_row_spacing( GTK_TABLE(searchdlg_table) , 4 , 8 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_hbox ,
      0 , 2 , 0 , 1 , GTK_FILL | GTK_EXPAND , GTK_FILL | GTK_EXPAND , 0 , 2 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_label_title ,
      0 , 1 , 1 , 2 , GTK_FILL , GTK_FILL | GTK_EXPAND , 0 , 2 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_entry_title ,
      1 , 2 , 1 , 2 , GTK_FILL | GTK_EXPAND , GTK_FILL | GTK_EXPAND , 0 , 2 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_label_album,
      0 , 1 , 2 , 3 , GTK_FILL , GTK_FILL | GTK_EXPAND , 0 , 2 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_entry_album,
      1 , 2 , 2 , 3 , GTK_FILL | GTK_EXPAND , GTK_FILL | GTK_EXPAND , 0 , 2 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_label_performer ,
      0 , 1 , 3 , 4 , GTK_FILL , GTK_FILL | GTK_EXPAND , 0 , 2 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_entry_performer ,
      1 , 2 , 3 , 4 , GTK_FILL | GTK_EXPAND , GTK_FILL | GTK_EXPAND , 0 , 2 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_label_file_name ,
      0 , 1 , 4 , 5 , GTK_FILL , GTK_FILL | GTK_EXPAND , 0 , 2 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_entry_file_name ,
      1 , 2 , 4 , 5 , GTK_FILL | GTK_EXPAND , GTK_FILL | GTK_EXPAND , 0 , 2 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_checkbt_clearprevsel ,
      0 , 2 , 5 , 6 , GTK_FILL | GTK_EXPAND , GTK_FILL | GTK_EXPAND , 0 , 1 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_checkbt_autoenqueue ,
      0 , 2 , 6 , 7 , GTK_FILL | GTK_EXPAND , GTK_FILL | GTK_EXPAND , 0 , 1 );
    gtk_table_attach( GTK_TABLE(searchdlg_table) , searchdlg_checkbt_newplaylist ,
      0 , 2 , 7 , 8 , GTK_FILL | GTK_EXPAND , GTK_FILL | GTK_EXPAND , 0 , 1 );

    gtk_container_set_border_width( GTK_CONTAINER(searchdlg_table) , 5 );
    gtk_container_add ((GtkContainer *) gtk_dialog_get_content_area
     ((GtkDialog *) searchdlg_win), searchdlg_table);
    gtk_widget_show_all( searchdlg_win );
    result = gtk_dialog_run( GTK_DIALOG(searchdlg_win) );

    switch(result)
    {
      case GTK_RESPONSE_ACCEPT:
      {
         /* create a TitleInput tuple with user search data */
         Tuple *tuple = tuple_new();
         gchar *searchdata = NULL;

         searchdata = (gchar*)gtk_entry_get_text( GTK_ENTRY(searchdlg_entry_title) );
         AUDDBG("title=\"%s\"\n", searchdata);
         tuple_associate_string(tuple, FIELD_TITLE, NULL, searchdata);

         searchdata = (gchar*)gtk_entry_get_text( GTK_ENTRY(searchdlg_entry_album) );
         AUDDBG("album=\"%s\"\n", searchdata);
         tuple_associate_string(tuple, FIELD_ALBUM, NULL, searchdata);

         searchdata = (gchar*)gtk_entry_get_text( GTK_ENTRY(searchdlg_entry_performer) );
         AUDDBG("performer=\"%s\"\n", searchdata);
         tuple_associate_string(tuple, FIELD_ARTIST, NULL, searchdata);

         searchdata = (gchar*)gtk_entry_get_text( GTK_ENTRY(searchdlg_entry_file_name) );
         AUDDBG("filename=\"%s\"\n", searchdata);
         tuple_associate_string(tuple, FIELD_FILE_NAME, NULL, searchdata);

         /* check if previous selection should be cleared before searching */
         if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(searchdlg_checkbt_clearprevsel)) == TRUE )
             playlistwin_select_none();
         /* now send this tuple to the real search function */
         aud_playlist_select_by_patterns (active_playlist, tuple);

         /* we do not need the tuple and its data anymore */
         mowgli_object_unref(tuple);
         /* check if a new playlist should be created after searching */
         if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(searchdlg_checkbt_newplaylist)) == TRUE )
             copy_selected_to_new (active_playlist);
         else
         {
             /* set focus on the first entry found */
             gint entries = aud_playlist_entry_count (active_playlist);
             gint count;

             for (count = 0; count < entries; count ++)
             {
                 if (aud_playlist_entry_get_selected (active_playlist, count))
                 {
                     ui_skinned_playlist_set_focused (playlistwin_list, count);
                     break;
                 }
             }

             /* check if matched entries should be queued */
             if (gtk_toggle_button_get_active ((GtkToggleButton *)
              searchdlg_checkbt_autoenqueue))
                 aud_playlist_queue_insert_selected (active_playlist, -1);
         }

         playlistwin_update ();
         break;
      }
      default:
         break;
    }
    /* done here :) */
    gtk_widget_destroy( searchdlg_win );
}

static void playlistwin_inverse_selection (void)
{
    gint entries = aud_playlist_entry_count (active_playlist);
    gint entry;

    for (entry = 0; entry < entries; entry ++)
        aud_playlist_entry_set_selected (active_playlist, entry,
         ! aud_playlist_entry_get_selected (active_playlist, entry));
}

static void
playlistwin_resize(gint width, gint height)
{
    gint tx, ty;

    g_return_if_fail(width > 0 && height > 0);

    tx = (width - PLAYLISTWIN_MIN_WIDTH) / PLAYLISTWIN_WIDTH_SNAP;
    tx = (tx * PLAYLISTWIN_WIDTH_SNAP) + PLAYLISTWIN_MIN_WIDTH;
    if (tx < PLAYLISTWIN_MIN_WIDTH)
        tx = PLAYLISTWIN_MIN_WIDTH;

    if (!config.playlist_shaded)
    {
        ty = (height - PLAYLISTWIN_MIN_HEIGHT) / PLAYLISTWIN_HEIGHT_SNAP;
        ty = (ty * PLAYLISTWIN_HEIGHT_SNAP) + PLAYLISTWIN_MIN_HEIGHT;
        if (ty < PLAYLISTWIN_MIN_HEIGHT)
            ty = PLAYLISTWIN_MIN_HEIGHT;
    }
    else
        ty = config.playlist_height;

    if (tx == config.playlist_width && ty == config.playlist_height)
        return;

    config.playlist_width = width = tx;
    config.playlist_height = height = ty;

    g_mutex_lock(resize_mutex);

    ui_skinned_playlist_resize (playlistwin_list, config.playlist_width - 31,
     config.playlist_height - 58);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal,
     playlistwin_slider, config.playlist_width - 15, 20);
    ui_skinned_playlist_slider_resize (playlistwin_slider,
     config.playlist_height - 58);

    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->shaded, playlistwin_shaded_shade, config.playlist_width - 21, 3);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->shaded, playlistwin_shaded_close, config.playlist_width - 11, 3);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_shade, config.playlist_width - 21, 3);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_close, config.playlist_width - 11, 3);

    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_time_min, config.playlist_width - 82, config.playlist_height - 15);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_time_sec, config.playlist_width - 64, config.playlist_height - 15);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_info, config.playlist_width - 143, config.playlist_height - 28);

    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_srew, config.playlist_width - 144, config.playlist_height - 16);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_splay, config.playlist_width - 138, config.playlist_height - 16);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_spause, config.playlist_width - 128, config.playlist_height - 16);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_sstop, config.playlist_width - 118, config.playlist_height - 16);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_sfwd, config.playlist_width - 109, config.playlist_height - 16);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_seject, config.playlist_width - 100, config.playlist_height - 16);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_sscroll_up, config.playlist_width - 14, config.playlist_height - 35);
    gtk_fixed_move ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_sscroll_down, config.playlist_width - 14, config.playlist_height - 30);

    textbox_set_width (playlistwin_sinfo, config.playlist_width - 35);

    g_mutex_unlock(resize_mutex);
}

static void
playlistwin_motion(GtkWidget * widget,
                   GdkEventMotion * event,
                   gpointer callback_data)
{
    /*
     * GDK2's resize is broken and doesn't really play nice, so we have
     * to do all of this stuff by hand.
     */
    if (playlistwin_resizing == TRUE)
    {
        if (event->x + playlistwin_resize_x != playlistwin_get_width() ||
            event->y + playlistwin_resize_y != playlistwin_get_height())
        {
            playlistwin_resize(event->x + playlistwin_resize_x,
                               event->y + playlistwin_resize_y);
            resize_window(playlistwin, config.playlist_width,
             playlistwin_get_height());
        }
    }
    else if (dock_is_moving(GTK_WINDOW(playlistwin)))
        dock_move_motion(GTK_WINDOW(playlistwin), event);
}

static void
playlistwin_fileinfo(void)
{
    gint rows, first, focused;

    ui_skinned_playlist_row_info (playlistwin_list, & rows, & first, & focused);
    audgui_infowin_show (active_playlist, focused);
}

#define REGION_L(x1,x2,y1,y2)                   \
    (event->x >= (x1) && event->x < (x2) &&     \
     event->y >= config.playlist_height - (y1) &&  \
     event->y < config.playlist_height - (y2))

#define REGION_R(x1,x2,y1,y2)                      \
    (event->x >= playlistwin_get_width() - (x1) && \
     event->x < playlistwin_get_width() - (x2) &&  \
     event->y >= config.playlist_height - (y1) &&     \
     event->y < config.playlist_height - (y2))

static void
playlistwin_scrolled(GtkWidget * widget,
                     GdkEventScroll * event,
                     gpointer callback_data)
{
    switch (event->direction)
    {
    case GDK_SCROLL_DOWN:
        playlistwin_scroll (FALSE);
        break;
    case GDK_SCROLL_UP:
        playlistwin_scroll (TRUE);
        break;
    default:
        break;
    }
}

static gboolean
playlistwin_press(GtkWidget * widget,
                  GdkEventButton * event,
                  gpointer callback_data)
{
    gint xpos, ypos;

    gtk_window_get_position(GTK_WINDOW(playlistwin), &xpos, &ypos);

    if (event->button == 1 && !config.show_wm_decorations &&
        ((!config.playlist_shaded &&
          event->x > playlistwin_get_width() - 20 &&
          event->y > config.playlist_height - 20) ||
         (config.playlist_shaded &&
          event->x >= playlistwin_get_width() - 31 &&
          event->x < playlistwin_get_width() - 22))) {

        if (event->type != GDK_2BUTTON_PRESS &&
            event->type != GDK_3BUTTON_PRESS) {
            playlistwin_resizing = TRUE;
            playlistwin_resize_x = config.playlist_width - event->x;
            playlistwin_resize_y = config.playlist_height - event->y;
        }
    }
    else if (event->button == 1 && REGION_L(12, 37, 29, 11))
        /* ADD button menu */
        ui_popup_menu_show(UI_MENU_PLAYLIST_ADD, xpos + 12, ypos +
         playlistwin_get_height() - 8, FALSE, TRUE, event->button, event->time);
    else if (event->button == 1 && REGION_L(41, 66, 29, 11))
        /* SUB button menu */
        ui_popup_menu_show(UI_MENU_PLAYLIST_REMOVE, xpos + 40, ypos +
         playlistwin_get_height() - 8, FALSE, TRUE, event->button, event->time);
    else if (event->button == 1 && REGION_L(70, 95, 29, 11))
        /* SEL button menu */
        ui_popup_menu_show(UI_MENU_PLAYLIST_SELECT, xpos + 68, ypos +
         playlistwin_get_height() - 8, FALSE, TRUE, event->button, event->time);
    else if (event->button == 1 && REGION_L(99, 124, 29, 11))
        /* MISC button menu */
        ui_popup_menu_show(UI_MENU_PLAYLIST_SORT, xpos + 100, ypos +
         playlistwin_get_height() - 8, FALSE, TRUE, event->button, event->time);
    else if (event->button == 1 && REGION_R(46, 23, 29, 11))
        /* LIST button menu */
        ui_popup_menu_show(UI_MENU_PLAYLIST_GENERAL, xpos +
         playlistwin_get_width() - 12, ypos + playlistwin_get_height() - 8,
         TRUE, TRUE, event->button, event->time);
    else if (event->button == 1 && event->type == GDK_BUTTON_PRESS &&
             (config.easy_move || event->y < 14))
    {
        return FALSE;
    }
    else if (event->button == 1 && event->type == GDK_2BUTTON_PRESS
             && event->y < 14) {
        /* double click on title bar */
        playlistwin_shade_toggle();
        if (dock_is_moving(GTK_WINDOW(playlistwin)))
            dock_move_release(GTK_WINDOW(playlistwin));
        return TRUE;
    }
    else if (event->button == 3)
        ui_popup_menu_show(UI_MENU_PLAYLIST, event->x_root, event->y_root,
         FALSE, FALSE, 3, event->time);

    return TRUE;
}

static gboolean playlistwin_delete(GtkWidget *widget, void *data)
{
    if (config.show_wm_decorations)
        playlistwin_show(FALSE);
    else
        aud_drct_quit();

    return TRUE;
}

void
playlistwin_hide_timer(void)
{
    textbox_set_text (playlistwin_time_min, "   ");
    textbox_set_text (playlistwin_time_sec, "  ");
}

void playlistwin_set_time (const gchar * minutes, const gchar * seconds)
{
    textbox_set_text (playlistwin_time_min, minutes);
    textbox_set_text (playlistwin_time_sec, seconds);
}

static void drag_motion (GtkWidget * widget, GdkDragContext * context, gint x,
 gint y, guint time, void * unused)
{
    if (! config.playlist_shaded)
        ui_skinned_playlist_hover (playlistwin_list, x - 12, y - 20);
}

static void drag_leave (GtkWidget * widget, GdkDragContext * context, guint time,
 void * unused)
{
    if (! config.playlist_shaded)
        ui_skinned_playlist_hover_end (playlistwin_list);
}

static void drag_drop (GtkWidget * widget, GdkDragContext * context, gint x,
 gint y, guint time, void * unused)
{
    if (config.playlist_shaded)
        drop_position = -1;
    else
    {
        ui_skinned_playlist_hover (playlistwin_list, x - 12, y - 20);
        drop_position = ui_skinned_playlist_hover_end (playlistwin_list);
    }
}

static void drag_data_received (GtkWidget * widget, GdkDragContext * context,
 gint x, gint y, GtkSelectionData * data, guint info, guint time, void * unused)
{
    audgui_urilist_insert (active_playlist, drop_position, (const gchar *)
     gtk_selection_data_get_data (data));
    drop_position = -1;
}

static void playlistwin_hide (void)
{
    playlistwin_show (0);
}

static void
playlistwin_create_widgets(void)
{
    /* This function creates the custom widgets used by the playlist editor */

    playlistwin_sinfo = textbox_new (config.playlist_width - 35);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->shaded, playlistwin_sinfo, 4, 4);
    playlistwin_set_sinfo_font (config.playlist_font);
    playlistwin_set_sinfo_scroll (config.autoscroll);

    playlistwin_shaded_shade = button_new (9, 9, 128, 45, 150, 42, SKIN_PLEDIT, SKIN_PLEDIT);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->shaded, playlistwin_shaded_shade, config.playlist_width - 21, 3);
    button_on_release (playlistwin_shaded_shade, (ButtonCB) playlistwin_shade_toggle);

    playlistwin_shaded_close = button_new (9, 9, 138, 45, 52, 42, SKIN_PLEDIT, SKIN_PLEDIT);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->shaded, playlistwin_shaded_close, config.playlist_width - 11, 3);
    button_on_release (playlistwin_shaded_close, (ButtonCB) playlistwin_hide);

    playlistwin_shade = button_new (9, 9, 157, 3, 62, 42, SKIN_PLEDIT, SKIN_PLEDIT);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_shade, config.playlist_width - 21, 3);
    button_on_release (playlistwin_shade, (ButtonCB) playlistwin_shade_toggle);

    playlistwin_close = button_new (9, 9, 167, 3, 52, 42, SKIN_PLEDIT, SKIN_PLEDIT);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_close, config.playlist_width - 11, 3);
    button_on_release (playlistwin_close, (ButtonCB) playlistwin_hide);

    playlistwin_list = ui_skinned_playlist_new (config.playlist_width - 31, config.playlist_height - 58, config.playlist_font);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_list, 12, 20);

    /* playlist list box slider */
    playlistwin_slider = ui_skinned_playlist_slider_new (playlistwin_list, config.playlist_height - 58);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_slider, config.playlist_width - 15, 20);
    ui_skinned_playlist_set_slider (playlistwin_list, playlistwin_slider);

    playlistwin_time_min = textbox_new (15);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_time_min, config.playlist_width - 82, config.playlist_height - 15);

    playlistwin_time_sec = textbox_new (10);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_time_sec, config.playlist_width - 64, config.playlist_height - 15);

    g_signal_connect(playlistwin_time_min, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);
    g_signal_connect(playlistwin_time_sec, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);

    playlistwin_info = textbox_new (90);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_info, config.playlist_width - 143, config.playlist_height - 28);

    /* mini play control buttons at right bottom corner */

    playlistwin_srew = button_new_small (8, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_srew, config.playlist_width - 144, config.playlist_height - 16);
    button_on_release (playlistwin_srew, (ButtonCB) aud_drct_pl_prev);

    playlistwin_splay = button_new_small (10, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_splay, config.playlist_width - 138, config.playlist_height - 16);
    button_on_release (playlistwin_splay, (ButtonCB) mainwin_play_pushed);

    playlistwin_spause = button_new_small (10, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_spause, config.playlist_width - 128, config.playlist_height - 16);
    button_on_release (playlistwin_spause, (ButtonCB) aud_drct_pause);

    playlistwin_sstop = button_new_small (9, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_sstop, config.playlist_width - 118, config.playlist_height - 16);
    button_on_release (playlistwin_sstop, (ButtonCB) aud_drct_stop);

    playlistwin_sfwd = button_new_small (8, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_sfwd, config.playlist_width - 109, config.playlist_height - 16);
    button_on_release (playlistwin_sfwd, (ButtonCB) aud_drct_pl_next);

    playlistwin_seject = button_new_small (9, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_seject, config.playlist_width - 100, config.playlist_height - 16);
    button_on_release (playlistwin_seject, (ButtonCB) action_play_file);

    playlistwin_sscroll_up = button_new_small (8, 5);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_sscroll_up, config.playlist_width - 14, config.playlist_height - 35);
    button_on_release (playlistwin_sscroll_up, (ButtonCB) playlistwin_scroll_up_pushed);

    playlistwin_sscroll_down = button_new_small (8, 5);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) playlistwin)->normal, playlistwin_sscroll_down, config.playlist_width - 14, config.playlist_height - 30);
    button_on_release (playlistwin_sscroll_down, (ButtonCB) playlistwin_scroll_down_pushed);
}

static void size_allocate (GtkWidget * widget, GtkAllocation * allocation)
{
    playlistwin_resize (allocation->width, allocation->height);
}

static void
playlistwin_create_window(void)
{
    GdkPixbuf *icon;

    playlistwin = ui_skinned_window_new("playlist", &config.playlist_x, &config.playlist_y);
    gtk_window_set_title(GTK_WINDOW(playlistwin), _("Audacious Playlist Editor"));
    gtk_window_set_role(GTK_WINDOW(playlistwin), "playlist");
    gtk_window_set_default_size(GTK_WINDOW(playlistwin),
                                playlistwin_get_width(),
                                playlistwin_get_height());
    gtk_window_set_resizable(GTK_WINDOW(playlistwin), TRUE);
    playlistwin_set_geometry_hints(config.playlist_shaded);

    gtk_window_set_transient_for(GTK_WINDOW(playlistwin),
                                 GTK_WINDOW(mainwin));
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(playlistwin), TRUE);

    icon = gdk_pixbuf_new_from_xpm_data((const gchar **) audacious_playlist_icon);
    gtk_window_set_icon(GTK_WINDOW(playlistwin), icon);
    g_object_unref(icon);

    gtk_widget_add_events(playlistwin, GDK_POINTER_MOTION_MASK |
                          GDK_FOCUS_CHANGE_MASK | GDK_BUTTON_MOTION_MASK |
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_SCROLL_MASK | GDK_VISIBILITY_NOTIFY_MASK);

    g_signal_connect(playlistwin, "delete_event",
                     G_CALLBACK(playlistwin_delete), NULL);
    g_signal_connect(playlistwin, "button_press_event",
                     G_CALLBACK(playlistwin_press), NULL);
    g_signal_connect(playlistwin, "button_release_event",
                     G_CALLBACK(playlistwin_release), NULL);
    g_signal_connect(playlistwin, "scroll_event",
                     G_CALLBACK(playlistwin_scrolled), NULL);
    g_signal_connect(playlistwin, "motion_notify_event",
                     G_CALLBACK(playlistwin_motion), NULL);

    aud_drag_dest_set(playlistwin);

    drop_position = -1;
    g_signal_connect ((GObject *) playlistwin, "drag-motion", (GCallback)
     drag_motion, 0);
    g_signal_connect ((GObject *) playlistwin, "drag-leave", (GCallback)
     drag_leave, 0);
    g_signal_connect ((GObject *) playlistwin, "drag-drop", (GCallback)
     drag_drop, 0);
    g_signal_connect ((GObject *) playlistwin, "drag-data-received", (GCallback)
     drag_data_received, 0);

    g_signal_connect ((GObject *) playlistwin, "key-press-event", (GCallback)
     mainwin_keypress, 0);

    g_signal_connect (playlistwin, "size-allocate", G_CALLBACK (size_allocate),
     0);
}

static void get_title (void)
{
    gint playlists = aud_playlist_count ();

    g_free (active_title);

    if (playlists > 1)
    {
        gchar * title = aud_playlist_get_title (active_playlist);
        active_title = g_strdup_printf (_("%s (%d of %d)"), title, 1 +
         active_playlist, playlists);
        g_free (title);
    }
    else
        active_title = NULL;
}

static void update_cb (void * unused, void * another)
{
    gint old = active_playlist;

    active_playlist = aud_playlist_get_active ();
    active_length = aud_playlist_entry_count (active_playlist);
    get_title ();

    if (active_playlist != old)
    {
        ui_skinned_playlist_scroll_to (playlistwin_list, 0);
        song_changed = TRUE;
    }

    if (song_changed)
    {
        ui_skinned_playlist_set_focused (playlistwin_list,
         aud_playlist_get_position (active_playlist));
        song_changed = FALSE;
    }

    real_update ();
}

static void follow_cb (void * data, void * another)
{
    gint list = GPOINTER_TO_INT (data);
    aud_playlist_select_all (list, FALSE);

    gint row = aud_playlist_get_position (list);
    if (row >= 0)
        aud_playlist_entry_set_selected (list, row, TRUE);

    if (list == active_playlist)
        song_changed = TRUE;
}

void
playlistwin_create(void)
{
    active_playlist = aud_playlist_get_active ();
    active_length = aud_playlist_entry_count (active_playlist);
    active_title = NULL;
    get_title ();

    resize_mutex = g_mutex_new();
    playlistwin_create_window();

    playlistwin_create_widgets();

    gtk_widget_show_all (((SkinnedWindow *) playlistwin)->normal);
    gtk_widget_show_all (((SkinnedWindow *) playlistwin)->shaded);

    gtk_window_add_accel_group(GTK_WINDOW(playlistwin), ui_manager_get_accel_group());

    aud_playlist_select_all (active_playlist, FALSE);

    gint row = aud_playlist_get_position (active_playlist);
    if (row >= 0)
        aud_playlist_entry_set_selected (active_playlist, row, TRUE);

    ui_skinned_playlist_set_focused (playlistwin_list, row);

    song_changed = FALSE;

    hook_associate ("playlist position", follow_cb, NULL);
    hook_associate ("playlist activate", update_cb, NULL);
    hook_associate ("playlist update", update_cb, NULL);
}

void playlistwin_unhook (void)
{
    hook_dissociate ("playlist position", follow_cb);
    hook_dissociate ("playlist activate", update_cb);
    hook_dissociate ("playlist update", update_cb);
    g_free (active_title);
    active_title = NULL;
    g_mutex_free (resize_mutex);
    resize_mutex = NULL;
}

static void playlistwin_real_show (gboolean show)
{
    if (show)
        gtk_window_present ((GtkWindow *) playlistwin);
    else
        gtk_widget_hide (playlistwin);
}

void playlistwin_show (char show)
{
    GtkAction * a;

    a = gtk_action_group_get_action (toggleaction_group_others, "show playlist editor");

    if (a && gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (a)) != show)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (a), show);
    else
    {
        config.playlist_visible = show;
        button_set_active (mainwin_pl, show);
        playlistwin_real_show (config.player_visible && show);
    }
}

void action_playlist_track_info(void)
{
    playlistwin_fileinfo();
}

void action_queue_toggle (void)
{
    gint rows, first, focused, at;

    ui_skinned_playlist_row_info (playlistwin_list, & rows, & first, & focused);
    at = (focused == -1) ? -1 : aud_playlist_queue_find_entry (active_playlist,
     focused);

    if (at == -1)
        aud_playlist_queue_insert_selected (active_playlist, -1);
    else
        aud_playlist_queue_delete (active_playlist, at, 1);
}

void action_jump_to_playlist_start (void)
{
    aud_drct_pl_set_pos (0);
}

void action_playlist_sort_by_track_number (void)
{
    aud_playlist_sort_by_scheme (active_playlist, PLAYLIST_SORT_TRACK);
}

void action_playlist_sort_by_title (void)
{
    aud_playlist_sort_by_scheme (active_playlist, PLAYLIST_SORT_TITLE);
}

void action_playlist_sort_by_album (void)
{
    aud_playlist_sort_by_scheme (active_playlist, PLAYLIST_SORT_ALBUM);
}

void action_playlist_sort_by_artist (void)
{
    aud_playlist_sort_by_scheme (active_playlist, PLAYLIST_SORT_ARTIST);
}

void action_playlist_sort_by_full_path (void)
{
    aud_playlist_sort_by_scheme (active_playlist, PLAYLIST_SORT_PATH);
}

void action_playlist_sort_by_date (void)
{
    aud_playlist_sort_by_scheme (active_playlist, PLAYLIST_SORT_DATE);
}

void action_playlist_sort_by_filename (void)
{
    aud_playlist_sort_by_scheme (active_playlist, PLAYLIST_SORT_FILENAME);
}

void action_playlist_sort_selected_by_track_number (void)
{
    aud_playlist_sort_selected_by_scheme (active_playlist, PLAYLIST_SORT_TRACK);
}

void action_playlist_sort_selected_by_title (void)
{
    aud_playlist_sort_selected_by_scheme (active_playlist, PLAYLIST_SORT_TITLE);
}

void action_playlist_sort_selected_by_album (void)
{
    aud_playlist_sort_selected_by_scheme (active_playlist, PLAYLIST_SORT_ALBUM);
}

void action_playlist_sort_selected_by_artist (void)
{
    aud_playlist_sort_selected_by_scheme (active_playlist, PLAYLIST_SORT_ARTIST);
}

void action_playlist_sort_selected_by_full_path (void)
{
    aud_playlist_sort_selected_by_scheme (active_playlist, PLAYLIST_SORT_PATH);
}

void action_playlist_sort_selected_by_date (void)
{
    aud_playlist_sort_selected_by_scheme (active_playlist, PLAYLIST_SORT_DATE);
}

void action_playlist_sort_selected_by_filename (void)
{
    aud_playlist_sort_selected_by_scheme (active_playlist, PLAYLIST_SORT_FILENAME);
}

void action_playlist_randomize_list (void)
{
    aud_playlist_randomize (active_playlist);
}

void action_playlist_reverse_list (void)
{
    aud_playlist_reverse (active_playlist);
}

void action_playlist_clear_queue (void)
{
    aud_playlist_queue_delete (active_playlist, 0, aud_playlist_queue_count
     (active_playlist));
}

void action_playlist_remove_unavailable (void)
{
    aud_playlist_remove_failed (active_playlist);
}

void action_playlist_remove_dupes_by_title (void)
{
    aud_playlist_remove_duplicates_by_scheme (active_playlist,
     PLAYLIST_SORT_TITLE);
}

void action_playlist_remove_dupes_by_filename (void)
{
    aud_playlist_remove_duplicates_by_scheme (active_playlist,
     PLAYLIST_SORT_FILENAME);
}

void action_playlist_remove_dupes_by_full_path (void)
{
    aud_playlist_remove_duplicates_by_scheme (active_playlist,
     PLAYLIST_SORT_PATH);
}

void action_playlist_remove_all (void)
{
    aud_playlist_entry_delete (active_playlist, 0, aud_playlist_entry_count
     (active_playlist));
}

void action_playlist_remove_selected (void)
{
    aud_playlist_delete_selected (active_playlist);
}

void action_playlist_remove_unselected (void)
{
    playlistwin_inverse_selection ();
    aud_playlist_delete_selected (active_playlist);
    aud_playlist_select_all (active_playlist, TRUE);
}

void action_playlist_copy (void)
{
    GtkClipboard * clip = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    gchar * list = audgui_urilist_create_from_selected (active_playlist);

    if (list == NULL)
        return;

    gtk_clipboard_set_text (clip, list, -1);
    g_free (list);
}

void action_playlist_cut (void)
{
    action_playlist_copy ();
    action_playlist_remove_selected ();
}

void action_playlist_paste (void)
{
    GtkClipboard * clip = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    gchar * list = gtk_clipboard_wait_for_text (clip);
    gint rows, first, focused;

    if (list == NULL)
        return;

    ui_skinned_playlist_row_info (playlistwin_list, & rows, & first, & focused);
    audgui_urilist_insert (active_playlist, focused, list);
    g_free (list);
}

void
action_playlist_add_files(void)
{
    audgui_run_filebrowser(FALSE); /* FALSE = NO_PLAY_BUTTON */
}

void
action_playlist_add_url(void)
{
    audgui_show_add_url_window (FALSE);
}

void action_playlist_new (void)
{
    gint playlist = aud_playlist_count ();

    aud_playlist_insert (playlist);
    aud_playlist_set_active (playlist);
}

void action_playlist_prev (void)
{
    if (active_playlist > 0)
        aud_playlist_set_active (active_playlist - 1);
}

void action_playlist_next (void)
{
    if (active_playlist + 1 < aud_playlist_count ())
        aud_playlist_set_active (active_playlist + 1);
}

void action_playlist_delete (void)
{
    audgui_confirm_playlist_delete (active_playlist);
}

void action_playlist_save_all_playlists (void)
{
    aud_save_playlists ();
}

void
action_playlist_refresh_list(void)
{
    aud_playlist_rescan (active_playlist);
}

void
action_playlist_search_and_select(void)
{
    playlistwin_select_search();
}

void
action_playlist_invert_selection(void)
{
    playlistwin_inverse_selection();
}

void
action_playlist_select_none(void)
{
    playlistwin_select_none();
}

void
action_playlist_select_all(void)
{
    playlistwin_select_all();
}


static void
playlistwin_select_search_cbt_cb(GtkWidget *called_cbt, gpointer other_cbt)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(called_cbt)) == TRUE)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(other_cbt), FALSE);
    return;
}

static gboolean
playlistwin_select_search_kp_cb(GtkWidget *entry, GdkEventKey *event,
                                gpointer searchdlg_win)
{
    switch (event->keyval)
    {
        case GDK_Return:
            gtk_dialog_response(GTK_DIALOG(searchdlg_win), GTK_RESPONSE_ACCEPT);
            return TRUE;
        default:
            return FALSE;
    }
}

void action_show_playlist_editor (GtkToggleAction * action)
{
    playlistwin_show (gtk_toggle_action_get_active (action));
}

void action_roll_up_playlist_editor (GtkToggleAction * action)
{
    config.playlist_shaded = gtk_toggle_action_get_active (action);
    ui_skinned_window_set_shade (playlistwin, config.playlist_shaded);

    if (config.playlist_shaded)
    {
        playlistwin_set_sinfo_font (config.playlist_font);
        playlistwin_set_sinfo_scroll (config.autoscroll);
    }
    else
        playlistwin_set_sinfo_scroll (FALSE);

    playlistwin_set_geometry_hints (config.playlist_shaded);
    dock_shade (get_dock_window_list (), (GtkWindow *) playlistwin,
     playlistwin_get_height ());
}
