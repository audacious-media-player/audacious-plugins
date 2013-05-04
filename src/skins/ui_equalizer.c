/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team.
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

#include <string.h>
#include <strings.h>
#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui-gtk.h>

#include "actions-equalizer.h"
#include "plugin.h"
#include "skins_cfg.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_manager.h"
#include "ui_skinned_button.h"
#include "ui_skinned_equalizer_graph.h"
#include "ui_skinned_equalizer_slider.h"
#include "ui_skinned_horizontal_slider.h"
#include "ui_skinned_window.h"
#include "util.h"

enum PresetViewCols {
    PRESET_VIEW_COL_NAME,
    PRESET_VIEW_N_COLS
};

static gfloat equalizerwin_get_preamp (void);
static gfloat equalizerwin_get_band (gint band);
static void equalizerwin_set_preamp (gfloat preamp);
static void equalizerwin_set_band (gint band, gfloat value);

static void position_cb (void * data, void * user_data);

GtkWidget *equalizerwin;
static GtkWidget *equalizerwin_graph;

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

static Index * equalizer_presets = NULL, * equalizer_auto_presets = NULL;

static void
equalizer_preset_free(EqualizerPreset * preset)
{
    if (!preset)
        return;

    g_free(preset->name);
    g_free(preset);
}

static void free_presets (Index * presets)
{
    for (int p = 0; p < index_count (presets); p ++)
        equalizer_preset_free (index_get (presets, p));

    index_free (presets);
}

void equalizerwin_set_shape (void)
{
    gint id = config.equalizer_shaded ? SKIN_MASK_EQ_SHADE : SKIN_MASK_EQ;
    gtk_widget_shape_combine_region (equalizerwin, active_skin->masks[id]);
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
    aud_set_double (NULL, "equalizer_preamp", equalizerwin_get_preamp ());

    double bands[AUD_EQUALIZER_NBANDS];
    for (gint i = 0; i < AUD_EQUALIZER_NBANDS; i ++)
        bands[i] = equalizerwin_get_band (i);

    aud_eq_set_bands (bands);
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

static void eq_on_cb (GtkWidget * button, GdkEventButton * event)
 {aud_set_bool (NULL, "equalizer_active", button_get_active (button)); }
static void eq_auto_cb (GtkWidget * button, GdkEventButton * event)
 {aud_set_bool (NULL, "equalizer_autoload", button_get_active (equalizerwin_auto)); }

static void update_from_config (void * unused1, void * unused2)
{
    button_set_active (equalizerwin_on, aud_get_bool (NULL, "equalizer_active"));
    eq_slider_set_val (equalizerwin_preamp, aud_get_double (NULL, "equalizer_preamp"));

    gdouble bands[AUD_EQUALIZER_NBANDS];
    aud_eq_get_bands (bands);

    for (gint i = 0; i < AUD_EQUALIZER_NBANDS; i ++)
        eq_slider_set_val (equalizerwin_bands[i], bands[i]);

    eq_graph_update (equalizerwin_graph);
}

static void eq_presets_cb (GtkWidget * button, GdkEventButton * event)
{
    ui_popup_menu_show (UI_MENU_EQUALIZER_PRESET, event->x_root, event->y_root,
     FALSE, FALSE, event->button, event->time);
}

static GtkWidget * get_eq_effects_menu (void)
{
    static GtkWidget * menu = NULL;

    if (menu == NULL)
        menu = audgui_create_effects_menu ();

    return menu;
}

static gboolean
equalizerwin_press(GtkWidget * widget, GdkEventButton * event,
                   gpointer callback_data)
{
    if (event->button == 1 && event->type == GDK_2BUTTON_PRESS
             && event->y < 14) {
        equalizerwin_set_shade(!config.equalizer_shaded);
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

static void eqwin_volume_set_knob (void)
{
    gint pos = hslider_get_pos (equalizerwin_volume);

    gint x;
    if (pos < 32)
        x = 1;
    else if (pos < 63)
        x = 4;
    else
        x = 7;

    hslider_set_knob (equalizerwin_volume, x, 30, x, 30);
}

void equalizerwin_set_volume_slider (gint percent)
{
    hslider_set_pos (equalizerwin_volume, (percent * 94 + 50) / 100);
    eqwin_volume_set_knob ();
}

static void eqwin_volume_motion_cb (void)
{
    eqwin_volume_set_knob ();
    gint pos = hslider_get_pos (equalizerwin_volume);
    gint v = (pos * 100 + 47) / 94;

    mainwin_adjust_volume_motion(v);
    mainwin_set_volume_slider(v);
}

static void eqwin_volume_release_cb (void)
{
    eqwin_volume_set_knob ();
    mainwin_adjust_volume_release();
}

static void eqwin_balance_set_knob (void)
{
    gint pos = hslider_get_pos (equalizerwin_balance);

    gint x;
    if (pos < 13)
        x = 11;
    else if (pos < 26)
        x = 14;
    else
        x = 17;

    hslider_set_knob (equalizerwin_balance, x, 30, x, 30);
}

void equalizerwin_set_balance_slider (gint percent)
{
    if (percent > 0)
        hslider_set_pos (equalizerwin_balance, 19 + (percent * 19 + 50) / 100);
    else
        hslider_set_pos (equalizerwin_balance, 19 + (percent * 19 - 50) / 100);

    eqwin_balance_set_knob ();
}

static void eqwin_balance_motion_cb (void)
{
    eqwin_balance_set_knob ();
    gint pos = hslider_get_pos (equalizerwin_balance);
    pos = MIN(pos, 38);         /* The skin uses a even number of pixels
                                   for the balance-slider *sigh* */
    gint b;
    if (pos > 19)
        b = ((pos - 19) * 100 + 9) / 19;
    else
        b = ((pos - 19) * 100 - 9) / 19;

    mainwin_adjust_balance_motion(b);
    mainwin_set_balance_slider(b);
}

static void eqwin_balance_release_cb (void)
{
    eqwin_balance_set_knob ();
    mainwin_adjust_balance_release();
}

static void
equalizerwin_create_widgets(void)
{
    equalizerwin_on = button_new_toggle (25, 12, 10, 119, 128, 119, 69, 119, 187, 119, SKIN_EQMAIN, SKIN_EQMAIN);
    window_put_widget (equalizerwin, FALSE, equalizerwin_on, 14, 18);
    button_set_active (equalizerwin_on, aud_get_bool (NULL, "equalizer_active"));
    button_on_release (equalizerwin_on, eq_on_cb);

    equalizerwin_auto = button_new_toggle (33, 12, 35, 119, 153, 119, 94, 119, 212, 119, SKIN_EQMAIN, SKIN_EQMAIN);
    window_put_widget (equalizerwin, FALSE, equalizerwin_auto, 39, 18);
    button_set_active (equalizerwin_auto, aud_get_bool (NULL, "equalizer_autoload"));
    button_on_release (equalizerwin_auto, eq_auto_cb);

    equalizerwin_presets = button_new (44, 12, 224, 164, 224, 176, SKIN_EQMAIN, SKIN_EQMAIN);
    window_put_widget (equalizerwin, FALSE, equalizerwin_presets, 217, 18);
    button_on_release (equalizerwin_presets, eq_presets_cb);

    equalizerwin_close = button_new (9, 9, 0, 116, 0, 125, SKIN_EQMAIN, SKIN_EQMAIN);
    window_put_widget (equalizerwin, FALSE, equalizerwin_close, 264, 3);
    button_on_release (equalizerwin_close, (ButtonCB) equalizerwin_close_cb);

    equalizerwin_shade = button_new (9, 9, 254, 137, 1, 38, SKIN_EQMAIN, SKIN_EQ_EX);
    window_put_widget (equalizerwin, FALSE, equalizerwin_shade, 254, 3);
    button_on_release (equalizerwin_shade, (ButtonCB) equalizerwin_shade_toggle);

    equalizerwin_shaded_close = button_new (9, 9, 11, 38, 11, 47, SKIN_EQ_EX, SKIN_EQ_EX);
    window_put_widget (equalizerwin, TRUE, equalizerwin_shaded_close, 264, 3);
    button_on_release (equalizerwin_shaded_close, (ButtonCB) equalizerwin_close_cb);

    equalizerwin_shaded_shade = button_new (9, 9, 254, 3, 1, 47, SKIN_EQ_EX, SKIN_EQ_EX);
    window_put_widget (equalizerwin, TRUE, equalizerwin_shaded_shade, 254, 3);
    button_on_release (equalizerwin_shaded_shade, (ButtonCB) equalizerwin_shade_toggle);

    equalizerwin_graph = eq_graph_new ();
    window_put_widget (equalizerwin, FALSE, equalizerwin_graph, 86, 17);

    equalizerwin_preamp = eq_slider_new (_("Preamp"));
    window_put_widget (equalizerwin, FALSE, equalizerwin_preamp, 21, 38);
    eq_slider_set_val (equalizerwin_preamp, aud_get_double (NULL, "equalizer_preamp"));

    const gchar * const bandnames[AUD_EQUALIZER_NBANDS] = {N_("31 Hz"),
     N_("63 Hz"), N_("125 Hz"), N_("250 Hz"), N_("500 Hz"), N_("1 kHz"),
     N_("2 kHz"), N_("4 kHz"), N_("8 kHz"), N_("16 kHz")};
    gdouble bands[AUD_EQUALIZER_NBANDS];
    aud_eq_get_bands (bands);

    for (gint i = 0; i < AUD_EQUALIZER_NBANDS; i ++)
    {
        equalizerwin_bands[i] = eq_slider_new (_(bandnames[i]));
        window_put_widget (equalizerwin, FALSE, equalizerwin_bands[i], 78 + 18 * i, 38);
        eq_slider_set_val (equalizerwin_bands[i], bands[i]);
    }

    equalizerwin_volume = hslider_new (0, 94, SKIN_EQ_EX, 97, 8, 61, 4, 3, 7, 1, 30, 1, 30);
    window_put_widget (equalizerwin, TRUE, equalizerwin_volume, 61, 4);
    hslider_on_motion (equalizerwin_volume, eqwin_volume_motion_cb);
    hslider_on_release (equalizerwin_volume, eqwin_volume_release_cb);

    equalizerwin_balance = hslider_new (0, 39, SKIN_EQ_EX, 42, 8, 164, 4, 3, 7, 11, 30, 11, 30);
    window_put_widget (equalizerwin, TRUE, equalizerwin_balance, 164, 4);
    hslider_on_motion (equalizerwin_balance, eqwin_balance_motion_cb);
    hslider_on_release (equalizerwin_balance, eqwin_balance_release_cb);
}

static void eq_win_draw (GtkWidget * window, cairo_t * cr)
{
    gint height = config.equalizer_shaded ? 14 : 116;

    skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 0, 0, 0, 275, height);

    if (config.equalizer_shaded)
        skin_draw_pixbuf (cr, SKIN_EQ_EX, 0, 0, 0, 0, 275, 14);
    else
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 134, 0, 0, 275, 14);
}

static void
equalizerwin_create_window(void)
{
    equalizerwin = window_new (& config.equalizer_x, & config.equalizer_y, 275,
     config.equalizer_shaded ? 14 : 116, FALSE, config.equalizer_shaded,
     eq_win_draw);

    gtk_window_set_title(GTK_WINDOW(equalizerwin), _("Audacious Equalizer"));

    /* this will hide only mainwin. it's annoying! yaz */
    gtk_window_set_transient_for(GTK_WINDOW(equalizerwin),
                                 GTK_WINDOW(mainwin));
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(equalizerwin), TRUE);

    gtk_widget_set_app_paintable(equalizerwin, TRUE);

    g_signal_connect (equalizerwin, "delete-event", (GCallback) handle_window_close, NULL);
    g_signal_connect (equalizerwin, "button-press-event", (GCallback) equalizerwin_press, NULL);
    g_signal_connect (equalizerwin, "key-press-event", (GCallback) mainwin_keypress, NULL);
}

static void equalizerwin_destroyed (void)
{
    hook_dissociate ("set equalizer_active", (HookFunction) update_from_config);
    hook_dissociate ("set equalizer_bands", (HookFunction) update_from_config);
    hook_dissociate ("set equalizer_preamp", (HookFunction) update_from_config);

    hook_dissociate ("playlist position", position_cb);

    free_presets (equalizer_presets);
    free_presets (equalizer_auto_presets);
    equalizer_presets = NULL;
    equalizer_auto_presets = NULL;
}

void
equalizerwin_create(void)
{
    equalizer_presets = aud_equalizer_read_presets("eq.preset");
    equalizer_auto_presets = aud_equalizer_read_presets("eq.auto_preset");

    if (! equalizer_presets)
        equalizer_presets = index_new ();
    if (! equalizer_auto_presets)
        equalizer_auto_presets = index_new ();

    equalizerwin_create_window();

    gtk_window_add_accel_group( GTK_WINDOW(equalizerwin) , ui_manager_get_accel_group() );

    equalizerwin_create_widgets();
    window_show_all (equalizerwin);

    g_signal_connect (equalizerwin, "destroy", (GCallback) equalizerwin_destroyed, NULL);

    hook_associate ("set equalizer_active", (HookFunction) update_from_config, NULL);
    hook_associate ("set equalizer_bands", (HookFunction) update_from_config, NULL);
    hook_associate ("set equalizer_preamp", (HookFunction) update_from_config, NULL);

    int playlist = aud_playlist_get_playing ();

    /* Load preset for the first song. FIXME: Doing this at interface load is
     really too late as the song may already be started. Really, this stuff
     shouldn't be in the interface plugin at all but in core. -jlindgren */
    if (playlist != -1)
        position_cb (GINT_TO_POINTER (playlist), NULL);

    hook_associate ("playlist position", position_cb, NULL);
}

static void equalizerwin_real_show (gboolean show)
{
    if (show)
        gtk_window_present ((GtkWindow *) equalizerwin);
    else
        gtk_widget_hide (equalizerwin);
}

void equalizerwin_show (gboolean show)
{
    GtkAction * a = gtk_action_group_get_action (toggleaction_group_others,
     "show equalizer");

    if (a && gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (a)) != show)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (a), show);
    else
    {
        config.equalizer_visible = show;
        button_set_active (mainwin_eq, show);
        equalizerwin_real_show (show && gtk_widget_get_visible (mainwin));
    }
}

static int equalizerwin_find_preset (Index * list, const char * name)
{
    for (int p = 0; p < index_count (list); p ++)
    {
        EqualizerPreset * preset = index_get (list, p);
        if (!strcasecmp(preset->name, name))
            return p;
    }

    return -1;
}

static bool_t equalizerwin_load_preset (Index * list, const char * name)
{
    int p = equalizerwin_find_preset (list, name);
    if (p < 0)
        return FALSE;

    EqualizerPreset * preset = index_get (list, p);
    equalizerwin_set_preamp (preset->preamp);

    for (int i = 0; i < AUD_EQUALIZER_NBANDS; i ++)
        equalizerwin_set_band (i, preset->bands[i]);

    return TRUE;
}

static void equalizerwin_save_preset (Index * list, const char * name,
 const char * filename)
{
    int p = equalizerwin_find_preset (list, name);
    EqualizerPreset * preset = (p >= 0) ? index_get (list, p) : NULL;

    if (! preset)
    {
        preset = g_new0(EqualizerPreset, 1);
        preset->name = g_strdup(name);
        index_append (list, preset);
    }

    preset->preamp = equalizerwin_get_preamp();
    for (int i = 0; i < AUD_EQUALIZER_NBANDS; i ++)
        preset->bands[i] = equalizerwin_get_band(i);

    aud_equalizer_write_preset_file(list, filename);
}

static void equalizerwin_delete_preset (Index * list, gchar * name, gchar * filename)
{
    int p = equalizerwin_find_preset (list, name);
    if (p < 0)
        return;

    EqualizerPreset * preset = index_get (list, p);
    equalizer_preset_free(preset);
    index_delete (list, p, 1);

    aud_equalizer_write_preset_file(list, filename);
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
            equalizerwin_delete_preset (equalizer_presets, text, filename);
        else if (!strcmp(filename, "eq.auto_preset"))
            equalizerwin_delete_preset (equalizer_auto_presets, text, filename);

        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    }
}

static void
equalizerwin_read_winamp_eqf(VFSFile * file)
{
    Index * presets;
    if ((presets = aud_import_winamp_eqf(file)) == NULL)
        return;

    if (! index_count (presets))
        goto DONE;

    /* just get the first preset --asphyx */
    EqualizerPreset * preset = index_get (presets, 0);
    equalizerwin_set_preamp(preset->preamp);

    for (int i = 0; i < AUD_EQUALIZER_NBANDS; i ++)
        equalizerwin_set_band(i, preset->bands[i]);

    equalizerwin_eq_changed();

DONE:
    free_presets (presets);
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

    if (text[0])
        equalizerwin_save_preset (equalizer_presets, text, "eq.preset");

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

    if (text[0])
        equalizerwin_save_preset (equalizer_auto_presets, text, "eq.auto_preset");

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
    VFSFile * file = open_vfs_file (filename, "r");
    if (! file)
        return;

    Index * list = aud_import_winamp_eqf (file);
    if (! list)
        goto CLOSE;

    index_merge_append (equalizer_presets, list);
    index_free (list);

    aud_equalizer_write_preset_file(equalizer_presets, "eq.preset");

CLOSE:
    vfs_fclose(file);
}

static gboolean save_winamp_file (const gchar * filename)
{
    VFSFile *file;

    gchar name[257];
    gint i;
    guchar bands[11];

    if (!(file = open_vfs_file(filename, "wb")))
        return FALSE;

    if (vfs_fwrite ("Winamp EQ library file v1.1\x1a!--", 1, 31, file) != 31)
        goto ERR;

    memset(name, 0, 257);
    g_strlcpy(name, "Entry1", 257);

    if (vfs_fwrite (name, 1, 257, file) != 257)
        goto ERR;

    for (i = 0; i < AUD_EQUALIZER_NBANDS; i++)
        bands[i] = 63 - (((equalizerwin_get_band(i) + EQUALIZER_MAX_GAIN) * 63) / EQUALIZER_MAX_GAIN / 2);

    bands[AUD_EQUALIZER_NBANDS] = 63 - (((equalizerwin_get_preamp() + EQUALIZER_MAX_GAIN) * 63) / EQUALIZER_MAX_GAIN / 2);

    if (vfs_fwrite (bands, 1, 11, file) != 11)
        goto ERR;

    vfs_fclose (file);
    return TRUE;

ERR:
    vfs_fclose (file);
    return FALSE;
}

static GtkWidget * equalizerwin_create_list_window (Index * preset_list,
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

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(*window), vbox);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);


    /* fill the store with the names of all available presets */
    store = gtk_list_store_new(1, G_TYPE_STRING);
    for (int p = 0; p < index_count (preset_list); p ++)
    {
        EqualizerPreset * preset = index_get (preset_list, p);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set (store, & iter, 0, preset->name, -1);
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

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    button_cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect_swapped (button_cancel, "clicked", (GCallback)
     gtk_widget_destroy, * window);
    gtk_box_pack_start(GTK_BOX(bbox), button_cancel, TRUE, TRUE, 0);

    button_action = gtk_button_new_from_stock(action_name);
    g_signal_connect(button_action, "clicked", G_CALLBACK(action_func), view);
    gtk_widget_set_can_default (button_action, TRUE);

    if (select_row_func)
        g_signal_connect(view, "row-activated", G_CALLBACK(select_row_func), NULL);


    gtk_box_pack_start(GTK_BOX(bbox), button_action, TRUE, TRUE, 0);

    gtk_widget_grab_default(button_action);


    gtk_widget_show_all(*window);

    return *window;
}

static void equalizerwin_set_preamp (gfloat preamp)
{
    eq_slider_set_val (equalizerwin_preamp, preamp);
    equalizerwin_eq_changed();
}

static void equalizerwin_set_band (gint band, gfloat value)
{
    g_return_if_fail(band >= 0 && band < AUD_EQUALIZER_NBANDS);
    eq_slider_set_val (equalizerwin_bands[band], value);
    equalizerwin_eq_changed();
}

static gfloat equalizerwin_get_preamp (void)
{
    return eq_slider_get_val (equalizerwin_preamp);
}

static gfloat equalizerwin_get_band (gint band)
{
    g_return_val_if_fail(band >= 0 && band < AUD_EQUALIZER_NBANDS, 0.0);
    return eq_slider_get_val (equalizerwin_bands[band]);
}

void
action_equ_load_preset(void)
{
    if (equalizerwin_load_window) {
        gtk_window_present(GTK_WINDOW(equalizerwin_load_window));
        return;
    }

    equalizerwin_create_list_window(equalizer_presets,
                                    _("Load preset"),
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
                                    _("Load auto-preset"),
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

    dialog = make_filebrowser(_("Load equalizer preset"), FALSE);
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

    dialog = make_filebrowser(_("Load equalizer preset"), FALSE);
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

    dialog = make_filebrowser(_("Load equalizer preset"), FALSE);
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
                                    _("Save preset"),
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
    if (equalizerwin_save_auto_window)
        gtk_window_present(GTK_WINDOW(equalizerwin_save_auto_window));
    else
        equalizerwin_create_list_window(equalizer_auto_presets,
                                        _("Save auto-preset"),
                                        &equalizerwin_save_auto_window,
                                        GTK_SELECTION_SINGLE,
                                        &equalizerwin_save_auto_entry,
                                        GTK_STOCK_OK,
                                        G_CALLBACK(equalizerwin_save_auto_ok),
                                        G_CALLBACK(equalizerwin_save_auto_select));

    char * name = aud_drct_get_filename ();

    if (name != NULL)
    {
        char * base = g_path_get_basename (name);
        gtk_entry_set_text ((GtkEntry *) equalizerwin_save_auto_entry, base);
        g_free (base);
        str_unref (name);
    }
}

void action_equ_save_default_preset (void)
{
    equalizerwin_save_preset (equalizer_presets, _("Default"), "eq.preset");
}

void
action_equ_save_preset_file(void)
{
    GtkWidget *dialog;
    gchar *file_uri;
    gint i;

    dialog = make_filebrowser(_("Save equalizer preset"), TRUE);
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

    char * songname = aud_drct_get_filename ();

    if (songname != NULL)
    {
        gchar * ext = aud_get_string (NULL, "eqpreset_extension");
        gchar * eqname = g_strdup_printf ("%s.%s", songname, ext);
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),
                                      eqname);
        g_free (eqname);
        g_free (ext);
        str_unref (songname);
    }

    gtk_widget_destroy(dialog);
}

void
action_equ_save_preset_eqf(void)
{
    GtkWidget *dialog;
    gchar *file_uri;

    dialog = make_filebrowser(_("Save equalizer preset"), TRUE);
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
                                    _("Delete preset"),
                                    &equalizerwin_delete_window,
                                    GTK_SELECTION_MULTIPLE, NULL,
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
                                    _("Delete auto-preset"),
                                    &equalizerwin_delete_auto_window,
                                    GTK_SELECTION_MULTIPLE, NULL,
                                    GTK_STOCK_DELETE,
                                    G_CALLBACK(equalizerwin_delete_auto_delete),
                                    NULL);
}

static void load_auto_preset (const gchar * filename)
{
    gchar * ext = aud_get_string (NULL, "eqpreset_extension");
    if (ext[0])
    {
        gchar * eq_file = g_strconcat (filename, ".", ext, NULL);
        gboolean success = equalizerwin_read_aud_preset (eq_file);
        g_free (eq_file);

        if (success)
        {
            g_free (ext);
            return;
        }
    }
    g_free (ext);

    gchar * deffile = aud_get_string (NULL, "eqpreset_default_file");
    if (deffile[0])
    {
        gchar * folder = g_path_get_dirname (filename);
        gchar * eq_file = g_build_filename (folder, deffile, NULL);
        gboolean success = equalizerwin_read_aud_preset (eq_file);
        g_free (folder);
        g_free (eq_file);

        if (success)
        {
            g_free (deffile);
            return;
        }
    }
    g_free (deffile);

    gchar * base = g_path_get_basename (filename);

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

    if (! aud_get_bool (NULL, "equalizer_autoload") || playlist !=
     aud_playlist_get_playing () || position == -1)
        return;

    gchar * filename = aud_playlist_entry_get_filename (playlist, position);
    load_auto_preset (filename);
    str_unref (filename);
}

void action_show_equalizer (GtkToggleAction * action)
{
    equalizerwin_show (gtk_toggle_action_get_active (action));
}

void action_roll_up_equalizer (GtkToggleAction * action)
{
    config.equalizer_shaded = gtk_toggle_action_get_active (action);

    window_set_shaded (equalizerwin, config.equalizer_shaded);
    window_set_size (equalizerwin, 275, config.equalizer_shaded ? 14 : 116);
    equalizerwin_set_shape ();
}
