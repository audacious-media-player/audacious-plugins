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

#include <libaudcore/inifile.h>

#include "ui_skin.h"
#include "util.h"

static cairo_region_t * skin_create_mask (INIFile * inifile,
 const char * section, int width, int height)
{
    GArray * num = NULL, * point = NULL;

    if (inifile)
    {
        const char * str;

        if ((str = inifile_lookup (inifile, section, "numpoints")))
            num = string_to_garray (str);
        if ((str = inifile_lookup (inifile, section, "pointlist")))
            point = string_to_garray (str);
    }

    if (! num || ! point)
    {
        if (num)
            g_array_free (num, TRUE);
        if (point)
            g_array_free (point, TRUE);

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

    g_array_free (num, TRUE);
    g_array_free (point, TRUE);

    if (! created_mask)
    {
        cairo_rectangle_int_t rect = {0, 0, width, height};
        cairo_region_union_rectangle (mask, & rect);
    }

    return mask;
}

void skin_load_masks (Skin * skin, const char * path)
{
    VFSFile * file = open_local_file_nocase (path, "region.txt");
    INIFile * inifile = file ? inifile_read (file) : NULL;

    skin->masks[SKIN_MASK_MAIN] = skin_create_mask (inifile, "normal",
     skin->properties.mainwin_width, skin->properties.mainwin_height);
    skin->masks[SKIN_MASK_MAIN_SHADE] = skin_create_mask (inifile, "windowshade", 275, 16);
    skin->masks[SKIN_MASK_EQ] = skin_create_mask (inifile, "equalizer", 275, 116);
    skin->masks[SKIN_MASK_EQ_SHADE] = skin_create_mask (inifile, "equalizerws", 275, 16);

    if (file)
        vfs_fclose (file);
    if (inifile)
        inifile_destroy (inifile);
}
