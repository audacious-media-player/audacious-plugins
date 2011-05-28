/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007  Audacious development team.
 *
 * Based on:
 * BMP - Cross-platform multimedia player
 * Copyright (C) 2003-2004  BMP development team.
 * XMMS:
 * Copyright (C) 1998-2003  XMMS development team.
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

#include <string.h>

#include <gtk/gtk.h>

#include <audacious/drct.h>

#include "skins_cfg.h"
#include "ui_skin.h"
#include "ui_svis.h"
#include "ui_vis.h"
#include "util.h"

#define UI_TYPE_SVIS           (ui_svis_get_type())

/* FIXME: Are the svis_scope_colors correct? */
static guint8 svis_scope_colors[] = { 20, 19, 18, 19, 20 };
static guint8 svis_vu_normal_colors[] = { 17, 17, 17, 12, 12, 12, 2, 2 };

#define SVIS_HEIGHT 5
#define SVIS_WIDTH 38

enum
{
    DOUBLED,
    LAST_SIGNAL
};

static void ui_svis_class_init (UiSVisClass * klass);
static void ui_svis_init (UiSVis * svis);
static void ui_svis_destroy (GtkObject * object);
static void ui_svis_realize (GtkWidget * widget);
static void ui_svis_unrealize (GtkWidget * widget);
static void ui_svis_map (GtkWidget * widget);
static void ui_svis_unmap (GtkWidget * widget);
static void ui_svis_size_request (GtkWidget * widget,
                                  GtkRequisition * requisition);
static void ui_svis_size_allocate (GtkWidget * widget,
                                   GtkAllocation * allocation);
static gboolean ui_svis_expose (GtkWidget * widget, GdkEventExpose * event);
static void ui_svis_toggle_scaled (UiSVis * svis);

static GtkWidgetClass *parent_class = NULL;
static guint vis_signals[LAST_SIGNAL] = { 0 };

GType ui_svis_get_type ()
{
    static GType vis_type = 0;
    if (!vis_type)
    {
        static const GTypeInfo vis_info = {
            sizeof (UiSVisClass),
            NULL,
            NULL,
            (GClassInitFunc) ui_svis_class_init,
            NULL,
            NULL,
            sizeof (UiSVis),
            0,
            (GInstanceInitFunc) ui_svis_init,
        };
        vis_type =
            g_type_register_static (GTK_TYPE_WIDGET, "UiSVis", &vis_info, 0);
    }

    return vis_type;
}

static void ui_svis_class_init (UiSVisClass * klass)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GtkObjectClass *) klass;
    widget_class = (GtkWidgetClass *) klass;
    parent_class = g_type_class_peek_parent (klass);

    object_class->destroy = ui_svis_destroy;

    widget_class->realize = ui_svis_realize;
    widget_class->unrealize = ui_svis_unrealize;
    widget_class->map = ui_svis_map;
    widget_class->unmap = ui_svis_unmap;
    widget_class->expose_event = ui_svis_expose;
    widget_class->size_request = ui_svis_size_request;
    widget_class->size_allocate = ui_svis_size_allocate;

    klass->scaled = ui_svis_toggle_scaled;

    vis_signals[DOUBLED] =
        g_signal_new ("toggle-scaled", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSVisClass, scaled), NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void ui_svis_init (UiSVis * svis)
{

}

GtkWidget *ui_svis_new (GtkWidget * fixed, gint x, gint y)
{
    UiSVis *svis = g_object_new (ui_svis_get_type (), NULL);

    svis->x = x;
    svis->y = y;

    svis->width = SVIS_WIDTH;
    svis->height = SVIS_HEIGHT;

    svis->fixed = fixed;
    svis->scaled = FALSE;

    svis->visible_window = TRUE;
    svis->event_window = NULL;

    gtk_fixed_put (GTK_FIXED (svis->fixed), GTK_WIDGET (svis), svis->x,
                   svis->y);

    return GTK_WIDGET (svis);
}

static void ui_svis_destroy (GtkObject * object)
{
    g_return_if_fail (object != NULL);
    g_return_if_fail (UI_IS_SVIS (object));

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void ui_svis_realize (GtkWidget * widget)
{
    UiSVis *svis;
    GdkWindowAttr attributes;
    gint attributes_mask;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (UI_IS_SVIS (widget));

    GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    svis = UI_SVIS (widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget);
    attributes.event_mask |=
        GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK;

    if (svis->visible_window)
    {
        attributes.visual = gtk_widget_get_visual (widget);
        attributes.colormap = gtk_widget_get_colormap (widget);
        attributes.wclass = GDK_INPUT_OUTPUT;
        attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
        widget->window =
            gdk_window_new (widget->parent->window, &attributes,
                            attributes_mask);
        GTK_WIDGET_UNSET_FLAGS (widget, GTK_NO_WINDOW);
        gdk_window_set_user_data (widget->window, widget);
    }
    else
    {
        widget->window = gtk_widget_get_parent_window (widget);
        g_object_ref (widget->window);

        attributes.wclass = GDK_INPUT_ONLY;
        attributes_mask = GDK_WA_X | GDK_WA_Y;
        svis->event_window =
            gdk_window_new (widget->window, &attributes, attributes_mask);
        GTK_WIDGET_SET_FLAGS (widget, GTK_NO_WINDOW);
        gdk_window_set_user_data (svis->event_window, widget);
    }

    widget->style = gtk_style_attach (widget->style, widget->window);
}

static void ui_svis_unrealize (GtkWidget * widget)
{
    UiSVis *svis;
    svis = UI_SVIS (widget);

    if (svis->event_window != NULL)
    {
        gdk_window_set_user_data (svis->event_window, NULL);
        gdk_window_destroy (svis->event_window);
        svis->event_window = NULL;
    }

    if (GTK_WIDGET_CLASS (parent_class)->unrealize)
        (*GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void ui_svis_map (GtkWidget * widget)
{
    UiSVis *svis;
    svis = UI_SVIS (widget);

    if (svis->event_window != NULL)
        gdk_window_show (svis->event_window);

    if (GTK_WIDGET_CLASS (parent_class)->map)
        (*GTK_WIDGET_CLASS (parent_class)->map) (widget);
}

static void ui_svis_unmap (GtkWidget * widget)
{
    UiSVis *svis;
    svis = UI_SVIS (widget);

    if (svis->event_window != NULL)
        gdk_window_hide (svis->event_window);

    if (GTK_WIDGET_CLASS (parent_class)->unmap)
        (*GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

static void ui_svis_size_request (GtkWidget * widget,
                                  GtkRequisition * requisition)
{
    UiSVis *svis = UI_SVIS (widget);

    requisition->width = svis->width * (svis->scaled ? config.scale_factor : 1);
    requisition->height =
        svis->height * (svis->scaled ? config.scale_factor : 1);
}

static void ui_svis_size_allocate (GtkWidget * widget,
                                   GtkAllocation * allocation)
{
    UiSVis *svis = UI_SVIS (widget);

    widget->allocation = *allocation;
    widget->allocation.x *= (svis->scaled ? config.scale_factor : 1);
    widget->allocation.y *= (svis->scaled ? config.scale_factor : 1);
    if (GTK_WIDGET_REALIZED (widget))
    {
        if (svis->event_window != NULL)
            gdk_window_move_resize (svis->event_window, widget->allocation.x,
                                    widget->allocation.y, allocation->width,
                                    allocation->height);
        else
            gdk_window_move_resize (widget->window, widget->allocation.x,
                                    widget->allocation.y, allocation->width,
                                    allocation->height);
    }

    svis->x = widget->allocation.x / (svis->scaled ? config.scale_factor : 1);
    svis->y = widget->allocation.y / (svis->scaled ? config.scale_factor : 1);
}

#define OFFSET_NORMAL(p, x, y) ((p) + SVIS_WIDTH * (y) + (x))
#define OFFSET_SCALED(p, x, y) ((p) + 4 * SVIS_WIDTH * (y) + 2 * (x))

static inline void DRAW_SCALED (guchar * p, guchar value)
{
    p[0] = value;
    p[1] = value;
    p[2 * SVIS_WIDTH] = value;
    p[2 * SVIS_WIDTH + 1] = value;
}

#define DRAW_DS_PIXEL DRAW_SCALED

static gboolean ui_svis_expose (GtkWidget * widget, GdkEventExpose * event)
{
    UiSVis *svis = UI_SVIS (widget);

    gint x, y, h;
    guchar svis_color[24][3];
    guchar rgb_data[SVIS_WIDTH * 2 * SVIS_HEIGHT * 2], *ptr, c;
    guint32 colors[24];
    GdkRgbCmap *cmap;

    if (!GTK_WIDGET_VISIBLE (widget))
        return FALSE;

    if (!svis->visible_window)
        return FALSE;

    skin_get_viscolor (aud_active_skin, svis_color);
    for (y = 0; y < 24; y++)
    {
        colors[y] =
            svis_color[y][0] << 16 | svis_color[y][1] << 8 | svis_color[y][2];
    }
    cmap = gdk_rgb_cmap_new (colors, 24);

    if (!config.scaled)
    {
        memset (rgb_data, 0, SVIS_WIDTH * SVIS_HEIGHT);
        if (config.vis_type == VIS_ANALYZER && !aud_drct_get_paused ()
            && aud_drct_get_playing ())
        {
            for (y = 0; y < SVIS_HEIGHT; y++)
            {
                ptr = OFFSET_NORMAL (rgb_data, 0, SVIS_HEIGHT - 1 - y);

                if (config.analyzer_type == ANALYZER_BARS)
                {
                    for (x = 0; x < (SVIS_WIDTH + 1) / 3; x ++)
                    {
                        if (svis->data[x] > y << 1)
                        {
                            ptr[3 * x] = 23;
                            ptr[3 * x + 1] = 23;
                        }
                    }
                }
                else
                {
                    for (x = 0; x < SVIS_WIDTH; x++)
                    {
                        if (svis->data[x] > y << 1)
                            ptr[x] = 23;
                    }
                }
            }
        }
        else if (config.vis_type == VIS_VOICEPRINT)
        {
            switch (config.vu_mode)
            {
            case VU_NORMAL:
                for (y = 0; y < 2; y++)
                {
                    ptr = rgb_data + ((y * 3) * 38);
                    h = (svis->data[y] * 7) / 37;
                    for (x = 0; x < h; x++, ptr += 5)
                    {
                        c = svis_vu_normal_colors[x];
                        *(ptr) = c;
                        *(ptr + 1) = c;
                        *(ptr + 2) = c;
                        *(ptr + 38) = c;
                        *(ptr + 39) = c;
                        *(ptr + 40) = c;
                    }
                }
                break;
            case VU_SMOOTH:
                for (y = 0; y < 2; y++)
                {
                    ptr = rgb_data + ((y * 3) * SVIS_WIDTH);
                    for (x = 0; x < svis->data[y]; x++, ptr++)
                    {
                        c = 17 - ((x * 15) / 37);
                        *(ptr) = c;
                        *(ptr + 38) = c;
                    }
                }
                break;
            }
        }
        else if (config.vis_type == VIS_SCOPE)
        {
            for (x = 0; x < 38; x++)
            {
                h = svis->data[x << 1] / 3;
                ptr = rgb_data + ((4 - h) * 38) + x;
                *ptr = svis_scope_colors[h];
            }
        }

    }
    else
    {                           /*svis scaling, this needs some work, since a lot of stuff is hardcoded --majeru */

        memset (rgb_data, 0,
                SVIS_WIDTH * config.scale_factor * SVIS_HEIGHT *
                config.scale_factor);
        if (config.vis_type == VIS_ANALYZER && !aud_drct_get_paused ()
            && aud_drct_get_playing ())
        {
            for (y = 0; y < SVIS_HEIGHT; y++)
            {
                ptr = OFFSET_SCALED (rgb_data, 0, SVIS_HEIGHT - 1 - y);

                if (config.analyzer_type == ANALYZER_BARS)
                {
                    for (x = 0; x < (SVIS_WIDTH + 1) / 3; x ++)
                    {
                        if (svis->data[x] > y << 1)
                        {
                            DRAW_SCALED (OFFSET_SCALED (ptr, x * 3, 0), 23);
                            DRAW_SCALED (OFFSET_SCALED (ptr, x * 3 + 1, 0), 23);
                        }
                    }
                }
                else
                {
                    for (x = 0; x < SVIS_WIDTH; x++)
                    {
                        if (svis->data[x] > y << 1)
                            DRAW_SCALED (OFFSET_SCALED (ptr, x, 0), 23);
                    }
                }
            }
        }
        else if (config.vis_type == VIS_VOICEPRINT)
        {
            switch (config.vu_mode)
            {
            case VU_NORMAL:
                for (y = 0; y < 2; y++)
                {
                    ptr = rgb_data + ((y * 3) * 152);
                    h = (svis->data[y] * 8) / 37;
                    for (x = 0; x < h; x++, ptr += 10)
                    {
                        c = svis_vu_normal_colors[x];
                        DRAW_DS_PIXEL (ptr, c);
                        DRAW_DS_PIXEL (ptr + 2, c);
                        DRAW_DS_PIXEL (ptr + 4, c);
                        DRAW_DS_PIXEL (ptr + 152, c);
                        DRAW_DS_PIXEL (ptr + 154, c);
                        DRAW_DS_PIXEL (ptr + 156, c);
                    }
                }
                break;
            case VU_SMOOTH:
                for (y = 0; y < 2; y++)
                {
                    ptr = rgb_data + ((y * 3) * 152);
                    for (x = 0; x < svis->data[y]; x++, ptr += 2)
                    {
                        c = 17 - ((x * 15) / 37);
                        DRAW_DS_PIXEL (ptr, c);
                        DRAW_DS_PIXEL (ptr + 152, c);
                    }
                }
                break;
            }
        }
        else if (config.vis_type == VIS_SCOPE)
        {
            for (x = 0; x < 38; x++)
            {
                h = svis->data[x << 1] / 3;
                ptr = rgb_data + ((4 - h) * 152) + (x << 1);
                *ptr = svis_scope_colors[h];
                *(ptr + 1) = svis_scope_colors[h];
                *(ptr + 76) = svis_scope_colors[h];
                *(ptr + 77) = svis_scope_colors[h];
            }
        }


    }

    GdkPixmap *obj = NULL;
    GdkGC *gc;
    obj =
        gdk_pixmap_new (NULL,
                        svis->width * (svis->scaled ? config.scale_factor : 1),
                        svis->height * (svis->scaled ? config.scale_factor : 1),
                        gdk_rgb_get_visual ()->depth);
    gc = gdk_gc_new (obj);

    if (!svis->scaled)
    {
        gdk_draw_indexed_image (obj, gc, 0, 0, svis->width, svis->height,
                                GDK_RGB_DITHER_NORMAL, (guchar *) rgb_data,
                                38, cmap);
    }
    else
    {
        gdk_draw_indexed_image (obj, gc,
                                0 << 1, 0 << 1,
                                svis->width << 1, svis->height << 1,
                                GDK_RGB_DITHER_NONE, (guchar *) rgb_data,
                                76, cmap);
    }

    gdk_rgb_cmap_free (cmap);
    gdk_draw_drawable (widget->window, gc, obj, 0, 0, 0, 0,
                       svis->width * (svis->scaled ? config.scale_factor : 1),
                       svis->height * (svis->scaled ? config.scale_factor : 1));
    g_object_unref (obj);
    g_object_unref (gc);

    return FALSE;
}

static void ui_svis_toggle_scaled (UiSVis * svis)
{
    GtkWidget *widget = GTK_WIDGET (svis);
    svis->scaled = !svis->scaled;

    gtk_widget_set_size_request (widget, svis->width * config.scale_factor,
                                 svis->height * config.scale_factor);

    if (widget_really_drawable (widget))
        ui_svis_expose (widget, 0);
}

void ui_svis_clear_data (GtkWidget * widget)
{
    g_return_if_fail (UI_IS_SVIS (widget));

    gint i;
    UiSVis *svis = UI_SVIS (widget);

    for (i = 0; i < 75; i++)
    {
        svis->data[i] = (config.vis_type == VIS_SCOPE) ? 6 : 0;
    }

    svis->refresh_delay = 0;

    if (widget_really_drawable (widget))
        ui_svis_expose (widget, 0);
}

void ui_svis_timeout_func (GtkWidget * widget, guchar * data)
{
    g_return_if_fail (UI_IS_SVIS (widget));

    UiSVis *svis = UI_SVIS (widget);
    gint i;

    if (config.vis_type == VIS_VOICEPRINT)
    {
        for (i = 0; i < 2; i++)
            svis->data[i] = data[i];
    }
    else
    {
        for (i = 0; i < 75; i++)
            svis->data[i] = data[i];
    }

    if (widget_really_drawable (widget))
        ui_svis_expose (widget, 0);
}
