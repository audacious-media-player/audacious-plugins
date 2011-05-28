/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2007 William Pitcock <nenolod -at- sacredspiral.co.uk>
 * Copyright (c) 2011 John Lindgren
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

#include "skins_cfg.h"
#include "ui_dock.h"
#include "ui_playlist.h"
#include "ui_skin.h"
#include "ui_skinned_window.h"

static void ui_skinned_window_class_init(SkinnedWindowClass *klass);
static void ui_skinned_window_init(GtkWidget *widget);
static void ui_skinned_window_show(GtkWidget *widget);
static void ui_skinned_window_hide(GtkWidget *widget);
static GtkWindowClass *parent = NULL;

GType
ui_skinned_window_get_type(void)
{
  static GType window_type = 0;

  if (!window_type)
    {
      static const GTypeInfo window_info =
      {
        sizeof (SkinnedWindowClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) ui_skinned_window_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (SkinnedWindow),
        0,              /* n_preallocs */
        (GInstanceInitFunc) ui_skinned_window_init
      };

      window_type =
        g_type_register_static (GTK_TYPE_WINDOW, "SkinnedWindow",
                                &window_info, 0);
    }

  return window_type;
}

static void
ui_skinned_window_map(GtkWidget *widget)
{
    (* GTK_WIDGET_CLASS (parent)->map) (widget);
    gtk_window_set_keep_above(GTK_WINDOW(widget), config.always_on_top);
}

static gboolean
ui_skinned_window_motion_notify_event(GtkWidget *widget,
                                      GdkEventMotion *event)
{
    if (dock_is_moving(GTK_WINDOW(widget)))
        dock_move_motion(GTK_WINDOW(widget), event);

    return FALSE;
}

static gboolean ui_skinned_window_focus_in(GtkWidget *widget, GdkEventFocus *focus) {
    gboolean val = GTK_WIDGET_CLASS (parent)->focus_in_event (widget, focus);
    gtk_widget_queue_draw(widget);
    return val;
}

static gboolean ui_skinned_window_focus_out(GtkWidget *widget, GdkEventFocus *focus) {
    gboolean val = GTK_WIDGET_CLASS (parent)->focus_out_event (widget, focus);
    gtk_widget_queue_draw(widget);
    return val;
}

static gboolean ui_skinned_window_button_press(GtkWidget *widget, GdkEventButton *event) {
    if (event->button == 1 && event->type == GDK_BUTTON_PRESS &&
        (config.easy_move || config.equalizer_shaded || (event->y / config.scale_factor) < 14)) {
         dock_move_press(get_dock_window_list(), GTK_WINDOW(widget),
                         event, SKINNED_WINDOW(widget)->type == WINDOW_MAIN ? TRUE : FALSE);
    }

    return TRUE;
}

static gboolean ui_skinned_window_button_release(GtkWidget *widget, GdkEventButton *event) {
    if (dock_is_moving(GTK_WINDOW(widget)))
       dock_move_release(GTK_WINDOW(widget));

    return TRUE;
}

static gboolean ui_skinned_window_expose(GtkWidget *widget, GdkEventExpose *event) {
    SkinnedWindow *window = SKINNED_WINDOW(gtk_widget_get_parent(widget));

    GdkPixbuf *obj = NULL;

    gint width = 0, height = 0;

    switch (window->type) {
        case WINDOW_MAIN:
            width = aud_active_skin->properties.mainwin_width;
            height = aud_active_skin->properties.mainwin_height;
            break;
        case WINDOW_EQ:
            width = 275 * (config.scaled ? config.scale_factor : 1);
            height = 116 * (config.scaled ? config.scale_factor : 1) ;
            break;
        case WINDOW_PLAYLIST:
            width = playlistwin_get_width();
            height = config.playlist_height;
            break;
        default:
            return FALSE;
    }
    obj = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);

    gboolean focus = gtk_window_has_toplevel_focus(GTK_WINDOW(window));

    switch (window->type) {
        case WINDOW_MAIN:
            skin_draw_pixbuf(widget, aud_active_skin, obj,SKIN_MAIN, 0, 0, 0, 0, width, height);
            skin_draw_mainwin_titlebar(aud_active_skin, obj, config.player_shaded, focus || !config.dim_titlebar);
            break;
        case WINDOW_EQ:
            skin_draw_pixbuf(widget, aud_active_skin, obj, SKIN_EQMAIN, 0, 0, 0, 0, width, height);
            if (focus || !config.dim_titlebar) {
                if (!config.equalizer_shaded)
                    skin_draw_pixbuf(widget, aud_active_skin, obj, SKIN_EQMAIN, 0, 134, 0, 0, width, 14);
                else
                    skin_draw_pixbuf(widget, aud_active_skin, obj, SKIN_EQ_EX, 0, 0, 0, 0, width, 14);
            } else {
                if (!config.equalizer_shaded)
                    skin_draw_pixbuf(widget, aud_active_skin, obj, SKIN_EQMAIN, 0, 149, 0, 0, width, 14);
                else
                    skin_draw_pixbuf(widget, aud_active_skin, obj, SKIN_EQ_EX, 0, 15, 0, 0, width, 14);
            }
            break;
        case WINDOW_PLAYLIST:
            focus |= !config.dim_titlebar;
            if (config.playlist_shaded) {
                skin_draw_playlistwin_shaded(aud_active_skin, obj, width, focus);
            } else {
                skin_draw_playlistwin_frame(aud_active_skin, obj, width, config.playlist_height, focus);
            }
            break;
    }

    cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (widget));
    pixbuf_draw (cr, obj, 0, 0, config.scaled && window->type != WINDOW_PLAYLIST);
    cairo_destroy (cr);

    g_object_unref (obj);
    return FALSE;
}

static void
ui_skinned_window_class_init(SkinnedWindowClass *klass)
{
    GtkWidgetClass *widget_class;

    widget_class = (GtkWidgetClass*) klass;

    parent = g_type_class_peek_parent(klass);

    widget_class->show = ui_skinned_window_show;
    widget_class->hide = ui_skinned_window_hide;
    widget_class->motion_notify_event = ui_skinned_window_motion_notify_event;
    widget_class->focus_in_event = ui_skinned_window_focus_in;
    widget_class->focus_out_event = ui_skinned_window_focus_out;
    widget_class->button_press_event = ui_skinned_window_button_press;
    widget_class->button_release_event = ui_skinned_window_button_release;
    widget_class->map = ui_skinned_window_map;
}

static void
ui_skinned_window_hide(GtkWidget *widget)
{
    SkinnedWindow *window;

    g_return_if_fail(SKINNED_CHECK_WINDOW(widget));

    window = SKINNED_WINDOW(widget);

    if (window->x != NULL && window->y != NULL)
        gtk_window_get_position(GTK_WINDOW(window), window->x, window->y);
    GTK_WIDGET_CLASS(parent)->hide(widget);
}

static void
ui_skinned_window_show(GtkWidget *widget)
{
    SkinnedWindow *window;

    g_return_if_fail(SKINNED_CHECK_WINDOW(widget));

    window = SKINNED_WINDOW(widget);

    if (window->x != NULL && window->y != NULL)
        gtk_window_move(GTK_WINDOW(window), *(window->x), *(window->y));
    GTK_WIDGET_CLASS(parent)->show(widget);
}

static void
ui_skinned_window_init(GtkWidget *widget)
{
    SkinnedWindow *window;
    window = SKINNED_WINDOW(widget);
    window->x = NULL;
    window->y = NULL;
}

GtkWidget *
ui_skinned_window_new(const gchar *wmclass_name, gint *x, gint *y)
{
    GtkWidget *widget = g_object_new(ui_skinned_window_get_type(), NULL);

    if (wmclass_name)
        gtk_window_set_wmclass(GTK_WINDOW(widget), wmclass_name, "Audacious");

    gtk_widget_add_events(GTK_WIDGET(widget),
                          GDK_FOCUS_CHANGE_MASK | GDK_BUTTON_MOTION_MASK |
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_SCROLL_MASK | GDK_KEY_PRESS_MASK |
                          GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK);

    dock_window_set_decorated (widget);
    gtk_widget_set_app_paintable(GTK_WIDGET(widget), TRUE);

    if (!strcmp(wmclass_name, "player"))
        SKINNED_WINDOW(widget)->type = WINDOW_MAIN;
    if (!strcmp(wmclass_name, "equalizer"))
        SKINNED_WINDOW(widget)->type = WINDOW_EQ;
    if (!strcmp(wmclass_name, "playlist"))
        SKINNED_WINDOW(widget)->type = WINDOW_PLAYLIST;

    SKINNED_WINDOW(widget)->x = x;
    SKINNED_WINDOW(widget)->y = y;
    SKINNED_WINDOW(widget)->normal = gtk_fixed_new();
    SKINNED_WINDOW(widget)->shaded = gtk_fixed_new();
    g_object_ref(SKINNED_WINDOW(widget)->normal);
    g_object_ref(SKINNED_WINDOW(widget)->shaded);

    gtk_container_add(GTK_CONTAINER(widget), GTK_WIDGET(SKINNED_WINDOW(widget)->normal));

    g_signal_connect(SKINNED_WINDOW(widget)->normal, "expose-event", G_CALLBACK(ui_skinned_window_expose), NULL);
    g_signal_connect(SKINNED_WINDOW(widget)->shaded, "expose-event", G_CALLBACK(ui_skinned_window_expose), NULL);

    return widget;
}

void ui_skinned_window_draw_all(GtkWidget *widget) {
    gtk_widget_queue_draw (widget);
}

void ui_skinned_window_set_shade (GtkWidget * widget, gboolean shaded)
{
    SkinnedWindow * skinned = (SkinnedWindow *) widget;
    GtkWidget * old, * new;

    if (shaded)
    {
        old = skinned->normal;
        new = skinned->shaded;
    }
    else
    {
        old = skinned->shaded;
        new = skinned->normal;
    }

    if (gtk_widget_get_parent (old) == NULL)
        return;

    gtk_container_remove ((GtkContainer *) skinned, old);
    gtk_container_add ((GtkContainer *) skinned, new);
}
