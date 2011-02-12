#ifndef XS_CURVE_H
#define XS_CURVE_H

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Macros for type-classing this GtkWidget/object
 */
#define XS_TYPE_CURVE               (xs_curve_get_type())
#define XS_CURVE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), XS_TYPE_CURVE, XSCurve))
#define XS_CURVE_CLASS(luokka)      (G_TYPE_CHECK_CLASS_CAST ((luokka), XS_TYPE_CURVE, XSCurveClass))
#define XS_IS_CURVE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XS_TYPE_CURVE))
#define XS_IS_CURVE_CLASS(luokka)   (G_TYPE_CHECK_CLASS_TYPE ((luokka), XS_TYPE_CURVE))
#define XS_CURVE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), XS_TYPE_CURVE, XSCurveClass))


/* Structures
 */
typedef struct _XSCurve XSCurve;
typedef struct _XSCurveClass XSCurveClass;

typedef struct {
    gfloat x,y;
} xs_point_t;

typedef struct {
    gint x, y;
} xs_int_point_t;

struct _XSCurve {
    GtkDrawingArea graph;

    gint cursor_type;
    gfloat min_x;
    gfloat max_x;
    gfloat min_y;
    gfloat max_y;
    GdkPixmap *pixmap;
    gint grab_point;    /* point currently grabbed */

    /* control points */
    gint nctlpoints;    /* number of control points */
    xs_point_t *ctlpoints;    /* array of control points */
};

struct _XSCurveClass {
    GtkDrawingAreaClass parent_class;
};


GType        xs_curve_get_type    (void);
GtkWidget*    xs_curve_new        (void);
void        xs_curve_reset        (XSCurve *curve);
void        xs_curve_set_range    (XSCurve *curve,
                     gfloat min_x, gfloat min_y,
                     gfloat max_x, gfloat max_y);
gboolean    xs_curve_realloc_data    (XSCurve *curve, gint npoints);
void        xs_curve_get_data    (XSCurve *curve, xs_point_t ***points, gint **npoints);
gboolean    xs_curve_set_points    (XSCurve *curve, xs_int_point_t *points, gint npoints);
gboolean    xs_curve_get_points    (XSCurve *curve, xs_int_point_t **points, gint *npoints);

G_END_DECLS

#endif /* XS_CURVE_H */
