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

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/equalizer.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "menus.h"
#include "plugin.h"
#include "skins_cfg.h"
#include "equalizer.h"
#include "main.h"
#include "button.h"
#include "eq-graph.h"
#include "eq-slider.h"
#include "hslider.h"
#include "window.h"
#include "util.h"
#include "view.h"

class EqWindow : public Window
{
public:
    EqWindow (bool shaded) :
        Window (WINDOW_EQ, & config.equalizer_x, & config.equalizer_y, 275,
         shaded ? 14 : 116, shaded) {}

private:
    void draw (QPainter & cr);
    bool button_press (QMouseEvent * event);
};

Window * equalizerwin;

static EqGraph * equalizerwin_graph;
static Button * equalizerwin_on, * equalizerwin_auto;
static Button * equalizerwin_close, * equalizerwin_shade;
static Button * equalizerwin_shaded_close, * equalizerwin_shaded_shade;
static Button * equalizerwin_presets;
static EqSlider * equalizerwin_preamp, * equalizerwin_bands[10];
static HSlider * equalizerwin_volume, * equalizerwin_balance;

static void equalizerwin_shade_toggle ()
{
    view_set_equalizer_shaded (! aud_get_bool ("skins", "equalizer_shaded"));
}

static void eq_on_cb (Button * button, QMouseEvent * event)
{
    aud_set_bool (nullptr, "equalizer_active", button->get_active ());
}

static void update_from_config (void *, void *)
{
    equalizerwin_on->set_active (aud_get_bool (nullptr, "equalizer_active"));
    equalizerwin_preamp->set_value (aud_get_double (nullptr, "equalizer_preamp"));

    double bands[AUD_EQ_NBANDS];
    aud_eq_get_bands (bands);

    for (int i = 0; i < AUD_EQ_NBANDS; i ++)
        equalizerwin_bands[i]->set_value (bands[i]);

    equalizerwin_graph->refresh ();
}

bool EqWindow::button_press (QMouseEvent * event)
{
    if (event->button () == Qt::LeftButton &&
     event->type () == QEvent::MouseButtonDblClick &&
     event->y () < 14 * config.scale)
    {
        equalizerwin_shade_toggle ();
        return true;
    }

    if (event->button () == Qt::RightButton && event->type () == QEvent::MouseButtonPress)
    {
        menu_popup (UI_MENU_MAIN, event->globalX (), event->globalY (), false, false);
        return true;
    }

    return Window::button_press (event);
}

static void equalizerwin_close_cb ()
{
    view_set_show_equalizer (false);
}

static void eqwin_volume_set_knob ()
{
    int pos = equalizerwin_volume->get_pos ();
    int x = (pos < 32) ? 1 : (pos < 63) ? 4 : 7;
    equalizerwin_volume->set_knob (x, 30, x, 30);
}

void equalizerwin_set_volume_slider (int percent)
{
    equalizerwin_volume->set_pos ((percent * 94 + 50) / 100);
    eqwin_volume_set_knob ();
}

static void eqwin_volume_motion_cb ()
{
    eqwin_volume_set_knob ();
    int pos = equalizerwin_volume->get_pos ();
    int v = (pos * 100 + 47) / 94;

    mainwin_adjust_volume_motion (v);
    mainwin_set_volume_slider (v);
}

static void eqwin_volume_release_cb ()
{
    eqwin_volume_set_knob ();
    mainwin_adjust_volume_release ();
}

static void eqwin_balance_set_knob ()
{
    int pos = equalizerwin_balance->get_pos ();
    int x = (pos < 13) ? 11 : (pos < 26) ? 14 : 17;
    equalizerwin_balance->set_knob (x, 30, x, 30);
}

void equalizerwin_set_balance_slider (int percent)
{
    if (percent > 0)
        equalizerwin_balance->set_pos (19 + (percent * 19 + 50) / 100);
    else
        equalizerwin_balance->set_pos (19 + (percent * 19 - 50) / 100);

    eqwin_balance_set_knob ();
}

static void eqwin_balance_motion_cb ()
{
    eqwin_balance_set_knob ();
    int pos = equalizerwin_balance->get_pos ();
    pos = aud::min(pos, 38);    /* The skin uses a even number of pixels
                                   for the balance-slider *sigh* */
    int b;
    if (pos > 19)
        b = ((pos - 19) * 100 + 9) / 19;
    else
        b = ((pos - 19) * 100 - 9) / 19;

    mainwin_adjust_balance_motion (b);
    mainwin_set_balance_slider (b);
}

static void eqwin_balance_release_cb ()
{
    eqwin_balance_set_knob ();
    mainwin_adjust_balance_release ();
}

static void equalizerwin_create_widgets ()
{
    equalizerwin_on = new Button (25, 12, 10, 119, 128, 119, 69, 119, 187, 119, SKIN_EQMAIN, SKIN_EQMAIN);
    equalizerwin->put_widget (false, equalizerwin_on, 14, 18);
    equalizerwin_on->set_active (aud_get_bool (nullptr, "equalizer_active"));
    equalizerwin_on->on_release (eq_on_cb);

    // AUTO button currently does nothing
    equalizerwin_auto = new Button (33, 12, 35, 119, 153, 119, 94, 119, 212, 119, SKIN_EQMAIN, SKIN_EQMAIN);
    equalizerwin->put_widget (false, equalizerwin_auto, 39, 18);

    equalizerwin_presets = new Button (44, 12, 224, 164, 224, 176, SKIN_EQMAIN, SKIN_EQMAIN);
    equalizerwin->put_widget (false, equalizerwin_presets, 217, 18);
//    equalizerwin_presets->on_release ((ButtonCB) audgui_show_eq_preset_window);

    equalizerwin_close = new Button (9, 9, 0, 116, 0, 125, SKIN_EQMAIN, SKIN_EQMAIN);
    equalizerwin->put_widget (false, equalizerwin_close, 264, 3);
    equalizerwin_close->on_release ((ButtonCB) equalizerwin_close_cb);

    equalizerwin_shade = new Button (9, 9, 254, 137, 1, 38, SKIN_EQMAIN, SKIN_EQ_EX);
    equalizerwin->put_widget (false, equalizerwin_shade, 254, 3);
    equalizerwin_shade->on_release ((ButtonCB) equalizerwin_shade_toggle);

    equalizerwin_shaded_close = new Button (9, 9, 11, 38, 11, 47, SKIN_EQ_EX, SKIN_EQ_EX);
    equalizerwin->put_widget (true, equalizerwin_shaded_close, 264, 3);
    equalizerwin_shaded_close->on_release ((ButtonCB) equalizerwin_close_cb);

    equalizerwin_shaded_shade = new Button (9, 9, 254, 3, 1, 47, SKIN_EQ_EX, SKIN_EQ_EX);
    equalizerwin->put_widget (true, equalizerwin_shaded_shade, 254, 3);
    equalizerwin_shaded_shade->on_release ((ButtonCB) equalizerwin_shade_toggle);

    equalizerwin_graph = new EqGraph;
    equalizerwin->put_widget (false, equalizerwin_graph, 86, 17);

    equalizerwin_preamp = new EqSlider (_("Preamp"), -1);
    equalizerwin->put_widget (false, equalizerwin_preamp, 21, 38);
    equalizerwin_preamp->set_value (aud_get_double (nullptr, "equalizer_preamp"));

    const char * const bandnames[AUD_EQ_NBANDS] = {N_("31 Hz"),
     N_("63 Hz"), N_("125 Hz"), N_("250 Hz"), N_("500 Hz"), N_("1 kHz"),
     N_("2 kHz"), N_("4 kHz"), N_("8 kHz"), N_("16 kHz")};
    double bands[AUD_EQ_NBANDS];
    aud_eq_get_bands (bands);

    for (int i = 0; i < AUD_EQ_NBANDS; i ++)
    {
        equalizerwin_bands[i] = new EqSlider (_(bandnames[i]), i);
        equalizerwin->put_widget (false, equalizerwin_bands[i], 78 + 18 * i, 38);
        equalizerwin_bands[i]->set_value (bands[i]);
    }

    equalizerwin_volume = new HSlider (0, 94, SKIN_EQ_EX, 97, 8, 61, 4, 3, 7, 1, 30, 1, 30);
    equalizerwin->put_widget (true, equalizerwin_volume, 61, 4);
    equalizerwin_volume->on_move (eqwin_volume_motion_cb);
    equalizerwin_volume->on_release (eqwin_volume_release_cb);

    equalizerwin_balance = new HSlider (0, 39, SKIN_EQ_EX, 42, 8, 164, 4, 3, 7, 11, 30, 11, 30);
    equalizerwin->put_widget (true, equalizerwin_balance, 164, 4);
    equalizerwin_balance->on_move (eqwin_balance_motion_cb);
    equalizerwin_balance->on_release (eqwin_balance_release_cb);
}

void EqWindow::draw (QPainter & cr)
{
    skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 0, 0, 0, 275, is_shaded () ? 14 : 116);

    if (is_shaded ())
        skin_draw_pixbuf (cr, SKIN_EQ_EX, 0, 0, 0, 0, 275, 14);
    else
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 134, 0, 0, 275, 14);
}

static void equalizerwin_create_window ()
{
    bool shaded = aud_get_bool ("skins", "equalizer_shaded");

    /* do not allow shading the equalizer if eq_ex.bmp is missing */
    if (skin.pixmaps[SKIN_EQ_EX].isNull ())
        shaded = false;

    equalizerwin = new EqWindow (shaded);
    equalizerwin->setWindowTitle (_("Audacious Equalizer"));
}

void equalizerwin_unhook ()
{
    hook_dissociate ("set equalizer_active", (HookFunction) update_from_config);
    hook_dissociate ("set equalizer_bands", (HookFunction) update_from_config);
    hook_dissociate ("set equalizer_preamp", (HookFunction) update_from_config);
}

void equalizerwin_create ()
{
    equalizerwin_create_window ();
    equalizerwin_create_widgets ();

    hook_associate ("set equalizer_active", (HookFunction) update_from_config, nullptr);
    hook_associate ("set equalizer_bands", (HookFunction) update_from_config, nullptr);
    hook_associate ("set equalizer_preamp", (HookFunction) update_from_config, nullptr);
}
