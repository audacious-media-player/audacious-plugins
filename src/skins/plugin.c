/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2008 Tomasz Moń
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */


#include "plugin.h"
#include "skins_cfg.h"
#include "ui_skin.h"
#include "ui_skinned_window.h"
#include "ui_manager.h"
#include "icons-stock.h"
#include "ui_main_evlisteners.h"
#include "ui_playlist_evlisteners.h"
#include "ui_skinned_textbox.h"
#include "ui_skinned_playlist.h"
#include "ui_skinselector.h"
#include <audacious/i18n.h>
#include <libintl.h>

#define PACKAGE "audacious-plugins"

gchar *skins_paths[SKINS_PATH_COUNT] = {};

GeneralPlugin skins_gp =
{
    .description= "Audacious Skinned GUI",
    .init = skins_init,
    .about = skins_about,
    .configure = skins_configure,
    .cleanup = skins_cleanup
};

GeneralPlugin *skins_gplist[] = { &skins_gp, NULL };
SIMPLE_GENERAL_PLUGIN(skins, skins_gplist);
gboolean plugin_is_active = FALSE;


GtkWidget *skin_view;
GtkWidget *skin_refresh_button;

static void skins_free_paths(void) {
    int i;

    for (i = 0; i < SKINS_PATH_COUNT; i++)  {
        g_free(skins_paths[i]);
        skins_paths[i] = NULL;
    }
}

static void skins_init_paths() {
    char *xdg_data_home;
    char *xdg_cache_home;

    xdg_data_home = (getenv("XDG_DATA_HOME") == NULL
        ? g_build_filename(g_get_home_dir(), ".local", "share", NULL)
        : g_strdup(getenv("XDG_DATA_HOME")));
    xdg_cache_home = (getenv("XDG_CACHE_HOME") == NULL
        ? g_build_filename(g_get_home_dir(), ".cache", NULL)
        : g_strdup(getenv("XDG_CACHE_HOME")));

    skins_paths[SKINS_PATH_USER_SKIN_DIR] =
        g_build_filename(xdg_data_home, "audacious", "Skins", NULL);
    skins_paths[SKINS_PATH_SKIN_THUMB_DIR] =
        g_build_filename(xdg_cache_home, "audacious", "thumbs", NULL);

    g_free(xdg_data_home);
    g_free(xdg_cache_home);
}

void skins_init(void) {
    plugin_is_active = TRUE;
    g_log_set_handler(NULL, G_LOG_LEVEL_WARNING, g_log_default_handler, NULL);

    skins_init_paths();
    skins_cfg_load();

    register_aud_stock_icons();
    ui_manager_init();
    ui_manager_create_menus();
    mainwin_setup_menus();

    init_skins(config.skin);

    if (config.player_visible) mainwin_real_show();
    if (config.equalizer_visible) equalizerwin_show(TRUE);
    if (config.playlist_visible) playlistwin_show();

    return;
}

void skins_cleanup(void) {
    if (plugin_is_active == TRUE) {
        skins_cfg_save();
        skins_free_paths();
        ui_main_evlistener_dissociate();
        ui_playlist_evlistener_dissociate();
        skins_cfg_free();
        gtk_widget_destroy(mainwin);
        gtk_widget_destroy(equalizerwin);
        gtk_widget_destroy(playlistwin);
        ui_manager_destroy();
        skin_destroy(aud_active_skin);
        aud_active_skin = NULL;
        mainwin = NULL;
        equalizerwin = NULL;
        playlistwin = NULL;
        mainwin_info = NULL;
        plugin_is_active = FALSE;
    }

    return;
}

static void
playlist_show_pl_separator_numbers_cb()
{
    playlistwin_update_list(aud_playlist_get_active());
}

static void
mainwin_font_set_cb()
{
    ui_skinned_textbox_set_xfont(mainwin_info, !config.mainwin_use_bitmapfont, config.mainwin_font);
}

static void
playlist_font_set_cb()
{
    AUDDBG("Attempt to set font \"%s\"\n", config.playlist_font);
    ui_skinned_playlist_set_font(config.playlist_font);
    playlistwin_set_sinfo_font(config.playlist_font);  /* propagate font setting to playlistwin_sinfo */
    playlistwin_update_list(aud_playlist_get_active());
}

static void
bitmap_fonts_cb()
{
    ui_skinned_textbox_set_xfont(mainwin_info, !config.mainwin_use_bitmapfont, config.mainwin_font);
    playlistwin_set_sinfo_font(config.playlist_font);

    if (config.playlist_shaded) {
        playlistwin_update_list(aud_playlist_get_active());
        ui_skinned_window_draw_all(playlistwin);
    }
}

static void
show_wm_decorations_cb()
{
    gtk_window_set_decorated(GTK_WINDOW(mainwin), config.show_wm_decorations);
    gtk_window_set_decorated(GTK_WINDOW(playlistwin), config.show_wm_decorations);
    gtk_window_set_decorated(GTK_WINDOW(equalizerwin), config.show_wm_decorations);
}

static PreferencesWidget appearance_misc_widgets[] = {
    {WIDGET_LABEL, N_("<b>_Fonts</b>"), NULL, NULL, NULL, FALSE},
    {WIDGET_FONT_BTN, N_("_Player:"), &config.mainwin_font, G_CALLBACK(mainwin_font_set_cb), N_("Select main player window font:"), FALSE},
    {WIDGET_FONT_BTN, N_("_Playlist:"), &config.playlist_font, G_CALLBACK(playlist_font_set_cb), N_("Select playlist font:"), FALSE},
    {WIDGET_CHK_BTN, N_("Use Bitmap fonts if available"), &config.mainwin_use_bitmapfont, G_CALLBACK(bitmap_fonts_cb), N_("Use bitmap fonts if they are available. Bitmap fonts do not support Unicode strings."), FALSE},
    {WIDGET_LABEL, N_("<b>_Miscellaneous</b>"), NULL, NULL, NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Show track numbers in playlist"), &config.show_numbers_in_pl,
        G_CALLBACK(playlist_show_pl_separator_numbers_cb), NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Show separators in playlist"), &config.show_separator_in_pl,
        G_CALLBACK(playlist_show_pl_separator_numbers_cb), NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Show window manager decoration"), &config.show_wm_decorations, G_CALLBACK(show_wm_decorations_cb),
        N_("This enables the window manager to show decorations for windows."), FALSE},
    {WIDGET_CHK_BTN, N_("Use two-way text scroller"), &config.twoway_scroll, NULL,
        N_("If selected, the file information text in the main window will scroll back and forth. If not selected, the text will only scroll in one direction."), FALSE},
    {WIDGET_CHK_BTN, N_("Disable inline gtk theme"), &config.disable_inline_gtk, NULL, NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Allow loading incomplete skins"), &config.allow_broken_skins, NULL,
        N_("If selected, audacious won't refuse loading broken skins. Use only if your favourite skin doesn't work"), FALSE},
};

static gboolean
on_skin_view_realize(GtkTreeView * treeview,
                     gpointer data)
{
    skin_view_realize(treeview);
    skin_view_update(GTK_TREE_VIEW(skin_view), GTK_WIDGET(skin_refresh_button));

    return TRUE;
}

void skins_configure(void) {
    static GtkWidget *cfg_win = NULL;
    GtkWidget *appearance_page_vbox;
    GtkWidget *vbox37;
    GtkWidget *vbox38;
    GtkWidget *hbox12;
    GtkWidget *alignment94;
    GtkWidget *hbox13;
    GtkWidget *label103;
    GtkWidget *colorspace_button;
    GtkWidget *image11;
    GtkWidget *image12;
    GtkWidget *alignment95;
    GtkWidget *skin_view_scrolled_window;

    if (cfg_win != NULL) {
        gtk_window_present(GTK_WINDOW(cfg_win));
        return;
    }

    cfg_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(cfg_win), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_title(GTK_WINDOW(cfg_win), _("Audacious Skinned GUI Configuration"));
    gtk_container_set_border_width(GTK_CONTAINER(cfg_win), 10);
    g_signal_connect(G_OBJECT(cfg_win), "destroy" ,
                     G_CALLBACK(gtk_widget_destroyed), &cfg_win);
    gtk_widget_set_size_request(cfg_win, 500, -1);

    appearance_page_vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add(GTK_CONTAINER(cfg_win), appearance_page_vbox);

    vbox37 = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (appearance_page_vbox), vbox37, TRUE, TRUE, 0);

    vbox38 = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox37), vbox38, FALSE, TRUE, 0);

    hbox12 = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox38), hbox12, TRUE, TRUE, 0);

    alignment94 = gtk_alignment_new (0.5, 0.5, 1, 1);
    gtk_box_pack_start (GTK_BOX (hbox12), alignment94, TRUE, TRUE, 0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment94), 0, 4, 0, 0);

    hbox13 = gtk_hbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (alignment94), hbox13);

    label103 = gtk_label_new_with_mnemonic (_("<b>_Skin</b>"));
    gtk_box_pack_start (GTK_BOX (hbox13), label103, TRUE, TRUE, 0);
    gtk_label_set_use_markup (GTK_LABEL (label103), TRUE);
    gtk_misc_set_alignment (GTK_MISC (label103), 0, 0);

    colorspace_button = gtk_button_new ();
    gtk_box_pack_start (GTK_BOX (hbox13), colorspace_button, FALSE, FALSE, 0);

    image11 = gtk_image_new_from_stock ("gtk-properties", GTK_ICON_SIZE_BUTTON);
    gtk_container_add (GTK_CONTAINER (colorspace_button), image11);

    skin_refresh_button = gtk_button_new ();
    gtk_box_pack_start (GTK_BOX (hbox13), skin_refresh_button, FALSE, FALSE, 0);
    GTK_WIDGET_UNSET_FLAGS (skin_refresh_button, GTK_CAN_FOCUS);
    gtk_button_set_relief (GTK_BUTTON (skin_refresh_button), GTK_RELIEF_HALF);
    gtk_button_set_focus_on_click (GTK_BUTTON (skin_refresh_button), FALSE);

    image12 = gtk_image_new_from_stock ("gtk-refresh", GTK_ICON_SIZE_BUTTON);
    gtk_container_add (GTK_CONTAINER (skin_refresh_button), image12);

    alignment95 = gtk_alignment_new (0.5, 0.5, 1, 1);
    gtk_box_pack_start (GTK_BOX (vbox38), alignment95, TRUE, TRUE, 0);
    gtk_widget_set_size_request (alignment95, -1, 172);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment95), 0, 0, 12, 0);

    skin_view_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (alignment95), skin_view_scrolled_window);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (skin_view_scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (skin_view_scrolled_window), GTK_SHADOW_IN);

    skin_view = gtk_tree_view_new ();
    gtk_container_add (GTK_CONTAINER (skin_view_scrolled_window), skin_view);
    gtk_widget_set_size_request (skin_view, -1, 100);

    aud_create_widgets(GTK_BOX(vbox37), appearance_misc_widgets, G_N_ELEMENTS(appearance_misc_widgets));
    gtk_widget_show_all(appearance_page_vbox);

#if 0
    g_signal_connect(G_OBJECT(colorspace_button), "clicked",
                     G_CALLBACK(on_colorize_button_clicked),
                     NULL);

    g_signal_connect(skin_view, "drag-data-received",
                     G_CALLBACK(on_skin_view_drag_data_received),
                     NULL);
    aud_drag_dest_set(skin_view);

    g_signal_connect(mainwin, "drag-data-received",
                     G_CALLBACK(mainwin_drag_data_received),
                     skin_view);

    g_signal_connect(skin_refresh_button, "clicked",
                     G_CALLBACK(on_skin_refresh_button_clicked),
                     NULL);

    g_signal_connect_swapped(G_OBJECT(skin_refresh_button), "clicked",
                             G_CALLBACK(on_skin_refresh_button_clicked),
                             prefswin);
#endif
    g_signal_connect_after(G_OBJECT(skin_view), "realize",
                           G_CALLBACK(on_skin_view_realize),
                           NULL);

    gtk_window_present(GTK_WINDOW(cfg_win));

    return;
}

void skins_about(void) {
    static GtkWidget* about_window = NULL;

    if (about_window) {
        gtk_window_present(GTK_WINDOW(about_window));
        return;
    }

    about_window = audacious_info_dialog(_("About Skinned GUI"),
                   _("Copyright (c) 2008, by Tomasz Moń <desowin@gmail.com>\n\n"),
                   _("OK"), FALSE, NULL, NULL);

    g_signal_connect(G_OBJECT(about_window), "destroy",	G_CALLBACK(gtk_widget_destroyed), &about_window);
}
