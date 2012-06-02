/*
 * Copyright (c) 2011 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <gtk/gtk.h>

#include <audacious/debug.h>
#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"

static gint width;
static gint height;

static gboolean draw_event (GtkWidget * widget, cairo_t * cr, void * unused)
{
    GdkPixbuf *album = NULL;

    if (aud_drct_get_playing() && album == NULL)
    {
        album = audgui_pixbuf_for_current ();
        g_return_val_if_fail (album != NULL, TRUE);
        if (gdk_pixbuf_get_width(album) > width ||
            gdk_pixbuf_get_height(album) > height)
            audgui_pixbuf_scale_within(&album, width < height ? width : height);
    }

    if (album != NULL)
    {
        double x = (width - gdk_pixbuf_get_width(album)) / 2;
        double y = (height - gdk_pixbuf_get_height(album)) / 2;

        gdk_cairo_set_source_pixbuf(cr, album, x, y);
        cairo_paint_with_alpha(cr, 1.0);
    }

    if (album != NULL)
        g_object_unref(album);

    return TRUE;
}

static gboolean configure_event (GtkWidget * widget, GdkEventConfigure * event)
{
    width = event->width;
    height = event->height;
    gtk_widget_queue_draw(widget);

    return TRUE;
}

static void playback_start (gpointer data, GtkWidget *area)
{
    gtk_widget_queue_draw(area);
}

static /* GtkWidget * */ gpointer get_widget (void)
{
    GtkWidget *area = gtk_drawing_area_new();

    g_signal_connect(area, "draw", (GCallback) draw_event, NULL);
    g_signal_connect(area, "configure-event", (GCallback) configure_event, NULL);

    hook_associate("playback begin", (HookFunction) playback_start, area);

    gtk_widget_set_size_request(area, 128, 128);

    return area;
}

static void cleanup(void)
{
    hook_dissociate("playback begin", (HookFunction) playback_start);
}

AUD_GENERAL_PLUGIN
(
    .name = N_("Album Art"),
    .domain = PACKAGE,
    .cleanup = cleanup,
    .get_widget = get_widget
)
