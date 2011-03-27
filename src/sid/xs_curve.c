/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   XSCurve, a custom Gtk+ spline widget for representing SIDPlay2/reSID
   filter curves in the configuration GUI. Implementation based heavily
   on GtkCurve from Gtk+ 1.2.10 (C) 1997 David Mosberger.
   Spline formula from reSID 0.16 (C) 2004 Dag Lem.

   Programmed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 2006-2007 Tecnic Software productions (TNSP)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "xs_curve.h"
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkprivate.h>


#define RADIUS        3    /* radius of the control points */
#define RADIUS2        (RADIUS * 2)
#define MIN_DISTANCE    7    /* min distance between control points */


#define GRAPH_MASK    (GDK_EXPOSURE_MASK |        \
            GDK_POINTER_MOTION_MASK |    \
            GDK_POINTER_MOTION_HINT_MASK |    \
            GDK_ENTER_NOTIFY_MASK |        \
            GDK_BUTTON_PRESS_MASK |        \
            GDK_BUTTON_RELEASE_MASK |    \
            GDK_BUTTON1_MOTION_MASK)

#define GET_X(i)    curve->ctlpoints[i].x
#define GET_Y(i)    curve->ctlpoints[i].y


enum {
    PROP_0,
    PROP_MIN_X,
    PROP_MAX_X,
    PROP_MIN_Y,
    PROP_MAX_Y
};

static GtkDrawingAreaClass *parent_class = NULL;

static void xs_curve_class_init(XSCurveClass * class);
static void xs_curve_init(XSCurve * curve);
static void xs_curve_get_property(GObject * object, guint param_id,
            GValue * value, GParamSpec * pspec);
static void xs_curve_set_property(GObject * object, guint param_id,
            const GValue * value, GParamSpec * pspec);
static void xs_curve_finalize(GObject * object);
static gint xs_curve_graph_events(GtkWidget * widget, GdkEvent * event, XSCurve * c);
static void xs_curve_size_graph(XSCurve * curve);


GtkType xs_curve_get_type(void)
{
    static GType curve_type = 0;

    if (!curve_type) {
        static const GTypeInfo curve_info = {
            sizeof(XSCurveClass),
            NULL,    /* base_init */
            NULL,    /* base_finalize */
            (GClassInitFunc) xs_curve_class_init,
            NULL,    /* class_finalize */
            NULL,    /* class_data */
            sizeof(XSCurve),
            0,    /* n_preallocs */
            (GInstanceInitFunc) xs_curve_init,
        };

        curve_type = g_type_register_static(
            GTK_TYPE_DRAWING_AREA, "XSCurve",
            &curve_info, 0);
    }
    return curve_type;
}


static void xs_curve_class_init(XSCurveClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(class);

    parent_class = g_type_class_peek_parent(class);

    gobject_class->finalize = xs_curve_finalize;

    gobject_class->set_property = xs_curve_set_property;
    gobject_class->get_property = xs_curve_get_property;

    g_object_class_install_property(gobject_class, PROP_MIN_X,
        g_param_spec_float("min-x",
            "Minimum X",
            "Minimum possible value for X",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0,
            GTK_PARAM_READWRITE)
        );

    g_object_class_install_property(gobject_class, PROP_MAX_X,
        g_param_spec_float("max-x",
            "Maximum X",
            "Maximum possible X value",
            -G_MAXFLOAT, G_MAXFLOAT, 1.0,
            GTK_PARAM_READWRITE)
        );

    g_object_class_install_property(gobject_class, PROP_MIN_Y,
        g_param_spec_float("min-y",
            "Minimum Y",
            "Minimum possible value for Y",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0,
            GTK_PARAM_READWRITE)
        );

    g_object_class_install_property(gobject_class, PROP_MAX_Y,
        g_param_spec_float("max-y",
            "Maximum Y",
            "Maximum possible value for Y",
            -G_MAXFLOAT, G_MAXFLOAT, 1.0,
            GTK_PARAM_READWRITE)
        );
}


static void xs_curve_init(XSCurve *curve)
{
    gint old_mask;

    curve->pixmap = NULL;
    curve->grab_point = -1;

    curve->nctlpoints = 0;
    curve->ctlpoints = NULL;

    curve->min_x = 0.0;
    curve->max_x = 2047.0;
    curve->min_y = 0.0;
    curve->max_y = 24000.0;

    old_mask = gtk_widget_get_events(GTK_WIDGET(curve));
    gtk_widget_set_events(GTK_WIDGET(curve), old_mask | GRAPH_MASK);
    g_signal_connect(curve, "event", G_CALLBACK(xs_curve_graph_events), curve);
    xs_curve_size_graph(curve);
}


static void xs_curve_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
    XSCurve *curve = XS_CURVE(object);

    switch (prop_id) {
    case PROP_MIN_X:
        xs_curve_set_range(curve,
            g_value_get_float(value), curve->max_x,
            curve->min_y, curve->max_y);
        break;
    case PROP_MAX_X:
        xs_curve_set_range(curve,
            curve->min_x, g_value_get_float(value),
            curve->min_y, curve->max_y);
        break;
    case PROP_MIN_Y:
        xs_curve_set_range(curve,
            curve->min_x, curve->max_x,
            g_value_get_float(value), curve->max_y);
        break;
    case PROP_MAX_Y:
        xs_curve_set_range(curve,
            curve->min_x, curve->max_x,
            curve->min_y, g_value_get_float(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


static void xs_curve_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
    XSCurve *curve = XS_CURVE(object);

    switch (prop_id) {
    case PROP_MIN_X:
        g_value_set_float(value, curve->min_x);
        break;
    case PROP_MAX_X:
        g_value_set_float(value, curve->max_x);
        break;
    case PROP_MIN_Y:
        g_value_set_float(value, curve->min_y);
        break;
    case PROP_MAX_Y:
        g_value_set_float(value, curve->max_y);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


static int xs_project(gfloat value, gfloat min, gfloat max, int norm)
{
    return (norm - 1) * ((value - min) / (max - min)) + 0.5;
}


static gfloat xs_unproject(gint value, gfloat min, gfloat max, int norm)
{
    return value / (gfloat) (norm - 1) * (max - min) + min;
}


static inline void xs_cubic_coeff(gfloat x1, gfloat y1,
            gfloat x2, gfloat y2,
            gfloat k1, gfloat k2,
            gfloat *a, gfloat *b,
            gfloat *c, gfloat *d)
{
    gfloat dx = x2 - x1, dy = y2 - y1;

    *a = ((k1 + k2) - 2 * dy / dx) / (dx * dx);
    *b = ((k2 - k1) / dx - 3 * (x1 + x2) * (*a)) / 2;
    *c = k1 - (3 * x1 * (*a) + 2 * (*b)) * x1;
    *d = y1 - ((x1 * (*a) + (*b)) * x1 + (*c)) * x1;
}


static void xs_curve_draw(XSCurve *curve, gint width, gint height)
{
    gfloat res = 5.0f;
    GtkStateType state;
    GtkStyle *style;
    gint i, ox = -1, oy = -1;
    xs_point_t *p0, *p1, *p2, *p3;

    if (!curve->pixmap)
        return;

    state = GTK_STATE_NORMAL;
    if (!GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(curve)))
        state = GTK_STATE_INSENSITIVE;

    style = GTK_WIDGET(curve)->style;

    /* Clear the pixmap */
    gtk_paint_flat_box(style, curve->pixmap,
        GTK_STATE_NORMAL, GTK_SHADOW_NONE,
        NULL, GTK_WIDGET(curve), "curve_bg",
        0, 0,
        width + RADIUS2,
        height + RADIUS2);


    /* Draw the grid */
    for (i = 0; i < 5; i++) {
        gdk_draw_line(curve->pixmap, style->dark_gc[state],
            RADIUS,        i * (height / 4.0) + RADIUS,
            width + RADIUS,    i * (height / 4.0) + RADIUS);

        gdk_draw_line(curve->pixmap, style->dark_gc[state],
            i * (width / 4.0) + RADIUS, RADIUS,
            i * (width / 4.0) + RADIUS, height + RADIUS);
    }

#if 1
    /* Draw the spline/curve itself */
    p0 = curve->ctlpoints;
    p1 = p0;
    p2 = p1; p2++;
    p3 = p2; p3++;

    /* Draw each curve segment */
    if (curve->nctlpoints > 5)
    for (i = 0; i < curve->nctlpoints; i++, ++p0, ++p1, ++p2, ++p3) {
        gint n;
        gfloat k1, k2, a, b, c, d, x;

        if (p1->x == p2->x)
            continue;

        if (p0->x == p1->x && p2->x == p3->x) {
            k1 = k2 = (p2->y - p1->y) / (p2->x - p1->x);
        } else if (p0->x == p1->x) {
            k2 = (p3->y - p1->y) / (p3->x - p1->x);
            k1 = (3 * (p2->y - p1->y) / (p2->x - p1->x) - k2) / 2;
        } else if (p2->x == p3->x) {
            k1 = (p2->y - p0->y) / (p2->x - p0->x);
            k2 = (3 * (p2->y - p1->y) / (p2->x - p1->x) - k1) / 2;
        } else {
            k1 = (p2->y - p0->y) / (p2->x - p0->x);
            k2 = (p3->y - p1->y) / (p3->x - p1->x);
        }

        xs_cubic_coeff(p1->x, p1->y, p2->x, p2->y, k1, k2, &a, &b, &c, &d);

        for (x = p1->x; x <= p2->x; x += res, n++) {
            gfloat y = ((a * x + b) * x + c) * x + d;
            gint qx, qy;
            qx = RADIUS + xs_project(x, curve->min_x, curve->max_x, width);
            qy = RADIUS + xs_project(y, curve->min_y, curve->max_y, height);

            if (ox != -1) {
                gdk_draw_line(curve->pixmap, style->fg_gc[state],
                    ox, oy, qx, qy);
            }
            ox = qx; oy = qy;
        }
    }

#endif

    /* Draw control points */
    for (i = 0; i < curve->nctlpoints; ++i) {
        gint x, y;
        GtkStateType cstate;

        if (GET_X(i) < curve->min_x || GET_Y(i) < curve->min_y ||
            GET_X(i) >= curve->max_x || GET_Y(i) >= curve->max_y)
            continue;

        x = xs_project(GET_X(i), curve->min_x, curve->max_x, width);
        y = xs_project(GET_Y(i), curve->min_y, curve->max_y, height);

        if (i == curve->grab_point) {
            cstate = GTK_STATE_SELECTED;
            gdk_draw_line(curve->pixmap, style->fg_gc[cstate],
                x + RADIUS, RADIUS, x + RADIUS, height + RADIUS);
            gdk_draw_line(curve->pixmap, style->fg_gc[cstate],
                RADIUS, y + RADIUS, width + RADIUS, y + RADIUS);
        } else
            cstate = state;

        gdk_draw_arc(curve->pixmap, style->fg_gc[cstate], TRUE,
            x, y, RADIUS2, RADIUS2, 0, 360 * 64);
    }

    /* Draw pixmap in the widget */
    gdk_draw_pixmap(GTK_WIDGET(curve)->window,
            style->fg_gc[state], curve->pixmap,
            0, 0, 0, 0,
            width + RADIUS2,
            height + RADIUS2);
}


static gint xs_curve_graph_events(GtkWidget *widget, GdkEvent *event, XSCurve *curve)
{
    GdkCursorType new_type = curve->cursor_type;
    GtkWidget *w;
    gint i, width, height, x, y, tx, ty, cx, closest_point = 0, min_x;
    guint distance;

    w = GTK_WIDGET(curve);
    width = w->allocation.width - RADIUS2;
    height = w->allocation.height - RADIUS2;

    if ((width < 0) || (height < 0))
        return FALSE;

    /* get the pointer position */
    gdk_window_get_pointer(w->window, &tx, &ty, NULL);
    x = CLAMP((tx - RADIUS), 0, width - 1);
    y = CLAMP((ty - RADIUS), 0, height - 1);
    min_x = curve->min_x;

    distance = ~0U;
    for (i = 0; i < curve->nctlpoints; ++i) {
        cx = xs_project(GET_X(i), min_x, curve->max_x, width);
        if ((guint) abs(x - cx) < distance) {
            distance = abs(x - cx);
            closest_point = i;
        }
    }

    /* Act based on event type */
    switch (event->type) {
    case GDK_CONFIGURE:
        if (curve->pixmap)
            gdk_pixmap_unref(curve->pixmap);
        curve->pixmap = 0;

        /* fall through */

    case GDK_EXPOSE:
        if (!curve->pixmap) {
            curve->pixmap = gdk_pixmap_new(w->window,
            w->allocation.width, w->allocation.height, -1);
        }
        xs_curve_draw(curve, width, height);
        break;

    case GDK_BUTTON_PRESS:
        gtk_grab_add(widget);

        new_type = GDK_TCROSS;

        if (distance > MIN_DISTANCE) {
            /* insert a new control point */
            if (curve->nctlpoints > 0) {
                cx = xs_project(GET_X(closest_point), min_x, curve->max_x, width);
                if (x > cx) closest_point++;
            }

            curve->nctlpoints++;

            curve->ctlpoints = g_realloc(curve->ctlpoints,
                curve->nctlpoints * sizeof(*curve->ctlpoints));

            for (i = curve->nctlpoints - 1; i > closest_point; --i) {
                memcpy(curve->ctlpoints + i,
                    curve->ctlpoints + i - 1,
                    sizeof(*curve->ctlpoints));
            }
        }

        curve->grab_point = closest_point;
        GET_X(curve->grab_point) = xs_unproject(x, min_x, curve->max_x, width);
        GET_Y(curve->grab_point) = xs_unproject(y, curve->min_y, curve->max_y, height);

        xs_curve_draw(curve, width, height);
        break;

    case GDK_BUTTON_RELEASE:
        {
        gint src, dst;

        gtk_grab_remove(widget);

        /* delete inactive points: */
        for (src = dst = 0; src < curve->nctlpoints; ++src) {
            if (GET_X(src) >= min_x) {
                memcpy(curve->ctlpoints + dst,
                    curve->ctlpoints + src,
                    sizeof(*curve->ctlpoints));
                dst++;
            }
        }

        if (dst < src) {
            curve->nctlpoints -= (src - dst);
            if (curve->nctlpoints <= 0) {
                curve->nctlpoints = 1;
                GET_X(0) = min_x;
                GET_Y(0) = curve->min_y;
                xs_curve_draw(curve, width, height);
            }
            curve->ctlpoints = g_realloc(curve->ctlpoints,
                curve->nctlpoints * sizeof(*curve->ctlpoints));
        }

        new_type = GDK_FLEUR;
        curve->grab_point = -1;
        }
        xs_curve_draw(curve, width, height);
        break;

    case GDK_MOTION_NOTIFY:
        if (curve->grab_point == -1) {
            /* if no point is grabbed...  */
            if (distance <= MIN_DISTANCE)
                new_type = GDK_FLEUR;
            else
                new_type = GDK_TCROSS;
        } else {
            gint leftbound, rightbound;

            /* drag the grabbed point  */
            new_type = GDK_TCROSS;

            leftbound = -MIN_DISTANCE;
            if (curve->grab_point > 0) {
                leftbound = xs_project(
                    GET_X(curve->grab_point-1),
                    min_x, curve->max_x, width);
            }

            rightbound = width + RADIUS2 + MIN_DISTANCE;
            if (curve->grab_point + 1 < curve->nctlpoints) {
                rightbound = xs_project(
                    GET_X(curve->grab_point+1),
                    min_x, curve->max_x, width);
            }

            if ((tx <= leftbound) || (tx >= rightbound) ||
                (ty > height + RADIUS2 + MIN_DISTANCE) || (ty < -MIN_DISTANCE)) {
                GET_X(curve->grab_point) = min_x - 1.0;
            } else {
                GET_X(curve->grab_point) =
                    xs_unproject(x, min_x, curve->max_x, width);
                GET_Y(curve->grab_point) =
                    xs_unproject(y, curve->min_y, curve->max_y, height);
            }

            xs_curve_draw(curve, width, height);
        }

        /* See if cursor type was changed and update accordingly */
        if (new_type != (GdkCursorType) curve->cursor_type) {
            GdkCursor *cursor;
            curve->cursor_type = new_type;
            cursor = gdk_cursor_new(curve->cursor_type);
            gdk_window_set_cursor(w->window, cursor);
            gdk_cursor_destroy(cursor);
        }
        break;

    default:
        break;
    }

    return FALSE;
}


static void xs_curve_size_graph(XSCurve *curve)
{
    gint width, height;
    gfloat aspect;
    GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(curve));

    width = (curve->max_x - curve->min_x) + 1;
    height = (curve->max_y - curve->min_y) + 1;
    aspect = width / (gfloat) height;

    if (width > gdk_screen_get_width(screen) / 4)
        width = gdk_screen_get_width(screen) / 4;

    if (height > gdk_screen_get_height(screen) / 4)
        height = gdk_screen_get_height(screen) / 4;

    if (aspect < 1.0)
        width = height * aspect;
    else
        height = width / aspect;

    gtk_widget_set_size_request(GTK_WIDGET(curve), width + RADIUS2, height + RADIUS2);
}


static void xs_curve_update(XSCurve *curve)
{
    if (curve->pixmap) {
        gint width, height;

        width = GTK_WIDGET(curve)->allocation.width - RADIUS2;
        height = GTK_WIDGET(curve)->allocation.height - RADIUS2;
        xs_curve_draw(curve, width, height);
    }
}


void xs_curve_reset(XSCurve *curve)
{
    if (curve->ctlpoints)
        g_free(curve->ctlpoints);

    curve->nctlpoints = 4;
    curve->ctlpoints = g_malloc(curve->nctlpoints * sizeof(curve->ctlpoints[0]));

    GET_X(0) = curve->min_x;
    GET_Y(0) = curve->min_y;
    GET_X(1) = curve->min_x;
    GET_Y(1) = curve->min_y;

    GET_X(2) = curve->max_x;
    GET_Y(2) = curve->max_y;
    GET_X(3) = curve->max_x;
    GET_Y(3) = curve->max_y;

    xs_curve_update(curve);
}


void xs_curve_set_range(XSCurve *curve, gfloat min_x, gfloat min_y, gfloat max_x, gfloat max_y)
{
    g_object_freeze_notify(G_OBJECT(curve));
    if (curve->min_x != min_x) {
        curve->min_x = min_x;
        g_object_notify(G_OBJECT(curve), "min-x");
    }
    if (curve->max_x != max_x) {
        curve->max_x = max_x;
        g_object_notify(G_OBJECT(curve), "max-x");
    }
    if (curve->min_y != min_y) {
        curve->min_y = min_y;
        g_object_notify(G_OBJECT(curve), "min-y");
    }
    if (curve->max_y != max_y) {
        curve->max_y = max_y;
        g_object_notify(G_OBJECT(curve), "max-y");
    }
    g_object_thaw_notify(G_OBJECT(curve));

    xs_curve_size_graph(curve);
    xs_curve_reset(curve);
}


gboolean xs_curve_realloc_data(XSCurve *curve, gint npoints)
{
    if (npoints != curve->nctlpoints) {
        curve->nctlpoints = npoints;
        curve->ctlpoints = (xs_point_t *) g_realloc(curve->ctlpoints,
            curve->nctlpoints * sizeof(*curve->ctlpoints));

        if (curve->ctlpoints == NULL)
            return FALSE;
    }

    return TRUE;
}


void xs_curve_get_data(XSCurve *curve, xs_point_t ***points, gint **npoints)
{
    *points = &(curve->ctlpoints);
    *npoints = &(curve->nctlpoints);
}


gboolean xs_curve_set_points(XSCurve *curve, xs_int_point_t *points, gint npoints)
{
    gint i;

    if (!xs_curve_realloc_data(curve, npoints + 4))
        return FALSE;

    GET_X(0) = curve->min_x;
    GET_Y(0) = curve->min_y;
    GET_X(1) = curve->min_x;
    GET_Y(1) = curve->min_y;

    for (i = 0; i < npoints; i++) {
        GET_X(i+2) = points[i].x;
        GET_Y(i+2) = points[i].y;
    }

    GET_X(npoints+2) = curve->max_x;
    GET_Y(npoints+2) = curve->max_y;
    GET_X(npoints+3) = curve->max_x;
    GET_Y(npoints+3) = curve->max_y;

    xs_curve_update(curve);
    return TRUE;
}


gboolean xs_curve_get_points(XSCurve *curve, xs_int_point_t **points, gint *npoints)
{
    gint i, n;

    n = curve->nctlpoints - 4;

    *points = g_malloc(n * sizeof(xs_int_point_t));
    if (*points == NULL)
        return FALSE;

    *npoints = n;
    for (i = 2; i < curve->nctlpoints - 2; i++) {
        (*points)[i].x = GET_X(i);
        (*points)[i].y = GET_Y(i);
    }

    return TRUE;
}


GtkWidget *xs_curve_new(void)
{
    return g_object_new(XS_TYPE_CURVE, NULL);
}


static void xs_curve_finalize(GObject *object)
{
    XSCurve *curve;

    g_return_if_fail(object != NULL);
    g_return_if_fail(XS_IS_CURVE(object));

    curve = XS_CURVE(object);
    if (curve->pixmap)
        g_object_unref(curve->pixmap);
    if (curve->ctlpoints)
        g_free(curve->ctlpoints);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}
