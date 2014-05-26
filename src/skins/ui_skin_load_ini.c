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

#include "ui_skin.h"
#include "util.h"

/*
 * skin.hints parsing
 */

typedef struct {
    const char * name;
    int * value_ptr;
} HintPair;

typedef struct {
    bool_t valid_heading;
} HintsLoadState;

const SkinProperties skin_default_hints = {
    .mainwin_vis_x = 24,
    .mainwin_vis_y = 43,
    .mainwin_vis_visible = TRUE,

    .mainwin_text_x = 112,
    .mainwin_text_y = 27,
    .mainwin_text_width = 153,
    .mainwin_text_visible = TRUE,

    .mainwin_infobar_x = 112,
    .mainwin_infobar_y = 43,
    .mainwin_othertext_visible = FALSE,

    .mainwin_number_0_x = 36,
    .mainwin_number_0_y = 26,

    .mainwin_number_1_x = 48,
    .mainwin_number_1_y = 26,

    .mainwin_number_2_x = 60,
    .mainwin_number_2_y = 26,

    .mainwin_number_3_x = 78,
    .mainwin_number_3_y = 26,

    .mainwin_number_4_x = 90,
    .mainwin_number_4_y = 26,

    .mainwin_playstatus_x = 24,
    .mainwin_playstatus_y = 28,

    .mainwin_volume_x = 107,
    .mainwin_volume_y = 57,

    .mainwin_balance_x = 177,
    .mainwin_balance_y = 57,

    .mainwin_position_x = 16,
    .mainwin_position_y = 72,

    .mainwin_previous_x = 16,
    .mainwin_previous_y = 88,

    .mainwin_play_x = 39,
    .mainwin_play_y = 88,

    .mainwin_pause_x = 62,
    .mainwin_pause_y = 88,

    .mainwin_stop_x = 85,
    .mainwin_stop_y = 88,

    .mainwin_next_x = 108,
    .mainwin_next_y = 88,

    .mainwin_eject_x = 136,
    .mainwin_eject_y = 89,

    .mainwin_eqbutton_x = 219,
    .mainwin_eqbutton_y = 58,

    .mainwin_plbutton_x = 242,
    .mainwin_plbutton_y = 58,

    .mainwin_shuffle_x = 164,
    .mainwin_shuffle_y = 89,

    .mainwin_repeat_x = 210,
    .mainwin_repeat_y = 89,

    .mainwin_about_x = 247,
    .mainwin_about_y = 83,

    .mainwin_minimize_x = 244,
    .mainwin_minimize_y = 3,

    .mainwin_shade_x = 254,
    .mainwin_shade_y = 3,

    .mainwin_close_x = 264,
    .mainwin_close_y = 3,

    .mainwin_width = 275,
    .mainwin_height = 116,

    .mainwin_menurow_visible = TRUE,
    .mainwin_streaminfo_visible = TRUE,
    .mainwin_othertext_is_status = FALSE,

    .textbox_bitmap_font_width = 5,
    .textbox_bitmap_font_height = 6
};

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

static int hint_pair_compare (const void * a, const void * b)
{
    return g_ascii_strcasecmp (((const HintPair *) a)->name, ((const HintPair *) b)->name);
}

static void hints_handle_heading (const char * heading, void * data)
{
    HintsLoadState * state = data;

    state->valid_heading = ! g_ascii_strcasecmp (heading, "skin");
}

static void hints_handle_entry (const char * key, const char * value, void * data)
{
    HintsLoadState * state = data;

    if (! state->valid_heading)
        return;

    HintPair key_pair = {.name = key};
    HintPair * pair = bsearch (& key_pair, hint_pairs,
     ARRAY_LEN (hint_pairs), sizeof (HintPair), hint_pair_compare);

    if (pair)
        * pair->value_ptr = atoi (value);
}

void skin_load_hints (Skin * skin, const char * path)
{
    static_hints = skin_default_hints;

    HintsLoadState state = {.valid_heading = FALSE};

    VFSFile * file = open_local_file_nocase (path, "skin.hints");

    if (file)
    {
        inifile_parse (file, hints_handle_heading, hints_handle_entry, & state);
        vfs_fclose (file);
    }

    skin->properties = static_hints;
}

/*
 * pledit.txt parsing
 */

typedef struct {
    bool_t valid_heading;
    Skin * skin;
} PLColorsLoadState;

static void pl_colors_handle_heading (const char * heading, void * data)
{
    PLColorsLoadState * state = data;

    state->valid_heading = ! g_ascii_strcasecmp (heading, "text");
}

static uint32_t convert_color_string (const char * str)
{
    if (* str == '#')
        str ++;

    return strtol (str, NULL, 16);
}

static void pl_colors_handle_entry (const char * key, const char * value, void * data)
{
    PLColorsLoadState * state = data;

    if (! state->valid_heading)
        return;

    if (! g_ascii_strcasecmp (key, "normal"))
        state->skin->colors[SKIN_PLEDIT_NORMAL] = convert_color_string (value);
    else if (! g_ascii_strcasecmp (key, "current"))
        state->skin->colors[SKIN_PLEDIT_CURRENT] = convert_color_string (value);
    else if (! g_ascii_strcasecmp (key, "normalbg"))
        state->skin->colors[SKIN_PLEDIT_NORMALBG] = convert_color_string (value);
    else if (! g_ascii_strcasecmp (key, "selectedbg"))
        state->skin->colors[SKIN_PLEDIT_SELECTEDBG] = convert_color_string (value);
}

void skin_load_pl_colors (Skin * skin, const char * path)
{
    skin->colors[SKIN_PLEDIT_NORMAL] = 0x2499ff;
    skin->colors[SKIN_PLEDIT_CURRENT] = 0xffeeff;
    skin->colors[SKIN_PLEDIT_NORMALBG] = 0x0a120a;
    skin->colors[SKIN_PLEDIT_SELECTEDBG] = 0x0a124a;

    PLColorsLoadState state = {
        .valid_heading = FALSE,
        .skin = skin
    };

    VFSFile * file = open_local_file_nocase (path, "pledit.txt");

    if (file)
    {
        inifile_parse (file, pl_colors_handle_heading, pl_colors_handle_entry, & state);
        vfs_fclose (file);
    }
}

/*
 * region.txt parsing
 */

typedef struct {
    SkinMaskId current_id;
    GArray * numpoints[SKIN_MASK_COUNT];
    GArray * pointlist[SKIN_MASK_COUNT];
} MaskLoadState;

static void mask_handle_heading (const char * heading, void * data)
{
    MaskLoadState * state = data;

    if (! g_ascii_strcasecmp (heading, "normal"))
        state->current_id = SKIN_MASK_MAIN;
    else if (! g_ascii_strcasecmp (heading, "windowshade"))
        state->current_id = SKIN_MASK_MAIN_SHADE;
    else if (! g_ascii_strcasecmp (heading, "equalizer"))
        state->current_id = SKIN_MASK_EQ;
    else if (! g_ascii_strcasecmp (heading, "equalizerws"))
        state->current_id = SKIN_MASK_EQ_SHADE;
    else
        state->current_id = -1;
}

static void mask_handle_entry (const char * key, const char * value, void * data)
{
    MaskLoadState * state = data;
    SkinMaskId id = state->current_id;

    if (id == (SkinMaskId) -1)
        return;

    if (! g_ascii_strcasecmp (key, "numpoints"))
    {
        if (! state->numpoints[id])
            state->numpoints[id] = string_to_garray (value);
    }
    else if (! g_ascii_strcasecmp (key, "pointlist"))
    {
        if (! state->pointlist[id])
            state->pointlist[id] = string_to_garray (value);
    }
}

static cairo_region_t * skin_create_mask (const GArray * num,
 const GArray * point, int width, int height)
{
    if (! num || ! point)
    {
        cairo_rectangle_int_t rect = {0, 0, width, height};
        return cairo_region_create_rectangle (& rect);
    }

    cairo_region_t * mask = cairo_region_create ();
    gboolean created_mask = FALSE;

    int j = 0;
    for (int i = 0; i < num->len; i ++)
    {
        int n_points = g_array_index (num, int, i);
        if (n_points <= 0 || j + 2 * n_points > point->len)
            break;

        GdkPoint gpoints[n_points];
        for (int k = 0; k < n_points; k ++)
        {
            gpoints[k].x = g_array_index (point, int, j + k * 2);
            gpoints[k].y = g_array_index (point, int, j + k * 2 + 1);
        }

        int xmin = width, ymin = height, xmax = 0, ymax = 0;
        for (int k = 0; k < n_points; k ++)
        {
            xmin = MIN (xmin, gpoints[k].x);
            ymin = MIN (ymin, gpoints[k].y);
            xmax = MAX (xmax, gpoints[k].x);
            ymax = MAX (ymax, gpoints[k].y);
        }

        if (xmax > xmin && ymax > ymin)
        {
            cairo_rectangle_int_t rect = {xmin, ymin, xmax - xmin, ymax - ymin};
            cairo_region_union_rectangle (mask, & rect);
        }

        created_mask = TRUE;
        j += n_points * 2;
    }

    if (! created_mask)
    {
        cairo_rectangle_int_t rect = {0, 0, width, height};
        cairo_region_union_rectangle (mask, & rect);
    }

    return mask;
}

void skin_load_masks (Skin * skin, const char * path)
{
    int sizes[SKIN_MASK_COUNT][2] = {
        {skin->properties.mainwin_width, skin->properties.mainwin_height},
        {275, 16},
        {275, 116},
        {275, 16}
    };

    MaskLoadState state = {.current_id = -1};

    VFSFile * file = open_local_file_nocase (path, "region.txt");

    if (file)
    {
        inifile_parse (file, mask_handle_heading, mask_handle_entry, & state);
        vfs_fclose (file);
    }

    for (SkinMaskId id = 0; id < SKIN_MASK_COUNT; id ++)
    {
        skin->masks[id] = skin_create_mask (state.numpoints[id],
         state.pointlist[id], sizes[id][0], sizes[id][1]);

        if (state.numpoints[id])
            g_array_free (state.numpoints[id], TRUE);
        if (state.pointlist[id])
            g_array_free (state.pointlist[id], TRUE);
    }
}
