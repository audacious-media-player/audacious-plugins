/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2014  Audacious development team.
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
#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui-gtk.h>

#include "menus.h"
#include "plugin.h"
#include "preset-list.h"
#include "skins_cfg.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_skinned_button.h"
#include "ui_skinned_equalizer_graph.h"
#include "ui_skinned_equalizer_slider.h"
#include "ui_skinned_horizontal_slider.h"
#include "ui_skinned_window.h"
#include "util.h"
#include "view.h"

static gfloat equalizerwin_get_preamp (void);
static gfloat equalizerwin_get_band (gint band);
static void equalizerwin_set_preamp (gfloat preamp);
static void equalizerwin_set_band (gint band, gfloat value);

static void position_cb (void * data, void * user_data);

GtkWidget *equalizerwin;
GtkWidget *equalizerwin_graph;

static GtkWidget *equalizerwin_on, *equalizerwin_auto;

static GtkWidget *equalizerwin_close, *equalizerwin_shade;
static GtkWidget *equalizerwin_shaded_close, *equalizerwin_shaded_shade;
static GtkWidget *equalizerwin_presets;
static GtkWidget *equalizerwin_preamp,*equalizerwin_bands[10];
static GtkWidget *equalizerwin_volume, *equalizerwin_balance;

Index * equalizer_presets, * equalizer_auto_presets;

void equalizerwin_set_shape (void)
{
    int id = aud_get_bool ("skins", "equalizer_shaded") ? SKIN_MASK_EQ_SHADE : SKIN_MASK_EQ;
    gtk_widget_shape_combine_region (equalizerwin, active_skin->masks[id]);
}

static void
equalizerwin_shade_toggle(void)
{
    view_set_equalizer_shaded (! aud_get_bool ("skins", "equalizer_shaded"));
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

void equalizerwin_apply_preset (const EqualizerPreset * preset)
{
    equalizerwin_set_preamp (preset->preamp);
    for (int i = 0; i < AUD_EQUALIZER_NBANDS; i ++)
        equalizerwin_set_band (i, preset->bands[i]);
}

void equalizerwin_update_preset (EqualizerPreset * preset)
{
    preset->preamp = equalizerwin_get_preamp ();
    for (int i = 0; i < AUD_EQUALIZER_NBANDS; i ++)
        preset->bands[i] = equalizerwin_get_band (i);
}

void equalizerwin_import_presets (Index * presets)
{
    index_copy_insert (presets, 0, equalizer_presets, -1, -1);
    index_free (presets);

    aud_equalizer_write_presets (equalizer_presets, "eq.preset");
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
    menu_popup (UI_MENU_EQUALIZER_PRESET, event->x_root, event->y_root, FALSE,
     FALSE, event->button, event->time);
}

static gboolean
equalizerwin_press(GtkWidget * widget, GdkEventButton * event,
                   gpointer callback_data)
{
    if (event->button == 1 && event->type == GDK_2BUTTON_PRESS &&
     event->window == gtk_widget_get_window (widget) && event->y < 14)
    {
        equalizerwin_shade_toggle ();
        return TRUE;
    }

    if (event->button == 3)
    {
        menu_popup (UI_MENU_MAIN, event->x_root, event->y_root, FALSE, FALSE,
         event->button, event->time);
        return TRUE;
    }

    return FALSE;
}

static void
equalizerwin_close_cb(void)
{
    view_set_show_equalizer (FALSE);
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
    gtk_widget_set_no_show_all (equalizerwin_graph, TRUE);  // shown or hidden in skin_load()
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
    bool_t shaded = aud_get_bool ("skins", "equalizer_shaded");

    skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 0, 0, 0, 275, shaded ? 14 : 116);

    if (shaded)
        skin_draw_pixbuf (cr, SKIN_EQ_EX, 0, 0, 0, 0, 275, 14);
    else
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 134, 0, 0, 275, 14);
}

static void
equalizerwin_create_window(void)
{
    bool_t shaded = aud_get_bool ("skins", "equalizer_shaded");

    equalizerwin = window_new (& config.equalizer_x, & config.equalizer_y, 275,
     shaded ? 14 : 116, FALSE, shaded, eq_win_draw);

    gtk_window_set_title(GTK_WINDOW(equalizerwin), _("Audacious Equalizer"));

    /* this will hide only mainwin. it's annoying! yaz */
    gtk_window_set_transient_for(GTK_WINDOW(equalizerwin),
                                 GTK_WINDOW(mainwin));
    gtk_window_set_skip_pager_hint(GTK_WINDOW(equalizerwin), TRUE);
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

    index_free_full (equalizer_presets, (IndexFreeFunc) aud_equalizer_preset_free);
    index_free_full (equalizer_auto_presets, (IndexFreeFunc) aud_equalizer_preset_free);
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

    gtk_window_add_accel_group ((GtkWindow *) equalizerwin, menu_get_accel_group ());

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

static int equalizerwin_find_preset (Index * list, const char * name)
{
    for (int p = 0; p < index_count (list); p ++)
    {
        EqualizerPreset * preset = index_get (list, p);
        if (!g_ascii_strcasecmp(preset->name, name))
            return p;
    }

    return -1;
}

bool_t equalizerwin_load_preset (Index * list, const char * name)
{
    int p = equalizerwin_find_preset (list, name);
    if (p < 0)
        return FALSE;

    equalizerwin_apply_preset (index_get (list, p));
    return TRUE;
}

void equalizerwin_save_preset (Index * list, const char * name, const char * filename)
{
    int p = equalizerwin_find_preset (list, name);
    EqualizerPreset * preset = (p >= 0) ? index_get (list, p) : NULL;

    if (! preset)
    {
        preset = aud_equalizer_preset_new (name);
        index_insert (list, -1, preset);
    }

    equalizerwin_update_preset (preset);

    aud_equalizer_write_presets (list, filename);
}

void equalizerwin_delete_preset (Index * list, const char * name, const char * filename)
{
    int p = equalizerwin_find_preset (list, name);
    if (p < 0)
        return;

    index_delete_full (list, p, 1, (IndexFreeFunc) aud_equalizer_preset_free);

    aud_equalizer_write_presets (list, filename);
}

static gboolean equalizerwin_read_aud_preset (const gchar * file)
{
    EqualizerPreset * preset = aud_load_preset_file (file);

    if (preset == NULL)
        return FALSE;

    equalizerwin_apply_preset (preset);
    aud_equalizer_preset_free (preset);
    return TRUE;
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

static void load_auto_preset (const gchar * filename)
{
    gchar * ext = EQUALIZER_DEFAULT_PRESET_EXT;
    gchar * eq_file = g_strconcat (filename, ".", ext, NULL);
    gboolean success = equalizerwin_read_aud_preset (eq_file);
    g_free (eq_file);

    if (success)
        return;

    gchar * deffile = EQUALIZER_DEFAULT_DIR_PRESET;
    gchar * folder = g_path_get_dirname (filename);
    eq_file = g_build_filename (folder, deffile, NULL);
    success = equalizerwin_read_aud_preset (eq_file);

    g_free (folder);
    g_free (eq_file);

    if (success)
        return;

    gchar * base = g_path_get_basename (filename);

    if (! equalizerwin_load_preset (equalizer_auto_presets, base))
        eq_preset_load_default ();

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
