/*  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>

#include "blur_scope.h"

static GtkWidget *area = NULL;
static gboolean config_read = FALSE;

static void bscope_init(void);
static void bscope_cleanup(void);
static void bscope_playback_stop(void);
static void bscope_render_pcm(gint16 data[2][512]);
/* static GtkWidget * bscope_get_widget (void); */
static void * bscope_get_widget (void);

BlurScopeConfig bscope_cfg;

enum { SCOPE_TOGGLE, SCOPE_CLOSE };

VisPlugin bscope_vp = {
    .description = "Blur Scope",                       /* description */
    .num_pcm_chs_wanted = 1, /* Number of PCM channels wanted */
    .num_freq_chs_wanted = 0, /* Number of freq channels wanted */
    .init = bscope_init,                /* init */
    .cleanup = bscope_cleanup,             /* cleanup */
    .configure = bscope_configure,           /* configure */
    .playback_stop = bscope_playback_stop,       /* playback_stop */
    .render_pcm = bscope_render_pcm,          /* render_pcm */
    .get_widget = bscope_get_widget,
};

VisPlugin *bscope_vplist[] = { &bscope_vp, NULL };

DECLARE_PLUGIN(bscope, NULL, NULL, NULL, NULL, NULL, NULL, bscope_vplist,NULL);

#define D_WIDTH 256
#define D_HEIGHT 128
#define min(x,y) ((x)<(y)?(x):(y))
gint width = D_WIDTH;
gint height = D_HEIGHT;
gint bpl = (D_WIDTH + 2);

static GStaticMutex rgb_buf_mutex = G_STATIC_MUTEX_INIT;
static guchar *rgb_buf = NULL;
static GdkRgbCmap *cmap = NULL;

inline static void
draw_pixel_8(guchar * buffer, gint x, gint y, guchar c)
{
    if (buffer == NULL)
        return;

    buffer[((y + 1) * bpl) + (x + 1)] = c;
}

inline static void
bscope_resize_video(gint w, gint h)
{
    g_static_mutex_lock(&rgb_buf_mutex);

    width = w;
    height = h;
    bpl = (width + 2);

    if (rgb_buf != NULL) {
        g_free(rgb_buf);
        rgb_buf = NULL;
    }

    rgb_buf = g_malloc0((w + 2) * (h + 2));

    g_static_mutex_unlock(&rgb_buf_mutex);
}

gboolean
bscope_reconfigure(GtkWidget *widget, GdkEventConfigure *event, gpointer unused)
{
    bscope_resize_video(event->width, event->height);

    return FALSE;
}

void
bscope_read_config(void)
{
    mcs_handle_t *db;

    if (!config_read) {
        bscope_cfg.color = 0xFF3F7F;
        db = aud_cfg_db_open();

        if (db) {
            aud_cfg_db_get_int(db, "BlurScope", "color",
                               (int *) &bscope_cfg.color);
            aud_cfg_db_close(db);
        }
        config_read = TRUE;
    }
}


void
bscope_blur_8(guchar * ptr, gint w, gint h, gint bpl_)
{
    register guint i, sum;
    register guchar *iptr;

    iptr = ptr + bpl_ + 1;
    i = bpl_ * h;
    while (i--) {
        sum = (iptr[-bpl_] + iptr[-1] + iptr[1] + iptr[bpl_]) >> 2;
        if (sum > 2)
            sum -= 2;
        *(iptr++) = sum;
    }
}

void
generate_cmap(void)
{
    guint32 colors[256], i, red, blue, green;

    red = (guint32) (bscope_cfg.color / 0x10000);
    green = (guint32) ((bscope_cfg.color % 0x10000) / 0x100);
    blue = (guint32) (bscope_cfg.color % 0x100);
    for (i = 255; i > 0; i--) {
        colors[i] =
            (((guint32) (i * red / 256) << 16) |
             ((guint32) (i * green / 256) << 8) |
             ((guint32) (i * blue / 256)));
    }
    colors[0] = 0;
    if (cmap) {
        gdk_rgb_cmap_free(cmap);
    }
    cmap = gdk_rgb_cmap_new(colors, 256);
}

static void
bscope_init(void)
{
    bscope_read_config();
    generate_cmap();
}

/* static GtkWidget * bscope_get_widget (void) */
static void * bscope_get_widget (void)
{
    if (area == NULL)
    {
        area = gtk_drawing_area_new ();
        gtk_widget_set_size_request (area, D_WIDTH, D_HEIGHT);
        bscope_resize_video (D_WIDTH, D_HEIGHT);

        g_signal_connect (area, "configure-event", (GCallback)
         bscope_reconfigure, NULL);
        g_signal_connect (area, "destroy", (GCallback) gtk_widget_destroyed,
         & area);
    }

    return area;
}

static void
bscope_cleanup(void)
{
    if (cmap) {
        gdk_rgb_cmap_free(cmap);
        cmap = NULL;
    }

    area = NULL;
}

static void
bscope_playback_stop(void)
{
    if (GTK_WIDGET_REALIZED(area))
        gdk_window_clear(area->window);
}

static inline void
draw_vert_line(guchar * buffer, gint x, gint y1, gint y2)
{
    int y;
    if (y1 < y2) {
        for (y = y1 + 1; y <= y2; y++)
            draw_pixel_8(buffer, x, y, 0xFF);
    }
    else if (y2 < y1) {
        for (y = y2; y < y1; y++)
            draw_pixel_8(buffer, x, y, 0xFF);
    }
    else
        draw_pixel_8(buffer, x, y1, 0xFF);
}

static void
bscope_render_pcm(gint16 data[2][512])
{
    gint i, y, prev_y;

    g_static_mutex_lock(&rgb_buf_mutex);

    bscope_blur_8(rgb_buf, width, height, bpl);
    prev_y = (height / 2) + (data[0][0] >> 9);
    prev_y = CLAMP (prev_y, 0, height - 1);
    for (i = 0; i < width; i++) {
        y = (height / 2) + (data[0][i * 512 / width] >> 9);
        y = CLAMP (y, 0, height - 1);
        draw_vert_line(rgb_buf, i, prev_y, y);
        prev_y = y;
    }

    GDK_THREADS_ENTER();
    if (area != NULL)
        gdk_draw_indexed_image(area->window, area->style->white_gc, 0, 0,
                               width, height, GDK_RGB_DITHER_NONE,
                               rgb_buf + bpl + 1, (width + 2), cmap);
    GDK_THREADS_LEAVE();

    g_static_mutex_unlock(&rgb_buf_mutex);

    return;
}
