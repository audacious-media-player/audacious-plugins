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

#include <libaudcore/inifile.h>

#include "skins_cfg.h"
#include "ui_skin.h"
#include "util.h"

/*
 * skin.hints parsing
 */

typedef struct {
    const char * name;
    int * value_ptr;
} HintPair;

const SkinProperties skin_default_hints = SkinProperties ();

/* so we can use static addresses in the table below */
static SkinProperties static_hints;

/* in alphabetical order to allow binary search */
static const HintPair hint_pairs[] = {
    {"mainwinaboutx", & static_hints.mainwin_about_x},
    {"mainwinabouty", & static_hints.mainwin_about_y},
    {"mainwinbalancex", & static_hints.mainwin_balance_x},
    {"mainwinbalancey", & static_hints.mainwin_balance_y},
    {"mainwinclosex", & static_hints.mainwin_close_x},
    {"mainwinclosey", & static_hints.mainwin_close_y},
    {"mainwinejectx", & static_hints.mainwin_eject_x},
    {"mainwinejecty", & static_hints.mainwin_eject_y},
    {"mainwineqbuttonx", & static_hints.mainwin_eqbutton_x},
    {"mainwineqbuttony", & static_hints.mainwin_eqbutton_y},
    {"mainwinheight", & static_hints.mainwin_height},
    {"mainwininfobarx", & static_hints.mainwin_infobar_x},
    {"mainwininfobary", & static_hints.mainwin_infobar_y},
    {"mainwinmenurowvisible", & static_hints.mainwin_menurow_visible},
    {"mainwinminimizex", & static_hints.mainwin_minimize_x},
    {"mainwinminimizey", & static_hints.mainwin_minimize_y},
    {"mainwinnextx", & static_hints.mainwin_next_x},
    {"mainwinnexty", & static_hints.mainwin_next_y},
    {"mainwinnumber0x", & static_hints.mainwin_number_0_x},
    {"mainwinnumber0y", & static_hints.mainwin_number_0_y},
    {"mainwinnumber1x", & static_hints.mainwin_number_1_x},
    {"mainwinnumber1y", & static_hints.mainwin_number_1_y},
    {"mainwinnumber2x", & static_hints.mainwin_number_2_x},
    {"mainwinnumber2y", & static_hints.mainwin_number_2_y},
    {"mainwinnumber3x", & static_hints.mainwin_number_3_x},
    {"mainwinnumber3y", & static_hints.mainwin_number_3_y},
    {"mainwinnumber4x", & static_hints.mainwin_number_4_x},
    {"mainwinnumber4y", & static_hints.mainwin_number_4_y},
    {"mainwinothertextisstatus", & static_hints.mainwin_othertext_is_status},
    {"mainwinothertextvisible", & static_hints.mainwin_othertext_visible},
    {"mainwinpausex", & static_hints.mainwin_pause_x},
    {"mainwinpausey", & static_hints.mainwin_pause_y},
    {"mainwinplaystatusx", & static_hints.mainwin_playstatus_x},
    {"mainwinplaystatusy", & static_hints.mainwin_playstatus_y},
    {"mainwinplayx", & static_hints.mainwin_play_x},
    {"mainwinplayy", & static_hints.mainwin_play_y},
    {"mainwinplbuttonx", & static_hints.mainwin_plbutton_x},
    {"mainwinplbuttony", & static_hints.mainwin_plbutton_y},
    {"mainwinpositionx", & static_hints.mainwin_position_x},
    {"mainwinpositiony", & static_hints.mainwin_position_y},
    {"mainwinpreviousx", & static_hints.mainwin_previous_x},
    {"mainwinpreviousy", & static_hints.mainwin_previous_y},
    {"mainwinrepeatx", & static_hints.mainwin_repeat_x},
    {"mainwinrepeaty", & static_hints.mainwin_repeat_y},
    {"mainwinshadex", & static_hints.mainwin_shade_x},
    {"mainwinshadey", & static_hints.mainwin_shade_y},
    {"mainwinshufflex", & static_hints.mainwin_shuffle_x},
    {"mainwinshuffley", & static_hints.mainwin_shuffle_y},
    {"mainwinstopx", & static_hints.mainwin_stop_x},
    {"mainwinstopy", & static_hints.mainwin_stop_y},
    {"mainwinstreaminfovisible", & static_hints.mainwin_streaminfo_visible},
    {"mainwintextvisible", & static_hints.mainwin_text_visible},
    {"mainwintextwidth", & static_hints.mainwin_text_width},
    {"mainwintextx", & static_hints.mainwin_text_x},
    {"mainwintexty", & static_hints.mainwin_text_y},
    {"mainwinvisvisible", & static_hints.mainwin_vis_visible},
    {"mainwinvisx", & static_hints.mainwin_vis_x},
    {"mainwinvisy", & static_hints.mainwin_vis_y},
    {"mainwinvolumex", & static_hints.mainwin_volume_x},
    {"mainwinvolumey", & static_hints.mainwin_volume_y},
    {"mainwinwidth", & static_hints.mainwin_width},
    {"textboxbitmapfontheight", & static_hints.textbox_bitmap_font_height},
    {"textboxbitmapfontwidth", & static_hints.textbox_bitmap_font_width},
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

void skin_load_hints (Skin * skin, const char * path)
{
    static_hints = skin_default_hints;

    VFSFile file = open_local_file_nocase (path, "skin.hints");
    if (file)
        HintsParser ().parse (file);

    skin->properties = static_hints;
}

/*
 * pledit.txt parsing
 */

class PLColorsParser : public IniParser
{
public:
    PLColorsParser (Skin & skin) :
        skin (skin),
        valid_heading (false) {}

private:
    Skin & skin;
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

void skin_load_pl_colors (Skin * skin, const char * path)
{
    skin->colors[SKIN_PLEDIT_NORMAL] = 0x2499ff;
    skin->colors[SKIN_PLEDIT_CURRENT] = 0xffeeff;
    skin->colors[SKIN_PLEDIT_NORMALBG] = 0x0a120a;
    skin->colors[SKIN_PLEDIT_SELECTEDBG] = 0x0a124a;

    VFSFile file = open_local_file_nocase (path, "pledit.txt");
    if (file)
        PLColorsParser (* skin).parse (file);
}

/*
 * region.txt parsing
 */

class MaskParser : public IniParser
{
public:
    GArray * numpoints[SKIN_MASK_COUNT] {};
    GArray * pointlist[SKIN_MASK_COUNT] {};

    ~MaskParser ()
    {
        for (GArray * array : numpoints)
            if (array) g_array_free (array, true);
        for (GArray * array : pointlist)
            if (array) g_array_free (array, true);
    }

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
        {
            if (! numpoints[current_id])
                numpoints[current_id] = string_to_garray (value);
        }
        else if (! g_ascii_strcasecmp (key, "pointlist"))
        {
            if (! pointlist[current_id])
                pointlist[current_id] = string_to_garray (value);
        }
    }
};

static GdkBitmap * skin_create_mask (const GArray * num,
 const GArray * point, int width, int height)
{
    width *= config.scale;
    height *= config.scale;

    GdkBitmap * bitmap = gdk_pixmap_new (nullptr, width, height, 1);
    cairo_t * cr = gdk_cairo_create (bitmap);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    cairo_set_source_rgba (cr, 0, 0, 0, 0);
    cairo_rectangle (cr, 0, 0, width, height);
    cairo_fill (cr);

    cairo_set_source_rgba (cr, 0, 0, 0, 1);

    gboolean created_mask = FALSE;

    if (num && point)
    {
        unsigned j = 0;
        for (unsigned i = 0; i < num->len; i ++)
        {
            int n_points = g_array_index (num, int, i);
            if (n_points <= 0 || j + 2 * n_points > point->len)
                break;

            int xmin = width, ymin = height, xmax = 0, ymax = 0;

            for (int k = 0; k < n_points; k ++)
            {
                int x = g_array_index (point, int, j + k * 2) * config.scale;
                int y = g_array_index (point, int, j + k * 2 + 1) * config.scale;

                xmin = aud::min (xmin, x);
                ymin = aud::min (ymin, y);
                xmax = aud::max (xmax, x);
                ymax = aud::max (ymax, y);
            }

            if (xmax > xmin && ymax > ymin)
                cairo_rectangle (cr, xmin, ymin, xmax - xmin, ymax - ymin);

            created_mask = TRUE;
            j += n_points * 2;
        }
    }

    if (! created_mask)
        cairo_rectangle (cr, 0, 0, width, height);

    cairo_fill (cr);

    cairo_destroy (cr);
    return bitmap;
}

void skin_load_masks (Skin * skin, const char * path)
{
    int sizes[SKIN_MASK_COUNT][2] = {
        {skin->properties.mainwin_width, skin->properties.mainwin_height},
        {275, 16},
        {275, 116},
        {275, 16}
    };

    MaskParser parser;
    VFSFile file = open_local_file_nocase (path, "region.txt");
    if (file)
        parser.parse (file);

    for (int id = 0; id < SKIN_MASK_COUNT; id ++)
        skin->masks[id] = skin_create_mask (parser.numpoints[id],
         parser.pointlist[id], sizes[id][0], sizes[id][1]);
}
