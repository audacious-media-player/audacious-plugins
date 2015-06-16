/*
 * Audacious
 * Copyright 2011-2013 Audacious development team
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

#include <stdlib.h>
#include <glib.h>

#include <libaudcore/inifile.h>

#include "skins_cfg.h"
#include "skin.h"
#include "util.h"

/*
 * skin.hints parsing
 */

typedef struct {
    const char * name;
    int * value_ptr;
} HintPair;

/* in alphabetical order to allow binary search */
static const HintPair hint_pairs[] = {
    {"mainwinaboutx", & skin.hints.mainwin_about_x},
    {"mainwinabouty", & skin.hints.mainwin_about_y},
    {"mainwinbalancex", & skin.hints.mainwin_balance_x},
    {"mainwinbalancey", & skin.hints.mainwin_balance_y},
    {"mainwinclosex", & skin.hints.mainwin_close_x},
    {"mainwinclosey", & skin.hints.mainwin_close_y},
    {"mainwinejectx", & skin.hints.mainwin_eject_x},
    {"mainwinejecty", & skin.hints.mainwin_eject_y},
    {"mainwineqbuttonx", & skin.hints.mainwin_eqbutton_x},
    {"mainwineqbuttony", & skin.hints.mainwin_eqbutton_y},
    {"mainwinheight", & skin.hints.mainwin_height},
    {"mainwininfobarx", & skin.hints.mainwin_infobar_x},
    {"mainwininfobary", & skin.hints.mainwin_infobar_y},
    {"mainwinmenurowvisible", & skin.hints.mainwin_menurow_visible},
    {"mainwinminimizex", & skin.hints.mainwin_minimize_x},
    {"mainwinminimizey", & skin.hints.mainwin_minimize_y},
    {"mainwinnextx", & skin.hints.mainwin_next_x},
    {"mainwinnexty", & skin.hints.mainwin_next_y},
    {"mainwinnumber0x", & skin.hints.mainwin_number_0_x},
    {"mainwinnumber0y", & skin.hints.mainwin_number_0_y},
    {"mainwinnumber1x", & skin.hints.mainwin_number_1_x},
    {"mainwinnumber1y", & skin.hints.mainwin_number_1_y},
    {"mainwinnumber2x", & skin.hints.mainwin_number_2_x},
    {"mainwinnumber2y", & skin.hints.mainwin_number_2_y},
    {"mainwinnumber3x", & skin.hints.mainwin_number_3_x},
    {"mainwinnumber3y", & skin.hints.mainwin_number_3_y},
    {"mainwinnumber4x", & skin.hints.mainwin_number_4_x},
    {"mainwinnumber4y", & skin.hints.mainwin_number_4_y},
    {"mainwinothertextisstatus", & skin.hints.mainwin_othertext_is_status},
    {"mainwinothertextvisible", & skin.hints.mainwin_othertext_visible},
    {"mainwinpausex", & skin.hints.mainwin_pause_x},
    {"mainwinpausey", & skin.hints.mainwin_pause_y},
    {"mainwinplaystatusx", & skin.hints.mainwin_playstatus_x},
    {"mainwinplaystatusy", & skin.hints.mainwin_playstatus_y},
    {"mainwinplayx", & skin.hints.mainwin_play_x},
    {"mainwinplayy", & skin.hints.mainwin_play_y},
    {"mainwinplbuttonx", & skin.hints.mainwin_plbutton_x},
    {"mainwinplbuttony", & skin.hints.mainwin_plbutton_y},
    {"mainwinpositionx", & skin.hints.mainwin_position_x},
    {"mainwinpositiony", & skin.hints.mainwin_position_y},
    {"mainwinpreviousx", & skin.hints.mainwin_previous_x},
    {"mainwinpreviousy", & skin.hints.mainwin_previous_y},
    {"mainwinrepeatx", & skin.hints.mainwin_repeat_x},
    {"mainwinrepeaty", & skin.hints.mainwin_repeat_y},
    {"mainwinshadex", & skin.hints.mainwin_shade_x},
    {"mainwinshadey", & skin.hints.mainwin_shade_y},
    {"mainwinshufflex", & skin.hints.mainwin_shuffle_x},
    {"mainwinshuffley", & skin.hints.mainwin_shuffle_y},
    {"mainwinstopx", & skin.hints.mainwin_stop_x},
    {"mainwinstopy", & skin.hints.mainwin_stop_y},
    {"mainwinstreaminfovisible", & skin.hints.mainwin_streaminfo_visible},
    {"mainwintextvisible", & skin.hints.mainwin_text_visible},
    {"mainwintextwidth", & skin.hints.mainwin_text_width},
    {"mainwintextx", & skin.hints.mainwin_text_x},
    {"mainwintexty", & skin.hints.mainwin_text_y},
    {"mainwinvisvisible", & skin.hints.mainwin_vis_visible},
    {"mainwinvisx", & skin.hints.mainwin_vis_x},
    {"mainwinvisy", & skin.hints.mainwin_vis_y},
    {"mainwinvolumex", & skin.hints.mainwin_volume_x},
    {"mainwinvolumey", & skin.hints.mainwin_volume_y},
    {"mainwinwidth", & skin.hints.mainwin_width},
    {"textboxbitmapfontheight", & skin.hints.textbox_bitmap_font_height},
    {"textboxbitmapfontwidth", & skin.hints.textbox_bitmap_font_width},
};

static int hint_pair_compare (const void * key, const void * pair)
{
    return g_ascii_strcasecmp ((const char *) key, ((const HintPair *) pair)->name);
}

class HintsParser : public IniParser
{
private:
    bool valid_heading = false;

    void handle_heading (const char * heading)
        { valid_heading = ! g_ascii_strcasecmp (heading, "skin"); }

    void handle_entry (const char * key, const char * value)
    {
        if (! valid_heading)
            return;

        HintPair * pair = (HintPair *) bsearch (key, hint_pairs,
         aud::n_elems (hint_pairs), sizeof (HintPair), hint_pair_compare);

        if (pair)
            * pair->value_ptr = atoi (value);
    }
};

void skin_load_hints (const char * path)
{
    VFSFile file = open_local_file_nocase (path, "skin.hints");
    if (file)
        HintsParser ().parse (file);
}

/*
 * pledit.txt parsing
 */

class PLColorsParser : public IniParser
{
public:
    PLColorsParser () :
        valid_heading (false) {}

private:
    bool valid_heading;

    void handle_heading (const char * heading)
        { valid_heading = ! g_ascii_strcasecmp (heading, "text"); }

    void handle_entry (const char * key, const char * value)
    {
        if (! valid_heading)
            return;

        if (value[0] == '#')
            value ++;

        uint32_t color = strtol (value, nullptr, 16);

        if (! g_ascii_strcasecmp (key, "normal"))
            skin.colors[SKIN_PLEDIT_NORMAL] = color;
        else if (! g_ascii_strcasecmp (key, "current"))
            skin.colors[SKIN_PLEDIT_CURRENT] = color;
        else if (! g_ascii_strcasecmp (key, "normalbg"))
            skin.colors[SKIN_PLEDIT_NORMALBG] = color;
        else if (! g_ascii_strcasecmp (key, "selectedbg"))
            skin.colors[SKIN_PLEDIT_SELECTEDBG] = color;
    }
};

void skin_load_pl_colors (const char * path)
{
    skin.colors[SKIN_PLEDIT_NORMAL] = 0x2499ff;
    skin.colors[SKIN_PLEDIT_CURRENT] = 0xffeeff;
    skin.colors[SKIN_PLEDIT_NORMALBG] = 0x0a120a;
    skin.colors[SKIN_PLEDIT_SELECTEDBG] = 0x0a124a;

    VFSFile file = open_local_file_nocase (path, "pledit.txt");
    if (file)
        PLColorsParser ().parse (file);
}

/*
 * region.txt parsing
 */

class MaskParser : public IniParser
{
public:
    Index<int> numpoints[SKIN_MASK_COUNT];
    Index<int> pointlist[SKIN_MASK_COUNT];

private:
    SkinMaskId current_id = SkinMaskId (-1);

    void handle_heading (const char * heading)
    {
        if (! g_ascii_strcasecmp (heading, "normal"))
            current_id = SKIN_MASK_MAIN;
        else if (! g_ascii_strcasecmp (heading, "windowshade"))
            current_id = SKIN_MASK_MAIN_SHADE;
        else if (! g_ascii_strcasecmp (heading, "equalizer"))
            current_id = SKIN_MASK_EQ;
        else if (! g_ascii_strcasecmp (heading, "equalizerws"))
            current_id = SKIN_MASK_EQ_SHADE;
        else
            current_id = (SkinMaskId) -1;
    }

    void handle_entry (const char * key, const char * value)
    {
        if (current_id == (SkinMaskId) -1)
            return;

        if (! g_ascii_strcasecmp (key, "numpoints"))
            numpoints[current_id] = string_to_int_array (value);
        else if (! g_ascii_strcasecmp (key, "pointlist"))
            pointlist[current_id] = string_to_int_array (value);
    }
};

static Index<GdkRectangle> skin_create_mask (const Index<int> & num,
 const Index<int> & point, int width, int height)
{
    Index<GdkRectangle> mask;

    int j = 0;
    for (int i = 0; i < num.len (); i ++)
    {
        int n_points = num[i];
        if (n_points <= 0 || j + 2 * n_points > point.len ())
            break;

        int xmin = width, ymin = height, xmax = 0, ymax = 0;

        for (int k = 0; k < n_points; k ++)
        {
            int x = point[j + k * 2];
            int y = point[j + k * 2 + 1];

            xmin = aud::min (xmin, x);
            ymin = aud::min (ymin, y);
            xmax = aud::max (xmax, x);
            ymax = aud::max (ymax, y);
        }

        if (xmax > xmin && ymax > ymin)
            mask.append (xmin, ymin, xmax - xmin, ymax - ymin);

        j += n_points * 2;
    }

    return mask;
}

void skin_load_masks (const char * path)
{
    int sizes[SKIN_MASK_COUNT][2] = {
        {skin.hints.mainwin_width, skin.hints.mainwin_height},
        {275, 16},
        {275, 116},
        {275, 16}
    };

    MaskParser parser;
    VFSFile file = open_local_file_nocase (path, "region.txt");
    if (file)
        parser.parse (file);

    for (int id = 0; id < SKIN_MASK_COUNT; id ++)
        skin.masks[id] = skin_create_mask (parser.numpoints[id],
         parser.pointlist[id], sizes[id][0], sizes[id][1]);
}
