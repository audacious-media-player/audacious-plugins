/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2007  Audacious development team.
 *
 *  Based on BMP:
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

/*#define AUD_DEBUG*/

#include "config.h"

#include "ui_equalizer.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "platform/smartinclude.h"
#include "ui_skin.h"
#include "ui_manager.h"
#include "actions-equalizer.h"
#include "util.h"
#include "ui_main.h"
#include "ui_playlist.h"

#include <audacious/audconfig.h>
#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui-gtk.h>

#include "images/audacious_eq.xpm"

#include "ui_dock.h"
#include "ui_skinned_window.h"
#include "ui_skinned_button.h"
#include "ui_skinned_horizontal_slider.h"
#include "ui_skinned_equalizer_slider.h"
#include "ui_skinned_equalizer_graph.h"
#include "skins_cfg.h"

enum PresetViewCols {
    PRESET_VIEW_COL_NAME,
    PRESET_VIEW_N_COLS
};

GtkWidget *equalizerwin;
GtkWidget *equalizerwin_graph;

static GtkWidget *equalizerwin_load_window = NULL;
static GtkWidget *equalizerwin_load_auto_window = NULL;
static GtkWidget *equalizerwin_save_window = NULL;
static GtkWidget *equalizerwin_save_entry = NULL;
static GtkWidget *equalizerwin_save_auto_window = NULL;
static GtkWidget *equalizerwin_save_auto_entry = NULL;
static GtkWidget *equalizerwin_delete_window = NULL;
static GtkWidget *equalizerwin_delete_auto_window = NULL;

static GtkWidget *equalizerwin_on, *equalizerwin_auto;

static GtkWidget *equalizerwin_close, *equalizerwin_shade;
static GtkWidget *equalizerwin_shaded_close, *equalizerwin_shaded_shade;
static GtkWidget *equalizerwin_presets;
static GtkWidget *equalizerwin_preamp,*equalizerwin_bands[10];
static GtkWidget *equalizerwin_volume, *equalizerwin_balance;

static GList *equalizer_presets = NULL, *equalizer_auto_presets = NULL;

EqualizerPreset *
equalizer_preset_new(const gchar * name)
{
    EqualizerPreset *preset = g_new0(EqualizerPreset, 1);
    preset->name = g_strdup(name);
    return preset;
}

void
equalizer_preset_free(EqualizerPreset * preset)
{
    if (!preset)
        return;

    g_free(preset->name);
    g_free(preset);
}

void equalizerwin_set_shape (void)
{
    if (config.show_wm_decorations)
        gtk_widget_shape_combine_mask (equalizerwin, 0, 0, 0);
    else
        gtk_widget_shape_combine_mask (equalizerwin, skin_get_mask
         (aud_active_skin, config.equalizer_shaded ? SKIN_MASK_EQ_SHADE :
         SKIN_MASK_EQ), 0, 0);
}

void
equalizerwin_set_scaled(gboolean ds)
{
    gint height;
    GList * list;
    SkinnedWindow * skinned;
    GtkFixed * fixed;
    GtkFixedChild * child;

    if (config.equalizer_shaded)
        height = 14;
    else
        height = 116;

    if (config.scaled)
        resize_window(equalizerwin, 275 * config.scale_factor, height *
         config.scale_factor);
    else
        resize_window(equalizerwin, 275, height);

    skinned = (SkinnedWindow *) equalizerwin;
    fixed = (GtkFixed *) skinned->normal;

    for (list = fixed->children; list; list = list->next)
    {
        child = (GtkFixedChild *) list->data;
        g_signal_emit_by_name ((GObject *) child->widget, "toggle-scaled");
    }

    fixed = (GtkFixed *) skinned->shaded;

    for (list = fixed->children; list; list = list->next)
    {
        child = (GtkFixedChild *) list->data;
        g_signal_emit_by_name ((GObject *) child->widget, "toggle-scaled");
    }

    equalizerwin_set_shape ();
}

void
equalizerwin_set_shade_menu_cb(gboolean shaded)
{
    config.equalizer_shaded = shaded;
    ui_skinned_window_set_shade(equalizerwin, shaded);

    if (shaded) {
        dock_shade(get_dock_window_list(), GTK_WINDOW(equalizerwin),
                   14 * EQUALIZER_SCALE_FACTOR);
    } else {
        dock_shade(get_dock_window_list(), GTK_WINDOW(equalizerwin),
                   116 * EQUALIZER_SCALE_FACTOR);
    }

    equalizerwin_set_shape ();
}

static void
equalizerwin_set_shade(gboolean shaded)
{
    GtkAction *action = gtk_action_group_get_action(
      toggleaction_group_others , "roll up equalizer" );
    gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(action) , shaded );
}

static void
equalizerwin_shade_toggle(void)
{
    equalizerwin_set_shade(!config.equalizer_shaded);
}

void
equalizerwin_eq_changed(void)
{
    gint i;

    aud_cfg->equalizer_preamp = equalizerwin_get_preamp();
    for (i = 0; i < AUD_EQUALIZER_NBANDS; i++)
        aud_cfg->equalizer_bands[i] = equalizerwin_get_band(i);

    hook_call("equalizer changed", NULL);
}

static void
equalizerwin_apply_preset(EqualizerPreset *preset)
{
    if (preset == NULL)
       return;

    gint i;

    equalizerwin_set_preamp(preset->preamp);
    for (i = 0; i < AUD_EQUALIZER_NBANDS; i++)
        equalizerwin_set_band(i, preset->bands[i]);
}

static void
equalizerwin_on_pushed(void)
{
    equalizerwin_activate(!aud_cfg->equalizer_active);
}

static void
update_from_config(void *unused1, void *unused2)
{
    gint i;

    ui_skinned_button_set_inside(equalizerwin_on, aud_cfg->equalizer_active);
    ui_skinned_equalizer_slider_set_position(equalizerwin_preamp, aud_cfg->equalizer_preamp);
    for (i = 0; i < AUD_EQUALIZER_NBANDS; i++)
        ui_skinned_equalizer_slider_set_position(equalizerwin_bands[i], aud_cfg->equalizer_bands[i]);
    ui_skinned_equalizer_graph_update(equalizerwin_graph);
}

static void
equalizerwin_presets_pushed(void)
{
    GdkModifierType modmask;
    gint x, y;

    gdk_window_get_pointer(NULL, &x, &y, &modmask);
    ui_popup_menu_show(UI_MENU_EQUALIZER_PRESET, x, y, FALSE, FALSE, 1,
     GDK_CURRENT_TIME);
}

static void
equalizerwin_auto_pushed(void)
{
    aud_cfg->equalizer_autoload = UI_SKINNED_BUTTON(equalizerwin_auto)->inside;
}

static GtkWidget * get_eq_effects_menu (void)
{
    static GtkWidget * menu = NULL;

    if (menu == NULL)
        menu = audgui_create_effects_menu ();

    return menu;
}

gboolean
equalizerwin_press(GtkWidget * widget, GdkEventButton * event,
                   gpointer callback_data)
{
    if (event->button == 1 && event->type == GDK_2BUTTON_PRESS
             && event->y < 14) {
        equalizerwin_set_shade(!config.equalizer_shaded);
        if (dock_is_moving(GTK_WINDOW(equalizerwin)))
            dock_move_release(GTK_WINDOW(equalizerwin));
        return TRUE;
    }

    if (event->button == 3)
    {
        gtk_menu_popup ((GtkMenu *) get_eq_effects_menu (), NULL, NULL, NULL,
         NULL, 3, event->time);
        return TRUE;
    }

    return FALSE;
}

static void
equalizerwin_close_cb(void)
{
    equalizerwin_show(FALSE);
}

static gboolean equalizerwin_delete(GtkWidget *widget, void *data)
{
    if (config.show_wm_decorations)
        equalizerwin_show(FALSE);
    else
        aud_drct_quit();

    return TRUE;
}

gint
equalizerwin_volume_frame_cb(gint pos)
{
    if (equalizerwin_volume) {
        gint x;
        if (pos < 32)
            x = 1;
        else if (pos < 63)
            x = 4;
        else
            x = 7;

        UI_SKINNED_HORIZONTAL_SLIDER(equalizerwin_volume)->knob_nx = x;
        UI_SKINNED_HORIZONTAL_SLIDER(equalizerwin_volume)->knob_px = x;
    }
    return 1;
}

static void
equalizerwin_volume_motion_cb(GtkWidget *widget, gint pos)
{
    gint v = (gint) rint(pos * 100 / 94.0);
    mainwin_adjust_volume_motion(v);
    mainwin_set_volume_slider(v);
}

static void
equalizerwin_volume_release_cb(GtkWidget *widget, gint pos)
{
    mainwin_adjust_volume_release();
}

static gint
equalizerwin_balance_frame_cb(gint pos)
{
    if (equalizerwin_balance) {
        gint x;
        if (pos < 13)
            x = 11;
        else if (pos < 26)
            x = 14;
        else
            x = 17;

        UI_SKINNED_HORIZONTAL_SLIDER(equalizerwin_balance)->knob_nx = x;
        UI_SKINNED_HORIZONTAL_SLIDER(equalizerwin_balance)->knob_px = x;
    }

    return 1;
}

static void
equalizerwin_balance_motion_cb(GtkWidget *widget, gint pos)
{
    gint b;
    pos = MIN(pos, 38);         /* The skin uses a even number of pixels
                                   for the balance-slider *sigh* */
    b = (gint) rint((pos - 19) * 100 / 19.0);
    mainwin_adjust_balance_motion(b);
    mainwin_set_balance_slider(b);
}

static void
equalizerwin_balance_release_cb(GtkWidget *widget, gint pos)
{
    mainwin_adjust_balance_release();
}

void
equalizerwin_set_balance_slider(gint percent)
{
    ui_skinned_horizontal_slider_set_position(equalizerwin_balance,
                         (gint) rint((percent * 19 / 100.0) + 19));
}

void
equalizerwin_set_volume_slider(gint percent)
{
    ui_skinned_horizontal_slider_set_position(equalizerwin_volume,
                         (gint) rint(percent * 94 / 100.0));
}

static void
equalizerwin_create_widgets(void)
{
    gint i;

    equalizerwin_on = ui_skinned_button_new();
    ui_skinned_toggle_button_setup(equalizerwin_on, SKINNED_WINDOW(equalizerwin)->normal,
                                   14, 18, 25, 12, 10, 119, 128, 119, 69, 119, 187, 119, SKIN_EQMAIN);
    g_signal_connect(equalizerwin_on, "clicked", equalizerwin_on_pushed, NULL);
    ui_skinned_button_set_inside (equalizerwin_on, aud_cfg->equalizer_active);

    equalizerwin_auto = ui_skinned_button_new();
    ui_skinned_toggle_button_setup(equalizerwin_auto, SKINNED_WINDOW(equalizerwin)->normal,
                                   39, 18, 33, 12, 35, 119, 153, 119, 94, 119, 212, 119, SKIN_EQMAIN);
    g_signal_connect(equalizerwin_auto, "clicked", equalizerwin_auto_pushed, NULL);
    ui_skinned_button_set_inside(equalizerwin_auto, aud_cfg->equalizer_autoload);

    equalizerwin_presets = ui_skinned_button_new();
    ui_skinned_push_button_setup(equalizerwin_presets, SKINNED_WINDOW(equalizerwin)->normal,
                                 217, 18, 44, 12, 224, 164, 224, 176, SKIN_EQMAIN);
    g_signal_connect(equalizerwin_presets, "clicked", equalizerwin_presets_pushed, NULL );

    equalizerwin_close = ui_skinned_button_new();
    ui_skinned_push_button_setup(equalizerwin_close, SKINNED_WINDOW(equalizerwin)->normal,
                                 264, 3, 9, 9, 0, 116, 0, 125, SKIN_EQMAIN);
    g_signal_connect(equalizerwin_close, "clicked", equalizerwin_close_cb, NULL );

    equalizerwin_shade = ui_skinned_button_new();
    ui_skinned_push_button_setup(equalizerwin_shade, SKINNED_WINDOW(equalizerwin)->normal,
                                 254, 3, 9, 9, 254, 137, 1, 38, SKIN_EQMAIN);
    ui_skinned_button_set_skin_index2(equalizerwin_shade, SKIN_EQ_EX);
    g_signal_connect(equalizerwin_shade, "clicked", equalizerwin_shade_toggle, NULL );

    equalizerwin_shaded_close = ui_skinned_button_new();
    ui_skinned_push_button_setup(equalizerwin_shaded_close, SKINNED_WINDOW(equalizerwin)->shaded,
                                 264, 3, 9, 9, 11, 38, 11, 47, SKIN_EQ_EX);
    g_signal_connect(equalizerwin_shaded_close, "clicked", equalizerwin_close_cb, NULL );

    equalizerwin_shaded_shade = ui_skinned_button_new();
    ui_skinned_push_button_setup(equalizerwin_shaded_shade, SKINNED_WINDOW(equalizerwin)->shaded,
                                 254, 3, 9, 9, 254, 3, 1, 47, SKIN_EQ_EX);
    g_signal_connect(equalizerwin_shaded_shade, "clicked", equalizerwin_shade_toggle, NULL );

    equalizerwin_graph = ui_skinned_equalizer_graph_new(SKINNED_WINDOW(equalizerwin)->normal, 86, 17);

    equalizerwin_preamp = ui_skinned_equalizer_slider_new(SKINNED_WINDOW(equalizerwin)->normal, 21, 38);
    ui_skinned_equalizer_slider_set_position (equalizerwin_preamp,
     aud_cfg->equalizer_preamp);

    for (i = 0; i < AUD_EQUALIZER_NBANDS; i++) {
        equalizerwin_bands[i] =
            ui_skinned_equalizer_slider_new(SKINNED_WINDOW(equalizerwin)->normal, 78 + (i * 18), 38);
        ui_skinned_equalizer_slider_set_position (equalizerwin_bands [i],
         aud_cfg->equalizer_bands [i]);
    }

    equalizerwin_volume =
        ui_skinned_horizontal_slider_new(SKINNED_WINDOW(equalizerwin)->shaded,
                                         61, 4, 97, 8, 1, 30, 1, 30, 3, 7, 4, 61, 0, 94,
                                         equalizerwin_volume_frame_cb, SKIN_EQ_EX);
    g_signal_connect(equalizerwin_volume, "motion", G_CALLBACK(equalizerwin_volume_motion_cb), NULL);
    g_signal_connect(equalizerwin_volume, "release", G_CALLBACK(equalizerwin_volume_release_cb), NULL);


    equalizerwin_balance =
        ui_skinned_horizontal_slider_new(SKINNED_WINDOW(equalizerwin)->shaded,
                       164, 4, 42, 8, 11, 30, 11, 30, 3, 7, 4, 164, 0, 39,
                       equalizerwin_balance_frame_cb, SKIN_EQ_EX);
    g_signal_connect(equalizerwin_balance, "motion", G_CALLBACK(equalizerwin_balance_motion_cb), NULL);
    g_signal_connect(equalizerwin_balance, "release", G_CALLBACK(equalizerwin_balance_release_cb), NULL);
}


static void
equalizerwin_create_window(void)
{
    GdkPixbuf *icon;
    gint width, height;

    width = 275;
    height = config.equalizer_shaded ? 14 : 116;

    equalizerwin = ui_skinned_window_new("equalizer", &config.equalizer_x, &config.equalizer_y);
    gtk_window_set_title(GTK_WINDOW(equalizerwin), _("Audacious Equalizer"));
    gtk_window_set_role(GTK_WINDOW(equalizerwin), "equalizer");
    gtk_window_set_resizable(GTK_WINDOW(equalizerwin), FALSE);

    if (config.scaled && config.eq_scaled_linked) {
        width *= config.scale_factor;
        height *= config.scale_factor;
    }

    gtk_widget_set_size_request(equalizerwin, width, height);

    /* this will hide only mainwin. it's annoying! yaz */
    gtk_window_set_transient_for(GTK_WINDOW(equalizerwin),
                                 GTK_WINDOW(mainwin));
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(equalizerwin), TRUE);

    icon = gdk_pixbuf_new_from_xpm_data((const gchar **) audacious_eq_icon);
    gtk_window_set_icon(GTK_WINDOW(equalizerwin), icon);
    g_object_unref(icon);

    gtk_widget_set_app_paintable(equalizerwin, TRUE);

    g_signal_connect(equalizerwin, "delete_event",
                     G_CALLBACK(equalizerwin_delete), NULL);
    g_signal_connect(equalizerwin, "button_press_event",
                     G_CALLBACK(equalizerwin_press), NULL);
    g_signal_connect ((GObject *) equalizerwin, "key-press-event", (GCallback)
     mainwin_keypress, 0);
}

void
equalizerwin_create(void)
{
    equalizer_presets = aud_equalizer_read_presets("eq.preset");
    equalizer_auto_presets = aud_equalizer_read_presets("eq.auto_preset");

    equalizerwin_create_window();

    gtk_window_add_accel_group( GTK_WINDOW(equalizerwin) , ui_manager_get_accel_group() );

    equalizerwin_create_widgets();

    hook_associate("equalizer changed", (HookFunction) update_from_config, NULL);

    gtk_widget_show_all (((SkinnedWindow *) equalizerwin)->normal);
    gtk_widget_show_all (((SkinnedWindow *) equalizerwin)->shaded);
}

static void equalizerwin_real_show (void)
{
    if (config.scaled && config.eq_scaled_linked)
        gtk_widget_set_size_request(equalizerwin, 275 * config.scale_factor,
                                    ((config.equalizer_shaded ? 14 : 116) * config.scale_factor));
    else
        gtk_widget_set_size_request(equalizerwin, 275,
                                    (config.equalizer_shaded ? 14 : 116));
    ui_skinned_button_set_inside(mainwin_eq, TRUE);

    gtk_window_present(GTK_WINDOW(equalizerwin));
}

static void equalizerwin_real_hide (void)
{
    gtk_widget_hide(equalizerwin);
    ui_skinned_button_set_inside(mainwin_eq, FALSE);
}

void
equalizerwin_show(gboolean show)
{
    GtkAction * a;

    a = gtk_action_group_get_action (toggleaction_group_others, "show equalizer");

    if (a && gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (a)) != show)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (a), show);
    else
    {
        if (show != config.equalizer_visible) {
            config.equalizer_visible = show;
            config.equalizer_visible_prev = !show;
            aud_cfg->equalizer_visible = show;
        }

        if (show)
           equalizerwin_real_show ();
        else
           equalizerwin_real_hide ();
    }
 }

static EqualizerPreset *
equalizerwin_find_preset(GList * list, const gchar * name)
{
    GList *node = list;
    EqualizerPreset *preset;

    while (node) {
        preset = node->data;
        if (!strcasecmp(preset->name, name))
            return preset;
        node = g_list_next(node);
    }
    return NULL;
}

static gboolean
equalizerwin_load_preset(GList * list, const gchar * name)
{
    EqualizerPreset *preset;
    gint i;

    if ((preset = equalizerwin_find_preset(list, name)) != NULL) {
        equalizerwin_set_preamp(preset->preamp);
        for (i = 0; i < AUD_EQUALIZER_NBANDS; i++)
            equalizerwin_set_band(i, preset->bands[i]);

        return TRUE;
    }
    return FALSE;
}

static GList *
equalizerwin_save_preset(GList * list, const gchar * name,
                         const gchar * filename)
{
    gint i;
    EqualizerPreset *preset;

    if (!(preset = equalizerwin_find_preset(list, name))) {
        preset = g_new0(EqualizerPreset, 1);
        preset->name = g_strdup(name);
        list = g_list_append(list, preset);
    }

    preset->preamp = equalizerwin_get_preamp();
    for (i = 0; i < AUD_EQUALIZER_NBANDS; i++)
        preset->bands[i] = equalizerwin_get_band(i);

    aud_equalizer_write_preset_file(list, filename);

    return list;
}

static GList *
equalizerwin_delete_preset(GList * list, gchar * name, gchar * filename)
{
    EqualizerPreset *preset;
    GList *node;

    if (!(preset = equalizerwin_find_preset(list, name)))
        return list;

    if (!(node = g_list_find(list, preset)))
        return list;

    list = g_list_remove_link(list, node);
    equalizer_preset_free(preset);
    g_list_free_1(node);

    aud_equalizer_write_preset_file(list, filename);

    return list;
}

static void
equalizerwin_delete_selected_presets(GtkTreeView *view, gchar *filename)
{
    gchar *text;

    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
    GtkTreeModel *model = gtk_tree_view_get_model(view);

    /*
     * first we are making a list of the selected rows, then we convert this
     * list into a list of GtkTreeRowReferences, so that when you delete an
     * item you can still access the other items
     * finally we iterate through all GtkTreeRowReferences, convert them to
     * GtkTreeIters and delete those one after the other
     */

    GList *list = gtk_tree_selection_get_selected_rows(selection, &model);
    GList *rrefs = NULL;
    GList *litr;

    for (litr = list; litr; litr = litr->next)
    {
        GtkTreePath *path = litr->data;
        rrefs = g_list_append(rrefs, gtk_tree_row_reference_new(model, path));
    }

    for (litr = rrefs; litr; litr = litr->next)
    {
        GtkTreeRowReference *ref = litr->data;
        GtkTreePath *path = gtk_tree_row_reference_get_path(ref);
        GtkTreeIter iter;
        gtk_tree_model_get_iter(model, &iter, path);

        gtk_tree_model_get(model, &iter, 0, &text, -1);

        if (!strcmp(filename, "eq.preset"))
            equalizer_presets = equalizerwin_delete_preset(equalizer_presets, text, filename);
        else if (!strcmp(filename, "eq.auto_preset"))
            equalizer_auto_presets = equalizerwin_delete_preset(equalizer_auto_presets, text, filename);

        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    }
}

static void
free_cb (gpointer data, gpointer user_data)
{
    equalizer_preset_free((EqualizerPreset*)data);
}

static void
equalizerwin_read_winamp_eqf(VFSFile * file)
{
    GList *presets;
    gint i;

    if ((presets = aud_import_winamp_eqf(file)) == NULL)
        return;

    /* just get the first preset --asphyx */
    EqualizerPreset *preset = (EqualizerPreset*)presets->data;
    equalizerwin_set_preamp(preset->preamp);

    for (i = 0; i < AUD_EQUALIZER_NBANDS; i++)
        equalizerwin_set_band(i, preset->bands[i]);

    g_list_foreach(presets, free_cb, NULL);
    g_list_free(presets);

    equalizerwin_eq_changed();
}

static gboolean equalizerwin_read_aud_preset (const gchar * file)
{
    EqualizerPreset * preset = aud_load_preset_file (file);

    if (preset == NULL)
        return FALSE;

    equalizerwin_apply_preset (preset);
    equalizer_preset_free (preset);
    return TRUE;
}

static void
equalizerwin_save_ok(GtkWidget * widget, gpointer data)
{
    const gchar *text;

    text = gtk_entry_get_text(GTK_ENTRY(equalizerwin_save_entry));
    if (strlen(text) != 0)
        equalizer_presets =
            equalizerwin_save_preset(equalizer_presets, text, "eq.preset");
    gtk_widget_destroy(equalizerwin_save_window);
}

static void
equalizerwin_save_select(GtkTreeView *treeview, GtkTreePath *path,
                         GtkTreeViewColumn *col, gpointer data)
{
    gchar *text;

    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (selection)
    {
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
            gtk_tree_model_get(model, &iter, 0, &text, -1);
            gtk_entry_set_text(GTK_ENTRY(equalizerwin_save_entry), text);
            equalizerwin_save_ok(NULL, NULL);

            g_free(text);
        }
    }
}

static void
equalizerwin_load_ok(GtkWidget *widget, gpointer data)
{
    gchar *text;

    GtkTreeView* view = GTK_TREE_VIEW(data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (selection)
    {
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
            gtk_tree_model_get(model, &iter, 0, &text, -1);
            equalizerwin_load_preset(equalizer_presets, text);

            g_free(text);
        }
    }
    gtk_widget_destroy(equalizerwin_load_window);
}

static void
equalizerwin_load_select(GtkTreeView *treeview, GtkTreePath *path,
                         GtkTreeViewColumn *col, gpointer data)
{
    equalizerwin_load_ok(NULL, treeview);
}

static void
equalizerwin_delete_delete(GtkWidget *widget, gpointer data)
{
    equalizerwin_delete_selected_presets(GTK_TREE_VIEW(data), "eq.preset");
}

static void
equalizerwin_save_auto_ok(GtkWidget *widget, gpointer data)
{
    const gchar *text;

    text = gtk_entry_get_text(GTK_ENTRY(equalizerwin_save_auto_entry));
    if (strlen(text) != 0)
        equalizer_auto_presets =
            equalizerwin_save_preset(equalizer_auto_presets, text,
                                     "eq.auto_preset");
    gtk_widget_destroy(equalizerwin_save_auto_window);
}

static void
equalizerwin_save_auto_select(GtkTreeView *treeview, GtkTreePath *path,
                              GtkTreeViewColumn *col, gpointer data)
{
    gchar *text;

    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (selection)
    {
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
            gtk_tree_model_get(model, &iter, 0, &text, -1);
            gtk_entry_set_text(GTK_ENTRY(equalizerwin_save_auto_entry), text);
            equalizerwin_save_auto_ok(NULL, NULL);

            g_free(text);
        }
    }
}

static void
equalizerwin_load_auto_ok(GtkWidget *widget, gpointer data)
{
    gchar *text;

    GtkTreeView *view = GTK_TREE_VIEW(data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (selection)
    {
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
            gtk_tree_model_get(model, &iter, 0, &text, -1);
            equalizerwin_load_preset(equalizer_auto_presets, text);

            g_free(text);
        }
    }
    gtk_widget_destroy(equalizerwin_load_auto_window);
}

static void
equalizerwin_load_auto_select(GtkTreeView *treeview, GtkTreePath *path,
                              GtkTreeViewColumn *col, gpointer data)
{
    equalizerwin_load_auto_ok(NULL, treeview);
}

static void
equalizerwin_delete_auto_delete(GtkWidget *widget, gpointer data)
{
    equalizerwin_delete_selected_presets(GTK_TREE_VIEW(data), "eq.auto_preset");
}

static VFSFile *
open_vfs_file(const gchar *filename, const gchar *mode)
{
    VFSFile *file;
    GtkWidget *dialog;

    if (!(file = vfs_fopen(filename, mode))) {
        dialog = gtk_message_dialog_new (GTK_WINDOW (mainwin),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "Error loading file '%s'",
                                         filename);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
    }

    return file;
}

static void
load_winamp_file(const gchar * filename)
{
    VFSFile *file;

    if (!(file = open_vfs_file(filename, "rb")))
        return;

    equalizerwin_read_winamp_eqf(file);
    vfs_fclose(file);
}

static void
import_winamp_file(const gchar * filename)
{
    VFSFile *file;
    GList *list;

    if (!(file = open_vfs_file(filename, "rb")) ||
        !(list = aud_import_winamp_eqf(file)))
        return;

    equalizer_presets = g_list_concat(equalizer_presets, list);
    aud_equalizer_write_preset_file(equalizer_presets, "eq.preset");

    vfs_fclose(file);
}

static void
save_winamp_file(const gchar * filename)
{
    VFSFile *file;

    gchar name[257];
    gint i;
    guchar bands[11];

    if (!(file = open_vfs_file(filename, "wb")))
        return;

    vfs_fwrite("Winamp EQ library file v1.1\x1a!--", 1, 31, file);

    memset(name, 0, 257);
    g_strlcpy(name, "Entry1", 257);
    vfs_fwrite(name, 1, 257, file);

    for (i = 0; i < AUD_EQUALIZER_NBANDS; i++)
        bands[i] = 63 - (((equalizerwin_get_band(i) + EQUALIZER_MAX_GAIN) * 63) / EQUALIZER_MAX_GAIN / 2);

    bands[AUD_EQUALIZER_NBANDS] = 63 - (((equalizerwin_get_preamp() + EQUALIZER_MAX_GAIN) * 63) / EQUALIZER_MAX_GAIN / 2);

    vfs_fwrite(bands, 1, 11, file);
    vfs_fclose(file);
}

static GtkWidget *
equalizerwin_create_list_window(GList *preset_list,
                                const gchar *title,
                                GtkWidget **window,
                                GtkSelectionMode sel_mode,
                                GtkWidget **entry,
                                const gchar *action_name,
                                GCallback action_func,
                                GCallback select_row_func)
{
    GtkWidget *vbox, *scrolled_window, *bbox, *view;
    GtkWidget *button_cancel, *button_action;
    GList *node;

    GtkListStore *store;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GtkTreeSortable *sortable;



    *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(*window), title);
    gtk_window_set_type_hint(GTK_WINDOW(*window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_default_size(GTK_WINDOW(*window), 350, 300);
    gtk_window_set_position(GTK_WINDOW(*window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(*window), 10);
    gtk_window_set_transient_for(GTK_WINDOW(*window),
                                 GTK_WINDOW(equalizerwin));
    g_signal_connect(*window, "destroy",
                     G_CALLBACK(gtk_widget_destroyed), window);

    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(*window), vbox);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);


    /* fill the store with the names of all available presets */
    store = gtk_list_store_new(1, G_TYPE_STRING);
    for (node = preset_list; node; node = g_list_next(node))
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, ((EqualizerPreset*)node->data)->name,
                           -1);
    }
    model = GTK_TREE_MODEL(store);


    sortable = GTK_TREE_SORTABLE(store);
    gtk_tree_sortable_set_sort_column_id(sortable, 0, GTK_SORT_ASCENDING);


    view = gtk_tree_view_new();
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1,
                                                _("Presets"), renderer,
                                                "text", 0, NULL);
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
    g_object_unref(model);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_set_mode(selection, sel_mode);




    gtk_container_add(GTK_CONTAINER(scrolled_window), view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    if (entry) {
        *entry = gtk_entry_new();
        g_signal_connect(*entry, "activate", action_func, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), *entry, FALSE, FALSE, 0);
    }

    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    button_cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect_swapped(button_cancel, "clicked",
                             G_CALLBACK(gtk_widget_destroy),
                             GTK_OBJECT(*window));
    gtk_box_pack_start(GTK_BOX(bbox), button_cancel, TRUE, TRUE, 0);

    button_action = gtk_button_new_from_stock(action_name);
    g_signal_connect(button_action, "clicked", G_CALLBACK(action_func), view);
    GTK_WIDGET_SET_FLAGS(button_action, GTK_CAN_DEFAULT);

    if (select_row_func)
        g_signal_connect(view, "row-activated", G_CALLBACK(select_row_func), NULL);


    gtk_box_pack_start(GTK_BOX(bbox), button_action, TRUE, TRUE, 0);

    gtk_widget_grab_default(button_action);


    gtk_widget_show_all(*window);

    return *window;
}

void
equalizerwin_set_preamp(gfloat preamp)
{
    ui_skinned_equalizer_slider_set_position(equalizerwin_preamp, preamp);
    equalizerwin_eq_changed();
}

void
equalizerwin_set_band(gint band, gfloat value)
{
    g_return_if_fail(band >= 0 && band < AUD_EQUALIZER_NBANDS);
    ui_skinned_equalizer_slider_set_position(equalizerwin_bands[band], value);
    equalizerwin_eq_changed();
}

gfloat
equalizerwin_get_preamp(void)
{
    return ui_skinned_equalizer_slider_get_position(equalizerwin_preamp);
}

gfloat
equalizerwin_get_band(gint band)
{
    g_return_val_if_fail(band >= 0 && band < AUD_EQUALIZER_NBANDS, 0.0);
    return ui_skinned_equalizer_slider_get_position(equalizerwin_bands[band]);
}

void
action_equ_load_preset(void)
{
    if (equalizerwin_load_window) {
        gtk_window_present(GTK_WINDOW(equalizerwin_load_window));
        return;
    }

    equalizerwin_create_list_window(equalizer_presets,
                                    Q_("Load preset"),
                                    &equalizerwin_load_window,
                                    GTK_SELECTION_SINGLE, NULL,
                                    GTK_STOCK_OK,
                                    G_CALLBACK(equalizerwin_load_ok),
                                    G_CALLBACK(equalizerwin_load_select));
}

void
action_equ_load_auto_preset(void)
{
    if (equalizerwin_load_auto_window) {
        gtk_window_present(GTK_WINDOW(equalizerwin_load_auto_window));
        return;
    }

    equalizerwin_create_list_window(equalizer_auto_presets,
                                    Q_("Load auto-preset"),
                                    &equalizerwin_load_auto_window,
                                    GTK_SELECTION_SINGLE, NULL,
                                    GTK_STOCK_OK,
                                    G_CALLBACK(equalizerwin_load_auto_ok),
                                    G_CALLBACK(equalizerwin_load_auto_select));
}

void
action_equ_load_default_preset(void)
{
    equalizerwin_load_preset(equalizer_presets, "Default");
}

void
action_equ_zero_preset(void)
{
    gint i;

    equalizerwin_set_preamp(0);
    for (i = 0; i < AUD_EQUALIZER_NBANDS; i++)
        equalizerwin_set_band(i, 0);
}

void
action_equ_load_preset_file(void)
{
    GtkWidget *dialog;
    gchar *file_uri;

    dialog = make_filebrowser(Q_("Load equalizer preset"), FALSE);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        file_uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
        EqualizerPreset *preset = aud_load_preset_file(file_uri);
        equalizerwin_apply_preset(preset);
        equalizer_preset_free(preset);
        g_free(file_uri);
    }
    gtk_widget_destroy(dialog);
}

void
action_equ_load_preset_eqf(void)
{
    GtkWidget *dialog;
    gchar *file_uri;

    dialog = make_filebrowser(Q_("Load equalizer preset"), FALSE);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        file_uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
        load_winamp_file(file_uri);
        g_free(file_uri);
    }
    gtk_widget_destroy(dialog);
}

void
action_equ_import_winamp_presets(void)
{
    GtkWidget *dialog;
    gchar *file_uri;

    dialog = make_filebrowser(Q_("Load equalizer preset"), FALSE);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        file_uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
        import_winamp_file(file_uri);
        g_free(file_uri);
    }
    gtk_widget_destroy(dialog);
}

void
action_equ_save_preset(void)
{
    if (equalizerwin_save_window) {
        gtk_window_present(GTK_WINDOW(equalizerwin_save_window));
        return;
    }

    equalizerwin_create_list_window(equalizer_presets,
                                    Q_("Save preset"),
                                    &equalizerwin_save_window,
                                    GTK_SELECTION_SINGLE,
                                    &equalizerwin_save_entry,
                                    GTK_STOCK_OK,
                                    G_CALLBACK(equalizerwin_save_ok),
                                    G_CALLBACK(equalizerwin_save_select));
}

void
action_equ_save_auto_preset(void)
{
    gchar *name;

    if (equalizerwin_save_auto_window)
        gtk_window_present(GTK_WINDOW(equalizerwin_save_auto_window));
    else
        equalizerwin_create_list_window(equalizer_auto_presets,
                                        Q_("Save auto-preset"),
                                        &equalizerwin_save_auto_window,
                                        GTK_SELECTION_SINGLE,
                                        &equalizerwin_save_auto_entry,
                                        GTK_STOCK_OK,
                                        G_CALLBACK(equalizerwin_save_auto_ok),
                                        G_CALLBACK(equalizerwin_save_auto_select));

    name = aud_drct_pl_get_file (aud_drct_pl_get_pos ());

    if (name != NULL)
    {
        gtk_entry_set_text(GTK_ENTRY(equalizerwin_save_auto_entry),
                           g_basename(name));
        g_free(name);
    }
}

void
action_equ_save_default_preset(void)
{
    equalizer_presets =
        equalizerwin_save_preset(equalizer_presets, Q_("Default"), "eq.preset");
}

void
action_equ_save_preset_file(void)
{
    GtkWidget *dialog;
    gchar *file_uri;
    gchar *songname;
    gint i;

    dialog = make_filebrowser(Q_("Save equalizer preset"), TRUE);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        file_uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
        EqualizerPreset *preset = g_new0(EqualizerPreset, 1);
        preset->name = g_strdup(file_uri);
        preset->preamp = equalizerwin_get_preamp();
        for (i = 0; i < AUD_EQUALIZER_NBANDS; i++)
            preset->bands[i] = equalizerwin_get_band(i);
        aud_save_preset_file(preset, file_uri);
        equalizer_preset_free(preset);
        g_free(file_uri);
    }

    songname = aud_drct_pl_get_file (aud_drct_pl_get_pos ());

    if (songname != NULL)
    {
        gchar *eqname = g_strdup_printf("%s.%s", songname,
                                        aud_cfg->eqpreset_extension);
        g_free(songname);
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),
                                      eqname);
        g_free(eqname);
    }

    gtk_widget_destroy(dialog);
}

void
action_equ_save_preset_eqf(void)
{
    GtkWidget *dialog;
    gchar *file_uri;

    dialog = make_filebrowser(Q_("Save equalizer preset"), TRUE);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        file_uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
        save_winamp_file(file_uri);
        g_free(file_uri);
    }
    gtk_widget_destroy(dialog);
}

void
action_equ_delete_preset(void)
{
    if (equalizerwin_delete_window) {
        gtk_window_present(GTK_WINDOW(equalizerwin_delete_window));
        return;
    }

    equalizerwin_create_list_window(equalizer_presets,
                                    Q_("Delete preset"),
                                    &equalizerwin_delete_window,
                                    GTK_SELECTION_EXTENDED, NULL,
                                    GTK_STOCK_DELETE,
                                    G_CALLBACK(equalizerwin_delete_delete),
                                    NULL);
}

void
action_equ_delete_auto_preset(void)
{
    if (equalizerwin_delete_auto_window) {
        gtk_window_present(GTK_WINDOW(equalizerwin_delete_auto_window));
        return;
    }

    equalizerwin_create_list_window(equalizer_auto_presets,
                                    Q_("Delete auto-preset"),
                                    &equalizerwin_delete_auto_window,
                                    GTK_SELECTION_EXTENDED, NULL,
                                    GTK_STOCK_DELETE,
                                    G_CALLBACK(equalizerwin_delete_auto_delete),
                                    NULL);
}

void
equalizerwin_activate(gboolean active)
{
    aud_cfg->equalizer_active = active;
    ui_skinned_button_set_inside(equalizerwin_on, active);
    equalizerwin_eq_changed();
}

static void load_auto_preset (const gchar * filename)
{
    gchar * base;

    if (aud_cfg->eqpreset_extension != NULL)
    {
        gchar * eq_file = g_strconcat (filename, ".",
         aud_cfg->eqpreset_extension, NULL);
        gboolean success = equalizerwin_read_aud_preset (eq_file);

        g_free (eq_file);

        if (success)
            return;
    }

    if (aud_cfg->eqpreset_default_file != NULL)
    {
        gchar * folder = g_path_get_dirname (filename);
        gchar * eq_file = g_build_filename (folder,
         aud_cfg->eqpreset_default_file, NULL);
        gboolean success = equalizerwin_read_aud_preset (eq_file);

        g_free (folder);
        g_free (eq_file);

        if (success)
            return;
    }

    base = g_path_get_basename (filename);

    if (! equalizerwin_load_preset (equalizer_auto_presets, base))
    {
        if (! equalizerwin_load_preset (equalizer_presets, "Default"))
            action_equ_zero_preset ();
    }

    g_free (base);
}

static void position_cb (void * data, void * user_data)
{
    gint playlist = GPOINTER_TO_INT (data);
    gint position = aud_playlist_get_position (playlist);

    if (! aud_cfg->equalizer_autoload || playlist != aud_playlist_get_playing ()
     || position == -1)
        return;

    load_auto_preset (aud_playlist_entry_get_filename (playlist, position));
}

void eq_init_hooks (void)
{
    gint playlist = aud_playlist_get_playing ();

    /* Load preset for the first song. FIXME: Doing this at interface load is
     really too late as the song may already be started. Really, this stuff
     shouldn't be in the interface plugin at all but in core. -jlindgren */
    if (playlist != -1)
        position_cb (GINT_TO_POINTER (playlist), NULL);

    hook_associate ("playlist position", position_cb, NULL);
}

void eq_end_hooks (void)
{
    hook_dissociate ("playlist position", position_cb);
}
