/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2008  Audacious development team
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>

#include "gtkui_cfg.h"
#include "ui_gtk.h"
#include "ui_playlist_widget.h"
#include "ui_playlist_model.h"
#include "ui_manager.h"

static GtkWidget *window;       /* the main window */
static GtkWidget *label_time;
static GtkWidget *slider;
static GtkWidget *playlist_notebook;
static GtkWidget *volume;
static GtkWidget *vispane_root = NULL;

static gulong slider_change_handler_id;
static gboolean slider_is_moving = FALSE;
static gint slider_position;
static guint update_song_timeout_source = 0;

static gulong volume_change_handler_id;

static gboolean _ui_initialize(InterfaceCbs * cbs);
static gboolean _ui_finalize(void);

Interface gtkui_interface = {
    .id = "gtkui",
    .desc = N_("GTK Foobar-like Interface"),
    .init = _ui_initialize,
    .fini = _ui_finalize,
};

SIMPLE_INTERFACE_PLUGIN("gtkui", &gtkui_interface);

static struct index *pages;

static void ui_playlist_create_tab(gint playlist)
{
    GtkWidget *scrollwin, *treeview;
    GtkWidget *label;

    scrollwin = gtk_scrolled_window_new(NULL, NULL);
    index_insert(pages, playlist, scrollwin);

    treeview = ui_playlist_widget_new(playlist);
    g_object_set_data(G_OBJECT(scrollwin), "treeview", treeview);

    gtk_container_add(GTK_CONTAINER(scrollwin), treeview);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwin), GTK_SHADOW_IN);
    gtk_widget_show_all(scrollwin);

    label = gtk_label_new(aud_playlist_get_title(playlist));
    gtk_notebook_append_page(GTK_NOTEBOOK(playlist_notebook), scrollwin, label);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(playlist_notebook), index_count(pages) > 1 ? TRUE : FALSE);
}

static void ui_playlist_destroy_tab(gint playlist)
{
    GtkWidget *page = index_get(pages, playlist);

    gtk_notebook_remove_page(GTK_NOTEBOOK(playlist_notebook), gtk_notebook_page_num(GTK_NOTEBOOK(playlist_notebook), page));
    index_delete(pages, playlist, 1);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(playlist_notebook), index_count(pages) > 1 ? TRUE : FALSE);
}

static void ui_run_gtk_plugin(GtkWidget *parent, const gchar *name)
{
    GtkWidget *label;

    g_return_if_fail(parent != NULL);
    g_return_if_fail(name != NULL);

    switch (config.vis_position) {
        case VIS_ON_TOP:
        case VIS_ON_BOTTOM:
        case VIS_ON_LEFT:
        case VIS_ON_RIGHT: {
                /* FIXME: nested panes */
                GtkWidget *w = gtk_paned_get_child1(GTK_PANED(vispane_root));
                if (w == NULL) {
                    gtk_paned_add1(GTK_PANED(vispane_root), parent);
                    break;
                }

                w = gtk_paned_get_child2(GTK_PANED(vispane_root));
                if (w == NULL) {
                    gtk_paned_add2(GTK_PANED(vispane_root), parent);
                    break;
                }
            }
            break;
        case VIS_IN_TABS:
        default:
            label = gtk_label_new(name);
            gtk_notebook_append_page(GTK_NOTEBOOK(playlist_notebook), parent, label);
    }
}

static void ui_stop_gtk_plugin(GtkWidget *parent)
{
    switch (config.vis_position) {
        case VIS_ON_TOP:
        case VIS_ON_BOTTOM:
        case VIS_ON_LEFT:
        case VIS_ON_RIGHT: {
                /* FIXME: nested panes */
                GtkWidget *w = gtk_paned_get_child1(GTK_PANED(vispane_root));
                if (w == parent) {
                    gtk_container_remove(GTK_CONTAINER(vispane_root), w);
                    break;
                }

                w = gtk_paned_get_child2(GTK_PANED(vispane_root));
                if (w == parent) {
                    gtk_container_remove(GTK_CONTAINER(vispane_root), w);
                    break;
                }
            }
            break;
        case VIS_IN_TABS:
        default:
            gtk_notebook_remove_page(GTK_NOTEBOOK(playlist_notebook), gtk_notebook_page_num(GTK_NOTEBOOK(playlist_notebook), parent));
    }
}

static void ui_playlist_change_tab(GtkNotebook * notebook, GtkNotebookPage * notebook_page, gint page_num, void *unused)
{
    GtkWidget *page = gtk_notebook_get_nth_page(notebook, page_num);
    GtkTreeView *treeview = g_object_get_data(G_OBJECT(page), "treeview");

    if (treeview != NULL) {
        GtkTreeModel *tree_model = gtk_tree_view_get_model(treeview);
        UiPlaylistModel *model = UI_PLAYLIST_MODEL(tree_model);

        aud_playlist_set_active(model->playlist);
    }
}

static void ui_populate_playlist_notebook(void)
{
    gint playlists = aud_playlist_count();
    gint count;

    pages = index_new();

    for (count = 0; count < playlists; count++)
        ui_playlist_create_tab(count);
}

static gboolean window_configured_cb(gpointer data)
{
    GtkWindow *window = GTK_WINDOW(data);
    gtk_window_get_position(window, &config.player_x, &config.player_y);
    gtk_window_get_size(window, &config.player_width, &config.player_height);

    return FALSE;
}

static gboolean window_delete()
{
    return FALSE;
}

static void window_destroy(GtkWidget * widget, gpointer data)
{
    gtk_main_quit();
}

void show_preferences_window(gboolean show)
{
    static GtkWidget **prefswin = NULL;

    if (show)
    {
        if ((prefswin != NULL) && (*prefswin != NULL))
        {
            gtk_window_present(GTK_WINDOW(*prefswin));
            return;
        }

        prefswin = gtkui_interface.ops->create_prefs_window();

        gtk_widget_show_all(*prefswin);
    }
    else
    {
        if ((prefswin != NULL) && (*prefswin != NULL))
        {
            gtkui_interface.ops->destroy_prefs_window();
        }
    }
}

static void button_open_pressed()
{
    audgui_run_filebrowser(TRUE);
}

static void button_add_pressed()
{
    audgui_run_filebrowser(FALSE);
}

static void button_play_pressed()
{
    audacious_drct_play();
}

static void button_pause_pressed()
{
    audacious_drct_pause();
}

static void button_stop_pressed()
{
    audacious_drct_stop();
    return;
}

static void button_previous_pressed()
{
    audacious_drct_pl_prev();
}

static void button_next_pressed()
{
    audacious_drct_pl_next();
}

static void ui_set_song_info(void *unused, void *another)
{
    gchar *title = aud_playback_get_title();
    gchar *title_s = g_strdup_printf("%s - Audacious", title);

    gint length = audacious_drct_get_length();

    if (!g_signal_handler_is_connected(slider, slider_change_handler_id))
        return;

    if (length == -1)
        return;

    /* block "value-changed" signal handler to prevent skipping track
       if the next track is shorter than the previous one --desowin */
    g_signal_handler_block(slider, slider_change_handler_id);
    gtk_range_set_range(GTK_RANGE(slider), 0.0, (gdouble) length);
    g_signal_handler_unblock(slider, slider_change_handler_id);

    gtk_widget_show(label_time);

    gtk_window_set_title(GTK_WINDOW(window), title_s);
    g_free(title_s);
}

static void ui_playlist_created(void *data, void *unused)
{
    ui_playlist_create_tab(GPOINTER_TO_INT(data));
}

static void ui_playlist_destroyed(void *data, void *unused)
{
    ui_playlist_destroy_tab(GPOINTER_TO_INT(data));
}

static void ui_mainwin_show()
{
    if (config.save_window_position)
        gtk_window_move(GTK_WINDOW(window), config.player_x, config.player_y);

    gtk_widget_show(window);
    gtk_window_present(GTK_WINDOW(window));
}

static void ui_mainwin_hide()
{
    if (config.save_window_position)
        gtk_window_get_position(GTK_WINDOW(window), &config.player_x, &config.player_y);

    gtk_widget_hide(window);
}


static void ui_mainwin_toggle_visibility(gpointer hook_data, gpointer user_data)
{
    gboolean show = GPOINTER_TO_INT(hook_data);

    config.player_visible = show;
    aud_cfg->player_visible = show;

    if (show)
    {
        ui_mainwin_show();
    }
    else
    {
        ui_mainwin_hide();
    }
}

static void ui_toggle_visibility(void)
{
    ui_mainwin_toggle_visibility(GINT_TO_POINTER(!config.player_visible), NULL);
}

static void ui_show_error(const gchar * markup)
{
    GtkWidget *dialog =
        gtk_message_dialog_new_with_markup(GTK_WINDOW(window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_OK,
                                           "%s",_(markup));

    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_widget_show(GTK_WIDGET(dialog));

    g_signal_connect_swapped(dialog, "response",
                             G_CALLBACK(gtk_widget_destroy),
                             dialog);
}

static void ui_update_time_info(gint time)
{
    gchar text[128];
    gint length = audacious_drct_get_length();

    time /= 1000;
    length /= 1000;

    g_snprintf(text, sizeof(text) / sizeof(gchar), "<tt><b>%d:%.2d/%d:%.2d</b></tt>", time / 60, time % 60, length / 60, length % 60);
    gtk_label_set_markup(GTK_LABEL(label_time), text);
}

static gboolean ui_update_song_info(gpointer hook_data, gpointer user_data)
{
    if (!audacious_drct_get_playing())
    {
        if (GTK_IS_WIDGET(slider))
            gtk_range_set_value(GTK_RANGE(slider), 0.0);
        return FALSE;
    }

    if (slider_is_moving)
        return TRUE;

    gint time = audacious_drct_get_time();

    if (!g_signal_handler_is_connected(slider, slider_change_handler_id))
        return TRUE;

    g_signal_handler_block(slider, slider_change_handler_id);
    gtk_range_set_value(GTK_RANGE(slider), (gdouble) time);
    g_signal_handler_unblock(slider, slider_change_handler_id);

    ui_update_time_info(time);

    return TRUE;
}

static void ui_clear_song_info()
{
    gtk_window_set_title(GTK_WINDOW(window), "Audacious");
    gtk_widget_hide(label_time);
    gtk_range_set_value(GTK_RANGE(slider), 0);
}

static gboolean ui_slider_value_changed_cb(GtkRange * range, gpointer user_data)
{
    gint seek_;

    seek_ = gtk_range_get_value(range);

    /* XXX: work around a horrible bug in playback_seek(), also
       we should do mseek here. --nenolod */
    audacious_drct_seek(seek_ != 0 ? seek_ : 1);

    slider_is_moving = FALSE;

    return TRUE;
}

static gboolean ui_slider_change_value_cb(GtkRange * range, GtkScrollType scroll)
{
    ui_update_time_info(gtk_range_get_value(range));
    return FALSE;
}

static gboolean ui_slider_button_press_cb(GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
    slider_is_moving = TRUE;
    slider_position = gtk_range_get_value(GTK_RANGE(widget));

    /* HACK: clicking with the left mouse button moves the slider
       to the location of the click. */
    if (event->button == 1)
        event->button = 2;

    return FALSE;
}

static gboolean ui_slider_button_release_cb(GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
    /* HACK: see ui_slider_button_press_cb */
    if (event->button == 1)
        event->button = 2;

    if (slider_position == (gint) gtk_range_get_value(GTK_RANGE(widget)))
        slider_is_moving = FALSE;

    return FALSE;
}

static gboolean ui_volume_value_changed_cb(GtkButton * button, gdouble volume, gpointer user_data)
{
    audacious_drct_set_volume((gint) volume, (gint) volume);

    return TRUE;
}

static void ui_playback_begin(gpointer hook_data, gpointer user_data)
{
    ui_update_song_info(NULL, NULL);

    /* update song info 4 times a second */
    update_song_timeout_source = g_timeout_add(250, (GSourceFunc) ui_update_song_info, NULL);
}

static void ui_playback_stop(gpointer hook_data, gpointer user_data)
{
    if (update_song_timeout_source)
    {
        g_source_remove(update_song_timeout_source);
        update_song_timeout_source = 0;
    }

    ui_clear_song_info();
}

static void ui_playback_end(gpointer hook_data, gpointer user_data)
{
    ui_update_song_info(NULL, NULL);
}

static GtkWidget *gtk_toolbar_button_add(GtkWidget * toolbar, void (*callback) (), const gchar * stock_id)
{
    GtkWidget *icon;
    GtkWidget *button = gtk_button_new();

    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    
    icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(button), icon);

    gtk_box_pack_start(GTK_BOX(toolbar), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(callback), NULL);

    return button;
}

static GtkWidget *gtk_markup_label_new(const gchar * str)
{
    GtkWidget *label = gtk_label_new(str);
    g_object_set(G_OBJECT(label), "use-markup", TRUE, NULL);
    return label;
}

static void ui_hooks_associate(void)
{
    aud_hook_associate("title change", ui_set_song_info, NULL);
    aud_hook_associate("playback seek", (HookFunction) ui_update_song_info, NULL);
    aud_hook_associate("playback begin", ui_playback_begin, NULL);
    aud_hook_associate("playback stop", ui_playback_stop, NULL);
    aud_hook_associate("playback end", ui_playback_end, NULL);
    aud_hook_associate("playlist insert", ui_playlist_created, NULL);
    aud_hook_associate("playlist delete", ui_playlist_destroyed, NULL);
    aud_hook_associate("mainwin show", ui_mainwin_toggle_visibility, NULL);
}

static void ui_hooks_disassociate(void)
{
    aud_hook_dissociate("title change", ui_set_song_info);
    aud_hook_dissociate("playback seek", (HookFunction) ui_update_song_info);
    aud_hook_dissociate("playback begin", ui_playback_begin);
    aud_hook_dissociate("playback stop", ui_playback_stop);
    aud_hook_dissociate("playback end", ui_playback_end);
    aud_hook_dissociate("playlist insert", ui_playlist_created);
    aud_hook_dissociate("playlist delete", ui_playlist_destroyed);
    aud_hook_dissociate("mainwin show", ui_mainwin_toggle_visibility);
}

static gboolean _ui_initialize(InterfaceCbs * cbs)
{
    GtkWidget *vbox;            /* the main vertical box */
    GtkWidget *tophbox;         /* box to contain toolbar and shbox */
    GtkWidget *buttonbox;       /* contains buttons like "open", "next" */
    GtkWidget *shbox;           /* box for volume control + slider + time combo --nenolod */
    GtkWidget *plbox;           /* box for playlist and volume control */
    GtkWidget *button_open, *button_add, *button_play, *button_pause, *button_stop, *button_previous, *button_next;
    GtkWidget *menu;
    GtkAccelGroup *accel;

    gint lvol = 0, rvol = 0;    /* Left and Right for the volume control */

    gtkui_cfg_load();

    audgui_set_default_icon();
    audgui_register_stock_icons();

    ui_manager_init();
    ui_manager_create_menus();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), MAINWIN_DEFAULT_WIDTH, MAINWIN_DEFAULT_HEIGHT);
    gtk_window_set_title(GTK_WINDOW(window), "Audacious");

    if (config.save_window_position && config.player_width && config.player_height)
        gtk_window_resize(GTK_WINDOW(window), config.player_width, config.player_height);

    if (config.save_window_position && config.player_x != -1)
        gtk_window_move(GTK_WINDOW(window), config.player_x, config.player_y);
    else
        gtk_window_move(GTK_WINDOW(window), MAINWIN_DEFAULT_POS_X, MAINWIN_DEFAULT_POS_Y);

    g_signal_connect(G_OBJECT(window), "configure-event", G_CALLBACK(window_configured_cb), window);
    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(window_delete), NULL);
    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(window_destroy), NULL);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    menu = ui_manager_get_menus();
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, TRUE, 0);

    accel = ui_manager_get_accel_group();
    gtk_window_add_accel_group(GTK_WINDOW(window), accel);

    tophbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), tophbox, FALSE, TRUE, 0);

    buttonbox = gtk_hbox_new(FALSE, 0);
    button_open = gtk_toolbar_button_add(buttonbox, button_open_pressed, GTK_STOCK_OPEN);
    button_add = gtk_toolbar_button_add(buttonbox, button_add_pressed, GTK_STOCK_ADD);
    button_play = gtk_toolbar_button_add(buttonbox, button_play_pressed, GTK_STOCK_MEDIA_PLAY);
    button_pause = gtk_toolbar_button_add(buttonbox, button_pause_pressed, GTK_STOCK_MEDIA_PAUSE);
    button_stop = gtk_toolbar_button_add(buttonbox, button_stop_pressed, GTK_STOCK_MEDIA_STOP);
    button_previous = gtk_toolbar_button_add(buttonbox, button_previous_pressed, GTK_STOCK_MEDIA_PREVIOUS);
    button_next = gtk_toolbar_button_add(buttonbox, button_next_pressed, GTK_STOCK_MEDIA_NEXT);

    gtk_box_pack_start(GTK_BOX(tophbox), buttonbox, FALSE, FALSE, 0);

    shbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tophbox), shbox, TRUE, TRUE, 0);

    slider = gtk_hscale_new(NULL);
    gtk_scale_set_draw_value(GTK_SCALE(slider), FALSE);
    /* TODO: make this configureable */
    gtk_range_set_update_policy(GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS);
    gtk_box_pack_start(GTK_BOX(shbox), slider, TRUE, TRUE, 0);

    label_time = gtk_markup_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(shbox), label_time, FALSE, FALSE, 5);

    volume = gtk_volume_button_new();
    gtk_scale_button_set_adjustment(GTK_SCALE_BUTTON(volume), GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 100, 1, 5, 0)));
    /* Set the default volume to the balance average.
       (I'll add balance control later) -Ryan */
    audacious_drct_get_volume(&lvol, &rvol);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume), (lvol + rvol) / 2);
    gtk_box_pack_start(GTK_BOX(shbox), volume, FALSE, FALSE, 0);

    plbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), plbox, TRUE, TRUE, 0);

    playlist_notebook = gtk_notebook_new();
    gtk_notebook_set_show_border(GTK_NOTEBOOK(playlist_notebook), FALSE);

    if (config.vis_position != VIS_IN_TABS)
    {
        GtkWidget *pane = NULL;

        g_print("vis not in tabs : %d\n", config.vis_position);

        switch (config.vis_position)
        {
            case VIS_ON_LEFT:
                pane = gtk_hpaned_new();
                gtk_paned_add2(GTK_PANED(pane), playlist_notebook);

                vispane_root = gtk_vpaned_new();
                gtk_paned_add1(GTK_PANED(pane), vispane_root);
                break;

            case VIS_ON_RIGHT:
                pane = gtk_hpaned_new();
                gtk_paned_add1(GTK_PANED(pane), playlist_notebook);

                vispane_root = gtk_vpaned_new();
                gtk_paned_add2(GTK_PANED(pane), vispane_root);
                break;

            case VIS_ON_TOP:
                pane = gtk_vpaned_new();
                gtk_paned_add2(GTK_PANED(pane), playlist_notebook);

                vispane_root = gtk_hpaned_new();
                gtk_paned_add1(GTK_PANED(pane), vispane_root);
                break;

            case VIS_ON_BOTTOM:
                pane = gtk_vpaned_new();
                gtk_paned_add1(GTK_PANED(pane), playlist_notebook);

                vispane_root = gtk_hpaned_new();
                gtk_paned_add2(GTK_PANED(pane), vispane_root);
                break;

            default:
                /* invalid vis_position, just display playlist_notebook */
                pane = playlist_notebook;
        }

        if (pane != NULL)
            gtk_box_pack_end(GTK_BOX(plbox), pane, TRUE, TRUE, 0);
    }
    else {
        g_print("vis in tabs\n");
        gtk_box_pack_end(GTK_BOX(plbox), playlist_notebook, TRUE, TRUE, 0);
    }

    ui_hooks_associate();
    ui_populate_playlist_notebook();

    g_signal_connect(playlist_notebook, "switch-page", G_CALLBACK(ui_playlist_change_tab), NULL);

    slider_change_handler_id = g_signal_connect(slider, "value-changed", G_CALLBACK(ui_slider_value_changed_cb), NULL);

    g_signal_connect(slider, "change-value", G_CALLBACK(ui_slider_change_value_cb), NULL);
    g_signal_connect(slider, "button-press-event", G_CALLBACK(ui_slider_button_press_cb), NULL);
    g_signal_connect(slider, "button-release-event", G_CALLBACK(ui_slider_button_release_cb), NULL);

    volume_change_handler_id = g_signal_connect(volume, "value-changed", G_CALLBACK(ui_volume_value_changed_cb), NULL);

    ui_set_song_info(NULL, NULL);

    gtk_widget_show_all(vbox);

    if (config.player_visible)
        ui_mainwin_toggle_visibility(GINT_TO_POINTER(config.player_visible), NULL);

    ui_clear_song_info();

    if (audacious_drct_get_playing())
        ui_playback_begin(0, 0);

    /* Register interface callbacks */
    cbs->show_prefs_window = show_preferences_window;
    cbs->run_filebrowser = audgui_run_filebrowser;
    cbs->hide_filebrowser = audgui_hide_filebrowser;
    cbs->toggle_visibility = ui_toggle_visibility;
    cbs->show_error = ui_show_error;
    cbs->show_jump_to_track = audgui_jump_to_track;
    cbs->hide_jump_to_track = audgui_jump_to_track_hide;
    cbs->show_about_window = audgui_show_about_window;
    cbs->hide_about_window = audgui_hide_about_window;
    cbs->run_gtk_plugin = ui_run_gtk_plugin;
    cbs->stop_gtk_plugin = ui_stop_gtk_plugin;

    gtk_main();

    return TRUE;
}

static gboolean _ui_finalize(void)
{
    if (update_song_timeout_source)
    {
        g_source_remove(update_song_timeout_source);
        update_song_timeout_source = 0;
    }

    gtkui_cfg_save();
    gtkui_cfg_free();
    ui_hooks_disassociate();
    return TRUE;
}
