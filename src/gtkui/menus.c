/*
 * menus.c
 * Copyright 2011 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/gtk-compat.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"
#include "gtkui.h"
#include "playlist_util.h"
#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"

#define SHIFT GDK_SHIFT_MASK
#define CTRL GDK_CONTROL_MASK
#define ALT GDK_MOD1_MASK

struct MenuItem {
    const gchar * name;
    const gchar * icon;
    guint key;
    GdkModifierType mod;

    /* for normal items */
    void (* func) (void);

    /* for toggle items */
    gboolean (* get) (void);
    void (* set) (gboolean on);
    const gchar * hook;

    /* for submenus */
    const struct MenuItem * items;
    gint n_items;

    /* for custom submenus */
    GtkWidget * (* get_sub) (void);

    /* for separators */
    gboolean sep;
};

static void open_files (void) {audgui_run_filebrowser (TRUE); }
static void open_url (void) {audgui_show_add_url_window (TRUE); }
static void add_files (void) {audgui_run_filebrowser (FALSE); }
static void add_url (void) {audgui_show_add_url_window (FALSE); }

static gboolean repeat_get (void) {return aud_get_bool (NULL, "repeat"); }
static void repeat_set (gboolean on) {aud_set_bool (NULL, "repeat", on); }
static gboolean shuffle_get (void) {return aud_get_bool (NULL, "shuffle"); }
static void shuffle_set (gboolean on) {aud_set_bool (NULL, "shuffle", on); }
static gboolean no_advance_get (void) {return aud_get_bool (NULL, "no_playlist_advance"); }
static void no_advance_set (gboolean on) {aud_set_bool (NULL, "no_playlist_advance", on); }
static gboolean stop_after_get (void) {return aud_get_bool (NULL, "stop_after_current_song"); }
static void stop_after_set (gboolean on) {aud_set_bool (NULL, "stop_after_current_song", on); }

static void pl_sort_track (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TRACK); }
static void pl_sort_title (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TITLE); }
static void pl_sort_artist (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ARTIST); }
static void pl_sort_album (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ALBUM); }
static void pl_sort_date (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_DATE); }
static void pl_sort_path (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_PATH); }
static void pl_sort_custom (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_FORMATTED_TITLE); }
static void pl_reverse (void) {aud_playlist_reverse (aud_playlist_get_active ()); }
static void pl_random (void) {aud_playlist_randomize (aud_playlist_get_active ()); }

static void pl_new (void)
{
    aud_playlist_insert (aud_playlist_get_active () + 1);
    aud_playlist_set_active (aud_playlist_get_active () + 1);
}

static void pl_refresh (void) {aud_playlist_rescan (aud_playlist_get_active ()); }
static void pl_close (void) {audgui_confirm_playlist_delete (aud_playlist_get_active ()); }
static void pl_refresh_sel (void) {aud_playlist_rescan_selected (aud_playlist_get_active ()); }
static void pl_select_all (void) {aud_playlist_select_all (aud_playlist_get_active (), TRUE); }
static void pl_rename (void) {ui_playlist_notebook_edit_tab_title (NULL); }

static GtkWidget * get_services_main (void) {return aud_get_plugin_menu (AUD_MENU_MAIN); }
static GtkWidget * get_services_pl (void) {return aud_get_plugin_menu (AUD_MENU_PLAYLIST_RCLICK); }

static void volume_up (void)
{
    int vol = 0;
    aud_drct_get_volume_main (& vol);
    aud_drct_set_volume_main (vol + 5);
}

static void volume_down (void)
{
    int vol = 0;
    aud_drct_get_volume_main (& vol);
    aud_drct_set_volume_main (vol - 5);
}

static gboolean menu_bar_get (void) {return aud_get_bool ("gtkui", "menu_visible"); }
static gboolean info_bar_get (void) {return aud_get_bool ("gtkui", "infoarea_visible"); }
static gboolean status_bar_get (void) {return aud_get_bool ("gtkui", "statusbar_visible"); }
static gboolean column_headers_get (void) {return aud_get_bool ("gtkui", "playlist_headers"); }
static gboolean autoscroll_get (void) {return aud_get_bool ("gtkui", "autoscroll"); }
static void autoscroll_set (gboolean on) {aud_set_bool ("gtkui", "autoscroll", on); }

static const struct MenuItem file_items[] = {
 {N_("_Open Files ..."), GTK_STOCK_OPEN, 'l', .func = open_files},
 {N_("Open _URL ..."), GTK_STOCK_NETWORK, 'l', CTRL, .func = open_url},
 {N_("_Add Files ..."), GTK_STOCK_ADD, 'f', .func = add_files},
 {N_("Add U_RL ..."), GTK_STOCK_NETWORK, 'h', CTRL, .func = add_url},
 {.sep = TRUE},
 {N_("A_bout ..."), GTK_STOCK_ABOUT, .func = audgui_show_about_window},
 {N_("_Preferences ..."), GTK_STOCK_PREFERENCES, 'p', CTRL, .func = aud_show_prefs_window},
 {N_("_Quit"), GTK_STOCK_QUIT, 'q', CTRL, .func = aud_drct_quit}};

static const struct MenuItem playback_items[] = {
 {N_("_Play"), GTK_STOCK_MEDIA_PLAY, 'x', .func = aud_drct_play},
 {N_("Paus_e"), GTK_STOCK_MEDIA_PAUSE, 'c', .func = aud_drct_pause},
 {N_("_Stop"), GTK_STOCK_MEDIA_STOP, 'v', .func = aud_drct_stop},
 {N_("Pre_vious"), GTK_STOCK_MEDIA_PREVIOUS, 'z', .func = aud_drct_pl_prev},
 {N_("_Next"), GTK_STOCK_MEDIA_NEXT, 'b', .func = aud_drct_pl_next},
 {.sep = TRUE},
 {N_("_Repeat"), NULL, 'r', .get = repeat_get, repeat_set, "set repeat"},
 {N_("S_huffle"), NULL, 's', .get = shuffle_get, shuffle_set, "set shuffle"},
 {N_("N_o Playlist Advance"), NULL, 'n', CTRL, .get = no_advance_get, no_advance_set, "set no_playlist_advance"},
 {N_("Stop _After This Song"), NULL, 'm', CTRL, .get = stop_after_get, stop_after_set, "set stop_after_current_song"},
 {.sep = TRUE},
 {N_("Song _Info ..."), GTK_STOCK_INFO, 'i', .func = audgui_infowin_show_current},
 {N_("Jump to _Time ..."), GTK_STOCK_JUMP_TO, 'j', CTRL, .func = audgui_jump_to_time},
 {N_("_Jump to Song ..."), GTK_STOCK_JUMP_TO, 'j', .func = audgui_jump_to_track}};

static const struct MenuItem sort_items[] = {
 {N_("By Track _Number"), .func = pl_sort_track},
 {N_("By _Title"), .func = pl_sort_title},
 {N_("By _Artist"), .func = pl_sort_artist},
 {N_("By A_lbum"), .func = pl_sort_album},
 {N_("By Release _Date"), .func = pl_sort_date},
 {N_("By _File Path"), .func = pl_sort_path},
 {N_("By _Custom Title"), .func = pl_sort_custom},
 {.sep = TRUE},
 {N_("R_everse Order"), GTK_STOCK_SORT_DESCENDING, .func = pl_reverse},
 {N_("_Random Order"), .func = pl_random}};

static const struct MenuItem playlist_items[] = {
 {N_("_Refresh"), GTK_STOCK_REFRESH, GDK_F5, .func = pl_refresh},
 {.sep = TRUE},
 {N_("_Sort"), GTK_STOCK_SORT_ASCENDING, .items = sort_items, G_N_ELEMENTS (sort_items)},
 {.sep = TRUE},
 {N_("_New"), GTK_STOCK_NEW, 't', CTRL, .func = pl_new},
 {N_("_Close"), GTK_STOCK_CLOSE, 'w', CTRL, .func = pl_close},
 {.sep = TRUE},
 {N_("_Import ..."), GTK_STOCK_OPEN, 'o', .func = audgui_import_playlist},
 {N_("_Export ..."), GTK_STOCK_SAVE, 's', SHIFT, .func = audgui_export_playlist},
 {.sep = TRUE},
 {N_("_Playlist Manager ..."), AUD_STOCK_PLAYLIST, 'p', .func = audgui_playlist_manager},
 {N_("_Queue Manager ..."), AUD_STOCK_QUEUETOGGLE, 'u', CTRL, .func = audgui_queue_manager_show}};

static const struct MenuItem output_items[] = {
 {N_("Volume _Up"), GTK_STOCK_GO_UP, '+', .func = volume_up},
 {N_("Volume _Down"), GTK_STOCK_GO_DOWN, '-', .func = volume_down},
 {.sep = TRUE},
 {N_("_Equalizer"), GTK_STOCK_PREFERENCES, 'e', CTRL, .func = audgui_show_equalizer_window},
 {.sep = TRUE},
 {N_("E_ffects"), .get_sub = audgui_create_effects_menu}};

static const struct MenuItem view_items[] = {
 {N_("_Interface"), .get_sub = audgui_create_iface_menu},
 {.sep = TRUE},
 {N_("Show _Menu Bar"), NULL, 'm', SHIFT | CTRL, .get = menu_bar_get, show_menu},
 {N_("Show I_nfo Bar"), NULL, 'i', SHIFT | CTRL, .get = info_bar_get, show_infoarea},
 {N_("Show _Status Bar"), NULL, 's', SHIFT | CTRL, .get = status_bar_get, show_statusbar},
 {.sep = TRUE},
 {N_("Show Column _Headers"), .get = column_headers_get, playlist_show_headers},
 {N_("Choose _Columns ..."), .func = pw_col_choose},
 {N_("Scrol_l on Song Change"), .get = autoscroll_get, autoscroll_set}};

static const struct MenuItem main_items[] = {
 {N_("_File"), .items = file_items, G_N_ELEMENTS (file_items)},
 {N_("_Playback"), .items = playback_items, G_N_ELEMENTS (playback_items)},
 {N_("P_laylist"), .items = playlist_items, G_N_ELEMENTS (playlist_items)},
 {N_("_Services"), .get_sub = get_services_main},
 {N_("_Output"), .items = output_items, G_N_ELEMENTS (output_items)},
 {N_("_View"), .items = view_items, G_N_ELEMENTS (view_items)}};

static const struct MenuItem rclick_items[] = {
 {N_("Song _Info ..."), GTK_STOCK_INFO, 'i', ALT, .func = playlist_song_info},
 {N_("_Queue/Unqueue"), AUD_STOCK_QUEUETOGGLE, 'q', .func = playlist_queue_toggle},
 {N_("_Refresh"), GTK_STOCK_REFRESH, GDK_F6, .func = pl_refresh_sel},
 {.sep = TRUE},
 {N_("Cu_t"), GTK_STOCK_CUT, 'x', CTRL, .func = playlist_cut},
 {N_("_Copy"), GTK_STOCK_COPY, 'c', CTRL, .func = playlist_copy},
 {N_("_Paste"), GTK_STOCK_PASTE, 'v', CTRL, .func = playlist_paste},
 {N_("Select _All"), GTK_STOCK_SELECT_ALL, 'a', CTRL, .func = pl_select_all},
 {.sep = TRUE},
 {N_("_Services"), .get_sub = get_services_pl}};

static const struct MenuItem tab_items[] = {
 {N_("_Rename"), GTK_STOCK_EDIT, GDK_F2, .func = pl_rename},
 {N_("_Close"), GTK_STOCK_CLOSE, .func = pl_close}};

static void toggled_cb (GtkCheckMenuItem * check, const struct MenuItem * item)
{
    if (item->get () == gtk_check_menu_item_get_active (check))
        return;

    item->set (gtk_check_menu_item_get_active (check));
}

static void hook_cb (void * data, GtkWidget * check)
{
    const struct MenuItem * item = g_object_get_data ((GObject *) check, "item");
    gtk_check_menu_item_set_active ((GtkCheckMenuItem *) check, item->get ());
}

static void unhook_cb (GtkCheckMenuItem * check, const struct MenuItem * item)
{
    hook_dissociate_full (item->hook, (HookFunction) hook_cb, check);
}

static void populate_menu (GtkWidget * shell, const struct MenuItem * items,
 gint n_items, GtkAccelGroup * accel)
{
    for (gint i = 0; i < n_items; i ++)
    {
        const struct MenuItem * item = & items[i];
        GtkWidget * widget = NULL;

        if (item->name && item->func) /* normal widget */
        {
            widget = gtk_image_menu_item_new_with_mnemonic (_(item->name));
            g_signal_connect (widget, "activate", item->func, NULL);

            if (item->icon)
                gtk_image_menu_item_set_image ((GtkImageMenuItem *) widget,
                 gtk_image_new_from_stock (item->icon, GTK_ICON_SIZE_MENU));
        }
        else if (item->name && item->get && item->set) /* toggle widget */
        {
            widget = gtk_check_menu_item_new_with_mnemonic (_(item->name));
            gtk_check_menu_item_set_active ((GtkCheckMenuItem *) widget,
             item->get ());
            g_signal_connect (widget, "toggled", (GCallback) toggled_cb,
             (void *) item);

            if (item->hook)
            {
                g_object_set_data ((GObject *) widget, "item", (void *) item);
                hook_associate (item->hook, (HookFunction) hook_cb, widget);
                g_signal_connect (widget, "destroy", (GCallback) unhook_cb,
                 (void *) item);
            }
        }
        else if (item->name && (item->items || item->get_sub)) /* submenu */
        {
            widget = gtk_image_menu_item_new_with_mnemonic (_(item->name));

            if (item->icon)
                gtk_image_menu_item_set_image ((GtkImageMenuItem *) widget,
                 gtk_image_new_from_stock (item->icon, GTK_ICON_SIZE_MENU));

            GtkWidget * sub;

            if (item->get_sub)
                sub = item->get_sub ();
            else
            {
                sub = gtk_menu_new ();
                populate_menu (sub, item->items, item->n_items, accel);
            }

            gtk_menu_item_set_submenu ((GtkMenuItem *) widget, sub);
        }
        else if (item->sep) /* separator */
            widget = gtk_separator_menu_item_new ();

        if (! widget)
            continue;

        if (item->key)
            gtk_widget_add_accelerator (widget, "activate", accel, item->key,
             item->mod, GTK_ACCEL_VISIBLE);

        gtk_widget_show (widget);
        gtk_menu_shell_append ((GtkMenuShell *) shell, widget);
    }
}

GtkWidget * make_menu_bar (GtkAccelGroup * accel)
{
    GtkWidget * bar = gtk_menu_bar_new ();
    populate_menu (bar, main_items, G_N_ELEMENTS (main_items), accel);
    return bar;
}

GtkWidget * make_menu_main (GtkAccelGroup * accel)
{
    GtkWidget * shell = gtk_menu_new ();
    populate_menu (shell, main_items, G_N_ELEMENTS (main_items), accel);
    return shell;
}

GtkWidget * make_menu_rclick (GtkAccelGroup * accel)
{
    GtkWidget * shell = gtk_menu_new ();
    populate_menu (shell, rclick_items, G_N_ELEMENTS (rclick_items), accel);
    return shell;
}

GtkWidget * make_menu_tab (GtkAccelGroup * accel)
{
    GtkWidget * shell = gtk_menu_new ();
    populate_menu (shell, tab_items, G_N_ELEMENTS (tab_items), accel);
    return shell;
}
