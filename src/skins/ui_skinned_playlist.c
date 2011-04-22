/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2008 William Pitcock
 * Copyright 2009 John Lindgren
 *
 * Based on:
 * BMP - Cross-platform multimedia player
 * Copyright (C) 2003-2004  BMP development team.
 *
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

/*
 *  A note about Pango and some funky spacey fonts: Weirdly baselined
 *  fonts, or fonts with weird ascents or descents _will_ display a
 *  little bit weird in the playlist widget, but the display engine
 *  won't make it look too bad, just a little deranged.  I honestly
 *  don't think it's worth fixing (around...), it doesn't have to be
 *  perfectly fitting, just the general look has to be ok, which it
 *  IMHO is.
 *
 *  A second note: The numbers aren't perfectly aligned, but in the
 *  end it looks better when using a single Pango layout for each
 *  number.
 */

#include <gdk/gdkkeysyms.h>

#include "ui_skinned_playlist.h"
#include "ui_skinned_playlist_slider.h"

#include "debug.h"
#include "ui_playlist.h"
#include "ui_manager.h"
#include "ui_skin.h"
#include "util.h"
#include "skins_cfg.h"

#include <audacious/audconfig.h>
#include <audacious/drct.h>
#include <audacious/playlist.h>
#include <libaudgui/libaudgui.h>

#define UI_SKINNED_PLAYLIST_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ui_skinned_playlist_get_type(), UiSkinnedPlaylistPrivate))
typedef struct _UiSkinnedPlaylistPrivate UiSkinnedPlaylistPrivate;

enum {DRAG_SELECT = 1, DRAG_MOVE};

struct _UiSkinnedPlaylistPrivate {
    GtkWidget * slider;
    PangoFontDescription * font;
    gint width, height, resize_width, resize_height, ascent, descent,
     letter_width, digit_width, row_height, offset, rows, first, focused,
     scroll, scroll_source, hover;
    gboolean slanted, drag;
};

static void ui_skinned_playlist_class_init         (UiSkinnedPlaylistClass *klass);
static void ui_skinned_playlist_init               (UiSkinnedPlaylist *playlist);
static void ui_skinned_playlist_destroy            (GtkObject *object);
static void ui_skinned_playlist_realize            (GtkWidget *widget);
static void ui_skinned_playlist_size_request       (GtkWidget *widget, GtkRequisition *requisition);
static void ui_skinned_playlist_size_allocate      (GtkWidget *widget, GtkAllocation *allocation);
static gboolean ui_skinned_playlist_expose         (GtkWidget *widget, GdkEventExpose *event);
static gboolean ui_skinned_playlist_button_press   (GtkWidget *widget, GdkEventButton *event);
static gboolean ui_skinned_playlist_button_release (GtkWidget *widget, GdkEventButton *event);
static gboolean ui_skinned_playlist_motion_notify  (GtkWidget *widget, GdkEventMotion *event);
static gboolean ui_skinned_playlist_leave_notify   (GtkWidget *widget, GdkEventCrossing *event);
static gboolean ui_skinned_playlist_popup_show     (gpointer data);
static void ui_skinned_playlist_popup_hide         (GtkWidget *widget);
static void ui_skinned_playlist_popup_timer_start  (GtkWidget *widget);
static void ui_skinned_playlist_popup_timer_stop   (GtkWidget *widget);

static GtkWidgetClass *parent_class = NULL;

GType ui_skinned_playlist_get_type() {
    static GType playlist_type = 0;
    if (!playlist_type) {
        static const GTypeInfo playlist_info = {
            sizeof (UiSkinnedPlaylistClass),
            NULL,
            NULL,
            (GClassInitFunc) ui_skinned_playlist_class_init,
            NULL,
            NULL,
            sizeof (UiSkinnedPlaylist),
            0,
            (GInstanceInitFunc) ui_skinned_playlist_init,
        };
        playlist_type = g_type_register_static (GTK_TYPE_WIDGET, "UiSkinnedPlaylist", &playlist_info, 0);
    }

    return playlist_type;
}

static void ui_skinned_playlist_class_init(UiSkinnedPlaylistClass *klass) {
    GObjectClass *gobject_class;
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(klass);
    object_class = (GtkObjectClass*) klass;
    widget_class = (GtkWidgetClass*) klass;
    parent_class = g_type_class_peek_parent(klass);

    object_class->destroy = ui_skinned_playlist_destroy;

    widget_class->realize = ui_skinned_playlist_realize;
    widget_class->expose_event = ui_skinned_playlist_expose;
    widget_class->size_request = ui_skinned_playlist_size_request;
    widget_class->size_allocate = ui_skinned_playlist_size_allocate;
    widget_class->button_press_event = ui_skinned_playlist_button_press;
    widget_class->button_release_event = ui_skinned_playlist_button_release;
    widget_class->motion_notify_event = ui_skinned_playlist_motion_notify;
    widget_class->leave_notify_event = ui_skinned_playlist_leave_notify;

    g_type_class_add_private (gobject_class, sizeof (UiSkinnedPlaylistPrivate));
}

static void ui_skinned_playlist_init(UiSkinnedPlaylist *playlist) {
    UiSkinnedPlaylistPrivate *priv = UI_SKINNED_PLAYLIST_GET_PRIVATE(playlist);
    priv->resize_width = 0;
    priv->resize_height = 0;
    priv->rows = 0;
    priv->first = 0;
    priv->focused = -1;
    priv->drag = FALSE;
    priv->scroll = 0;
    priv->hover = -1;

    g_object_set_data(G_OBJECT(playlist), "timer_id", GINT_TO_POINTER(0));
    g_object_set_data(G_OBJECT(playlist), "timer_active", GINT_TO_POINTER(0));

    g_object_set_data(G_OBJECT(playlist), "popup_active", GINT_TO_POINTER(0));
    g_object_set_data(G_OBJECT(playlist), "popup_position", GINT_TO_POINTER(-1));
}

GtkWidget * ui_skinned_playlist_new (GtkWidget * fixed, gint x, gint y, gint
 width, gint height, const gchar * font)
{
    UiSkinnedPlaylist *hs = g_object_new (ui_skinned_playlist_get_type (), NULL);
    UiSkinnedPlaylistPrivate *priv = UI_SKINNED_PLAYLIST_GET_PRIVATE(hs);

    priv->width = width;
    priv->height = height;
    priv->slider = NULL;

    ui_skinned_playlist_set_font ((GtkWidget *) hs, font);

    gtk_fixed_put ((GtkFixed *) fixed, (GtkWidget *) hs, x, y);
    gtk_widget_set_double_buffered(GTK_WIDGET(hs), TRUE);

    return GTK_WIDGET(hs);
}

void ui_skinned_playlist_set_slider (GtkWidget * list, GtkWidget * slider)
{
    UI_SKINNED_PLAYLIST_GET_PRIVATE ((UiSkinnedPlaylist *) list)->slider =
     slider;
}

static void cancel_all (GtkWidget * widget, UiSkinnedPlaylistPrivate * private)
{
    private->drag = FALSE;

    if (private->scroll)
    {
        private->scroll = 0;
        g_source_remove (private->scroll_source);
    }

    if (private->hover != -1)
    {
        private->hover = -1;
        gtk_widget_queue_draw (widget);
    }

    ui_skinned_playlist_popup_hide (widget);
    ui_skinned_playlist_popup_timer_stop (widget);
}

static void ui_skinned_playlist_destroy(GtkObject *object) {
    g_return_if_fail (object != NULL);
    g_return_if_fail (UI_SKINNED_IS_PLAYLIST (object));

    cancel_all ((GtkWidget *) object, UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) object));

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void ui_skinned_playlist_realize(GtkWidget *widget) {
    GdkWindowAttr attributes;
    gint attributes_mask;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (UI_SKINNED_IS_PLAYLIST(widget));

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget);
    attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
                             GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(widget->parent->window, &attributes, attributes_mask);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gdk_window_set_user_data(widget->window, widget);
}

static void ui_skinned_playlist_size_request(GtkWidget *widget, GtkRequisition *requisition) {
    UiSkinnedPlaylistPrivate *priv = UI_SKINNED_PLAYLIST_GET_PRIVATE(widget);

    requisition->width = priv->width;
    requisition->height = priv->height;
}

static void calc_layout (UiSkinnedPlaylistPrivate * private)
{
    private->row_height = private->ascent - private->descent;
    private->rows = private->height / private->row_height;

    if (private->rows && active_title != NULL)
    {
        private->offset = private->row_height;
        private->rows --;
    }
    else
        private->offset = 0;

    if (private->first + private->rows > active_length)
        private->first = active_length - private->rows;
    if (private->first < 0)
        private->first = 0;
}

static void scroll_to (UiSkinnedPlaylistPrivate * private, int position)
{
    if (position < private->first || position >= private->first + private->rows)
        private->first = position - private->rows / 2;

    calc_layout (private);
}

static void ui_skinned_playlist_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
    UiSkinnedPlaylist *playlist = UI_SKINNED_PLAYLIST (widget);
    UiSkinnedPlaylistPrivate *priv = UI_SKINNED_PLAYLIST_GET_PRIVATE(playlist);

    widget->allocation = *allocation;
    if (GTK_WIDGET_REALIZED (widget))
        gdk_window_move_resize(widget->window, widget->allocation.x, widget->allocation.y, allocation->width, allocation->height);

    if (priv->height != widget->allocation.height || priv->width != widget->allocation.width) {
        priv->width = priv->width + priv->resize_width;
        priv->height = priv->height + priv->resize_height;
        priv->resize_width = 0;
        priv->resize_height = 0;
    }

    calc_layout (priv);
    gtk_widget_queue_draw (widget);

    if (priv->slider != NULL)
        ui_skinned_playlist_slider_update (priv->slider);
}

static void
playlist_list_draw_string(cairo_t *cr, UiSkinnedPlaylist *pl,
                          PangoFontDescription * font,
                          gint line,
                          gint width,
                          const gchar * text,
                          guint ppos)
{
    UiSkinnedPlaylistPrivate * private;
    gint plist_length_int, padding;
    PangoLayout *layout;

    private = UI_SKINNED_PLAYLIST_GET_PRIVATE (pl);

    cairo_new_path(cr);

    if (aud_cfg->show_numbers_in_pl)
    {
        gchar *pos_string = g_strdup_printf(config.show_separator_in_pl == TRUE ? "%d" : "%d.", ppos);
        plist_length_int =
         gint_count_digits (active_length) + ! config.show_separator_in_pl + 1;

        padding = private->digit_width * (plist_length_int + 1);

        layout = gtk_widget_create_pango_layout(playlistwin, pos_string);
        pango_layout_set_font_description (layout, private->font);
        pango_layout_set_width(layout, plist_length_int * 100);

        pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

        cairo_move_to (cr, (private->digit_width *
                         (-1 + plist_length_int - strlen(pos_string))) +
         private->digit_width / 4, private->offset + private->row_height * line);
        pango_cairo_show_layout(cr, layout);

        g_free(pos_string);
        g_object_unref(layout);

        if (!config.show_separator_in_pl)
            padding -= private->digit_width * 1.5;
    } else {
        padding = 3;
    }

    width -= padding;

    layout = gtk_widget_create_pango_layout(playlistwin, text);

    pango_layout_set_font_description (layout, private->font);
    pango_layout_set_width(layout, width * PANGO_SCALE);
    pango_layout_set_single_paragraph_mode(layout, TRUE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    cairo_move_to (cr, padding + private->letter_width / 4, private->offset +
     private->row_height * line);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
}

static gboolean ui_skinned_playlist_expose(GtkWidget *widget, GdkEventExpose *event) {
    UiSkinnedPlaylist *pl = UI_SKINNED_PLAYLIST (widget);
    UiSkinnedPlaylistPrivate *priv = UI_SKINNED_PLAYLIST_GET_PRIVATE(pl);
    gint active_entry = aud_playlist_get_position (active_playlist);
    PangoLayout *layout;
    gint width, height;
    gint i;
    guint padding, padding_dwidth;
    guint max_time_len = 0;
    gfloat queue_tailpadding = 0;
    gint tpadding;
    gsize tpadding_dwidth = 0;
    gint x, y;
    guint tail_width;
    guint tail_len;

    gchar tail[100];
    gchar queuepos[255];
    gchar length[40];

    gchar **frags;
    gchar *frag0;

    cairo_t *cr;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (UI_SKINNED_IS_PLAYLIST (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    cr = gdk_cairo_create(widget->window);

    width = priv->width;
    height = priv->height;

    gdk_cairo_set_source_color(cr, skin_get_color(aud_active_skin, SKIN_PLEDIT_NORMALBG));

    cairo_rectangle(cr, 0, 0, width, height);
    cairo_paint(cr);

    if (priv->offset)
    {
        layout = gtk_widget_create_pango_layout (widget, active_title);
        pango_layout_set_font_description (layout, priv->font);
        pango_layout_set_width (layout, PANGO_SCALE * width);
        pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
        pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_MIDDLE);
        cairo_move_to (cr, 0, 0);
        gdk_cairo_set_source_color (cr, skin_get_color (aud_active_skin,
         SKIN_PLEDIT_NORMAL));
        pango_cairo_show_layout (cr, layout);
        g_object_unref(layout);
    }

    for (i = priv->first; i < priv->first + priv->rows && i < active_length; i ++)
    {
        gint i_length = aud_playlist_entry_get_length (active_playlist, i, TRUE);

        if (i_length > 0)
            g_snprintf (length, sizeof (length), "%d:%02d", i_length / 60000,
             i_length / 1000 % 60);
        else
            length[0] = 0;

        tpadding_dwidth = MAX (tpadding_dwidth, strlen (length));

        if (aud_playlist_entry_get_selected (active_playlist, i))
        {
            gdk_cairo_set_source_color (cr, skin_get_color (aud_active_skin,
             SKIN_PLEDIT_SELECTEDBG));
            cairo_new_path (cr);
            cairo_rectangle (cr, 0, priv->offset + priv->row_height * (i -
             priv->first), width, priv->row_height);
            cairo_fill (cr);
        }
    }

    for (i = priv->first; i < priv->first + priv->rows && i < active_length; i ++)
    {
        gchar * title = aud_playlist_entry_get_title (active_playlist, i, TRUE);
        gint i_length = aud_playlist_entry_get_length (active_playlist, i, TRUE);
        gint pos = aud_playlist_queue_find_entry (active_playlist, i);

        tail[0] = 0;
        queuepos[0] = 0;
        length[0] = 0;

        if (pos != -1)
            g_snprintf(queuepos, sizeof(queuepos), "%d", pos + 1);

        if (i_length > 0)
            g_snprintf (length, sizeof (length), "%d:%02d", i_length / 60000,
             i_length / 1000 % 60);
        else
            length[0] = 0;

        strncat(tail, length, sizeof(tail) - 1);
        tail_len = strlen(tail);

        max_time_len = MAX(max_time_len, tail_len);

        if (pos != -1 && tpadding_dwidth <= 0)
            tail_width = width - priv->digit_width * (strlen (queuepos) + 2.25);
        else if (pos != -1)
            tail_width = width - priv->digit_width * (tpadding_dwidth + strlen
             (queuepos) + 4);
        else if (tpadding_dwidth > 0)
            tail_width = width - priv->digit_width * (tpadding_dwidth + 2.5);
        else
            tail_width = width;

        if (i == active_entry)
            gdk_cairo_set_source_color(cr, skin_get_color(aud_active_skin, SKIN_PLEDIT_CURRENT));
        else
            gdk_cairo_set_source_color(cr, skin_get_color(aud_active_skin, SKIN_PLEDIT_NORMAL));

        playlist_list_draw_string (cr, pl, priv->font,
                                  i - priv->first, tail_width, title,
                                  i + 1);

        x = width - priv->digit_width * 2;
        y = priv->offset + (i - priv->first - 1) * priv->row_height +
         priv->ascent;

        frags = NULL;
        frag0 = NULL;

        if ((strlen(tail) > 0) && (tail != NULL)) {
            frags = g_strsplit(tail, ":", 0);
            frag0 = g_strconcat(frags[0], ":", NULL);

            layout = gtk_widget_create_pango_layout(playlistwin, frags[1]);
            pango_layout_set_font_description (layout, priv->font);
            pango_layout_set_width(layout, tail_len * 100);
            pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

            cairo_new_path(cr);
            cairo_move_to (cr, x - priv->digit_width * 0.5, y - priv->descent);
            pango_cairo_show_layout(cr, layout);
            g_object_unref(layout);

            layout = gtk_widget_create_pango_layout(playlistwin, frag0);
            pango_layout_set_font_description (layout, priv->font);
            pango_layout_set_width(layout, tail_len * 100);
            pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);

            cairo_move_to (cr, x - priv->digit_width * 0.75, y - priv->descent);
            pango_cairo_show_layout(cr, layout);
            g_object_unref(layout);

            g_free(frag0);
            g_strfreev(frags);
        }

        if (pos != -1) {
            if (tpadding_dwidth > 0)
                queue_tailpadding = tpadding_dwidth + 1;
            else
                queue_tailpadding = -0.75;

            cairo_rectangle(cr,
                            x -
                            (((queue_tailpadding +
                               strlen(queuepos)) *
             priv->digit_width) + priv->digit_width / 4), y - priv->descent,
                            (strlen(queuepos)) *
             priv->digit_width + priv->digit_width / 2, priv->row_height - 2);

            layout =
                gtk_widget_create_pango_layout(playlistwin, queuepos);
            pango_layout_set_font_description (layout, priv->font);
            pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

            cairo_move_to(cr,
                            x -
                            ((queue_tailpadding +
             strlen (queuepos)) * priv->digit_width) + priv->digit_width / 4, y
             - priv->descent);
            pango_cairo_show_layout(cr, layout);

            g_object_unref(layout);
        }

        cairo_stroke(cr);
        g_free (title);
    }

    if (priv->focused >= priv->first && priv->focused <= priv->first +
     priv->rows - 1)
    {
        cairo_set_line_width (cr, 1);
        gdk_cairo_set_source_color (cr, skin_get_color (aud_active_skin,
         SKIN_PLEDIT_NORMAL));
        cairo_new_path (cr);
        cairo_rectangle (cr, 0.5, priv->offset + priv->row_height *
         (priv->focused - priv->first) + 0.5, width - 1, priv->row_height - 1);
        cairo_stroke (cr);
    }

    if (priv->hover >= priv->first && priv->hover <= priv->first + priv->rows)
    {
        cairo_set_line_width (cr, 2);
        gdk_cairo_set_source_color (cr, skin_get_color (aud_active_skin,
         SKIN_PLEDIT_NORMAL));

        cairo_new_path (cr);
        cairo_move_to (cr, 0, priv->offset + priv->row_height * (priv->hover -
         priv->first));
        cairo_rel_line_to (cr, width, 0);
        cairo_stroke (cr);
    }

    gdk_cairo_set_source_color(cr, skin_get_color(aud_active_skin, SKIN_PLEDIT_NORMAL));
    cairo_set_line_width(cr, 1);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

    if (aud_cfg->show_numbers_in_pl)
    {
        if (active_length == 0)
            padding_dwidth = 0;
        else
            padding_dwidth = gint_count_digits (active_length);

        padding = (padding_dwidth + 1) * priv->digit_width;

        /* For italic or oblique fonts we add another half of the
         * approximate width */
        if (priv->slanted)
            padding += priv->digit_width / 2;

        if (config.show_separator_in_pl) {
            cairo_new_path(cr);

            cairo_move_to(cr, padding, 0);
            cairo_rel_line_to(cr, 0, priv->height - 1);

            cairo_stroke(cr);
        }
    }

    if (tpadding_dwidth != 0)
    {
        tpadding = priv->digit_width * (tpadding_dwidth + 1.5);

        if (priv->slanted)
            tpadding += priv->digit_width / 2;

        if (config.show_separator_in_pl) {
            cairo_new_path(cr);

            cairo_move_to(cr, priv->width - tpadding, 0);
            cairo_rel_line_to(cr, 0, priv->height - 1);

            cairo_stroke(cr);
        }
    }

    cairo_destroy(cr);

    return FALSE;
}

static gint calc_position (UiSkinnedPlaylistPrivate * private, gint y)
{
    gint position;

    if (y < private->offset)
        return -1;

    position = private->first + (y - private->offset) / private->row_height;

    if (position >= private->first + private->rows || position >= active_length)
        return active_length;

    return position;
}

static gint adjust_position (UiSkinnedPlaylistPrivate * private, gboolean
 relative, gint position)
{
    if (active_length == 0)
        return -1;

    if (relative)
    {
        if (private->focused == -1)
            return 0;

        position += private->focused;
    }

    if (position < 0)
        return 0;
    if (position >= active_length)
        return active_length - 1;

    return position;
}

static void select_single (UiSkinnedPlaylistPrivate * private, gboolean
 relative, gint position)
{
    position = adjust_position (private, relative, position);

    if (position == -1)
        return;

    if (private->focused != -1)
        aud_playlist_entry_set_selected (active_playlist, private->focused,
         FALSE);

    if (aud_playlist_selected_count (active_playlist) > 0)
        aud_playlist_select_all (active_playlist, FALSE);

    aud_playlist_entry_set_selected (active_playlist, position, TRUE);

    private->focused = position;
    scroll_to (private, position);
}

static void select_extend (UiSkinnedPlaylistPrivate * private, gboolean
 relative, gint position)
{
    gint count, sign;

    position = adjust_position (private, relative, position);

    if (position == -1 || position == private->focused)
        return;

    count = adjust_position (private, TRUE, 0);
    sign = (position > count) ? 1 : -1;

    for (; count != position; count += sign)
        aud_playlist_entry_set_selected (active_playlist, count,
         ! aud_playlist_entry_get_selected (active_playlist, count + sign));

    aud_playlist_entry_set_selected (active_playlist, position, TRUE);

    private->focused = position;
    scroll_to (private, position);
}

static void select_slide (UiSkinnedPlaylistPrivate * private, gboolean relative,
 gint position)
{
    position = adjust_position (private, relative, position);

    if (position == -1)
        return;

    private->focused = position;
    scroll_to (private, position);
}

static void select_toggle (UiSkinnedPlaylistPrivate * private, gboolean
 relative, gint position)
{
    position = adjust_position (private, relative, position);

    if (position == -1)
        return;

    aud_playlist_entry_set_selected (active_playlist, position,
     ! aud_playlist_entry_get_selected (active_playlist, position));

    private->focused = position;
    scroll_to (private, position);
}

static void select_move (UiSkinnedPlaylistPrivate * private, gboolean relative,
 gint position)
{
    position = adjust_position (private, relative, position);

    if (private->focused == -1 || position == -1 || position == private->focused)
        return;

    private->focused += aud_playlist_shift (active_playlist, private->focused,
     position - private->focused);

    scroll_to (private, private->focused);
}

static void delete_selected (UiSkinnedPlaylistPrivate * private)
{
    gint shift = 0, count;

    for (count = 0; count < private->focused; count ++)
    {
        if (aud_playlist_entry_get_selected (active_playlist, count))
            shift --;
    }

    aud_playlist_delete_selected (active_playlist);
    active_length = aud_playlist_entry_count (active_playlist);
    calc_layout (private);

    private->focused = adjust_position (private, TRUE, shift);
    select_single (private, TRUE, 0);
}

void ui_skinned_playlist_update (GtkWidget * widget)
{
    UiSkinnedPlaylistPrivate * private = UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) widget);

    calc_layout (private);

    if (private->focused != -1)
        private->focused = adjust_position (private, TRUE, 0);

    gtk_widget_queue_draw (widget);

    if (private->slider != NULL)
        ui_skinned_playlist_slider_update (private->slider);
}

gboolean ui_skinned_playlist_key (GtkWidget * widget, GdkEventKey * event)
{
    UiSkinnedPlaylistPrivate * private = UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) widget);

    cancel_all (widget, private);

    switch (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
    {
      case 0:
        switch (event->keyval)
        {
          case GDK_Up:
            select_single (private, TRUE, -1);
            break;
          case GDK_Down:
            select_single (private, TRUE, 1);
            break;
          case GDK_Page_Up:
            select_single (private, TRUE, - private->rows);
            break;
          case GDK_Page_Down:
            select_single (private, TRUE, private->rows);
            break;
          case GDK_Home:
            select_single (private, FALSE, 0);
            break;
          case GDK_End:
            select_single (private, FALSE, active_length - 1);
            break;
          case GDK_Return:
            select_single (private, TRUE, 0);
            aud_playlist_set_playing (active_playlist);
            aud_playlist_set_position (active_playlist, private->focused);
            aud_drct_play ();
            break;
          case GDK_Escape:
            select_single (private, FALSE, aud_playlist_get_position
             (active_playlist));
            break;
          case GDK_Delete:
            delete_selected (private);
            break;
          default:
            return FALSE;
        }
        break;
      case GDK_SHIFT_MASK:
        switch (event->keyval)
        {
          case GDK_Up:
            select_extend (private, TRUE, -1);
            break;
          case GDK_Down:
            select_extend (private, TRUE, 1);
            break;
          case GDK_Page_Up:
            select_extend (private, TRUE, -private->rows);
            break;
          case GDK_Page_Down:
            select_extend (private, TRUE, private->rows);
            break;
          case GDK_Home:
            select_extend (private, FALSE, 0);
            break;
          case GDK_End:
            select_extend (private, FALSE, active_length - 1);
            break;
          default:
            return FALSE;
        }
        break;
      case GDK_CONTROL_MASK:
        switch (event->keyval)
        {
          case GDK_space:
            select_toggle (private, TRUE, 0);
            break;
          case GDK_Up:
            select_slide (private, TRUE, -1);
            break;
          case GDK_Down:
            select_slide (private, TRUE, 1);
            break;
          case GDK_Page_Up:
            select_slide (private, TRUE, -private->rows);
            break;
          case GDK_Page_Down:
            select_slide (private, TRUE, private->rows);
            break;
          case GDK_Home:
            select_slide (private, FALSE, 0);
            break;
          case GDK_End:
            select_slide (private, FALSE, active_length - 1);
            break;
          default:
            return FALSE;
        }
        break;
      case GDK_MOD1_MASK:
        switch (event->keyval)
        {
          case GDK_Up:
            select_move (private, TRUE, -1);
            break;
          case GDK_Down:
            select_move (private, TRUE, 1);
            break;
          case GDK_Page_Up:
            select_move (private, TRUE, -private->rows);
            break;
          case GDK_Page_Down:
            select_move (private, TRUE, private->rows);
            break;
          case GDK_Home:
            select_move (private, FALSE, 0);
            break;
          case GDK_End:
            select_move (private, FALSE, active_length - 1);
            break;
          default:
            return FALSE;
        }
        break;
      default:
        return FALSE;
    }

    playlistwin_update ();
    return TRUE;
}

void ui_skinned_playlist_row_info (GtkWidget * widget, gint * rows, gint *
 first, gint * focused)
{
    UiSkinnedPlaylistPrivate * private = UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) widget);

    * rows = private->rows;
    * first = private->first;
    * focused = private->focused;
}

void ui_skinned_playlist_scroll_to (GtkWidget * widget, gint row)
{
    UiSkinnedPlaylistPrivate * private = UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) widget);

    cancel_all (widget, private);
    private->first = row;
    calc_layout (private);

    gtk_widget_queue_draw (widget);

    if (private->slider != NULL)
        ui_skinned_playlist_slider_update (private->slider);
}

void ui_skinned_playlist_set_focused (GtkWidget * widget, gint row)
{
    UiSkinnedPlaylistPrivate * private = UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) widget);

    cancel_all (widget, private);
    private->focused = row;
    scroll_to (private, row);

    gtk_widget_queue_draw (widget);
}

void ui_skinned_playlist_hover (GtkWidget * widget, gint x, gint y)
{
    UiSkinnedPlaylistPrivate * private = UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) widget);
    gint new;

    if (y < private->offset)
        new = private->first;
    else if (y > private->offset + private->row_height * private->rows)
        new = private->first + private->rows;
    else
        new = private->first + (y - private->offset + private->row_height / 2) /
         private->row_height;

    if (new > active_length)
        new = active_length;

    if (new != private->hover)
    {
        private->hover = new;
        gtk_widget_queue_draw (widget);
    }
}

gint ui_skinned_playlist_hover_end (GtkWidget * widget)
{
    UiSkinnedPlaylistPrivate * private = UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) widget);
    gint temp = private->hover;

    private->hover = -1;

    gtk_widget_queue_draw (widget);
    return temp;
}

static gboolean ui_skinned_playlist_button_press (GtkWidget * widget,
 GdkEventButton * event)
{
    UiSkinnedPlaylistPrivate * private = UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) widget);
    gint position = calc_position (private, event->y);
    gint state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK |
     GDK_MOD1_MASK);

    cancel_all (widget, private);

    switch (event->type)
    {
      case GDK_BUTTON_PRESS:
        switch (event->button)
        {
          case 1:
            if (position == -1 || position == active_length)
                return TRUE;

            switch (state)
            {
              case 0:
                if (aud_playlist_entry_get_selected (active_playlist, position))
                    select_slide (private, FALSE, position);
                else
                    select_single (private, FALSE, position);

                private->drag = DRAG_MOVE;
                break;
              case GDK_SHIFT_MASK:
                select_extend (private, FALSE, position);
                private->drag = DRAG_SELECT;
                break;
              case GDK_CONTROL_MASK:
                select_toggle (private, FALSE, position);
                private->drag = DRAG_SELECT;
                break;
              default:
                return TRUE;
            }

            break;
          case 3:
            if (state)
                return TRUE;

            if (position != -1 && position != active_length)
            {
                if (aud_playlist_entry_get_selected (active_playlist, position))
                    select_slide (private, FALSE, position);
                else
                    select_single (private, FALSE, position);
            }

            ui_popup_menu_show ((position == -1) ? UI_MENU_PLAYLIST :
             UI_MENU_PLAYLIST_CONTEXT, event->x_root, event->y_root, FALSE,
             FALSE, 3, event->time);
            break;
          default:
            return FALSE;
        }

        break;
      case GDK_2BUTTON_PRESS:
        if (event->button != 1 || state || position == -1 || position ==
         active_length)
            return TRUE;

        aud_playlist_set_playing (active_playlist);
        aud_playlist_set_position (active_playlist, position);
        aud_drct_play ();
        break;
      default:
        return TRUE;
    }

    playlistwin_update ();
    return TRUE;
}

static gboolean ui_skinned_playlist_button_release (GtkWidget * widget,
 GdkEventButton * event)
{
    cancel_all (widget, UI_SKINNED_PLAYLIST_GET_PRIVATE ((UiSkinnedPlaylist *)
     widget));
    return TRUE;
}

static gboolean scroll_cb (void * data)
{
    UiSkinnedPlaylistPrivate * private = data;
    gint position = adjust_position (private, TRUE, private->scroll);

    if (position == -1)
        return TRUE;

    switch (private->drag)
    {
      case DRAG_SELECT:
        select_extend (private, FALSE, position);
        break;
      case DRAG_MOVE:
        select_move (private, FALSE, position);
        break;
    }

    playlistwin_update ();
    return TRUE;
}

static gboolean ui_skinned_playlist_motion_notify (GtkWidget * widget,
 GdkEventMotion * event)
{
    UiSkinnedPlaylistPrivate * private = UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) widget);
    gint position = calc_position (private, event->y);
    gint new_scroll;

    if (private->drag)
    {
        if (position == -1 || position == active_length)
        {
            new_scroll = (position == -1 ? -1 : 1);

            if (private->scroll != new_scroll)
            {
                if (private->scroll)
                    g_source_remove (private->scroll_source);

                private->scroll = new_scroll;
                private->scroll_source = g_timeout_add (100, scroll_cb, private);
            }
        }
        else
        {
            if (private->scroll)
            {
                private->scroll = 0;
                g_source_remove (private->scroll_source);
            }

            switch (private->drag)
            {
              case DRAG_SELECT:
                select_extend (private, FALSE, position);
                break;
              case DRAG_MOVE:
                select_move (private, FALSE, position);
                break;
            }

            playlistwin_update ();
        }
    }
    else
    {
        if (position == -1 || position == active_length)
            cancel_all (widget, private);
        else if (aud_cfg->show_filepopup_for_tuple && (! GPOINTER_TO_INT
         (g_object_get_data ((GObject *) widget, "popup_active")) || position
         != GPOINTER_TO_INT (g_object_get_data ((GObject *) widget,
         "popup_position"))))
        {
            cancel_all (widget, private);
            g_object_set_data ((GObject *) widget, "popup_position",
             GINT_TO_POINTER (position));
            ui_skinned_playlist_popup_timer_start (widget);
        }
    }

    return TRUE;
}

static gboolean ui_skinned_playlist_leave_notify (GtkWidget * widget,
 GdkEventCrossing * event)
{
    UiSkinnedPlaylistPrivate * private = UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) widget);

    if (! private->drag)
        cancel_all (widget, private);

    return TRUE;
}

void ui_skinned_playlist_set_font (GtkWidget * list, const gchar * font)
{
    UiSkinnedPlaylistPrivate * private = UI_SKINNED_PLAYLIST_GET_PRIVATE
     ((UiSkinnedPlaylist *) list);
    gchar *font_lower;

    private->font = pango_font_description_from_string (font);

    text_get_extents(font,
                     "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz ",
     & private->letter_width, 0, & private->ascent, & private->descent);
    private->letter_width /= 53;

    text_get_extents (font, "0123456789", & private->digit_width, 0, 0, 0);
    private->digit_width /= 10;

    font_lower = g_utf8_strdown(font, strlen(font));
    /* This doesn't take any i18n into account, but i think there is none with TTF fonts
     * FIXME: This can probably be retrieved trough Pango too
     */
    private->slanted = strstr (font_lower, "oblique") != NULL || strstr
     (font_lower, "italic") != NULL;

    g_free(font_lower);

    calc_layout (private);
    gtk_widget_queue_draw (list);

    if (private->slider != NULL)
        ui_skinned_playlist_slider_update (private->slider);
}

void ui_skinned_playlist_resize_relative(GtkWidget *widget, gint w, gint h) {
    UiSkinnedPlaylistPrivate *priv = UI_SKINNED_PLAYLIST_GET_PRIVATE(widget);
    priv->resize_width += w;
    priv->resize_height += h;
    gtk_widget_set_size_request(widget, priv->width+priv->resize_width, priv->height+priv->resize_height);
}

static gboolean ui_skinned_playlist_popup_show(gpointer data) {
    GtkWidget *widget = data;
    gint pos = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "popup_position"));

    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "timer_active")) == 1 && pos != -1) {
        audgui_infopopup_show (active_playlist, pos);
        g_object_set_data(G_OBJECT(widget), "popup_active" , GINT_TO_POINTER(1));
    }

    ui_skinned_playlist_popup_timer_stop(widget);
    return FALSE;
}

static void ui_skinned_playlist_popup_hide(GtkWidget *widget) {
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "popup_active")) == 1) {
        g_object_set_data(G_OBJECT(widget), "popup_active", GINT_TO_POINTER(0));
        audgui_infopopup_hide ();
    }
}

static void ui_skinned_playlist_popup_timer_start(GtkWidget *widget) {
    int timer_id;

    timer_id = g_timeout_add (aud_cfg->filepopup_delay * 100,
     ui_skinned_playlist_popup_show, widget);
    g_object_set_data(G_OBJECT(widget), "timer_id", GINT_TO_POINTER(timer_id));
    g_object_set_data(G_OBJECT(widget), "timer_active", GINT_TO_POINTER(1));
}

static void ui_skinned_playlist_popup_timer_stop(GtkWidget *widget) {
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "timer_active")) == 1)
        g_source_remove(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "timer_id")));

    g_object_set_data(G_OBJECT(widget), "timer_id", GINT_TO_POINTER(0));
    g_object_set_data(G_OBJECT(widget), "timer_active", GINT_TO_POINTER(0));
}
