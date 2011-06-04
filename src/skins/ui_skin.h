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

#ifndef SKIN_H
#define SKIN_H

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION (3, 0, 0)
#define MASK_IS_REGION
#endif

#define COLOR(r,g,b) (((guint32) (r) << 16) | ((guint32) (g) << 8) | (guint32) (b))
#define COLOR_R(c) ((gint) (((c) & 0xff0000) >> 16))
#define COLOR_G(c) ((gint) (((c) & 0xff00) >> 8))
#define COLOR_B(c) ((gint) ((c) & 0xff))

typedef enum {
    SKIN_MAIN = 0,
    SKIN_CBUTTONS,
    SKIN_TITLEBAR,
    SKIN_SHUFREP,
    SKIN_TEXT,
    SKIN_VOLUME,
    SKIN_BALANCE,
    SKIN_MONOSTEREO,
    SKIN_PLAYPAUSE,
    SKIN_NUMBERS,
    SKIN_POSBAR,
    SKIN_PLEDIT,
    SKIN_EQMAIN,
    SKIN_EQ_EX,
    SKIN_PIXMAP_COUNT
} SkinPixmapId;

typedef enum {
    SKIN_MASK_MAIN = 0,
    SKIN_MASK_MAIN_SHADE,
    SKIN_MASK_EQ,
    SKIN_MASK_EQ_SHADE,
    SKIN_MASK_COUNT
} SkinMaskId;

typedef enum {
    SKIN_PLEDIT_NORMAL = 0,
    SKIN_PLEDIT_CURRENT,
    SKIN_PLEDIT_NORMALBG,
    SKIN_PLEDIT_SELECTEDBG,
    SKIN_TEXTBG,
    SKIN_TEXTFG,
    SKIN_COLOR_COUNT
} SkinColorId;

typedef struct {
    /* Vis properties */
    gint mainwin_vis_x;
    gint mainwin_vis_y;
    gboolean mainwin_vis_visible;

    /* Text properties */
    gint mainwin_text_x;
    gint mainwin_text_y;
    gint mainwin_text_width;
    gboolean mainwin_text_visible;

    /* Infobar properties */
    gint mainwin_infobar_x;
    gint mainwin_infobar_y;
    gboolean mainwin_othertext_visible;

    gint mainwin_number_0_x;
    gint mainwin_number_0_y;

    gint mainwin_number_1_x;
    gint mainwin_number_1_y;

    gint mainwin_number_2_x;
    gint mainwin_number_2_y;

    gint mainwin_number_3_x;
    gint mainwin_number_3_y;

    gint mainwin_number_4_x;
    gint mainwin_number_4_y;

    gint mainwin_playstatus_x;
    gint mainwin_playstatus_y;

    gint mainwin_volume_x;
    gint mainwin_volume_y;

    gint mainwin_balance_x;
    gint mainwin_balance_y;

    gint mainwin_position_x;
    gint mainwin_position_y;

    gint mainwin_previous_x;
    gint mainwin_previous_y;

    gint mainwin_play_x;
    gint mainwin_play_y;

    gint mainwin_pause_x;
    gint mainwin_pause_y;

    gint mainwin_stop_x;
    gint mainwin_stop_y;

    gint mainwin_next_x;
    gint mainwin_next_y;

    gint mainwin_eject_x;
    gint mainwin_eject_y;

    gint mainwin_eqbutton_x;
    gint mainwin_eqbutton_y;

    gint mainwin_plbutton_x;
    gint mainwin_plbutton_y;

    gint mainwin_shuffle_x;
    gint mainwin_shuffle_y;

    gint mainwin_repeat_x;
    gint mainwin_repeat_y;

    gint mainwin_about_x;
    gint mainwin_about_y;

    gint mainwin_minimize_x;
    gint mainwin_minimize_y;

    gint mainwin_shade_x;
    gint mainwin_shade_y;

    gint mainwin_close_x;
    gint mainwin_close_y;

    gint mainwin_width;
    gint mainwin_height;

    gboolean mainwin_menurow_visible;
    gboolean mainwin_othertext_is_status;

    gint textbox_bitmap_font_width;
    gint textbox_bitmap_font_height;
} SkinProperties;

typedef struct {
    gchar *path;
    cairo_surface_t * pixmaps[SKIN_PIXMAP_COUNT];
    guint32 colors[SKIN_COLOR_COUNT];
    guint32 vis_colors[24];
#ifdef MASK_IS_REGION
    cairo_region_t * masks[SKIN_MASK_COUNT];
#else
    GdkBitmap * masks[SKIN_MASK_COUNT];
#endif
    SkinProperties properties;
} Skin;

extern Skin * active_skin;

gboolean init_skins(const gchar * path);
void cleanup_skins(void);

gboolean active_skin_load(const gchar * path);

void skin_draw_pixbuf (cairo_t * cr, SkinPixmapId id, gint xsrc, gint ysrc,
 gint xdest, gint ydest, gint width, gint height);

void skin_get_eq_spline_colors(Skin * skin, guint32 colors[19]);
void skin_install_skin(const gchar * path);

void skin_draw_playlistwin_shaded (cairo_t * cr, gint width, gboolean focus);
void skin_draw_playlistwin_frame (cairo_t * cr, gint width, gint height,
 gboolean focus);
void skin_draw_mainwin_titlebar (cairo_t * cr, gboolean shaded, gboolean focus);

static inline void set_cairo_color (cairo_t * cr, guint32 c)
{
    cairo_set_source_rgb (cr, COLOR_R(c) / 255.0, COLOR_G(c) / 255.0, COLOR_B(c)
     / 255.0);
}

#endif
