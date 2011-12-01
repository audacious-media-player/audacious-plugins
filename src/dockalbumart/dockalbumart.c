/*
 * dockalbumart: An audacious plugin to change the MacOS dock tile to the
 *     album art.
 * dockalbumart.c: Plugin implementation
 *
 * Copyright (c) 2007 William Pitcock <nenolod -at- sacredspiral.co.uk>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
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

#include <glib.h>
#include <audacious/i18n.h>

#include <Carbon/Carbon.h>

#include <audacious/plugin.h>

#include "audacious_player.xpm"

extern gchar *fileinfo_recursive_get_image(const gchar* path, const gchar* file_name, gint depth);

/* utility functions */
static void
dock_set_icon_from_pixbuf(const GdkPixbuf *in)
{
    GdkPixbuf *pixbuf;
    CGColorSpaceRef colorspace;
    CGDataProviderRef data_provider;
    CGImageRef image;
    gpointer data;
    gint rowstride, pixbuf_width, pixbuf_height;
    gboolean has_alpha;
         
    pixbuf = gdk_pixbuf_scale_simple(in, 128, 128, GDK_INTERP_BILINEAR);
    
    data = gdk_pixbuf_get_pixels(pixbuf);
    pixbuf_width = gdk_pixbuf_get_width(pixbuf);
    pixbuf_height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
        
    /* create the colourspace for the CGImage. */
    colorspace = CGColorSpaceCreateDeviceRGB();
    data_provider = CGDataProviderCreateWithData(NULL, data, pixbuf_height * rowstride, NULL);
    image = CGImageCreate(pixbuf_width, pixbuf_height, 8,
        has_alpha ? 32 : 24, rowstride, colorspace,
        has_alpha ? kCGImageAlphaLast : 0,
        data_provider, NULL, FALSE,   
        kCGRenderingIntentDefault);
    
    /* release the colourspace and data provider, we have what we want. */
    CGDataProviderRelease(data_provider);
    CGColorSpaceRelease(colorspace);

    /* set the dock tile images */
    SetApplicationDockTileImage(image);

    /* and release */
    CGImageRelease(image);
    g_object_unref(pixbuf);
}

static void
pixbuf_find_and_load(Tuple *tuple)
{
    GdkPixbuf *out;
    gchar *tmp;
    const gchar *file_path, *file_name;

    file_name = tuple_get_str(tuple, FIELD_FILE_NAME, NULL);
    file_path = tuple_get_str(tuple, FIELD_FILE_PATH, NULL);

    if (file_name != NULL && file_path != NULL)
    {
        tmp = fileinfo_recursive_get_image(file_path, file_name, 0);
        if (tmp)
        {
            GdkPixbuf *new = gdk_pixbuf_new_from_file(tmp, NULL);
            dock_set_icon_from_pixbuf(new);
            g_object_unref(new);                
        }
        else
        {
            GdkPixbuf *new = gdk_pixbuf_new_from_xpm_data((const gchar **) audacious_player_xpm);
            dock_set_icon_from_pixbuf(new);
            g_object_unref(new);
        }
    }
}

/* trigger functions */

static void
dockart_trigger_func_pb_start_cb(gpointer plentry_p, gpointer unused)
{
    PlaylistEntry *entry = (PlaylistEntry *) plentry_p;
    Tuple *tuple;

    if (entry == NULL)
        return;

    tuple = entry->tuple;

    if (tuple == NULL)
        return;

    pixbuf_find_and_load(tuple);
}

static void
dockart_trigger_func_pb_end_cb(gpointer plentry_p, gpointer unused)
{
    GdkPixbuf *new;

    new = gdk_pixbuf_new_from_xpm_data((const gchar **) audacious_player_xpm);
    dock_set_icon_from_pixbuf(new);
    g_object_unref(new);
}

static void
dockart_init(void)
{
    hook_associate("playback begin", dockart_trigger_func_pb_start_cb, NULL);
    hook_associate("playback end", dockart_trigger_func_pb_end_cb, NULL);
}

static void
dockart_cleanup(void)
{
    GdkPixbuf *new;

    hook_dissociate("playback begin", dockart_trigger_func_pb_start_cb);
    hook_dissociate("playback end", dockart_trigger_func_pb_end_cb);

    /* reset dock tile */
    new = gdk_pixbuf_new_from_xpm_data((const gchar **) audacious_player_xpm);
    dock_set_icon_from_pixbuf(new);
    g_object_unref(new);
}

GeneralPlugin dockart_gp =
{
    .description = "Dock Album Art",
    .init = dockart_init,
    .cleanup = dockart_cleanup
};

GeneralPlugin *dockart_gplist[] = { &dockart_gp, NULL };
SIMPLE_GENERAL_PLUGIN(dockart, dockart_gplist);
