/*  Audacious
 *  Copyright (C) 2005-2011  Audacious development team.
 *
 *  BMP - Cross-platform multimedia player
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <gtk/gtk.h>

#include <libaudcore/runtime.h>
#include <libaudcore/runtime.h>

#include "plugin.h"
#include "skins_cfg.h"
#include "surface.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_playlist.h"
#include "ui_skin.h"
#include "ui_skinned_number.h"
#include "ui_skinned_playstatus.h"
#include "ui_skinned_textbox.h"
#include "ui_skinned_window.h"
#include "ui_skinselector.h"
#include "ui_vis.h"
#include "util.h"

#define EXTENSION_TARGETS 7

static const char *ext_targets[EXTENSION_TARGETS] =
{ "bmp", "xpm", "png", "svg", "gif", "jpg", "jpeg" };

struct _SkinPixmapIdMapping {
    SkinPixmapId id;
    const char *name;
    const char *alt_name;
    int width, height;
};

typedef struct _SkinPixmapIdMapping SkinPixmapIdMapping;

static gboolean skin_load (Skin * skin, const char * path);

Skin *active_skin = nullptr;

static SkinPixmapIdMapping skin_pixmap_id_map[] = {
    {SKIN_MAIN, "main", nullptr, 0, 0},
    {SKIN_CBUTTONS, "cbuttons", nullptr, 0, 0},
    {SKIN_SHUFREP, "shufrep", nullptr, 0, 0},
    {SKIN_TEXT, "text", nullptr, 0, 0},
    {SKIN_TITLEBAR, "titlebar", nullptr, 0, 0},
    {SKIN_VOLUME, "volume", nullptr, 0, 0},
    {SKIN_BALANCE, "balance", "volume", 0, 0},
    {SKIN_MONOSTEREO, "monoster", nullptr, 0, 0},
    {SKIN_PLAYPAUSE, "playpaus", nullptr, 0, 0},
    {SKIN_NUMBERS, "nums_ex", "numbers", 0, 0},
    {SKIN_POSBAR, "posbar", nullptr, 0, 0},
    {SKIN_EQMAIN, "eqmain", nullptr, 0, 0},
    {SKIN_PLEDIT, "pledit", nullptr, 0, 0},
    {SKIN_EQ_EX, "eq_ex", nullptr, 0, 0}
};

static const uint32_t default_vis_colors[24] = {
    COLOR (9, 34, 53),
    COLOR (10, 18, 26),
    COLOR (0, 54, 108),
    COLOR (0, 58, 116),
    COLOR (0, 62, 124),
    COLOR (0, 66, 132),
    COLOR (0, 70, 140),
    COLOR (0, 74, 148),
    COLOR (0, 78, 156),
    COLOR (0, 82, 164),
    COLOR (0, 86, 172),
    COLOR (0, 92, 184),
    COLOR (0, 98, 196),
    COLOR (0, 104, 208),
    COLOR (0, 110, 220),
    COLOR (0, 116, 232),
    COLOR (0, 122, 244),
    COLOR (0, 128, 255),
    COLOR (0, 128, 255),
    COLOR (0, 104, 208),
    COLOR (0, 80, 160),
    COLOR (0, 56, 112),
    COLOR (0, 32, 64),
    COLOR (200, 200, 200)
};

gboolean active_skin_load (const char * path)
{
    AUDDBG("%s\n", path);
    g_return_val_if_fail(active_skin != nullptr, FALSE);

    if (!skin_load(active_skin, path)) {
        AUDDBG("loading failed\n");
        return FALSE;
    }

    mainwin_refresh_hints ();
    textbox_update_all ();
    ui_vis_set_colors ();
    gtk_widget_queue_draw (mainwin);
    gtk_widget_queue_draw (equalizerwin);
    gtk_widget_queue_draw (playlistwin);

    aud_set_str ("skins", "skin", path);

    return TRUE;
}

static Skin *
skin_new(void)
{
    Skin *skin;
    skin = g_new0(Skin, 1);
    return skin;
}

/**
 * Frees the data associated for skin.
 *
 * Does not free skin itself or lock variable so that the skin can immediately
 * populated with new skin data if needed.
 */
static void
skin_free(Skin * skin)
{
    int i;

    g_return_if_fail(skin != nullptr);

    for (i = 0; i < SKIN_PIXMAP_COUNT; i++)
    {
        if (skin->pixmaps[i])
        {
            cairo_surface_destroy (skin->pixmaps[i]);
            skin->pixmaps[i] = nullptr;
        }
    }

    g_free(skin->path);
    skin->path = nullptr;
}

static void
skin_destroy(Skin * skin)
{
    g_return_if_fail(skin != nullptr);
    skin_free(skin);
    g_free(skin);
}

static const SkinPixmapIdMapping *
skin_pixmap_id_lookup(unsigned id)
{
    for (auto & info : skin_pixmap_id_map)
    {
        if (info.id == id)
            return & info;
    }

    return nullptr;
}

static char * skin_pixmap_locate (const char * dirname, char * * basenames)
{
    char * filename = nullptr;
    int i;

    for (i = 0; basenames[i] != nullptr; i ++)
    {
        if ((filename = find_file_case_path (dirname, basenames[i])) != nullptr)
            break;
    }

    return filename;
}

/**
 * Creates possible file names for a pixmap.
 *
 * Basically this makes list of all possible file names that pixmap data
 * can be found from by using the static ext_targets variable to get all
 * possible extensions that pixmap file might have.
 */
static char **
skin_pixmap_create_basenames(const SkinPixmapIdMapping * pixmap_id_mapping)
{
    char **basenames = g_new0 (char *, EXTENSION_TARGETS * 2 + 1);
    int i, y;

    // Create list of all possible image formats that can be loaded
    for (i = 0, y = 0; i < EXTENSION_TARGETS; i++, y++)
    {
        basenames[y] =
            g_strdup_printf("%s.%s", pixmap_id_mapping->name, ext_targets[i]);

        if (pixmap_id_mapping->alt_name)
            basenames[++y] =
                g_strdup_printf("%s.%s", pixmap_id_mapping->alt_name,
                                ext_targets[i]);
    }

    return basenames;
}

/**
 * Frees the data allocated by skin_pixmap_create_basenames
 */
static void
skin_pixmap_free_basenames(char ** basenames)
{
    int i;
    for (i = 0; basenames[i] != nullptr; i++)
    {
        g_free(basenames[i]);
        basenames[i] = nullptr;
    }
    g_free(basenames);
}

/**
 * Locates a pixmap file for skin.
 */
static char *
skin_pixmap_locate_basenames(const Skin * skin,
                             const SkinPixmapIdMapping * pixmap_id_mapping,
                             const char * path_p)
{
    char *filename = nullptr;
    const char *path = path_p ? path_p : skin->path;
    char **basenames = skin_pixmap_create_basenames(pixmap_id_mapping);

    filename = skin_pixmap_locate(path, basenames);

    skin_pixmap_free_basenames(basenames);

    if (! filename)
        AUDERR ("Skin does not contain a \"%s\" pixmap.\n",
         pixmap_id_mapping->name);

    return filename;
}


static gboolean
skin_load_pixmap_id(Skin * skin, SkinPixmapId id, const char * path_p)
{
    const SkinPixmapIdMapping *pixmap_id_mapping;
    char *filename;

    g_return_val_if_fail(skin != nullptr, FALSE);
    g_return_val_if_fail(id < SKIN_PIXMAP_COUNT, FALSE);
    g_return_val_if_fail(! skin->pixmaps[id], FALSE);

    pixmap_id_mapping = skin_pixmap_id_lookup(id);
    g_return_val_if_fail(pixmap_id_mapping != nullptr, FALSE);

    filename = skin_pixmap_locate_basenames(skin, pixmap_id_mapping, path_p);

    if (filename == nullptr)
        return FALSE;

    skin->pixmaps[id] = surface_new_from_file (filename);

    g_free (filename);
    return skin->pixmaps[id] ? TRUE : FALSE;
}

static int color_diff (uint32_t a, uint32_t b)
{
    return abs (COLOR_R (a) - COLOR_R (b)) + abs (COLOR_G (a) - COLOR_G (b)) +
     abs (COLOR_B (a) - COLOR_B (b));
}

static void skin_get_textcolors (Skin * skin, cairo_surface_t * s)
{
    /*
     * Try to extract reasonable background and foreground colors
     * from the font pixmap
     */

    /* Get a pixel from the middle of the space character */
    skin->colors[SKIN_TEXTBG] = surface_get_pixel (s, 152, 3);

    int max_d = -1;
    for (int y = 0; y < 6; y ++)
    {
        for (int x = 1; x < 150; x ++)
        {
            int c = surface_get_pixel (s, x, y);
            int d = color_diff (skin->colors[SKIN_TEXTBG], c);
            if (d > max_d)
            {
                skin->colors[SKIN_TEXTFG] = c;
                max_d = d;
            }
        }
    }
}

gboolean
init_skins(const char * path)
{
    active_skin = skin_new();

    active_skin->properties = skin_default_hints;

    /* create the windows if they haven't been created yet, needed for bootstrapping */
    if (mainwin == nullptr)
    {
        mainwin_create();
        equalizerwin_create();
        playlistwin_create();
    }

    if (! path || ! active_skin_load (path))
    {
        if (path != nullptr)
            AUDDBG("Unable to load skin (%s), trying default...\n", path);
        else
            AUDDBG("Skin not defined: trying default...\n");

        /* can't load configured skin, retry with default */
        char * def = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "Skins"
         G_DIR_SEPARATOR_S "Default", aud_get_path (AudPath::DataDir));

        if (! active_skin_load (def))
        {
            AUDDBG ("Unable to load default skin (%s)! Giving up.\n", def);
            g_free (def);
            return FALSE;
        }

        g_free (def);
    }

    return TRUE;
}

void cleanup_skins()
{
    skin_destroy(active_skin);
    active_skin = nullptr;

    gtk_widget_destroy (mainwin);
    mainwin = nullptr;
    gtk_widget_destroy (playlistwin);
    playlistwin = nullptr;
    gtk_widget_destroy (equalizerwin);
    equalizerwin = nullptr;
}

static void skin_load_viscolor (Skin * skin, const char * path)
{
    memcpy (skin->vis_colors, default_vis_colors, sizeof skin->vis_colors);

    VFSFile file = open_local_file_nocase (path, "viscolor.txt");
    if (! file)
        return;

    Index<char> buffer = file.read_all ();
    buffer.append (0);  /* null-terminated */

    char * string = buffer.begin ();

    for (int line = 0; string && line < 24; line ++)
    {
        char * next = text_parse_line (string);
        GArray * array = string_to_garray (string);

        if (array->len >= 3)
            skin->vis_colors[line] = COLOR (g_array_index (array, int, 0),
             g_array_index (array, int, 1), g_array_index (array, int, 2));

        g_array_free (array, TRUE);
        string = next;
    }
}

static void
skin_numbers_generate_dash(Skin * skin)
{
    g_return_if_fail(skin != nullptr);

    cairo_surface_t * old = skin->pixmaps[SKIN_NUMBERS];
    if (! old || cairo_image_surface_get_width (old) < 99)
        return;

    int h = cairo_image_surface_get_height (old);
    cairo_surface_t * surface = surface_new (108, h);

    surface_copy_rect (old, 0, 0, 99, h, surface, 0, 0);
    surface_copy_rect (old, 90, 0, 9, h, surface, 99, 0);
    surface_copy_rect (old, 20, 6, 5, 1, surface, 101, 6);

    cairo_surface_destroy (old);
    skin->pixmaps[SKIN_NUMBERS] = surface;
}

static gboolean
skin_load_pixmaps(Skin * skin, const char * path)
{
    AUDDBG("Loading pixmaps in %s\n", path);

    for (int i = 0; i < SKIN_PIXMAP_COUNT; i++)
        if (! skin_load_pixmap_id (skin, (SkinPixmapId) i, path))
            return FALSE;

    if (skin->pixmaps[SKIN_TEXT])
        skin_get_textcolors (skin, skin->pixmaps[SKIN_TEXT]);

    if (skin->pixmaps[SKIN_NUMBERS] && cairo_image_surface_get_width
     (skin->pixmaps[SKIN_NUMBERS]) < 108)
        skin_numbers_generate_dash (skin);

    skin_load_pl_colors (skin, path);
    skin_load_viscolor (skin, path);

    return TRUE;
}

/**
 * Checks if all pixmap files exist that skin needs.
 */
static gboolean
skin_check_pixmaps(const Skin * skin, const char * skin_path)
{
    unsigned i;
    for (i = 0; i < SKIN_PIXMAP_COUNT; i++)
    {
        char *filename = skin_pixmap_locate_basenames(skin,
                                                       skin_pixmap_id_lookup(i),
                                                       skin_path);
        if (!filename)
            return FALSE;
        g_free(filename);
    }
    return TRUE;
}

static gboolean
skin_load_nolock(Skin * skin, const char * path, gboolean force)
{
    char *newpath, *skin_path;
    int archive = 0;

    AUDDBG("Attempt to load skin \"%s\"\n", path);

    g_return_val_if_fail(skin != nullptr, FALSE);
    g_return_val_if_fail(path != nullptr, FALSE);

    if (! g_file_test (path, G_FILE_TEST_EXISTS))
        return FALSE;

    if(force) AUDDBG("reloading forced!\n");
    if (!force && skin->path && !strcmp(skin->path, path)) {
        AUDDBG("skin %s already loaded\n", path);
        return FALSE;
    }

    if (file_is_archive(path)) {
        AUDDBG("Attempt to load archive\n");
        if (!(skin_path = archive_decompress(path))) {
            AUDDBG("Unable to extract skin archive (%s)\n", path);
            return FALSE;
        }
        archive = 1;
    } else {
        skin_path = g_strdup(path);
    }

    // Check if skin path has all necessary files.
    if (!skin_check_pixmaps(skin, skin_path)) {
        if(archive) del_directory(skin_path);
        AUDDBG("Skin path (%s) doesn't have all wanted pixmaps\n", skin_path);
        g_free(skin_path);
        return FALSE;
    }

    // skin_free() frees skin->path and variable path can actually be skin->path
    // and we want to get the path before possibly freeing it.
    newpath = g_strdup(path);
    skin_free(skin);
    skin->path = newpath;

    skin_load_hints (skin, skin_path);

    if (!skin_load_pixmaps(skin, skin_path)) {
        if(archive) del_directory(skin_path);
        g_free(skin_path);
        AUDDBG("Skin loading failed\n");
        return FALSE;
    }

    GdkRegion * masks[SKIN_MASK_COUNT];
    skin_load_masks (skin, skin_path, masks);
    window_set_shapes (mainwin, masks[SKIN_MASK_MAIN], masks[SKIN_MASK_MAIN_SHADE]);
    window_set_shapes (equalizerwin, masks[SKIN_MASK_EQ], masks[SKIN_MASK_EQ_SHADE]);

    if(archive) del_directory(skin_path);
    g_free(skin_path);

    return TRUE;
}

void skin_install_skin (const char * path)
{
    GError * err = 0;
    char * data;
    size_t len;

    if (! g_file_get_contents (path, & data, & len, & err))
    {
        AUDERR ("Failed to read %s: %s\n", path, err->message);
        g_error_free (err);
        return;
    }

    const char * user_skin_dir = skins_get_user_skin_dir ();
    make_directory (user_skin_dir);

    char * base = g_path_get_basename (path);
    char * target = g_build_filename (user_skin_dir, base, nullptr);

    if (! g_file_set_contents (target, data, len, & err))
    {
        AUDERR ("Failed to write %s: %s\n", path, err->message);
        g_error_free (err);
        g_free (data);
        g_free (base);
        g_free (target);
        return;
    }

    g_free (data);
    g_free (base);
    g_free (target);
}

static gboolean skin_load (Skin * skin, const char * path)
{
    gboolean ret;

    g_return_val_if_fail(skin != nullptr, FALSE);

    if (!path)
        return FALSE;

    ret = skin_load_nolock(skin, path, FALSE);

    if(!ret) {
        AUDDBG("loading failed\n");
        return FALSE; /* don't try to update anything if loading failed --asphyx */
    }

    if (skin->pixmaps[SKIN_NUMBERS])
    {
        int h = cairo_image_surface_get_height (skin->pixmaps[SKIN_NUMBERS]);
        ui_skinned_number_set_size (mainwin_minus_num, 9, h);
        ui_skinned_number_set_size (mainwin_10min_num, 9, h);
        ui_skinned_number_set_size (mainwin_min_num, 9, h);
        ui_skinned_number_set_size (mainwin_10sec_num, 9, h);
        ui_skinned_number_set_size (mainwin_sec_num, 9, h);
    }

    if (skin->pixmaps[SKIN_PLAYPAUSE])
        ui_skinned_playstatus_set_size (mainwin_playstatus, 11,
         cairo_image_surface_get_height (skin->pixmaps[SKIN_PLAYPAUSE]));

    // hide the menurow if it is not present in the skin
    if (cairo_image_surface_get_width (skin->pixmaps[SKIN_TITLEBAR]) < 344)
        skin->properties.mainwin_menurow_visible = false;

    // hide the equalizer graph if we have a short eqmain.bmp
    gtk_widget_set_visible (equalizerwin_graph,
     cairo_image_surface_get_height (skin->pixmaps[SKIN_EQMAIN]) >= 315);

    return TRUE;
}

void skin_draw_pixbuf (cairo_t * cr, SkinPixmapId id, int xsrc, int ysrc, int
 xdest, int ydest, int width, int height)
{
    if (! active_skin->pixmaps[id])
        return;

    cairo_save (cr);
    cairo_scale (cr, config.scale, config.scale);
    cairo_set_source_surface (cr, active_skin->pixmaps[id], xdest - xsrc, ydest - ysrc);
    cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_NEAREST);
    cairo_rectangle (cr, xdest, ydest, width, height);
    cairo_fill (cr);
    cairo_restore (cr);
}

void skin_get_eq_spline_colors (Skin * skin, uint32_t colors[19])
{
    if (! skin->pixmaps[SKIN_EQMAIN])
    {
        memset (colors, 0, sizeof (uint32_t) * 19);
        return;
    }

    for (int i = 0; i < 19; i ++)
        colors[i] = surface_get_pixel (skin->pixmaps[SKIN_EQMAIN], 115, i + 294);
}

static void skin_draw_playlistwin_frame_top (cairo_t * cr, int width, int
 height, gboolean focus)
{
    /* The title bar skin consists of 2 sets of 4 images, 1 set
     * for focused state and the other for unfocused. The 4 images
     * are:
     *
     * a. right corner (25,20)
     * b. left corner  (25,20)
     * c. tiler        (25,20)
     * d. title        (100,20)
     *
     * min allowed width = 100+25+25 = 150
     */

    int i, y, c;

    /* get y offset of the pixmap set to use */
    if (focus)
        y = 0;
    else
        y = 21;

    /* left corner */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 0, y, 0, 0, 25, 20);

    /* titlebar title */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 26, y, (width - 100) / 2, 0, 100, 20);

    /* titlebar right corner  */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 153, y, width - 25, 0, 25, 20);

    /* tile draw the remaining frame */

    /* compute tile count */
    c = (width - (100 + 25 + 25)) / 25;

    for (i = 0; i < c / 2; i++) {
        /* left of title */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 127, y, 25 + i * 25, 0, 25, 20);

        /* right of title */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 127, y, (width + 100) / 2 + i * 25,
         0, 25, 20);
    }

    if (c & 1) {
        /* Odd tile count, so one remaining to draw. Here we split
         * it into two and draw half on either side of the title */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 127, y, ((c / 2) * 25) + 25, 0, 12,
         20);
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 127, y, (width / 2) + ((c / 2) * 25)
         + 50, 0, 13, 20);
    }
}

static void skin_draw_playlistwin_frame_bottom (cairo_t * cr, int width, int
 height, gboolean focus)
{
    /* The bottom frame skin consists of 1 set of 4 images. The 4
     * images are:
     *
     * a. left corner with menu buttons (125,38)
     * b. visualization window (75,38)
     * c. right corner with play buttons (150,38)
     * d. frame tile (25,38)
     *
     * (min allowed width = 125+150+25=300
     */

    int i, c;

    /* bottom left corner (menu buttons) */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 0, 72, 0, height - 38, 125, 38);

    c = (width - 275) / 25;

    /* draw visualization window, if width allows */
    if (c >= 3) {
        c -= 3;
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 205, 0, width - (150 + 75), height -
         38, 75, 38);
    }

    /* Bottom right corner (playbuttons etc) */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 126, 72, width - 150, height - 38, 150,
     38);

    /* Tile draw the remaining undrawn portions */
    for (i = 0; i < c; i++)
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 179, 0, 125 + i * 25, height - 38,
         25, 38);
}

static void skin_draw_playlistwin_frame_sides (cairo_t * cr, int width, int
 height, gboolean focus)
{
    /* The side frames consist of 2 tile images. 1 for the left, 1 for
     * the right.
     * a. left  (12,29)
     * b. right (19,29)
     */

    int i;

    /* frame sides */
    for (i = 0; i < (height - (20 + 38)) / 29; i++) {
        /* left */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 0, 42, 0, 20 + i * 29, 12, 29);

        /* right */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 32, 42, width - 19, 20 + i * 29, 19,
         29);
    }
}

void skin_draw_playlistwin_frame (cairo_t * cr, int width, int height,
 gboolean focus)
{
    skin_draw_playlistwin_frame_top (cr, width, height, focus);
    skin_draw_playlistwin_frame_bottom (cr, width, height, focus);
    skin_draw_playlistwin_frame_sides (cr, width, height, focus);
}

void skin_draw_playlistwin_shaded (cairo_t * cr, int width, gboolean focus)
{
    /* The shade mode titlebar skin consists of 4 images:
     * a) left corner               offset (72,42) size (25,14)
     * b) right corner, focused     offset (99,57) size (50,14)
     * c) right corner, unfocused   offset (99,42) size (50,14)
     * d) bar tile                  offset (72,57) size (25,14)
     */

    int i;

    /* left corner */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 72, 42, 0, 0, 25, 14);

    /* bar tile */
    for (i = 0; i < (width - 75) / 25; i++)
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 72, 57, (i * 25) + 25, 0, 25, 14);

    /* right corner */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 99, focus ? 42 : 57, width - 50, 0, 50,
     14);
}

void skin_draw_mainwin_titlebar (cairo_t * cr, gboolean shaded, gboolean focus)
{
    /* The titlebar skin consists of 2 sets of 2 images, one for for
     * shaded and the other for unshaded mode, giving a total of 4.
     * The images are exactly 275x14 pixels, aligned and arranged
     * vertically on each other in the pixmap in the following order:
     *
     * a) unshaded, focused      offset (27, 0)
     * b) unshaded, unfocused    offset (27, 15)
     * c) shaded, focused        offset (27, 29)
     * d) shaded, unfocused      offset (27, 42)
     */

    int y_offset;

    if (shaded) {
        if (focus)
            y_offset = 29;
        else
            y_offset = 42;
    }
    else {
        if (focus)
            y_offset = 0;
        else
            y_offset = 15;
    }

    skin_draw_pixbuf (cr, SKIN_TITLEBAR, 27, y_offset, 0, 0,
     active_skin->properties.mainwin_width, MAINWIN_TITLEBAR_HEIGHT);
}
