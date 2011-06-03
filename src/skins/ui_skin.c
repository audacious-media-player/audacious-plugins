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

#include <stdlib.h>
#include <string.h>

#include <audacious/debug.h>
#include <audacious/misc.h>

#include "plugin.h"
#include "skins_cfg.h"
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

static gchar *ext_targets[EXTENSION_TARGETS] =
{ "bmp", "xpm", "png", "svg", "gif", "jpg", "jpeg" };

struct _SkinPixmapIdMapping {
    SkinPixmapId id;
    const gchar *name;
    const gchar *alt_name;
    gint width, height;
};

struct _SkinMaskInfo {
    gint width, height;
    gchar *inistr;
};
/* I know, it's not nice to copy'n'paste stuff, but I wanted it to
   be atleast parially working, before dealing with such things */

typedef struct {
    const gchar *to_match;
    gchar *match;
    gboolean found;
} FindFileContext;

typedef struct _SkinPixmapIdMapping SkinPixmapIdMapping;
typedef struct _SkinMaskInfo SkinMaskInfo;

static gboolean skin_load (Skin * skin, const gchar * path);
static void skin_parse_hints (Skin * skin, const gchar * path_p);

Skin *aud_active_skin = NULL;

static gint skin_current_num;

static SkinMaskInfo skin_mask_info[] = {
    {275, 116, "Normal"},
    {275, 16,  "WindowShade"},
    {275, 116, "Equalizer"},
    {275, 16,  "EqualizerWS"}
};

static SkinPixmapIdMapping skin_pixmap_id_map[] = {
    {SKIN_MAIN, "main", NULL, 0, 0},
    {SKIN_CBUTTONS, "cbuttons", NULL, 0, 0},
    {SKIN_SHUFREP, "shufrep", NULL, 0, 0},
    {SKIN_TEXT, "text", NULL, 0, 0},
    {SKIN_TITLEBAR, "titlebar", NULL, 0, 0},
    {SKIN_VOLUME, "volume", NULL, 0, 0},
    {SKIN_BALANCE, "balance", "volume", 0, 0},
    {SKIN_MONOSTEREO, "monoster", NULL, 0, 0},
    {SKIN_PLAYPAUSE, "playpaus", NULL, 0, 0},
    {SKIN_NUMBERS, "nums_ex", "numbers", 0, 0},
    {SKIN_POSBAR, "posbar", NULL, 0, 0},
    {SKIN_EQMAIN, "eqmain", NULL, 0, 0},
    {SKIN_PLEDIT, "pledit", NULL, 0, 0},
    {SKIN_EQ_EX, "eq_ex", NULL, 0, 0}
};

static guint skin_pixmap_id_map_size = G_N_ELEMENTS(skin_pixmap_id_map);

static const guchar skin_default_viscolor[24][3] = {
    {9, 34, 53},
    {10, 18, 26},
    {0, 54, 108},
    {0, 58, 116},
    {0, 62, 124},
    {0, 66, 132},
    {0, 70, 140},
    {0, 74, 148},
    {0, 78, 156},
    {0, 82, 164},
    {0, 86, 172},
    {0, 92, 184},
    {0, 98, 196},
    {0, 104, 208},
    {0, 110, 220},
    {0, 116, 232},
    {0, 122, 244},
    {0, 128, 255},
    {0, 128, 255},
    {0, 104, 208},
    {0, 80, 160},
    {0, 56, 112},
    {0, 32, 64},
    {200, 200, 200}
};

static gchar *original_gtk_theme = NULL;

#ifdef MASK_IS_REGION
cairo_region_t * skin_create_transparent_mask (const gchar * path, const gchar *
 file, const gchar * section, GdkWindow * window, gint width, gint height);
#else
GdkBitmap * skin_create_transparent_mask (const gchar * path, const gchar *
 file, const gchar * section, GdkWindow * window, gint width, gint height);
#endif

static void skin_set_default_vis_color(Skin * skin);

gboolean active_skin_load (const gchar * path)
{
    AUDDBG("%s\n", path);
    g_return_val_if_fail(aud_active_skin != NULL, FALSE);

    if (!skin_load(aud_active_skin, path)) {
        AUDDBG("loading failed\n");
        return FALSE;
    }

    mainwin_refresh_hints ();
    textbox_update_all ();
    ui_vis_set_colors ();
    ui_svis_set_colors ();
    gtk_widget_queue_draw (mainwin);
    gtk_widget_queue_draw (equalizerwin);
    gtk_widget_queue_draw (playlistwin);

    GdkPixbuf * p = aud_active_skin->pixmaps[SKIN_POSBAR];
    /* last 59 pixels of SKIN_POSBAR are knobs (normal and selected) */
    gtk_widget_set_size_request (mainwin_position, gdk_pixbuf_get_width (p) -
     59, gdk_pixbuf_get_height (p));

    g_free (config.skin);
    config.skin = g_strdup (path);

    return TRUE;
}

Skin *
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
void
skin_free(Skin * skin)
{
    gint i;

    g_return_if_fail(skin != NULL);

    for (i = 0; i < SKIN_PIXMAP_COUNT; i++)
    {
        if (skin->pixmaps[i])
        {
            g_object_unref (skin->pixmaps[i]);
            skin->pixmaps[i] = NULL;
        }
    }

    for (i = 0; i < SKIN_MASK_COUNT; i++) {
        if (skin->masks[i])
#ifdef MASK_IS_REGION
            cairo_region_destroy (skin->masks[i]);
#else
            g_object_unref (skin->masks[i]);
#endif

        skin->masks[i] = NULL;
    }

    for (i = 0; i < SKIN_COLOR_COUNT; i++) {
        if (skin->colors[i])
            g_free(skin->colors[i]);

        skin->colors[i] = NULL;
    }

    g_free(skin->path);
    skin->path = NULL;

    skin_set_default_vis_color(skin);

    if (original_gtk_theme != NULL)
    {
        gtk_settings_set_string_property (gtk_settings_get_default (),
         "gtk-theme-name", original_gtk_theme, "audacious");
        g_free (original_gtk_theme);
        original_gtk_theme = NULL;
    }
}

void
skin_destroy(Skin * skin)
{
    g_return_if_fail(skin != NULL);
    skin_free(skin);
    g_free(skin);
}

const SkinPixmapIdMapping *
skin_pixmap_id_lookup(guint id)
{
    guint i;

    for (i = 0; i < skin_pixmap_id_map_size; i++) {
        if (id == skin_pixmap_id_map[i].id) {
            return &skin_pixmap_id_map[i];
        }
    }

    return NULL;
}

const gchar *
skin_pixmap_id_to_name(SkinPixmapId id)
{
    guint i;

    for (i = 0; i < skin_pixmap_id_map_size; i++) {
        if (id == skin_pixmap_id_map[i].id)
            return skin_pixmap_id_map[i].name;
    }
    return NULL;
}

static void
skin_set_default_vis_color(Skin * skin)
{
    memcpy(skin->vis_color, skin_default_viscolor,
           sizeof(skin_default_viscolor));
}

gchar * skin_pixmap_locate (const gchar * dirname, gchar * * basenames)
{
    gchar * filename = NULL;
    gint i;

    for (i = 0; basenames[i] != NULL; i ++)
    {
        if ((filename = find_file_case_path (dirname, basenames[i])) != NULL)
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
static gchar **
skin_pixmap_create_basenames(const SkinPixmapIdMapping * pixmap_id_mapping)
{
    gchar **basenames = g_malloc0(sizeof(gchar*) * (EXTENSION_TARGETS * 2 + 1));
    gint i, y;

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
skin_pixmap_free_basenames(gchar ** basenames)
{
    int i;
    for (i = 0; basenames[i] != NULL; i++)
    {
        g_free(basenames[i]);
        basenames[i] = NULL;
    }
    g_free(basenames);
}

/**
 * Locates a pixmap file for skin.
 */
static gchar *
skin_pixmap_locate_basenames(const Skin * skin,
                             const SkinPixmapIdMapping * pixmap_id_mapping,
                             const gchar * path_p)
{
    gchar *filename = NULL;
    const gchar *path = path_p ? path_p : skin->path;
    gchar **basenames = skin_pixmap_create_basenames(pixmap_id_mapping);

    filename = skin_pixmap_locate(path, basenames);

    skin_pixmap_free_basenames(basenames);

    return filename;
}


static gboolean
skin_load_pixmap_id(Skin * skin, SkinPixmapId id, const gchar * path_p)
{
    const SkinPixmapIdMapping *pixmap_id_mapping;
    gchar *filename;

    g_return_val_if_fail(skin != NULL, FALSE);
    g_return_val_if_fail(id < SKIN_PIXMAP_COUNT, FALSE);
    g_return_val_if_fail(! skin->pixmaps[id], FALSE);

    pixmap_id_mapping = skin_pixmap_id_lookup(id);
    g_return_val_if_fail(pixmap_id_mapping != NULL, FALSE);

    filename = skin_pixmap_locate_basenames(skin, pixmap_id_mapping, path_p);

    if (filename == NULL)
        return FALSE;

    skin->pixmaps[id] = gdk_pixbuf_new_from_file (filename, NULL);
    g_free (filename);
    return skin->pixmaps[id] ? TRUE : FALSE;
}

static void skin_mask_create (Skin * skin, const gchar * path, gint id,
 GdkWindow * window)
{
    skin->masks[id] = skin_create_transparent_mask (path, "region.txt",
     skin_mask_info[id].inistr, window, skin_mask_info[id].width,
     skin_mask_info[id].height);
}

#ifdef MASK_IS_REGION
static cairo_region_t * create_default_mask (GdkWindow * parent, gint w, gint h)
{
    cairo_rectangle_int_t rect = {0, 0, w, h};
    return cairo_region_create_rectangle (& rect);
}
#else
static GdkBitmap * create_default_mask (GdkWindow * parent, gint w, gint h)
{
    GdkBitmap *ret;
    GdkGC *gc;
    GdkColor pattern;

    ret = gdk_pixmap_new(parent, w, h, 1);
    gc = gdk_gc_new(ret);
    pattern.pixel = 1;
    gdk_gc_set_foreground(gc, &pattern);
    gdk_draw_rectangle(ret, gc, TRUE, 0, 0, w, h);
    g_object_unref(gc);

    return ret;
}
#endif

static void pixbuf_get_pixel_color (GdkPixbuf * p, gint x, gint y, GdkColor * c)
{
    g_return_if_fail (p);
    g_return_if_fail (x >= 0 && x < gdk_pixbuf_get_width (p));
    g_return_if_fail (y >= 0 && y < gdk_pixbuf_get_height (p));

    guchar * b = gdk_pixbuf_get_pixels (p) + gdk_pixbuf_get_rowstride (p) * y +
     gdk_pixbuf_get_n_channels (p) * x;

    c->red = ((gint) b[0]) * 65535 / 255;
    c->green = ((gint) b[1]) * 65535 / 255;
    c->blue = ((gint) b[2]) * 65535 / 255;
}

static glong
skin_calc_luminance(GdkColor * c)
{
    return (0.212671 * c->red + 0.715160 * c->green + 0.072169 * c->blue);
}

static void skin_get_textcolors (GdkPixbuf * p, GdkColor * bgc, GdkColor * fgc)
{
    /*
     * Try to extract reasonable background and foreground colors
     * from the font pixmap
     */
    g_return_if_fail (p);

    /* Get a pixel from the middle of the space character */
    pixbuf_get_pixel_color (p, 152, 3, bgc);

    gint max_d = 0;
    for (gint i = 0; i < 6; i ++)
    {
        for (gint x = 1; x < 150; x ++)
        {
            GdkColor c;
            pixbuf_get_pixel_color (p, x, i, & c);

            gint d = labs(skin_calc_luminance(&c) - skin_calc_luminance(bgc));
            if (d > max_d) {
                memcpy (fgc, & c, sizeof (GdkColor));
                max_d = d;
            }
        }
    }
}

gboolean
init_skins(const gchar * path)
{
    aud_active_skin = skin_new();

    skin_parse_hints(aud_active_skin, NULL);

    /* create the windows if they haven't been created yet, needed for bootstrapping */
    if (mainwin == NULL)
    {
        mainwin_create();
        equalizerwin_create();
        playlistwin_create();
    }

    if (! active_skin_load (path))
    {
        if (path != NULL)
            AUDDBG("Unable to load skin (%s), trying default...\n", path);
        else
            AUDDBG("Skin not defined: trying default...\n");

        /* can't load configured skin, retry with default */
        gchar * def = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "Skins"
         G_DIR_SEPARATOR_S "Default", aud_get_path (AUD_PATH_DATA_DIR));

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
    skin_destroy(aud_active_skin);
    aud_active_skin = NULL;

    gtk_widget_destroy (mainwin);
    mainwin = NULL;
    gtk_widget_destroy (playlistwin);
    playlistwin = NULL;
    gtk_widget_destroy (equalizerwin);
    equalizerwin = NULL;
}


/*
 * Opens and parses a skin's hints file.
 * Hints files are somewhat like "scripts" in Winamp3/5.
 * We'll probably add scripts to it next.
 */
static void skin_parse_hints (Skin * skin, const gchar * path_p)
{
    gchar *filename, *tmp;
    INIFile *inifile;

    path_p = path_p ? path_p : skin->path;

    skin->properties.mainwin_vis_x = 24;
    skin->properties.mainwin_vis_y = 43;
    skin->properties.mainwin_text_x = 112;
    skin->properties.mainwin_text_y = 27;
    skin->properties.mainwin_text_width = 153;
    skin->properties.mainwin_infobar_x = 112;
    skin->properties.mainwin_infobar_y = 43;
    skin->properties.mainwin_number_0_x = 36;
    skin->properties.mainwin_number_0_y = 26;
    skin->properties.mainwin_number_1_x = 48;
    skin->properties.mainwin_number_1_y = 26;
    skin->properties.mainwin_number_2_x = 60;
    skin->properties.mainwin_number_2_y = 26;
    skin->properties.mainwin_number_3_x = 78;
    skin->properties.mainwin_number_3_y = 26;
    skin->properties.mainwin_number_4_x = 90;
    skin->properties.mainwin_number_4_y = 26;
    skin->properties.mainwin_playstatus_x = 24;
    skin->properties.mainwin_playstatus_y = 28;
    skin->properties.mainwin_menurow_visible = TRUE;
    skin->properties.mainwin_volume_x = 107;
    skin->properties.mainwin_volume_y = 57;
    skin->properties.mainwin_balance_x = 177;
    skin->properties.mainwin_balance_y = 57;
    skin->properties.mainwin_position_x = 16;
    skin->properties.mainwin_position_y = 72;
    skin->properties.mainwin_othertext_is_status = FALSE;
    skin->properties.mainwin_othertext_visible = FALSE;
    skin->properties.mainwin_text_visible = TRUE;
    skin->properties.mainwin_vis_visible = TRUE;
    skin->properties.mainwin_previous_x = 16;
    skin->properties.mainwin_previous_y = 88;
    skin->properties.mainwin_play_x = 39;
    skin->properties.mainwin_play_y = 88;
    skin->properties.mainwin_pause_x = 62;
    skin->properties.mainwin_pause_y = 88;
    skin->properties.mainwin_stop_x = 85;
    skin->properties.mainwin_stop_y = 88;
    skin->properties.mainwin_next_x = 108;
    skin->properties.mainwin_next_y = 88;
    skin->properties.mainwin_eject_x = 136;
    skin->properties.mainwin_eject_y = 89;
    skin->properties.mainwin_width = 275;
    skin_mask_info[0].width = skin->properties.mainwin_width;
    skin->properties.mainwin_height = 116;
    skin_mask_info[0].height = skin->properties.mainwin_height;
    skin->properties.mainwin_about_x = 247;
    skin->properties.mainwin_about_y = 83;
    skin->properties.mainwin_shuffle_x = 164;
    skin->properties.mainwin_shuffle_y = 89;
    skin->properties.mainwin_repeat_x = 210;
    skin->properties.mainwin_repeat_y = 89;
    skin->properties.mainwin_eqbutton_x = 219;
    skin->properties.mainwin_eqbutton_y = 58;
    skin->properties.mainwin_plbutton_x = 242;
    skin->properties.mainwin_plbutton_y = 58;
    skin->properties.textbox_bitmap_font_width = 5;
    skin->properties.textbox_bitmap_font_height = 6;
    skin->properties.mainwin_minimize_x = 244;
    skin->properties.mainwin_minimize_y = 3;
    skin->properties.mainwin_shade_x = 254;
    skin->properties.mainwin_shade_y = 3;
    skin->properties.mainwin_close_x = 264;
    skin->properties.mainwin_close_y = 3;

    if (path_p == NULL)
        return;

    filename = find_file_case_uri (path_p, "skin.hints");

    if (filename == NULL)
        return;

    inifile = open_ini_file(filename);
    if (!inifile)
        return;

    tmp = read_ini_string(inifile, "skin", "mainwinVisX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_vis_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinVisY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_vis_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinTextX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_text_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinTextY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_text_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinTextWidth");

    if (tmp != NULL)
    {
        skin->properties.mainwin_text_width = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinInfoBarX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_infobar_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinInfoBarY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_infobar_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNumber0X");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_0_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNumber0Y");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_0_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNumber1X");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_1_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNumber1Y");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_1_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNumber2X");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_2_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNumber2Y");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_2_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNumber3X");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_3_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNumber3Y");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_3_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNumber4X");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_4_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNumber4Y");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_4_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPlayStatusX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_playstatus_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPlayStatusY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_playstatus_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinMenurowVisible");

    if (tmp != NULL)
    {
        skin->properties.mainwin_menurow_visible = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinVolumeX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_volume_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinVolumeY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_volume_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinBalanceX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_balance_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinBalanceY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_balance_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPositionX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_position_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPositionY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_position_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinOthertextIsStatus");

    if (tmp != NULL)
    {
        skin->properties.mainwin_othertext_is_status = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinOthertextVisible");

    if (tmp != NULL)
    {
        skin->properties.mainwin_othertext_visible = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinTextVisible");

    if (tmp != NULL)
    {
        skin->properties.mainwin_text_visible = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinVisVisible");

    if (tmp != NULL)
    {
        skin->properties.mainwin_vis_visible = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPreviousX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_previous_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPreviousY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_previous_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPlayX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_play_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPlayY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_play_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPauseX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_pause_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPauseY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_pause_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinStopX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_stop_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinStopY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_stop_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNextX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_next_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinNextY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_next_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinEjectX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_eject_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinEjectY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_eject_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinWidth");

    if (tmp != NULL)
    {
        skin->properties.mainwin_width = atoi(tmp);
        g_free(tmp);
    }

    skin_mask_info[0].width = skin->properties.mainwin_width;

    tmp = read_ini_string(inifile, "skin", "mainwinHeight");

    if (tmp != NULL)
    {
        skin->properties.mainwin_height = atoi(tmp);
        g_free(tmp);
    }

    skin_mask_info[0].height = skin->properties.mainwin_height;

    tmp = read_ini_string(inifile, "skin", "mainwinAboutX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_about_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinAboutY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_about_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinShuffleX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_shuffle_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinShuffleY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_shuffle_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinRepeatX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_repeat_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinRepeatY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_repeat_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinEQButtonX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_eqbutton_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinEQButtonY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_eqbutton_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPLButtonX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_plbutton_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinPLButtonY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_plbutton_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "textboxBitmapFontWidth");

    if (tmp != NULL)
    {
        skin->properties.textbox_bitmap_font_width = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "textboxBitmapFontHeight");

    if (tmp != NULL)
    {
        skin->properties.textbox_bitmap_font_height = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinMinimizeX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_minimize_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinMinimizeY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_minimize_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinShadeX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_shade_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinShadeY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_shade_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinCloseX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_close_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = read_ini_string(inifile, "skin", "mainwinCloseY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_close_y = atoi(tmp);
        g_free(tmp);
    }

    if (filename != NULL)
        g_free(filename);

    close_ini_file(inifile);
}

static guint
hex_chars_to_int(gchar hi, gchar lo)
{
    /*
     * Converts a value in the range 0x00-0xFF
     * to a integer in the range 0-65535
     */
    gchar str[3];

    str[0] = hi;
    str[1] = lo;
    str[2] = 0;

    return (CLAMP(strtol(str, NULL, 16), 0, 0xFF) << 8);
}

static GdkColor *
skin_load_color(INIFile *inifile,
                const gchar * section, const gchar * key,
                gchar * default_hex)
{
    gchar *value;
    GdkColor *color = NULL;

    if (inifile || default_hex) {
        if (inifile) {
            value = read_ini_string(inifile, section, key);
            if (value == NULL) {
                value = g_strdup(default_hex);
            }
        } else {
            value = g_strdup(default_hex);
        }
        if (value) {
            gchar *ptr = value;
            gint len;

            color = g_new0(GdkColor, 1);
            g_strstrip(value);

            if (value[0] == '#')
                ptr++;
            len = strlen(ptr);
            /*
             * The handling of incomplete values is done this way
             * to maximize winamp compatibility
             */
            if (len >= 6) {
                color->red = hex_chars_to_int(*ptr, *(ptr + 1));
                ptr += 2;
            }
            if (len >= 4) {
                color->green = hex_chars_to_int(*ptr, *(ptr + 1));
                ptr += 2;
            }
            if (len >= 2)
                color->blue = hex_chars_to_int(*ptr, *(ptr + 1));

            g_free(value);
        }
    }
    return color;
}

#ifdef MASK_IS_REGION
cairo_region_t * skin_create_transparent_mask (const gchar * path, const gchar *
 file, const gchar * section, GdkWindow * window, gint width, gint height)
#else
GdkBitmap * skin_create_transparent_mask (const gchar * path, const gchar *
 file, const gchar * section, GdkWindow * window, gint width, gint height)
#endif
{
    gchar *filename = NULL;
    INIFile *inifile = NULL;
    gboolean created_mask = FALSE;
    GArray *num, *point;
    guint i, j;
    gint k;

    if (path != NULL)
        filename = find_file_case_uri (path, file);

    /* filename will be null if path wasn't set */
    if (!filename)
        return create_default_mask(window, width, height);

    inifile = open_ini_file(filename);

    if ((num = read_ini_array(inifile, section, "NumPoints")) == NULL) {
        g_free(filename);
        close_ini_file(inifile);
        return NULL;
    }

    if ((point = read_ini_array(inifile, section, "PointList")) == NULL) {
        g_array_free(num, TRUE);
        g_free(filename);
        close_ini_file(inifile);
        return NULL;
    }

    close_ini_file(inifile);

#ifdef MASK_IS_REGION
    cairo_region_t * mask = cairo_region_create ();
#else
    GdkBitmap * mask = gdk_pixmap_new (window, width, height, 1);
    GdkGC * gc = gdk_gc_new (mask);

    GdkColor pattern;
    pattern.pixel = 0;
    gdk_gc_set_foreground(gc, &pattern);
    gdk_draw_rectangle(mask, gc, TRUE, 0, 0, width, height);
    pattern.pixel = 1;
    gdk_gc_set_foreground(gc, &pattern);
#endif

    j = 0;
    for (i = 0; i < num->len; i ++)
    {
        gint n_points = g_array_index (num, gint, i);
        if (n_points <= 0 || j + 2 * n_points > point->len)
            break;

        GdkPoint gpoints[n_points];
        for (k = 0; k < n_points; k ++)
        {
            gpoints[k].x = g_array_index (point, gint, j + k * 2);
            gpoints[k].y = g_array_index (point, gint, j + k * 2 + 1);
        }

#ifdef MASK_IS_REGION
        gint xmin = width, ymin = height, xmax = 0, ymax = 0;
        for (k = 0; k < n_points; k ++)
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
#else
        gdk_draw_polygon (mask, gc, TRUE, gpoints, n_points);
#endif

        created_mask = TRUE;
        j += n_points * 2;
    }

    g_array_free(num, TRUE);
    g_array_free(point, TRUE);
    g_free(filename);

    if (!created_mask)
    {
#ifdef MASK_IS_REGION
        cairo_rectangle_int_t rect = {0, 0, width, height};
        cairo_region_union_rectangle (mask, & rect);
#else
        gdk_draw_rectangle(mask, gc, TRUE, 0, 0, width, height);
#endif
    }

#ifndef MASK_IS_REGION
    g_object_unref(gc);
#endif

    return mask;
}

static void skin_load_viscolor (Skin * skin, const gchar * path, const gchar *
 basename)
{
    gchar * filename, * buffer, * string, * next;
    gint line;

    skin_set_default_vis_color (skin);

    filename = find_file_case_uri (path, basename);
    if (filename == NULL)
        return;

    buffer = load_text_file (filename);
    g_free (filename);
    string = buffer;

    for (line = 0; string != NULL && line < 24; line ++)
    {
        GArray * array;
        gint column;

        next = text_parse_line (string);
        array = string_to_garray (string);

        if (array->len >= 3)
        {
            for (column = 0; column < 3; column ++)
                skin->vis_color[line][column] = g_array_index (array, gint,
                 column);
        }

        g_array_free (array, TRUE);
        string = next;
    }

    g_free (buffer);
}

static void
skin_numbers_generate_dash(Skin * skin)
{
    g_return_if_fail(skin != NULL);

    GdkPixbuf * old = skin->pixmaps[SKIN_NUMBERS];
    if (! old || gdk_pixbuf_get_width (old) < 99)
        return;

    gint h = gdk_pixbuf_get_height (old);
    GdkPixbuf * new = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 108, h);

    gdk_pixbuf_copy_area (old, 0, 0, 99, h, new, 0, 0);
    gdk_pixbuf_copy_area (old, 90, 0, 9, h, new, 99, 0);
    gdk_pixbuf_copy_area (old, 20, 6, 5, 1, new, 101, 6);

    g_object_unref (old);
    skin->pixmaps[SKIN_NUMBERS] = new;
}

static gboolean
skin_load_pixmaps(Skin * skin, const gchar * path)
{
    GdkPixbuf *text_pb;
    guint i;
    gchar *filename;
    INIFile *inifile;

    if(!skin) return FALSE;
    if(!path) return FALSE;

    AUDDBG("Loading pixmaps in %s\n", path);

    for (i = 0; i < SKIN_PIXMAP_COUNT; i++)
        if (! skin_load_pixmap_id (skin, i, path))
            return FALSE;

    text_pb = skin->pixmaps[SKIN_TEXT];

    if (text_pb)
        skin_get_textcolors (text_pb, & skin->textbg, & skin->textfg);

    if (skin->pixmaps[SKIN_NUMBERS] && gdk_pixbuf_get_width
     (skin->pixmaps[SKIN_NUMBERS]) < 108)
        skin_numbers_generate_dash (skin);

    filename = find_file_case_uri (path, "pledit.txt");
    inifile = (filename != NULL) ? open_ini_file (filename) : NULL;

    skin->colors[SKIN_PLEDIT_NORMAL] =
        skin_load_color(inifile, "Text", "Normal", "#2499ff");
    skin->colors[SKIN_PLEDIT_CURRENT] =
        skin_load_color(inifile, "Text", "Current", "#ffeeff");
    skin->colors[SKIN_PLEDIT_NORMALBG] =
        skin_load_color(inifile, "Text", "NormalBG", "#0a120a");
    skin->colors[SKIN_PLEDIT_SELECTEDBG] =
        skin_load_color(inifile, "Text", "SelectedBG", "#0a124a");

    if (inifile)
        close_ini_file(inifile);

    if (filename)
        g_free(filename);

    skin_mask_create (skin, path, SKIN_MASK_MAIN, gtk_widget_get_window (mainwin));
    skin_mask_create (skin, path, SKIN_MASK_MAIN_SHADE, gtk_widget_get_window (mainwin));
    skin_mask_create (skin, path, SKIN_MASK_EQ, gtk_widget_get_window (equalizerwin));
    skin_mask_create (skin, path, SKIN_MASK_EQ_SHADE, gtk_widget_get_window (equalizerwin));

    skin_load_viscolor(skin, path, "viscolor.txt");

    return TRUE;
}

/**
 * Checks if all pixmap files exist that skin needs.
 */
static gboolean
skin_check_pixmaps(const Skin * skin, const gchar * skin_path)
{
    guint i;
    for (i = 0; i < SKIN_PIXMAP_COUNT; i++)
    {
        gchar *filename = skin_pixmap_locate_basenames(skin,
                                                       skin_pixmap_id_lookup(i),
                                                       skin_path);
        if (!filename)
            return FALSE;
        g_free(filename);
    }
    return TRUE;
}

static gboolean
skin_load_nolock(Skin * skin, const gchar * path, gboolean force)
{
    gchar *newpath, *skin_path;
    int archive = 0;

    AUDDBG("Attempt to load skin \"%s\"\n", path);

    g_return_val_if_fail(skin != NULL, FALSE);
    g_return_val_if_fail(path != NULL, FALSE);

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
        g_free(skin_path);
        AUDDBG("Skin path (%s) doesn't have all wanted pixmaps\n", skin_path);
        return FALSE;
    }

    // skin_free() frees skin->path and variable path can actually be skin->path
    // and we want to get the path before possibly freeing it.
    newpath = g_strdup(path);
    skin_free(skin);
    skin->path = newpath;

    memset(&(skin->properties), 0, sizeof(SkinProperties)); /* do it only if all tests above passed! --asphyx */

    skin_current_num++;

    /* Parse the hints for this skin. */
    skin_parse_hints(skin, skin_path);

    if (!skin_load_pixmaps(skin, skin_path)) {
        if(archive) del_directory(skin_path);
        g_free(skin_path);
        AUDDBG("Skin loading failed\n");
        return FALSE;
    }

    if(archive) del_directory(skin_path);
    g_free(skin_path);

    mainwin_set_shape ();
    equalizerwin_set_shape ();

    return TRUE;
}

void
skin_install_skin(const gchar * path)
{
    gchar *command;

    g_return_if_fail(path != NULL);

    command = g_strdup_printf("cp %s %s",
                              path, skins_paths[SKINS_PATH_USER_SKIN_DIR]);
    if (system(command)) {
        AUDDBG("Unable to install skin (%s) into user directory (%s)\n",
                  path, skins_paths[SKINS_PATH_USER_SKIN_DIR]);
    }
    g_free(command);
}

static gboolean skin_load (Skin * skin, const gchar * path)
{
    gboolean ret;

    g_return_val_if_fail(skin != NULL, FALSE);

    if (!path)
        return FALSE;

    ret = skin_load_nolock(skin, path, FALSE);

    if(!ret) {
        AUDDBG("loading failed\n");
        return FALSE; /* don't try to update anything if loading failed --asphyx */
    }

    if (skin->pixmaps[SKIN_NUMBERS])
    {
        gint h = gdk_pixbuf_get_height (skin->pixmaps[SKIN_NUMBERS]);
        ui_skinned_number_set_size (mainwin_minus_num, 9, h);
        ui_skinned_number_set_size (mainwin_10min_num, 9, h);
        ui_skinned_number_set_size (mainwin_min_num, 9, h);
        ui_skinned_number_set_size (mainwin_10sec_num, 9, h);
        ui_skinned_number_set_size (mainwin_sec_num, 9, h);
    }

    if (skin->pixmaps[SKIN_PLAYPAUSE])
        ui_skinned_playstatus_set_size (mainwin_playstatus, 11,
         gdk_pixbuf_get_height (skin->pixmaps[SKIN_PLAYPAUSE]));

    return TRUE;
}

gboolean
skin_reload_forced(void)
{
   gboolean error;
   AUDDBG("\n");

   error = skin_load_nolock(aud_active_skin, aud_active_skin->path, TRUE);

   return error;
}

void
skin_reload(Skin * skin)
{
    AUDDBG("\n");
    g_return_if_fail(skin != NULL);
    skin_load_nolock(skin, skin->path, TRUE);
}

#ifdef MASK_IS_REGION
cairo_region_t * skin_get_mask (Skin * skin, SkinMaskId mi)
#else
GdkBitmap * skin_get_mask (Skin * skin, SkinMaskId mi)
#endif
{
    g_return_val_if_fail(skin != NULL, NULL);
    g_return_val_if_fail(mi < SKIN_MASK_COUNT, NULL);

    return skin->masks[mi];
}

GdkColor *
skin_get_color(Skin * skin, SkinColorId color_id)
{
    GdkColor *ret = NULL;

    g_return_val_if_fail(skin != NULL, NULL);

    switch (color_id) {
    case SKIN_TEXTBG:
        ret = & skin->textbg;
        break;
    case SKIN_TEXTFG:
        ret = & skin->textfg;
        break;
    default:
        if (color_id < SKIN_COLOR_COUNT)
            ret = skin->colors[color_id];
        break;
    }
    return ret;
}

gint
skin_get_id(void)
{
    return skin_current_num;
}

void skin_draw_pixbuf (cairo_t * cr, SkinPixmapId id, gint xsrc, gint ysrc, gint
 xdest, gint ydest, gint width, gint height)
{
    GdkPixbuf * p = aud_active_skin->pixmaps[id];
    g_return_if_fail (p);

    cairo_save (cr);
    cairo_rectangle (cr, xdest, ydest, width, height);
    cairo_clip (cr);
    gdk_cairo_set_source_pixbuf (cr, p, xdest - xsrc, ydest - ysrc);
    cairo_paint (cr);
    cairo_restore (cr);
}

void
skin_get_eq_spline_colors(Skin * skin, guint32 colors[19])
{
    gint i;
    guchar* pixels,*p;
    guint rowstride, n_channels;
    g_return_if_fail(skin != NULL);

    memset (colors, 0, sizeof colors);

    GdkPixbuf * pixbuf = skin->pixmaps[SKIN_EQMAIN];
    if (! pixbuf || gdk_pixbuf_get_width (pixbuf) < 116 || gdk_pixbuf_get_height
     (pixbuf) < 313)
        return;

    pixels = gdk_pixbuf_get_pixels (pixbuf);
    rowstride = gdk_pixbuf_get_rowstride (pixbuf);
    n_channels = gdk_pixbuf_get_n_channels (pixbuf);
    for (i = 0; i < 19; i++)
    {
        p = pixels + rowstride * (i + 294) + 115 * n_channels;
        colors[i] = (p[0] << 16) | (p[1] << 8) | p[2];
    }
}

static void skin_draw_playlistwin_frame_top (cairo_t * cr, gint width, gint
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

    gint i, y, c;

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

static void skin_draw_playlistwin_frame_bottom (cairo_t * cr, gint width, gint
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

    gint i, c;

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

static void skin_draw_playlistwin_frame_sides (cairo_t * cr, gint width, gint
 height, gboolean focus)
{
    /* The side frames consist of 2 tile images. 1 for the left, 1 for
     * the right.
     * a. left  (12,29)
     * b. right (19,29)
     */

    gint i;

    /* frame sides */
    for (i = 0; i < (height - (20 + 38)) / 29; i++) {
        /* left */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 0, 42, 0, 20 + i * 29, 12, 29);

        /* right */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 32, 42, width - 19, 20 + i * 29, 19,
         29);
    }
}

void skin_draw_playlistwin_frame (cairo_t * cr, gint width, gint height,
 gboolean focus)
{
    skin_draw_playlistwin_frame_top (cr, width, height, focus);
    skin_draw_playlistwin_frame_bottom (cr, width, height, focus);
    skin_draw_playlistwin_frame_sides (cr, width, height, focus);
}

void skin_draw_playlistwin_shaded (cairo_t * cr, gint width, gboolean focus)
{
    /* The shade mode titlebar skin consists of 4 images:
     * a) left corner               offset (72,42) size (25,14)
     * b) right corner, focused     offset (99,57) size (50,14)
     * c) right corner, unfocused   offset (99,42) size (50,14)
     * d) bar tile                  offset (72,57) size (25,14)
     */

    gint i;

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

    gint y_offset;

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
     aud_active_skin->properties.mainwin_width, MAINWIN_TITLEBAR_HEIGHT);
}
