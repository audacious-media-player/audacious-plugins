/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
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

#include <libaudcore/audstrings.h>

#include "skins_cfg.h"
#include "ui_main.h"
#include "ui_skinned_textbox.h"
#include "util.h"

#define UI_SKINNED_TEXTBOX_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ui_skinned_textbox_get_type(), UiSkinnedTextboxPrivate))
typedef struct _UiSkinnedTextboxPrivate UiSkinnedTextboxPrivate;

#define TEXTBOX_SCROLL_SMOOTH_TIMEOUT  30
#define TEXTBOX_SCROLL_WAIT            80

enum {
    CLICKED,
    DOUBLE_CLICKED,
    RIGHT_CLICKED,
    DOUBLED,
    LAST_SIGNAL
};

struct _UiSkinnedTextboxPrivate {
    SkinPixmapId     skin_index;
    gboolean         scaled;
    gboolean         scroll_back;
    gint             nominal_y, nominal_height;
    gint             scroll_timeout;
    gint crop;
    PangoFontDescription *font;
    gchar            *fontname;
    gchar            *pixbuf_text;
    gint             skin_id;
    gint             drag_x, drag_off, offset;
    gboolean         is_scrollable, is_dragging;
    gint             pixbuf_width;
    cairo_surface_t * surface;
    gboolean         scroll_allowed, scroll_enabled;
    gint             scroll_dummy;
    gint             move_x, move_y;
};

static void ui_skinned_textbox_class_init         (UiSkinnedTextboxClass *klass);
static void ui_skinned_textbox_init               (UiSkinnedTextbox *textbox);
static void ui_skinned_textbox_destroy            (GtkObject *object);
static void ui_skinned_textbox_realize            (GtkWidget *widget);
static void ui_skinned_textbox_unrealize          (GtkWidget *widget);
static void ui_skinned_textbox_map                (GtkWidget *widget);
static void ui_skinned_textbox_unmap              (GtkWidget *widget);
static void ui_skinned_textbox_size_request       (GtkWidget *widget, GtkRequisition *requisition);
static void ui_skinned_textbox_size_allocate      (GtkWidget *widget, GtkAllocation *allocation);
static gboolean ui_skinned_textbox_expose         (GtkWidget *widget, GdkEventExpose *event);
static gboolean ui_skinned_textbox_button_press   (GtkWidget *widget, GdkEventButton *event);
static gboolean ui_skinned_textbox_button_release (GtkWidget *widget, GdkEventButton *event);
static gboolean ui_skinned_textbox_motion_notify  (GtkWidget *widget, GdkEventMotion *event);
static void ui_skinned_textbox_toggle_scaled      (UiSkinnedTextbox *textbox);
static gboolean textbox_scroll                    (gpointer data);
static void textbox_generate_pixmap               (UiSkinnedTextbox *textbox);
static void textbox_handle_special_char           (gchar *c, gint * x, gint * y);

static GtkWidgetClass *parent_class = NULL;
static guint textbox_signals[LAST_SIGNAL] = { 0 };

GType ui_skinned_textbox_get_type() {
    static GType textbox_type = 0;
    if (!textbox_type) {
        static const GTypeInfo textbox_info = {
            sizeof (UiSkinnedTextboxClass),
            NULL,
            NULL,
            (GClassInitFunc) ui_skinned_textbox_class_init,
            NULL,
            NULL,
            sizeof (UiSkinnedTextbox),
            0,
            (GInstanceInitFunc) ui_skinned_textbox_init,
        };
        textbox_type = g_type_register_static (GTK_TYPE_WIDGET, "UiSkinnedTextbox", &textbox_info, 0);
    }

    return textbox_type;
}

static void ui_skinned_textbox_class_init(UiSkinnedTextboxClass *klass) {
    GObjectClass *gobject_class;
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(klass);
    object_class = (GtkObjectClass*) klass;
    widget_class = (GtkWidgetClass*) klass;
    parent_class = g_type_class_peek_parent(klass);

    object_class->destroy = ui_skinned_textbox_destroy;

    widget_class->realize = ui_skinned_textbox_realize;
    widget_class->unrealize = ui_skinned_textbox_unrealize;
    widget_class->map = ui_skinned_textbox_map;
    widget_class->unmap = ui_skinned_textbox_unmap;
    widget_class->expose_event = ui_skinned_textbox_expose;
    widget_class->size_request = ui_skinned_textbox_size_request;
    widget_class->size_allocate = ui_skinned_textbox_size_allocate;
    widget_class->button_press_event = ui_skinned_textbox_button_press;
    widget_class->button_release_event = ui_skinned_textbox_button_release;
    widget_class->motion_notify_event = ui_skinned_textbox_motion_notify;

    klass->clicked = NULL;
    klass->double_clicked = NULL;
    klass->right_clicked = NULL;
    klass->scaled = ui_skinned_textbox_toggle_scaled;

    textbox_signals[CLICKED] =
        g_signal_new ("clicked", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedTextboxClass, clicked), NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    textbox_signals[DOUBLE_CLICKED] =
        g_signal_new ("double-clicked", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedTextboxClass, double_clicked), NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    textbox_signals[RIGHT_CLICKED] =
        g_signal_new ("right-clicked", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedTextboxClass, right_clicked), NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

    textbox_signals[DOUBLED] =
        g_signal_new ("toggle-scaled", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedTextboxClass, scaled), NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_type_class_add_private (gobject_class, sizeof (UiSkinnedTextboxPrivate));
}

static void ui_skinned_textbox_init(UiSkinnedTextbox *textbox) {
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);
    priv->move_x = 0;
    priv->move_y = 0;

    textbox->event_window = NULL;
    GTK_WIDGET_SET_FLAGS (textbox, GTK_NO_WINDOW);
}

GtkWidget* ui_skinned_textbox_new(GtkWidget *fixed, gint x, gint y, gint w, gboolean allow_scroll, SkinPixmapId si) {
    UiSkinnedTextbox *textbox = g_object_new (ui_skinned_textbox_get_type (), NULL);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    textbox->height = aud_active_skin->properties.textbox_bitmap_font_height;
    textbox->x = x;
    textbox->y = y;
    textbox->text = g_strdup("");
    textbox->width = w;
    priv->scroll_allowed = allow_scroll;
    priv->scroll_enabled = TRUE;
    priv->skin_index = si;
    priv->nominal_y = y;
    priv->nominal_height = textbox->height;
    priv->scroll_timeout = 0;
    priv->scroll_dummy = 0;

    priv->scaled = FALSE;

    gtk_fixed_put(GTK_FIXED(fixed), GTK_WIDGET(textbox), textbox->x, textbox->y);

    return GTK_WIDGET(textbox);
}

static void ui_skinned_textbox_destroy(GtkObject *object) {
    UiSkinnedTextbox *textbox;
    UiSkinnedTextboxPrivate *priv;

    g_return_if_fail (object != NULL);
    g_return_if_fail (UI_SKINNED_IS_TEXTBOX (object));

    textbox = UI_SKINNED_TEXTBOX (object);
    priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(object);

    if (priv->scroll_timeout) {
        g_source_remove(priv->scroll_timeout);
        priv->scroll_timeout = 0;
    }

    g_free (textbox->text);
    textbox->text = NULL;
    g_free (priv->pixbuf_text);
    priv->pixbuf_text = NULL;
    g_free (priv->fontname);
    priv->fontname = NULL;

    if (priv->surface)
    {
        cairo_surface_destroy (priv->surface);
        priv->surface = NULL;
    }

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void ui_skinned_textbox_realize(GtkWidget *widget) {
    UiSkinnedTextbox *textbox;
    GdkWindowAttr attributes;
    gint attributes_mask;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (UI_SKINNED_IS_TEXTBOX(widget));

    if (GTK_WIDGET_CLASS (parent_class)->realize)
        (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

    textbox = UI_SKINNED_TEXTBOX(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_ONLY;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget);
    attributes.event_mask |= GDK_BUTTON_PRESS_MASK |
                             GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
                             GDK_POINTER_MOTION_HINT_MASK;

    attributes_mask = GDK_WA_X | GDK_WA_Y;
    textbox->event_window = gdk_window_new (widget->window, &attributes, attributes_mask);

    widget->style = gtk_style_attach(widget->style, widget->window);

    gdk_window_set_user_data(textbox->event_window, widget);
}

static void ui_skinned_textbox_unrealize(GtkWidget *widget) {
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX(widget);

    if ( textbox->event_window != NULL )
    {
      gdk_window_set_user_data( textbox->event_window , NULL );
      gdk_window_destroy( textbox->event_window );
      textbox->event_window = NULL;
    }

    if (GTK_WIDGET_CLASS (parent_class)->unrealize)
        (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void ui_skinned_textbox_map (GtkWidget *widget)
{
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX (widget);

    if (textbox->event_window != NULL)
      gdk_window_show (textbox->event_window);

    if (GTK_WIDGET_CLASS (parent_class)->map)
      (* GTK_WIDGET_CLASS (parent_class)->map) (widget);
}

static void ui_skinned_textbox_unmap (GtkWidget *widget)
{
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX (widget);

    if (textbox->event_window != NULL)
      gdk_window_hide (textbox->event_window);

    if (GTK_WIDGET_CLASS (parent_class)->unmap)
      (* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

static void ui_skinned_textbox_size_request(GtkWidget *widget, GtkRequisition *requisition) {
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX(widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    requisition->width = textbox->width*(priv->scaled ? config.scale_factor : 1);
    requisition->height = textbox->height*(priv->scaled ?  config.scale_factor : 1 );
}

static void ui_skinned_textbox_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX (widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    widget->allocation = *allocation;
    widget->allocation.x *= (priv->scaled ? config.scale_factor : 1);
    widget->allocation.y *= (priv->scaled ? config.scale_factor : 1);
    if (GTK_WIDGET_REALIZED (widget))
        if (textbox->event_window)
            gdk_window_move_resize(textbox->event_window, widget->allocation.x, widget->allocation.y, allocation->width, allocation->height);

    if (textbox->x + priv->move_x - widget->allocation.x/(priv->scaled ? config.scale_factor : 1) <3);
        priv->move_x = 0;
    if (textbox->y + priv->move_y - widget->allocation.y/(priv->scaled ? config.scale_factor : 1) <3);
        priv->move_y = 0;
    textbox->x = widget->allocation.x/(priv->scaled ? config.scale_factor : 1);
    textbox->y = widget->allocation.y/(priv->scaled ? config.scale_factor : 1);

    if (textbox->width - (guint) (widget->allocation.width / (priv->scaled ? config.scale_factor : 1)) > 2) {
            textbox->width = (guint) (widget->allocation.width / (priv->scaled ? config.scale_factor : 1));
            if (priv->pixbuf_text) g_free(priv->pixbuf_text);
            priv->pixbuf_text = NULL;
            priv->offset = 0;
            gtk_widget_set_size_request(widget, textbox->width, textbox->height);

            if (gtk_widget_is_drawable (widget))
                ui_skinned_textbox_expose (widget, 0);
    }
}

static gboolean ui_skinned_textbox_expose (GtkWidget * widget, GdkEventExpose *
 event)
{
    UiSkinnedTextbox * textbox = (UiSkinnedTextbox *) widget;
    UiSkinnedTextboxPrivate * priv = UI_SKINNED_TEXTBOX_GET_PRIVATE (textbox);

    if ((textbox->text && (! priv->pixbuf_text || strcmp (textbox->text,
     priv->pixbuf_text))) || priv->skin_id != skin_get_id ())
    {
        priv->skin_id = skin_get_id ();
        textbox_generate_pixmap (textbox);
    }

    if (! priv->surface)
        return TRUE;

    cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (widget));

    if (! priv->is_scrollable)
    {
        cairo_set_source_surface (cr, priv->surface, widget->allocation.x,
         widget->allocation.y);
        cairo_paint (cr);
    }
    else
    {
        cairo_surface_t * clip = cairo_surface_create_for_rectangle
         (cairo_get_target (cr), widget->allocation.x, widget->allocation.y,
         textbox->width, textbox->height);
        cairo_t * clipped = cairo_create (clip);

        cairo_set_source_surface (clipped, priv->surface, -priv->offset, 0);
        cairo_paint (clipped);

        if (-priv->offset + priv->pixbuf_width < textbox->width)
        {
            cairo_set_source_surface (clipped, priv->surface, -priv->offset +
             priv->pixbuf_width, 0);
            cairo_paint (clipped);
        }

        cairo_destroy (clipped);
        cairo_surface_destroy (clip);
    }

    cairo_destroy (cr);
    return TRUE;
}

static gboolean ui_skinned_textbox_button_press(GtkWidget *widget, GdkEventButton *event) {
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (UI_SKINNED_IS_TEXTBOX (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX (widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    if (event->type == GDK_BUTTON_PRESS) {
        textbox = UI_SKINNED_TEXTBOX(widget);
        if (event->button == 3 && !g_signal_has_handler_pending(widget, textbox_signals[RIGHT_CLICKED], 0, TRUE))
            return FALSE;
        else if (event->button == 1) {
            if (priv->scroll_allowed) {
                if ((priv->pixbuf_width > textbox->width) && priv->is_scrollable) {
                    priv->is_dragging = TRUE;
                    priv->drag_off = priv->offset;
                    priv->drag_x = event->x;
                }
            } else
                g_signal_emit(widget, textbox_signals[CLICKED], 0);

        } else if (event->button == 3) {
            g_signal_emit(widget, textbox_signals[RIGHT_CLICKED], 0, event);
        } else
            priv->is_dragging = FALSE;
    } else if (event->type == GDK_2BUTTON_PRESS) {
        if (event->button == 1) {
           if (g_signal_has_handler_pending(widget, textbox_signals[DOUBLE_CLICKED], 0, TRUE))
               g_signal_emit(widget, textbox_signals[DOUBLE_CLICKED], 0);
           else
               return FALSE;
        }
    }

    return TRUE;
}

static gboolean ui_skinned_textbox_button_release(GtkWidget *widget, GdkEventButton *event) {
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(widget);

    if (event->button == 1) {
        priv->is_dragging = FALSE;
    }

    return TRUE;
}

static gboolean ui_skinned_textbox_motion_notify(GtkWidget *widget, GdkEventMotion *event) {
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (UI_SKINNED_IS_TEXTBOX (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX(widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(widget);

    if (priv->is_dragging) {
        if (priv->scroll_allowed &&
            priv->pixbuf_width > textbox->width) {
            priv->offset = priv->drag_off - (event->x - priv->drag_x);

            while (priv->offset < 0)
                priv->offset = 0;

            while (priv->offset > (priv->pixbuf_width - textbox->width))
                priv->offset = (priv->pixbuf_width - textbox->width);

            if (gtk_widget_is_drawable (widget))
                ui_skinned_textbox_expose (widget, 0);
        }
    }

  return TRUE;
}

static void ui_skinned_textbox_toggle_scaled(UiSkinnedTextbox *textbox) {
    GtkWidget *widget = GTK_WIDGET (textbox);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    priv->scaled = !priv->scaled;

    gtk_widget_set_size_request(widget, textbox->width*(priv->scaled ? config.scale_factor : 1 ),
    textbox->height*(priv->scaled ? config.scale_factor : 1 ));

    if (gtk_widget_is_drawable (widget))
        ui_skinned_textbox_expose (widget, 0);
}

void ui_skinned_textbox_set_xfont(GtkWidget *widget, gboolean use_xfont, const gchar * fontname) {
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX (widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    gint ascent, descent;

    g_return_if_fail(textbox != NULL);
    gtk_widget_queue_resize (widget);

    if (priv->font) {
        pango_font_description_free(priv->font);
        priv->font = NULL;
    }

    textbox->y = priv->nominal_y;
    textbox->height = priv->nominal_height;

    /* Make sure the pixmap is regenerated */
    if (priv->pixbuf_text) {
        g_free(priv->pixbuf_text);
        priv->pixbuf_text = NULL;
    }

    if (!use_xfont || strlen(fontname) == 0)
        return;

    priv->font = pango_font_description_from_string(fontname);
    g_free (priv->fontname);
    priv->fontname = g_strdup(fontname);

    text_get_extents(fontname,
                     "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz ",
                     NULL, NULL, &ascent, &descent);

    if (priv->font == NULL)
        return;

    textbox->height = ascent - descent;
    priv->crop = textbox->height / 5;
    textbox->height -= priv->crop;
}

void ui_skinned_textbox_set_text(GtkWidget *widget, const gchar *text) {
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX (widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    if (!strcmp(textbox->text, text))
         return;
    if (textbox->text)
        g_free(textbox->text);

    textbox->text = str_to_utf8(text);
    priv->scroll_back = FALSE;

    if (gtk_widget_is_drawable (widget))
        ui_skinned_textbox_expose (widget, 0);
}

static void textbox_generate_vector (UiSkinnedTextbox * textbox, const gchar *
 text)
{
    g_return_if_fail (textbox && text);
    UiSkinnedTextboxPrivate * priv = UI_SKINNED_TEXTBOX_GET_PRIVATE (textbox);
    g_return_if_fail (! priv->surface);

    PangoLayout * layout = gtk_widget_create_pango_layout (mainwin, text);
    pango_layout_set_font_description (layout, priv->font);

    PangoRectangle rect;
    pango_layout_get_pixel_extents (layout, NULL, & rect);

    priv->pixbuf_width = MAX (rect.width, textbox->width);
    priv->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
     priv->pixbuf_width, textbox->height);

    cairo_t * cr = cairo_create (priv->surface);

    GdkColor * bg = skin_get_color (aud_active_skin, SKIN_TEXTBG);
    cairo_set_source_rgba (cr, ((gfloat) bg->red) / 65535, ((gfloat) bg->green)
     / 65535, ((gfloat) bg->blue) / 65535, 1);
    cairo_paint (cr);

    GdkColor * fg = skin_get_color (aud_active_skin, SKIN_TEXTFG);
    cairo_set_source_rgba (cr, ((gfloat) fg->red) / 65535, ((gfloat) fg->green)
     / 65535, ((gfloat) fg->blue) / 65535, 1);
    cairo_move_to (cr, 0, -priv->crop);
    pango_cairo_show_layout (cr, layout);

    cairo_destroy (cr);
    g_object_unref (layout);
}

static void textbox_generate_bitmap (UiSkinnedTextbox * textbox,
 const gchar * text)
{
    g_return_if_fail (textbox && text);
    UiSkinnedTextboxPrivate * priv = UI_SKINNED_TEXTBOX_GET_PRIVATE (textbox);
    g_return_if_fail (! priv->surface);

    gint cw = aud_active_skin->properties.textbox_bitmap_font_width;
    gint ch = aud_active_skin->properties.textbox_bitmap_font_height;

    gchar * upper = g_utf8_strup (text, -1);
    gint len = g_utf8_strlen (upper, -1);

    priv->pixbuf_width = MAX (cw * len, textbox->width);
    priv->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
     priv->pixbuf_width, textbox->height);

    cairo_t * cr = cairo_create (priv->surface);

    GdkColor * bg = skin_get_color (aud_active_skin, SKIN_TEXTBG);
    cairo_set_source_rgba (cr, ((gfloat) bg->red) / 65535, ((gfloat) bg->green)
     / 65535, ((gfloat) bg->blue) / 65535, 1);
    cairo_paint (cr);

    if (cw * len > 0 && ch > 0)
    {
        GdkPixbuf * p = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, cw * len,
         ch);

        gchar * c = upper;
        for (gint i = 0; i < len; i ++)
        {
            gint x = 0, y = 0;

            if (* c >= 'A' && * c <= 'Z')
                x = cw * (* c - 'A');
            else if (* c >= '0' && * c <= '9')
            {
                x = cw * (* c - '0');
                y = ch;
            }
            else
                textbox_handle_special_char (c, & x, & y);

            skin_draw_pixbuf (NULL, aud_active_skin, p, priv->skin_index, x, y,
             cw * i, 0, cw, ch);

            c = g_utf8_next_char (c);
        }

        gdk_cairo_set_source_pixbuf (cr, p, 0, 0);
        cairo_paint (cr);

        g_object_unref (p);
    }

    cairo_destroy (cr);
    g_free (upper);
}

static gboolean textbox_scroll(gpointer data) {
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX(data);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    if (!priv->is_dragging) {
        if (priv->scroll_dummy < TEXTBOX_SCROLL_WAIT)
            priv->scroll_dummy++;
        else {
            if(config.twoway_scroll) {
                if (priv->scroll_back)
                    priv->offset -= 1;
                else
                    priv->offset += 1;

                if (priv->offset >= (priv->pixbuf_width - textbox->width)) {
                    priv->scroll_back = TRUE;
                    priv->scroll_dummy = 0;
                    priv->offset = priv->pixbuf_width - textbox->width;
                }
                if (priv->offset <= 0) {
                    priv->scroll_back = FALSE;
                    priv->scroll_dummy = 0;
                    priv->offset = 0;
                }
            }
            else { // oneway scroll
                priv->scroll_back = FALSE;
                priv->offset ++;
                if (priv->offset >= priv->pixbuf_width)
                    priv->offset = 0;
            }

            if (gtk_widget_is_drawable (data))
                ui_skinned_textbox_expose (data, 0);
        }
    }
    return TRUE;
}

static void textbox_generate_pixmap (UiSkinnedTextbox * textbox)
{
    UiSkinnedTextboxPrivate * priv = UI_SKINNED_TEXTBOX_GET_PRIVATE (textbox);

    g_free (priv->pixbuf_text);
    priv->pixbuf_text = g_strdup (textbox->text);
    priv->is_scrollable = FALSE;
    priv->offset = 0;

    if (priv->surface)
    {
        cairo_surface_destroy (priv->surface);
        priv->surface = NULL;
    }

    if (priv->font)
        textbox_generate_vector (textbox, priv->pixbuf_text);
    else
        textbox_generate_bitmap (textbox, priv->pixbuf_text);

    if (priv->pixbuf_width > textbox->width)
    {
        priv->is_scrollable = TRUE;

        if (! config.twoway_scroll)
        {
            if (priv->surface)
            {
                cairo_surface_destroy (priv->surface);
                priv->surface = NULL;
            }

            gchar * temp = g_strdup_printf ("%s *** ", priv->pixbuf_text);

            if (priv->font)
                textbox_generate_vector (textbox, temp);
            else
                textbox_generate_bitmap (textbox, temp);

            g_free (temp);
        }
    }

    if (priv->is_scrollable)
    {
        if (priv->scroll_enabled && ! priv->scroll_timeout)
            priv->scroll_timeout = g_timeout_add (TEXTBOX_SCROLL_SMOOTH_TIMEOUT,
             textbox_scroll, textbox);
    }
    else
    {
        if (priv->scroll_timeout)
        {
            g_source_remove (priv->scroll_timeout);
            priv->scroll_timeout = 0;
        }
    }
}

void ui_skinned_textbox_set_scroll(GtkWidget *widget, gboolean scroll) {
    g_return_if_fail(widget != NULL);
    g_return_if_fail(UI_SKINNED_IS_TEXTBOX(widget));

    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX(widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    priv->scroll_enabled = scroll;
    if (priv->scroll_enabled && priv->is_scrollable && priv->scroll_allowed) {
        gint tag;
        tag = TEXTBOX_SCROLL_SMOOTH_TIMEOUT;
        if (priv->scroll_timeout) {
            g_source_remove(priv->scroll_timeout);
            priv->scroll_timeout = 0;
        }
        priv->scroll_timeout = g_timeout_add(tag, textbox_scroll, textbox);

    } else {

        if (priv->scroll_timeout) {
            g_source_remove(priv->scroll_timeout);
            priv->scroll_timeout = 0;
        }

        priv->offset = 0;

        if (gtk_widget_is_drawable (widget))
            ui_skinned_textbox_expose (widget, 0);
    }
}

static void textbox_handle_special_char(gchar *c, gint * x, gint * y) {
    gint tx, ty;

    switch (*c) {
    case '"':
        tx = 26;
        ty = 0;
        break;
    case '\r':
        tx = 10;
        ty = 1;
        break;
    case ':':
    case ';':
        tx = 12;
        ty = 1;
        break;
    case '(':
        tx = 13;
        ty = 1;
        break;
    case ')':
        tx = 14;
        ty = 1;
        break;
    case '-':
        tx = 15;
        ty = 1;
        break;
    case '`':
    case '\'':
        tx = 16;
        ty = 1;
        break;
    case '!':
        tx = 17;
        ty = 1;
        break;
    case '_':
        tx = 18;
        ty = 1;
        break;
    case '+':
        tx = 19;
        ty = 1;
        break;
    case '\\':
        tx = 20;
        ty = 1;
        break;
    case '/':
        tx = 21;
        ty = 1;
        break;
    case '[':
        tx = 22;
        ty = 1;
        break;
    case ']':
        tx = 23;
        ty = 1;
        break;
    case '^':
        tx = 24;
        ty = 1;
        break;
    case '&':
        tx = 25;
        ty = 1;
        break;
    case '%':
        tx = 26;
        ty = 1;
        break;
    case '.':
    case ',':
        tx = 27;
        ty = 1;
        break;
    case '=':
        tx = 28;
        ty = 1;
        break;
    case '$':
        tx = 29;
        ty = 1;
        break;
    case '#':
        tx = 30;
        ty = 1;
        break;
    case '?':
        tx = 3;
        ty = 2;
        break;
    case '*':
        tx = 4;
        ty = 2;
        break;
    default:
        tx = 29;
        ty = 0;
        break;
    }

    const gchar *change[] = {"Ą", "A", "Ę", "E", "Ć", "C", "Ł", "L", "Ó", "O", "Ś", "S", "Ż", "Z", "Ź", "Z",
                             "Ń", "N", "Ü", "U", NULL};
    int i;
    for (i = 0; change[i]; i+=2) {
         if (!strncmp(c, change[i], strlen(change[i]))) {
             tx = (*change[i+1] - 'A');
             break;
         }
    }

    /* those are commonly included into skins */
    if (!strncmp(c, "Å", strlen("Å"))) {
        tx = 0;
        ty = 2;
    } else if (!strncmp(c, "Ö", strlen("Ö"))) {
        tx = 1;
        ty = 2;
    } else if (!strncmp(c, "Ä", strlen("Ä"))) {
        tx = 2;
        ty = 2;
    }

    *x = tx * aud_active_skin->properties.textbox_bitmap_font_width;
    *y = ty * aud_active_skin->properties.textbox_bitmap_font_height;
}

void ui_skinned_textbox_move_relative(GtkWidget *widget, gint x, gint y) {
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX(widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(widget);
    priv->move_x += x;
    priv->move_y += y;
    gtk_fixed_move(GTK_FIXED(gtk_widget_get_parent(widget)), widget,
                   textbox->x+priv->move_x, textbox->y+priv->move_y);
}
