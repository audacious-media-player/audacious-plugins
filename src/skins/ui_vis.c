/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007-2010  Audacious development team.
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

#include "ui_skin.h"
#include "ui_vis.h"
#include "util.h"
#include "skins_cfg.h"
#include <audacious/plugin.h>

static const gfloat vis_afalloff_speeds[] = { 0.34, 0.5, 1.0, 1.3, 1.6 };
static const gfloat vis_pfalloff_speeds[] = { 1.2, 1.3, 1.4, 1.5, 1.6 };
static const gint vis_scope_colors[16] = {22, 21, 21, 20, 20, 19, 19, 18, 18,
 19, 19, 20, 20, 21, 21, 22};
static guchar voiceprint_data[76 * 16];

enum
{
    DOUBLED,
    LAST_SIGNAL
};

static void ui_vis_class_init (UiVisClass * klass);
static void ui_vis_init (UiVis * vis);
static void ui_vis_destroy (GtkObject * object);
static void ui_vis_realize (GtkWidget * widget);
static void ui_vis_unrealize (GtkWidget * widget);
static void ui_vis_map (GtkWidget * widget);
static void ui_vis_unmap (GtkWidget * widget);
static void ui_vis_size_request (GtkWidget * widget,
                                 GtkRequisition * requisition);
static void ui_vis_size_allocate (GtkWidget * widget,
                                  GtkAllocation * allocation);
static gboolean ui_vis_expose (GtkWidget * widget, GdkEventExpose * event);
static void ui_vis_toggle_scaled (UiVis * vis);

static GtkWidgetClass *parent_class = NULL;
static guint vis_signals[LAST_SIGNAL] = { 0 };

GType ui_vis_get_type ()
{
    static GType vis_type = 0;
    if (!vis_type)
    {
        static const GTypeInfo vis_info = {
            sizeof (UiVisClass),
            NULL,
            NULL,
            (GClassInitFunc) ui_vis_class_init,
            NULL,
            NULL,
            sizeof (UiVis),
            0,
            (GInstanceInitFunc) ui_vis_init,
        };
        vis_type =
            g_type_register_static (GTK_TYPE_WIDGET, "UiVis", &vis_info, 0);
    }

    return vis_type;
}

static void ui_vis_class_init (UiVisClass * klass)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GtkObjectClass *) klass;
    widget_class = (GtkWidgetClass *) klass;
    parent_class = g_type_class_peek_parent (klass);

    object_class->destroy = ui_vis_destroy;

    widget_class->realize = ui_vis_realize;
    widget_class->unrealize = ui_vis_unrealize;
    widget_class->map = ui_vis_map;
    widget_class->unmap = ui_vis_unmap;
    widget_class->expose_event = ui_vis_expose;
    widget_class->size_request = ui_vis_size_request;
    widget_class->size_allocate = ui_vis_size_allocate;

    klass->doubled = ui_vis_toggle_scaled;

    vis_signals[DOUBLED] =
        g_signal_new ("toggle-scaled", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiVisClass, doubled), NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void ui_vis_init (UiVis * vis)
{
    memset (voiceprint_data, 0, sizeof voiceprint_data);
}

GtkWidget *ui_vis_new (GtkWidget * fixed, gint x, gint y, gint width)
{
    UiVis *vis = g_object_new (ui_vis_get_type (), NULL);

    vis->x = x;
    vis->y = y;

    vis->width = width;
    vis->height = 16;

    vis->fixed = fixed;
    vis->scaled = FALSE;

    vis->visible_window = TRUE;
    vis->event_window = NULL;

    gtk_fixed_put (GTK_FIXED (vis->fixed), GTK_WIDGET (vis), vis->x, vis->y);

    return GTK_WIDGET (vis);
}

static void ui_vis_destroy (GtkObject * object)
{
    UiVis *vis;

    g_return_if_fail (object != NULL);
    g_return_if_fail (UI_IS_VIS (object));

    vis = UI_VIS (object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void ui_vis_realize (GtkWidget * widget)
{
    UiVis *vis;
    GdkWindowAttr attributes;
    gint attributes_mask;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (UI_IS_VIS (widget));

    GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    vis = UI_VIS (widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget);
    attributes.event_mask |=
        GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK;

    if (vis->visible_window)
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
        vis->event_window =
            gdk_window_new (widget->window, &attributes, attributes_mask);
        GTK_WIDGET_SET_FLAGS (widget, GTK_NO_WINDOW);
        gdk_window_set_user_data (vis->event_window, widget);
    }

    widget->style = gtk_style_attach (widget->style, widget->window);
}

static void ui_vis_unrealize (GtkWidget * widget)
{
    UiVis *vis;
    vis = UI_VIS (widget);

    if (vis->event_window != NULL)
    {
        gdk_window_set_user_data (vis->event_window, NULL);
        gdk_window_destroy (vis->event_window);
        vis->event_window = NULL;
    }

    if (GTK_WIDGET_CLASS (parent_class)->unrealize)
        (*GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void ui_vis_map (GtkWidget * widget)
{
    UiVis *vis;
    vis = UI_VIS (widget);

    if (vis->event_window != NULL)
        gdk_window_show (vis->event_window);

    if (GTK_WIDGET_CLASS (parent_class)->map)
        (*GTK_WIDGET_CLASS (parent_class)->map) (widget);
}

static void ui_vis_unmap (GtkWidget * widget)
{
    UiVis *vis;
    vis = UI_VIS (widget);

    if (vis->event_window != NULL)
        gdk_window_hide (vis->event_window);

    if (GTK_WIDGET_CLASS (parent_class)->unmap)
        (*GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

static void ui_vis_size_request (GtkWidget * widget,
                                 GtkRequisition * requisition)
{
    UiVis *vis = UI_VIS (widget);

    requisition->width = vis->width * (vis->scaled ? config.scale_factor : 1);
    requisition->height = vis->height * (vis->scaled ? config.scale_factor : 1);
}

static void ui_vis_size_allocate (GtkWidget * widget,
                                  GtkAllocation * allocation)
{
    UiVis *vis = UI_VIS (widget);

    widget->allocation = *allocation;
    widget->allocation.x *= (vis->scaled ? config.scale_factor : 1);
    widget->allocation.y *= (vis->scaled ? config.scale_factor : 1);
    if (GTK_WIDGET_REALIZED (widget))
    {
        if (vis->event_window != NULL)
            gdk_window_move_resize (vis->event_window, widget->allocation.x,
                                    widget->allocation.y, allocation->width,
                                    allocation->height);
        else
            gdk_window_move_resize (widget->window, widget->allocation.x,
                                    widget->allocation.y, allocation->width,
                                    allocation->height);
    }

    vis->x = widget->allocation.x / (vis->scaled ? config.scale_factor : 1);
    vis->y = widget->allocation.y / (vis->scaled ? config.scale_factor : 1);
}

#define MAKE_RGB(r,g,b) (((r) << 16) | ((g) << 8) | (b))
#define RGB_SEEK(x,y) (set = rgb + 76 * (y) + (x))
#define RGB_SET(c) (* set ++ = (c))
#define RGB_SET_Y(c) do {* set = (c); set += 76;} while (0)
#define RGB_SET_INDEX(c) RGB_SET (vis_color[c])
#define RGB_SET_INDEX_Y(c) RGB_SET_Y (vis_color[c])

static guint32 vis_color[24];
static guint32 vis_voice_color[256];
static guint32 vis_voice_color_fire[256];
static guint32 vis_voice_color_ice[256];
static guint32 pattern_fill[76 * 2];

void ui_vis_set_colors (void)
{
    g_return_if_fail (aud_active_skin != NULL);

    for (gint i = 0; i < 24; i ++)
        vis_color[i] = (aud_active_skin->vis_color[i][0] << 16) |
         (aud_active_skin->vis_color[i][1] << 8) |
         aud_active_skin->vis_color[i][2];

    GdkColor * fgc = skin_get_color (aud_active_skin, SKIN_TEXTFG);
    GdkColor * bgc = skin_get_color (aud_active_skin, SKIN_TEXTBG);
    gint fg[3] = {fgc->red >> 8, fgc->green >> 8, fgc->blue >> 8};
    gint bg[3] = {bgc->red >> 8, bgc->green >> 8, bgc->blue >> 8};

    for (gint x = 0; x < 256; x ++)
    {
        guchar c[3];
        for (gint n = 0; n < 3; n ++)
            c[n] = bg[n] + (fg[n] - bg[n]) * x / 255;
        vis_voice_color[x] = MAKE_RGB (c[0], c[1], c[2]);
    }

    for (gint x = 0; x < 256; x ++)
    {
        guchar r = MIN (x, 127) * 2;
        guchar g = CLAMP (x - 64, 0, 127) * 2;
        guchar b = MAX (x - 128, 0) * 2;
        vis_voice_color_fire[x] = MAKE_RGB (r, g, b);
    }

    for (gint x = 0; x < 256; x ++)
    {
        guchar g = MIN (x, 127) * 2;
        guchar b = MIN (x, 63) * 4;
        vis_voice_color_ice[x] = MAKE_RGB (x, g, b);
    }

    guint32 * set = pattern_fill;
    guint32 * end = set + 76;

    while (set < end)
        RGB_SET_INDEX (0);

    end = set + 76;

    while (set < end)
    {
        RGB_SET_INDEX (1);
        RGB_SET_INDEX (0);
    }
}

static gboolean ui_vis_expose (GtkWidget * widget, GdkEventExpose * event)
{
    UiVis * vis = (UiVis *) widget;
    guint32 rgb[76 * 16];
    guint32 rgb2[76 * 2 * 16 * 2];
    guint32 * set;

    if (config.vis_type != VIS_VOICEPRINT)
    {
        for (set = rgb; set < rgb + 76 * 16; set += 76 * 2)
            memcpy (set, pattern_fill, sizeof pattern_fill);
    }

    switch (config.vis_type)
    {
    case VIS_ANALYZER:;
        gboolean bars = (config.analyzer_type == ANALYZER_BARS);

        for (gint x = 0; x < 75; x ++)
        {
            if (bars && (x & 3) == 3)
                continue;

            gint h = vis->data[bars ? (x >> 2) : x];
            h = CLAMP (h, 0, 16);
            RGB_SEEK (x, 16 - h);

            switch (config.analyzer_mode)
            {
            case ANALYZER_NORMAL:
                for (gint y = 0; y < h; y ++)
                    RGB_SET_INDEX_Y (18 - h + y);
                break;
            case ANALYZER_FIRE:
                for (gint y = 0; y < h; y ++)
                    RGB_SET_INDEX_Y (2 + y);
                break;
            default: /* ANALYZER_VLINES */
                for (gint y = 0; y < h; y ++)
                    RGB_SET_INDEX_Y (18 - h);
                break;
            }

            if (config.analyzer_peaks)
            {
                gint h = vis->peak[bars ? (x >> 2) : x];
                h = CLAMP (h, 0, 16);

                if (h)
                {
                    RGB_SEEK (x, 16 - h);
                    RGB_SET_INDEX (23);
                }
            }
        }

        break;
    case VIS_VOICEPRINT:
        /* Move the ribbon only if we are called directly, not on an actual
         * expose. */
        if (event == NULL)
        {
            memmove (voiceprint_data, voiceprint_data + 1, sizeof
             voiceprint_data - 1);

            for (gint y = 0; y < 16; y ++)
                voiceprint_data[76 * y + 75] = vis->data[y];
        }

        guchar * get = voiceprint_data;
        guint32 * colors = (config.voiceprint_mode == VOICEPRINT_NORMAL) ?
         vis_voice_color : (config.voiceprint_mode == VOICEPRINT_FIRE) ?
         vis_voice_color_fire : /* VOICEPRINT_ICE */ vis_voice_color_ice;
        set = rgb;

        for (gint y = 0; y < 16; y ++)
        for (gint x = 0; x < 76; x ++)
            RGB_SET (colors[* get ++]);
        break;
    case VIS_SCOPE:
        switch (config.scope_mode)
        {
        case SCOPE_DOT:
            for (gint x = 0; x < 75; x ++)
            {
                gint h = CLAMP (1 + vis->data[x], 0, 15);
                RGB_SEEK (x, h);
                RGB_SET_INDEX (vis_scope_colors[h]);
            }
            break;
        case SCOPE_LINE:
            for (gint x = 0; x < 74; x++)
            {
                gint h = CLAMP (1 + vis->data[x], 0, 15);
                gint h2 = CLAMP (1 + vis->data[x + 1], 0, 15);

                if (h < h2) h2 --;
                else if (h > h2) {gint temp = h; h = h2 + 1; h2 = temp;}

                RGB_SEEK (x, h);

                for (gint y = h; y <= h2; y ++)
                    RGB_SET_INDEX_Y (vis_scope_colors[y]);
            }

            gint h = CLAMP (1 + vis->data[74], 0, 15);
            RGB_SEEK (74, h);
            RGB_SET_INDEX (vis_scope_colors[h]);
            break;
        default: /* SCOPE_SOLID */
            for (gint x = 0; x < 75; x++)
            {
                gint h = CLAMP (1 + vis->data[x], 0, 15);
                gint h2;

                if (h < 8) h2 = 7;
                else {h2 = h; h = 8;}

                RGB_SEEK (x, h);

                for (gint y = h; y <= h2; y ++)
                    RGB_SET_INDEX_Y (vis_scope_colors[y]);
            }
            break;
        }
        break;
    }

    if (vis->scaled)
    {
        guint32 * to = rgb2;

        for (gint y = 0; y < 16 * 2; y ++)
        {
            guint32 * from = rgb + 76 * (y >> 1);
            guint32 * end = from + 76;

            while (from < end)
            {
                * to ++ = * from;
                * to ++ = * from ++;
            }
        }
    }

    cairo_t * cr = gdk_cairo_create (widget->window);
    cairo_surface_t * surf;

    if (vis->scaled)
        surf = cairo_image_surface_create_for_data ((void *) rgb2,
         CAIRO_FORMAT_RGB24, 76 * 2, 16 * 2, 4 * 76 * 2);
    else
        surf = cairo_image_surface_create_for_data ((void *) rgb,
         CAIRO_FORMAT_RGB24, 76, 16, 4 * 76);

    cairo_set_source_surface (cr, surf, 0, 0);
    cairo_paint (cr);
    cairo_surface_destroy (surf);
    cairo_destroy (cr);

    return FALSE;
}

static void ui_vis_toggle_scaled (UiVis * vis)
{
    GtkWidget *widget = GTK_WIDGET (vis);
    vis->scaled = !vis->scaled;

    gtk_widget_set_size_request (widget,
                                 vis->width *
                                 (vis->scaled ? config.scale_factor : 1),
                                 vis->height *
                                 (vis->scaled ? config.scale_factor : 1));

    if (widget_really_drawable (widget))
        ui_vis_expose (widget, 0);
}

void ui_vis_clear_data (GtkWidget * widget)
{
    g_return_if_fail (UI_IS_VIS (widget));

    gint i;
    UiVis *vis = UI_VIS (widget);

    memset (voiceprint_data, 0, 16 * 76);
    for (i = 0; i < 75; i++)
    {
        vis->data[i] = (config.vis_type == VIS_SCOPE) ? 6 : 0;
        vis->peak[i] = 0;
    }

    if (widget_really_drawable (widget))
        ui_vis_expose (widget, 0);
}

void ui_vis_timeout_func (GtkWidget * widget, guchar * data)
{
    g_return_if_fail (UI_IS_VIS (widget));

    UiVis *vis = UI_VIS (widget);
    gint i;

    if (config.vis_type == VIS_ANALYZER)
    {
        const gint n = (config.analyzer_type == ANALYZER_BARS) ? 19 : 75;

        for (i = 0; i < n; i++)
        {
            if (data[i] > vis->data[i])
            {
                vis->data[i] = data[i];
                if (vis->data[i] > vis->peak[i])
                {
                    vis->peak[i] = vis->data[i];
                    vis->peak_speed[i] = 0.01;

                }
                else if (vis->peak[i] > 0.0)
                {
                    vis->peak[i] -= vis->peak_speed[i];
                    vis->peak_speed[i] *=
                        vis_pfalloff_speeds[config.peaks_falloff];
                    if (vis->peak[i] < vis->data[i])
                        vis->peak[i] = vis->data[i];
                    if (vis->peak[i] < 0.0)
                        vis->peak[i] = 0.0;
                }
            }
            else
            {
                if (vis->data[i] > 0.0)
                {
                    vis->data[i] -=
                        vis_afalloff_speeds[config.analyzer_falloff];
                    if (vis->data[i] < 0.0)
                        vis->data[i] = 0.0;
                }
                if (vis->peak[i] > 0.0)
                {
                    vis->peak[i] -= vis->peak_speed[i];
                    vis->peak_speed[i] *=
                        vis_pfalloff_speeds[config.peaks_falloff];
                    if (vis->peak[i] < vis->data[i])
                        vis->peak[i] = vis->data[i];
                    if (vis->peak[i] < 0.0)
                        vis->peak[i] = 0.0;
                }
            }
        }
    }
    else if (config.vis_type == VIS_VOICEPRINT)
    {
        for (i = 0; i < 16; i++)
            vis->data[i] = data[15 - i];
    }
    else
    {
        for (i = 0; i < 75; i++)
            vis->data[i] = data[i];
    }

    if (widget_really_drawable (widget))
        ui_vis_expose (widget, NULL);
}
