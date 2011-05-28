/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2008 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
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

#include <audacious/configdb.h>
#include <audacious/debug.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/preferences.h>
#include <libaudcore/audstrings.h>

#include "config.h"
#include "dnd.h"
#include "skins_cfg.h"
#include "ui_dock.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_playlist.h"
#include "ui_skin.h"
#include "ui_skinned_playlist.h"
#include "ui_skinned_textbox.h"
#include "ui_skinned_window.h"
#include "ui_skinselector.h"
#include "ui_vis.h"
#include "util.h"

skins_cfg_t config;
GtkWidget *skin_view;
static GtkWidget *colorize_settings = NULL;
/* colorize settings scales */
GtkWidget *green_scale;
GtkWidget *red_scale;
GtkWidget *blue_scale;


skins_cfg_t skins_default_config = {
    .scaled = FALSE,
    .autoscroll = TRUE,
    .always_on_top = FALSE,
    .sticky = FALSE,
    .scale_factor = 2.0,
    .always_show_cb = TRUE,
    .close_dialog_open = TRUE,
    .close_dialog_add = TRUE,
    .skin = NULL,
    .filesel_path = NULL,
    .playlist_visible = FALSE,
    .equalizer_visible = FALSE,
    .player_visible = TRUE,
    .player_shaded = FALSE,
    .equalizer_shaded = FALSE,
    .playlist_shaded = FALSE,
    .dim_titlebar = TRUE,
    .show_wm_decorations = FALSE,
    .easy_move = TRUE,
    .allow_broken_skins = FALSE,
    .warn_about_broken_gtk_engines = TRUE,
    .warn_about_win_visibility = TRUE,
    .disable_inline_gtk = FALSE,
    .timer_mode = 0,
    .vis_type = VIS_ANALYZER,
    .analyzer_mode = ANALYZER_NORMAL,
    .analyzer_type = ANALYZER_BARS,
    .scope_mode = SCOPE_DOT,
    .voiceprint_mode = VOICEPRINT_NORMAL,
    .vu_mode = VU_SMOOTH,
    .analyzer_falloff = FALLOFF_FAST,
    .peaks_falloff = FALLOFF_SLOW,
    .player_x = MAINWIN_DEFAULT_POS_X,
    .player_y = MAINWIN_DEFAULT_POS_Y,
    .equalizer_x = EQUALIZER_DEFAULT_POS_X,
    .equalizer_y = EQUALIZER_DEFAULT_POS_Y,
    .playlist_x = PLAYLISTWIN_DEFAULT_POS_X,
    .playlist_y = PLAYLISTWIN_DEFAULT_POS_Y,
    .playlist_width = PLAYLISTWIN_DEFAULT_WIDTH,
    .playlist_height = PLAYLISTWIN_DEFAULT_HEIGHT,
    .playlist_position = 0,
    .colorize_r = 255, .colorize_g = 255, .colorize_b = 255,
    .analyzer_peaks = TRUE,
    .twoway_scroll = TRUE,             /* use back and forth scroll */
    .mainwin_use_bitmapfont = TRUE,
    .eq_scaled_linked = TRUE,
    .show_separator_in_pl = TRUE,
    .playlist_font = NULL,
    .mainwin_font = NULL,
    .random_skin_on_play = FALSE,
};

typedef struct skins_cfg_boolent_t {
    char const *be_vname;
    gboolean *be_vloc;
    gboolean be_wrt;
} skins_cfg_boolent;

static skins_cfg_boolent skins_boolents[] = {
    {"always_show_cb", &config.always_show_cb, TRUE},
    {"always_on_top", &config.always_on_top, TRUE},
    {"sticky", &config.sticky, TRUE},
    {"always_show_cb", &config.always_show_cb, TRUE},
    {"scaled", &config.scaled, TRUE},
    {"autoscroll_songname", &config.autoscroll, TRUE},
    {"equalizer_visible", &config.equalizer_visible, TRUE},
    {"playlist_visible", &config.playlist_visible, TRUE},
    {"player_visible", &config.player_visible, TRUE},
    {"player_shaded", &config.player_shaded, TRUE},
    {"equalizer_shaded", &config.equalizer_shaded, TRUE},
    {"playlist_shaded", &config.playlist_shaded, TRUE},
    {"dim_titlebar", &config.dim_titlebar, TRUE},
    {"show_wm_decorations", &config.show_wm_decorations, TRUE},
    {"easy_move", &config.easy_move, TRUE},
    {"allow_broken_skins", &config.allow_broken_skins, TRUE},
    {"disable_inline_gtk", &config.disable_inline_gtk, TRUE},
    {"analyzer_peaks", &config.analyzer_peaks, TRUE},
    {"twoway_scroll", &config.twoway_scroll, TRUE},
    {"warn_about_win_visibility", &config.warn_about_win_visibility, TRUE},
    {"warn_about_broken_gtk_engines", &config.warn_about_broken_gtk_engines, TRUE},
    {"mainwin_use_bitmapfont", &config.mainwin_use_bitmapfont, TRUE},
    {"eq_scaled_linked", &config.eq_scaled_linked, TRUE},
    {"show_separator_in_pl", &config.show_separator_in_pl, TRUE},
    {"random_skin_on_play", &config.random_skin_on_play, TRUE},
};

static gint ncfgbent = G_N_ELEMENTS(skins_boolents);

typedef struct skins_cfg_nument_t {
    char const *ie_vname;
    gint *ie_vloc;
    gboolean ie_wrt;
} skins_cfg_nument;

static skins_cfg_nument skins_numents[] = {
    {"player_x", &config.player_x, TRUE},
    {"player_y", &config.player_y, TRUE},
    {"timer_mode", &config.timer_mode, TRUE},
    {"vis_type", &config.vis_type, TRUE},
    {"analyzer_mode", &config.analyzer_mode, TRUE},
    {"analyzer_type", &config.analyzer_type, TRUE},
    {"scope_mode", &config.scope_mode, TRUE},
    {"vu_mode", &config.vu_mode, TRUE},
    {"voiceprint_mode", &config.voiceprint_mode, TRUE},
    {"analyzer_falloff", &config.analyzer_falloff, TRUE},
    {"peaks_falloff", &config.peaks_falloff, TRUE},
    {"playlist_x", &config.playlist_x, TRUE},
    {"playlist_y", &config.playlist_y, TRUE},
    {"playlist_width", &config.playlist_width, TRUE},
    {"playlist_height", &config.playlist_height, TRUE},
    {"playlist_position", &config.playlist_position, TRUE},
    {"equalizer_x", &config.equalizer_x, TRUE},
    {"equalizer_y", &config.equalizer_y, TRUE},
    {"colorize_r", &config.colorize_r, TRUE},
    {"colorize_g", &config.colorize_g, TRUE},
    {"colorize_b", &config.colorize_b, TRUE},
};

static gint ncfgient = G_N_ELEMENTS(skins_numents);

typedef struct skins_cfg_strent_t {
    char const *se_vname;
    char **se_vloc;
    gboolean se_wrt;
} skins_cfg_strent;

static skins_cfg_strent skins_strents[] = {
    {"playlist_font", &config.playlist_font, TRUE},
    {"mainwin_font", &config.mainwin_font, TRUE},
    {"skin", &config.skin, FALSE},
};

static gint ncfgsent = G_N_ELEMENTS(skins_strents);

static void reload_skin (void);

void skins_cfg_free() {
    gint i;
    for (i = 0; i < ncfgsent; ++i) {
        if (*(skins_strents[i].se_vloc) != NULL) {
            g_free( *(skins_strents[i].se_vloc) );
            *(skins_strents[i].se_vloc) = NULL;
        }
    }
}

void skins_cfg_load() {
    mcs_handle_t *cfgfile = aud_cfg_db_open();

    memcpy(&config, &skins_default_config, sizeof(skins_cfg_t));
    int i;

    for (i = 0; i < ncfgbent; ++i) {
        aud_cfg_db_get_bool(cfgfile, "skins",
                            skins_boolents[i].be_vname,
                            skins_boolents[i].be_vloc);
    }

    for (i = 0; i < ncfgient; ++i) {
        aud_cfg_db_get_int(cfgfile, "skins",
                           skins_numents[i].ie_vname,
                           skins_numents[i].ie_vloc);
    }

    for (i = 0; i < ncfgsent; ++i) {
        aud_cfg_db_get_string(cfgfile, "skins",
                              skins_strents[i].se_vname,
                              skins_strents[i].se_vloc);
    }

    if (!config.mainwin_font)
        config.mainwin_font = g_strdup(MAINWIN_DEFAULT_FONT);

    if (!config.playlist_font)
        config.playlist_font = g_strdup(PLAYLISTWIN_DEFAULT_FONT);

    if (!aud_cfg_db_get_float(cfgfile, "skins", "scale_factor", &(config.scale_factor)))
        config.scale_factor = 2.0;

    aud_cfg_db_close(cfgfile);
}


void skins_cfg_save() {
    mcs_handle_t *cfgfile = aud_cfg_db_open();

    if (aud_active_skin != NULL) {
        if (aud_active_skin->path)
            aud_cfg_db_set_string(cfgfile, "skins", "skin", aud_active_skin->path);
        else
            aud_cfg_db_unset_key(cfgfile, "skins", "skin");
    }

    int i;

    for (i = 0; i < ncfgsent; ++i) {
        if (skins_strents[i].se_wrt)
            aud_cfg_db_set_string(cfgfile, "skins",
                                  skins_strents[i].se_vname,
                                  *skins_strents[i].se_vloc);
    }

    for (i = 0; i < ncfgbent; ++i)
        if (skins_boolents[i].be_wrt)
            aud_cfg_db_set_bool(cfgfile, "skins",
                                skins_boolents[i].be_vname,
                                *skins_boolents[i].be_vloc);

    for (i = 0; i < ncfgient; ++i)
        if (skins_numents[i].ie_wrt)
            aud_cfg_db_set_int(cfgfile, "skins",
                               skins_numents[i].ie_vname,
                               *skins_numents[i].ie_vloc);

    aud_cfg_db_close(cfgfile);
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
    ui_skinned_playlist_set_font (playlistwin_list, config.playlist_font);
    playlistwin_set_sinfo_font(config.playlist_font);  /* propagate font setting to playlistwin_sinfo */
    playlistwin_update ();
}

static void
bitmap_fonts_cb()
{
    ui_skinned_textbox_set_xfont(mainwin_info, !config.mainwin_use_bitmapfont, config.mainwin_font);
    playlistwin_set_sinfo_font(config.playlist_font);

    if (config.playlist_shaded) {
        playlistwin_update ();
        ui_skinned_window_draw_all(playlistwin);
    }
}

static void
show_wm_decorations_cb()
{
    dock_window_set_decorated (mainwin);
    dock_window_set_decorated (playlistwin);
    dock_window_set_decorated (equalizerwin);
    mainwin_set_shape ();
    equalizerwin_set_shape ();
}

static PreferencesWidget font_table_elements[] = {
    {WIDGET_FONT_BTN, N_("_Player:"), &config.mainwin_font, G_CALLBACK(mainwin_font_set_cb), NULL, FALSE, {.font_btn = {N_("Select main player window font:")}}},
    {WIDGET_FONT_BTN, N_("_Playlist:"), &config.playlist_font, G_CALLBACK(playlist_font_set_cb), NULL, FALSE, {.font_btn = {N_("Select playlist font:")}}},
};

static PreferencesWidget appearance_misc_widgets[] = {
    {WIDGET_LABEL, N_("<b>_Fonts</b>"), NULL, NULL, NULL, FALSE},
    {WIDGET_TABLE, NULL, NULL, NULL, NULL, TRUE, {.table = {font_table_elements, G_N_ELEMENTS(font_table_elements)}}},
    {WIDGET_CHK_BTN, N_("Use Bitmap fonts if available"), &config.mainwin_use_bitmapfont, G_CALLBACK(bitmap_fonts_cb), N_("Use bitmap fonts if they are available. Bitmap fonts do not support Unicode strings."), FALSE},
    {WIDGET_LABEL, N_("<b>_Miscellaneous</b>"), NULL, NULL, NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Show separators in playlist"), &config.show_separator_in_pl,
     (GCallback) playlistwin_update, 0, 0},
    {WIDGET_CHK_BTN, N_("Show window manager decoration"), &config.show_wm_decorations, G_CALLBACK(show_wm_decorations_cb),
        N_("This enables the window manager to show decorations for windows."), FALSE},
    {WIDGET_CHK_BTN, N_("Use two-way text scroller"), &config.twoway_scroll, NULL,
        N_("If selected, the file information text in the main window will scroll back and forth. If not selected, the text will only scroll in one direction."), FALSE},
    {WIDGET_CHK_BTN, N_("Disable inline gtk theme"), &config.disable_inline_gtk,
     reload_skin, NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Random skin on play"), &config.random_skin_on_play, NULL, NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Allow loading incomplete skins"), &config.allow_broken_skins, NULL,
        N_("If selected, audacious won't refuse loading broken skins. Use only if your favourite skin doesn't work"), FALSE},
};

static gboolean
on_skin_view_realize(GtkTreeView * treeview,
                     gpointer data)
{
    skin_view_realize(treeview);
    skin_view_update ((GtkTreeView *) skin_view);

    return TRUE;
}

static void
reload_skin()
{
    /* reload the skin to apply the change */
    skin_reload_forced();
    ui_skinned_window_draw_all(mainwin);
    ui_skinned_window_draw_all(equalizerwin);
    ui_skinned_window_draw_all(playlistwin);
}

static void
on_red_scale_value_changed(GtkHScale *scale, gpointer data)
{
    config.colorize_r = gtk_range_get_value(GTK_RANGE(scale));
    reload_skin();
}

static void
on_green_scale_value_changed(GtkHScale *scale, gpointer data)
{
    config.colorize_g = gtk_range_get_value(GTK_RANGE(scale));
    reload_skin();
}

static void
on_blue_scale_value_changed(GtkHScale *scale, gpointer data)
{
    config.colorize_b = gtk_range_get_value(GTK_RANGE(scale));
    reload_skin();
}

static void
on_colorize_close_clicked(GtkButton *button, gpointer data)
{
    gtk_widget_destroy(colorize_settings);
    colorize_settings = NULL;
}

void
create_colorize_settings(void)
{
    GtkWidget *vbox;
    GtkWidget *label;
    GtkWidget *table;
    GtkWidget *hbuttonbox;
    GtkWidget *colorize_close;

    GtkWidget *green_label;
    GtkWidget *red_label;
    GtkWidget *blue_label;

    colorize_settings = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(colorize_settings), 12);
    gtk_window_set_title(GTK_WINDOW(colorize_settings), _("Color Adjustment"));
    gtk_window_set_type_hint(GTK_WINDOW(colorize_settings), GDK_WINDOW_TYPE_HINT_DIALOG);

    vbox = gtk_vbox_new(FALSE, 12);
    gtk_container_add(GTK_CONTAINER(colorize_settings), vbox);

    label = gtk_label_new(_("Audacious allows you to alter the color balance of the skinned UI. The sliders below will allow you to do this."));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

    table = gtk_table_new(3, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    gtk_table_set_row_spacings(GTK_TABLE(table), 6);
    gtk_table_set_col_spacings(GTK_TABLE(table), 12);

    blue_label = gtk_label_new(_("Blue"));
    gtk_table_attach(GTK_TABLE(table), blue_label, 0, 1, 2, 3,
                     (GtkAttachOptions) (0),
                     (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(blue_label), GTK_JUSTIFY_RIGHT);
    gtk_misc_set_alignment(GTK_MISC(blue_label), 1, 0.5);

    green_label = gtk_label_new(_("Green"));
    gtk_table_attach(GTK_TABLE(table), green_label, 0, 1, 1, 2,
                     (GtkAttachOptions) (0),
                     (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(green_label), GTK_JUSTIFY_RIGHT);
    gtk_misc_set_alignment(GTK_MISC(green_label), 1, 0.5);

    red_label = gtk_label_new(_("Red"));
    gtk_table_attach(GTK_TABLE(table), red_label, 0, 1, 0, 1,
                     (GtkAttachOptions) (0),
                     (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(red_label), GTK_JUSTIFY_RIGHT);
    gtk_misc_set_alignment(GTK_MISC(red_label), 1, 0.5);

    red_scale = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 255, 0, 0, 0)));
    gtk_table_attach(GTK_TABLE(table), red_scale, 1, 2, 0, 1,
                     (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                     (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
    gtk_scale_set_draw_value(GTK_SCALE(red_scale), FALSE);
    gtk_scale_set_digits(GTK_SCALE(red_scale), 3);

    green_scale = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 255, 0, 0, 0)));
    gtk_table_attach(GTK_TABLE(table), green_scale, 1, 2, 1, 2,
                     (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                     (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
    gtk_scale_set_draw_value(GTK_SCALE(green_scale), FALSE);
    gtk_scale_set_digits(GTK_SCALE(green_scale), 3);

    blue_scale = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 255, 0, 0, 0)));
    gtk_table_attach(GTK_TABLE(table), blue_scale, 1, 2, 2, 3,
                     (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                     (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
    gtk_scale_set_draw_value(GTK_SCALE(blue_scale), FALSE);
    gtk_scale_set_digits(GTK_SCALE(blue_scale), 3);

    hbuttonbox = gtk_hbutton_box_new();
    gtk_box_pack_start(GTK_BOX(vbox), hbuttonbox, FALSE, FALSE, 0);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbuttonbox), 6);

    colorize_close = gtk_button_new_from_stock("gtk-close");
    gtk_container_add(GTK_CONTAINER(hbuttonbox), colorize_close);
    gtk_widget_set_can_default (colorize_close, TRUE);

    g_signal_connect((gpointer) red_scale, "value_changed",
                     G_CALLBACK(on_red_scale_value_changed),
                     NULL);
    g_signal_connect((gpointer) green_scale, "value_changed",
                     G_CALLBACK(on_green_scale_value_changed),
                     NULL);
    g_signal_connect((gpointer) blue_scale, "value_changed",
                     G_CALLBACK(on_blue_scale_value_changed),
                     NULL);
    g_signal_connect((gpointer) colorize_close, "clicked",
                     G_CALLBACK(on_colorize_close_clicked),
                     NULL);

    gtk_range_set_value(GTK_RANGE(red_scale), config.colorize_r);
    gtk_range_set_value(GTK_RANGE(green_scale), config.colorize_g);
    gtk_range_set_value(GTK_RANGE(blue_scale), config.colorize_b);

    gtk_widget_grab_default(colorize_close);
    gtk_widget_show_all(colorize_settings);
}

static void
on_colorize_button_clicked(GtkButton *button, gpointer data)
{
    if (colorize_settings)
        gtk_window_present(GTK_WINDOW(colorize_settings));
    else
        create_colorize_settings();
}

void
on_skin_view_drag_data_received(GtkWidget * widget,
                                GdkDragContext * context,
                                gint x, gint y,
                                GtkSelectionData * selection_data,
                                guint info, guint time,
                                gpointer user_data)
{
    mcs_handle_t *db;
    gchar *path;

    if (! gtk_selection_data_get_data (selection_data))
    {
        g_warning("DND data string is NULL");
        return;
    }

    path = (gchar *) gtk_selection_data_get_data (selection_data);

    /* FIXME: use a real URL validator/parser */

    if (str_has_prefix_nocase(path, "file:///")) {
        path[strlen(path) - 2] = 0; /* Why the hell a CR&LF? */
        path += 7;
    }
    else if (str_has_prefix_nocase(path, "file:")) {
        path += 5;
    }

    if (file_is_archive(path)) {
        if (!aud_active_skin_load(path))
            return;
        skin_install_skin(path);
        skin_view_update ((GtkTreeView *) widget);

        /* Change skin name in the config file */
        db = aud_cfg_db_open();
        aud_cfg_db_set_string(db, "skins", "skin", path);
        aud_cfg_db_close(db);
    }
}

static GtkWidget * config_window = NULL;

void skins_configure (void)
{
    if (config_window)
    {
        gtk_window_present ((GtkWindow *) config_window);
        return;
    }

    GtkWidget *appearance_page_vbox;
    GtkWidget *vbox37;
    GtkWidget *vbox38;
    GtkWidget *hbox12;
    GtkWidget *alignment94;
    GtkWidget *hbox13;
    GtkWidget *label103;
    GtkWidget *colorspace_button;
    GtkWidget *alignment95;
    GtkWidget *skin_view_scrolled_window;

    appearance_page_vbox = gtk_vbox_new (FALSE, 0);

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

    colorspace_button = gtk_button_new_with_label (_("Color adjustment ..."));
    gtk_button_set_image ((GtkButton *) colorspace_button,
     gtk_image_new_from_stock (GTK_STOCK_COLOR_PICKER, GTK_ICON_SIZE_BUTTON));
    gtk_box_pack_start (GTK_BOX (hbox13), colorspace_button, FALSE, FALSE, 0);

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
    g_signal_connect_after(G_OBJECT(skin_view), "realize",
                           G_CALLBACK(on_skin_view_realize),
                           NULL);

    GtkWidget * hbox = gtk_hbox_new (FALSE, 6);
    gtk_box_pack_start ((GtkBox *) appearance_page_vbox, hbox, FALSE, FALSE, 0);

    GtkWidget * button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
    g_signal_connect (button, "clicked", (GCallback) skins_configure_cleanup,
     NULL);
    gtk_widget_set_can_default (button, TRUE);
    gtk_box_pack_end ((GtkBox *) hbox, button, FALSE, FALSE, 0);

    config_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (config_window, "destroy", (GCallback)
     gtk_widget_destroyed, & config_window);

    gtk_window_set_type_hint ((GtkWindow *) config_window,
     GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_title ((GtkWindow *) config_window, _("Interface Preferences"));
    gtk_window_set_resizable ((GtkWindow *) config_window, FALSE);
    gtk_container_set_border_width ((GtkContainer *) config_window, 6);

    gtk_container_add ((GtkContainer *) config_window, appearance_page_vbox);
    gtk_widget_show_all (config_window);
}

void skins_configure_cleanup (void)
{
    if (config_window)
        gtk_widget_destroy (config_window);
}
