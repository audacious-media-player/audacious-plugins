/*
 * menus.c
 * Copyright 2011 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

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

int menu_tab_playlist_id = -1; /* should really be stored in the menu somehow */

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
static void pl_sort_length (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_LENGTH); }
static void pl_sort_path (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_PATH); }
static void pl_sort_custom (void) {aud_playlist_sort_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_FORMATTED_TITLE); }
static void pl_reverse (void) {aud_playlist_reverse (aud_playlist_get_active ()); }
static void pl_random (void) {aud_playlist_randomize (aud_playlist_get_active ()); }

static void pl_sort_selected_track (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TRACK); }
static void pl_sort_selected_title (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TITLE); }
static void pl_sort_selected_artist (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ARTIST); }
static void pl_sort_selected_album (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_ALBUM); }
static void pl_sort_selected_date (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_DATE); }
static void pl_sort_selected_length (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_LENGTH); }
static void pl_sort_selected_path (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_PATH); }
static void pl_sort_selected_custom (void) {aud_playlist_sort_selected_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_FORMATTED_TITLE); }
static void pl_reverse_selected (void) {aud_playlist_reverse_selected (aud_playlist_get_active ()); }
static void pl_random_selected (void) {aud_playlist_randomize_selected (aud_playlist_get_active ()); }

static void pl_new (void)
{
    aud_playlist_insert (-1);
    aud_playlist_set_active (aud_playlist_count () - 1);
}

static void pl_play (void) {aud_drct_play_playlist (aud_playlist_get_active ()); }
static void pl_refresh (void) {aud_playlist_rescan (aud_playlist_get_active ()); }
static void pl_remove_failed (void) {aud_playlist_remove_failed (aud_playlist_get_active ()); }
static void pl_remove_dupes_by_title (void) {aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_TITLE); }
static void pl_remove_dupes_by_filename (void) {aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_FILENAME); }
static void pl_remove_dupes_by_path (void) {aud_playlist_remove_duplicates_by_scheme (aud_playlist_get_active (), PLAYLIST_SORT_PATH); }
static void pl_rename (void) {ui_playlist_notebook_edit_tab_title (aud_playlist_get_active ()); }
static void pl_close (void) {audgui_confirm_playlist_delete (aud_playlist_get_active ()); }
static void pl_refresh_sel (void) {aud_playlist_rescan_selected (aud_playlist_get_active ()); }
static void pl_select_all (void) {aud_playlist_select_all (aud_playlist_get_active (), TRUE); }

static void pl_tab_play (void)
{
    int playlist = aud_playlist_by_unique_id (menu_tab_playlist_id);
    if (playlist >= 0)
        aud_drct_play_playlist (playlist);
}

static void pl_tab_rename (void)
{
    int playlist = aud_playlist_by_unique_id (menu_tab_playlist_id);
    if (playlist >= 0)
        ui_playlist_notebook_edit_tab_title (playlist);
}

static void pl_tab_close (void)
{
    int playlist = aud_playlist_by_unique_id (menu_tab_playlist_id);
    if (playlist >= 0)
        audgui_confirm_playlist_delete (playlist);
}

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
static gboolean infoarea_get (void) {return aud_get_bool ("gtkui", "infoarea_visible"); }
static gboolean infoarea_vis_get (void) {return aud_get_bool ("gtkui", "infoarea_show_vis"); }
static gboolean status_bar_get (void) {return aud_get_bool ("gtkui", "statusbar_visible"); }
static gboolean remaining_time_get (void) {return aud_get_bool ("gtkui", "show_remaining_time"); }
static void remaining_time_set (gboolean show) {aud_set_bool ("gtkui", "show_remaining_time", show); }
static gboolean close_button_get (void) {return aud_get_bool ("gtkui", "close_button_visible"); }
static gboolean column_headers_get (void) {return aud_get_bool ("gtkui", "playlist_headers"); }
static gboolean autoscroll_get (void) {return aud_get_bool ("gtkui", "autoscroll"); }
static void autoscroll_set (gboolean on) {aud_set_bool ("gtkui", "autoscroll", on); }

static const struct MenuItem file_items[] = {
 {N_("_Open Files ..."), GTK_STOCK_OPEN, 'o', CTRL, .func = open_files},
 {N_("Open _URL ..."), GTK_STOCK_NETWORK, 'l', CTRL, .func = open_url},
 {N_("_Add Files ..."), GTK_STOCK_ADD, 'o', SHIFT | CTRL, .func = add_files},
 {N_("Add U_RL ..."), GTK_STOCK_NETWORK, 'l', SHIFT | CTRL, .func = add_url},
 {.sep = TRUE},
 {N_("_Search Library"), GTK_STOCK_FIND, 'y', CTRL, .func = activate_search_tool},
 {.sep = TRUE},
 {N_("A_bout ..."), GTK_STOCK_ABOUT, .func = audgui_show_about_window},
 {N_("_Preferences ..."), GTK_STOCK_PREFERENCES, .func = aud_show_prefs_window},
 {N_("_Quit"), GTK_STOCK_QUIT, 'q', CTRL, .func = aud_drct_quit}};

static const struct MenuItem playback_items[] = {
 {N_("_Play"), GTK_STOCK_MEDIA_PLAY, GDK_KEY_Return, CTRL, .func = aud_drct_play},
 {N_("Paus_e"), GTK_STOCK_MEDIA_PAUSE, ',', CTRL, .func = aud_drct_pause},
 {N_("_Stop"), GTK_STOCK_MEDIA_STOP, '.', CTRL, .func = aud_drct_stop},
 {N_("Pre_vious"), GTK_STOCK_MEDIA_PREVIOUS, GDK_KEY_Up, ALT, .func = aud_drct_pl_prev},
 {N_("_Next"), GTK_STOCK_MEDIA_NEXT, GDK_KEY_Down, ALT, .func = aud_drct_pl_next},
 {.sep = TRUE},
 {N_("_Repeat"), NULL, 'r', CTRL, .get = repeat_get, repeat_set, "set repeat"},
 {N_("S_huffle"), NULL, 's', CTRL, .get = shuffle_get, shuffle_set, "set shuffle"},
 {N_("N_o Playlist Advance"), NULL, 'n', CTRL, .get = no_advance_get, no_advance_set, "set no_playlist_advance"},
 {N_("Stop A_fter This Song"), NULL, 'm', CTRL, .get = stop_after_get, stop_after_set, "set stop_after_current_song"},
 {.sep = TRUE},
 {N_("Song _Info ..."), GTK_STOCK_INFO, 'i', CTRL, .func = audgui_infowin_show_current},
 {N_("Jump to _Time ..."), GTK_STOCK_JUMP_TO, 'k', CTRL, .func = audgui_jump_to_time},
 {N_("_Jump to Song ..."), GTK_STOCK_JUMP_TO, 'j', CTRL, .func = audgui_jump_to_track},
 {.sep = TRUE},
 {N_("Set Repeat Point _A"), NULL, '1', CTRL, .func = set_ab_repeat_a},
 {N_("Set Repeat Point _B"), NULL, '2', CTRL, .func = set_ab_repeat_b},
 {N_("_Clear Repeat Points"), NULL, '3', CTRL, .func = clear_ab_repeat}};

static const struct MenuItem dupe_items[] = {
 {N_("By _Title"), .func = pl_remove_dupes_by_title},
 {N_("By _Filename"), .func = pl_remove_dupes_by_filename},
 {N_("By File _Path"), .func = pl_remove_dupes_by_path}};

static const struct MenuItem sort_items[] = {
 {N_("By Track _Number"), .func = pl_sort_track},
 {N_("By _Title"), .func = pl_sort_title},
 {N_("By _Artist"), .func = pl_sort_artist},
 {N_("By Al_bum"), .func = pl_sort_album},
 {N_("By Release _Date"), .func = pl_sort_date},
 {N_("By _Length"), .func = pl_sort_length},
 {N_("By _File Path"), .func = pl_sort_path},
 {N_("By _Custom Title"), .func = pl_sort_custom},
 {.sep = TRUE},
 {N_("R_everse Order"), GTK_STOCK_SORT_DESCENDING, .func = pl_reverse},
 {N_("_Random Order"), .func = pl_random}};

 static const struct MenuItem sort_selected_items[] = {
 {N_("By Track _Number"), .func = pl_sort_selected_track},
 {N_("By _Title"), .func = pl_sort_selected_title},
 {N_("By _Artist"), .func = pl_sort_selected_artist},
 {N_("By Al_bum"), .func = pl_sort_selected_album},
 {N_("By Release _Date"), .func = pl_sort_selected_date},
 {N_("By _Length"), .func = pl_sort_selected_length},
 {N_("By _File Path"), .func = pl_sort_selected_path},
 {N_("By _Custom Title"), .func = pl_sort_selected_custom},
 {.sep = TRUE},
 {N_("R_everse Order"), GTK_STOCK_SORT_DESCENDING, .func = pl_reverse_selected},
 {N_("_Random Order"), .func = pl_random_selected}};

static const struct MenuItem playlist_items[] = {
 {N_("_Play This Playlist"), GTK_STOCK_MEDIA_PLAY, GDK_KEY_Return, SHIFT | CTRL, .func = pl_play},
 {N_("_Refresh"), GTK_STOCK_REFRESH, GDK_KEY_F5, .func = pl_refresh},
 {.sep = TRUE},
 {N_("_Sort"), GTK_STOCK_SORT_ASCENDING, .items = sort_items, G_N_ELEMENTS (sort_items)},
 {N_("Sort Se_lected"), GTK_STOCK_SORT_ASCENDING, .items = sort_selected_items, G_N_ELEMENTS (sort_selected_items)},
 {N_("Remove _Duplicates"), GTK_STOCK_REMOVE, .items = dupe_items, G_N_ELEMENTS (dupe_items)},
 {N_("Remove _Unavailable Files"), GTK_STOCK_REMOVE, .func = pl_remove_failed},
 {.sep = TRUE},
 {N_("_New"), GTK_STOCK_NEW, 't', CTRL, .func = pl_new},
 {N_("Ren_ame ..."), GTK_STOCK_EDIT, GDK_KEY_F2, .func = pl_rename},
 {N_("_Close"), GTK_STOCK_CLOSE, 'w', CTRL, .func = pl_close},
 {.sep = TRUE},
 {N_("_Import ..."), GTK_STOCK_OPEN, .func = audgui_import_playlist},
 {N_("_Export ..."), GTK_STOCK_SAVE, .func = audgui_export_playlist},
 {.sep = TRUE},
 {N_("Playlist _Manager ..."), AUD_STOCK_PLAYLIST, 'p', CTRL, .func = audgui_playlist_manager},
 {N_("_Queue Manager ..."), AUD_STOCK_QUEUETOGGLE, 'u', CTRL, .func = audgui_queue_manager_show}};

static const struct MenuItem output_items[] = {
 {N_("Volume _Up"), GTK_STOCK_GO_UP, '+', CTRL, .func = volume_up},
 {N_("Volume _Down"), GTK_STOCK_GO_DOWN, '-', CTRL, .func = volume_down},
 {.sep = TRUE},
 {N_("_Equalizer"), GTK_STOCK_PREFERENCES, 'e', CTRL, .func = audgui_show_equalizer_window},
 {.sep = TRUE},
 {N_("E_ffects"), .get_sub = audgui_create_effects_menu}};

static const struct MenuItem view_items[] = {
 {N_("_Interface"), .get_sub = audgui_create_iface_menu},
 {N_("_Visualizations"), .get_sub = audgui_create_vis_menu},
 {.sep = TRUE},
 {N_("Show _Menu Bar"), NULL, 'm', SHIFT | CTRL, .get = menu_bar_get, show_menu},
 {N_("Show I_nfo Bar"), NULL, 'i', SHIFT | CTRL, .get = infoarea_get, show_infoarea},
 {N_("Show Info Bar Vis_ualization"), .get = infoarea_vis_get, show_infoarea_vis},
 {N_("Show _Status Bar"), NULL, 's', SHIFT | CTRL, .get = status_bar_get, show_statusbar},
 {.sep = TRUE},
 {N_("Show _Remaining Time"), NULL, 'r', SHIFT | CTRL, .get = remaining_time_get, remaining_time_set},
 {.sep = TRUE},
 {N_("Show Close _Buttons"), .get = close_button_get, show_close_buttons},
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
 {N_("_Queue/Unqueue"), AUD_STOCK_QUEUETOGGLE, 'q', ALT, .func = playlist_queue_toggle},
 {N_("_Refresh"), GTK_STOCK_REFRESH, GDK_KEY_F6, .func = pl_refresh_sel},
 {.sep = TRUE},
 {N_("Cu_t"), GTK_STOCK_CUT, .func = playlist_cut},
 {N_("_Copy"), GTK_STOCK_COPY, .func = playlist_copy},
 {N_("_Paste"), GTK_STOCK_PASTE, .func = playlist_paste},
 {N_("Select _All"), GTK_STOCK_SELECT_ALL, .func = pl_select_all},
 {.sep = TRUE},
 {N_("_Services"), .get_sub = get_services_pl}};

static const struct MenuItem tab_items[] = {
 {N_("_Play"), GTK_STOCK_MEDIA_PLAY, .func = pl_tab_play},
 {N_("_Rename ..."), GTK_STOCK_EDIT, .func = pl_tab_rename},
 {N_("_Close"), GTK_STOCK_CLOSE, .func = pl_tab_close}};

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
