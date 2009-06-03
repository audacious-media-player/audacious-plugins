/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2008 William Pitcock
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

#include "debug.h"
#include "ui_playlist.h"
#include "ui_manager.h"
#include "ui_skin.h"
#include "util.h"
#include "skins_cfg.h"
#include <audacious/plugin.h>

static PangoFontDescription *playlist_list_font = NULL;
static gint ascent, descent, width_delta_digit_one;
static gboolean has_slant;
static guint padding;

/* FIXME: the following globals should not be needed. */
static gint width_approx_letters;
static gint width_colon, width_colon_third;
static gint width_approx_digits, width_approx_digits_half;

#define UI_SKINNED_PLAYLIST_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ui_skinned_playlist_get_type(), UiSkinnedPlaylistPrivate))
typedef struct _UiSkinnedPlaylistPrivate UiSkinnedPlaylistPrivate;

enum {DRAG_SELECT = 1, DRAG_MOVE};

struct _UiSkinnedPlaylistPrivate {
    SkinPixmapId     skin_index;
    gint             width, height;
    gint             resize_width, resize_height;
    int row_height, rows, first, focused;
    char no_update, drag;
    int scroll, scroll_source;
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
    playlist->pressed = FALSE;
    priv->resize_width = 0;
    priv->resize_height = 0;
    priv->first = 0;
    priv->focused = -1;
    priv->no_update = 0;
    priv->drag = 0;
    priv->scroll = 0;

    g_object_set_data(G_OBJECT(playlist), "timer_id", GINT_TO_POINTER(0));
    g_object_set_data(G_OBJECT(playlist), "timer_active", GINT_TO_POINTER(0));

    GtkWidget *popup = audacious_fileinfopopup_create();
    g_object_set_data(G_OBJECT(playlist), "popup", popup);
    g_object_set_data(G_OBJECT(playlist), "popup_active", GINT_TO_POINTER(0));
    g_object_set_data(G_OBJECT(playlist), "popup_position", GINT_TO_POINTER(-1));
}

GtkWidget* ui_skinned_playlist_new(GtkWidget *fixed, gint x, gint y, gint w, gint h) {

    UiSkinnedPlaylist *hs = g_object_new (ui_skinned_playlist_get_type (), NULL);
    UiSkinnedPlaylistPrivate *priv = UI_SKINNED_PLAYLIST_GET_PRIVATE(hs);

    hs->x = x;
    hs->y = y;
    priv->width = w;
    priv->height = h;
    priv->skin_index = SKIN_PLEDIT;

    gtk_fixed_put(GTK_FIXED(fixed), GTK_WIDGET(hs), hs->x, hs->y);
    gtk_widget_set_double_buffered(GTK_WIDGET(hs), TRUE);

    return GTK_WIDGET(hs);
}

void cancel_all (GtkWidget * widget, UiSkinnedPlaylistPrivate * private)
{
    private->drag = 0;

    if (private->scroll)
    {
        private->scroll = 0;
        g_source_remove (private->scroll_source);
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
    UiSkinnedPlaylist *playlist;
    GdkWindowAttr attributes;
    gint attributes_mask;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (UI_SKINNED_IS_PLAYLIST(widget));

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    playlist = UI_SKINNED_PLAYLIST(widget);

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

static void ui_skinned_playlist_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
    UiSkinnedPlaylist *playlist = UI_SKINNED_PLAYLIST (widget);
    UiSkinnedPlaylistPrivate *priv = UI_SKINNED_PLAYLIST_GET_PRIVATE(playlist);
    int length;

    widget->allocation = *allocation;
    if (GTK_WIDGET_REALIZED (widget))
        gdk_window_move_resize(widget->window, widget->allocation.x, widget->allocation.y, allocation->width, allocation->height);

    playlist->x = widget->allocation.x;
    playlist->y = widget->allocation.y;

    if (priv->height != widget->allocation.height || priv->width != widget->allocation.width) {
        priv->width = priv->width + priv->resize_width;
        priv->height = priv->height + priv->resize_height;
        priv->resize_width = 0;
        priv->resize_height = 0;
        gtk_widget_queue_draw(widget);
    }

    if (! playlist_list_font)
    {
        printf ("ERROR: Cannot open playlist font.\n");
        abort ();
    }

    priv->row_height = ascent - descent;
    priv->rows = priv->height / priv->row_height;

    length = aud_playlist_get_length (aud_playlist_get_active ());

    if (priv->first + priv->rows > length)
        priv->first = length - priv->rows;
    if (priv->first < 0)
        priv->first = 0;
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
    guint plist_length_int;
    Playlist *playlist = aud_playlist_get_active();
    PangoLayout *layout;

    private = UI_SKINNED_PLAYLIST_GET_PRIVATE (pl);

    REQUIRE_LOCK(playlist->mutex);

    cairo_new_path(cr);

    if (config.show_numbers_in_pl) {
        gchar *pos_string = g_strdup_printf(config.show_separator_in_pl == TRUE ? "%d" : "%d.", ppos);
        plist_length_int =
            gint_count_digits(aud_playlist_get_length(playlist)) + !config.show_separator_in_pl + 1; /* cf.show_separator_in_pl will be 0 if false */

        padding = plist_length_int;
        padding = ((padding + 1) * width_approx_digits);

        layout = gtk_widget_create_pango_layout(playlistwin, pos_string);
        pango_layout_set_font_description(layout, playlist_list_font);
        pango_layout_set_width(layout, plist_length_int * 100);

        pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

        cairo_move_to(cr, (width_approx_digits *
                         (-1 + plist_length_int - strlen(pos_string))) +
         width_approx_digits / 4, (line - 1) * private->row_height +
                        ascent + abs(descent));
        pango_cairo_show_layout(cr, layout);

        g_free(pos_string);
        g_object_unref(layout);

        if (!config.show_separator_in_pl)
            padding -= (width_approx_digits * 1.5);
    } else {
        padding = 3;
    }

    width -= padding;

    layout = gtk_widget_create_pango_layout(playlistwin, text);

    pango_layout_set_font_description(layout, playlist_list_font);
    pango_layout_set_width(layout, width * PANGO_SCALE);
    pango_layout_set_single_paragraph_mode(layout, TRUE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    cairo_move_to(cr, padding + (width_approx_letters / 4),
     (line - 1) * private->row_height +
                    ascent + abs(descent));
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
}

static gboolean ui_skinned_playlist_expose(GtkWidget *widget, GdkEventExpose *event) {
    UiSkinnedPlaylist *pl = UI_SKINNED_PLAYLIST (widget);
    UiSkinnedPlaylistPrivate *priv = UI_SKINNED_PLAYLIST_GET_PRIVATE(pl);
    g_return_val_if_fail (priv->width > 0 && priv->height > 0, FALSE);

    Playlist *playlist = aud_playlist_get_active();
    PlaylistEntry *entry;
    GList *list;
    PangoLayout *layout;
    gchar *title;
    gint width, height;
    gint i;
    guint padding, padding_dwidth, padding_plength;
    guint max_time_len = 0;
    gfloat queue_tailpadding = 0;
    gint tpadding;
    gsize tpadding_dwidth = 0;
    gint x, y;
    guint tail_width;
    guint tail_len;
    gboolean in_selection = FALSE;

    gchar tail[100];
    gchar queuepos[255];
    gchar length[40];

    gchar **frags;
    gchar *frag0;

    gint plw_w, plw_h;

    cairo_t *cr;
    gint yc;
    gint pos;
    gdouble rounding_offset;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (UI_SKINNED_IS_PLAYLIST (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    cr = gdk_cairo_create(widget->window);

    width = priv->width;
    height = priv->height;

    plw_w = playlistwin_get_width();
    plw_h = playlistwin_get_height();

    gdk_cairo_set_source_color(cr, skin_get_color(aud_active_skin, SKIN_PLEDIT_NORMALBG));

    cairo_rectangle(cr, 0, 0, width, height);
    cairo_paint(cr);

    rounding_offset = priv->row_height / 3;

    PLAYLIST_LOCK(playlist);
    list = playlist->entries;
    list = g_list_nth(list, priv->first);

    /* It sucks having to run the iteration twice but this is the only
       way you can reliably get the maximum width so we can get our
       playlist nice and aligned... -- plasmaroo */

    for (i = priv->first;
         list && i < priv->first + priv->rows;
         list = g_list_next(list), i++) {
        entry = list->data;

        if (entry->length != -1)
        {
            g_snprintf(length, sizeof(length), "%d:%-2.2d",
                       entry->length / 60000, (entry->length / 1000) % 60);
            tpadding_dwidth = MAX(tpadding_dwidth, strlen(length));
        }
    }

    /* Reset */
    list = playlist->entries;
    list = g_list_nth(list, priv->first);

    for (i = priv->first;
         list && i < priv->first + priv->rows;
         list = g_list_next(list), i++) {
        entry = list->data;

        if (entry->selected && !in_selection) {
            yc = ((i - priv->first) * priv->row_height);

            cairo_new_path(cr);

            cairo_move_to(cr, 0, yc + (rounding_offset * 2));
            cairo_curve_to(cr, 0, yc + rounding_offset, 0, yc + 0.5, 0 + rounding_offset, yc + 0.5);

            cairo_line_to(cr, 0 + width - (rounding_offset * 2), yc + 0.5);
            cairo_curve_to(cr, 0 + width - rounding_offset, yc + 0.5,
                        0 + width, yc + 0.5, 0 + width, yc + rounding_offset);

            in_selection = TRUE;
        }

        if ((!entry->selected ||
            (i == priv->first + priv->rows - 1) || !g_list_next(list))
            && in_selection) {

            if (!entry->selected)
                yc = (((i - 1) - priv->first) * priv->row_height);
            else /* last visible item */
                yc = ((i - priv->first) * priv->row_height);

            cairo_line_to(cr, 0 + width, yc + priv->row_height - (rounding_offset * 2));
            cairo_curve_to (cr, 0 + width, yc + priv->row_height - rounding_offset,
                        0 + width, yc + priv->row_height - 0.5,
                        0 + width-rounding_offset, yc + priv->row_height - 0.5);

            cairo_line_to (cr, 0 + (rounding_offset * 2), yc + priv->row_height - 0.5);
            cairo_curve_to (cr, 0 + rounding_offset, yc + priv->row_height - 0.5,
                        0, yc + priv->row_height - 0.5,
                        0, yc + priv->row_height - rounding_offset);

            cairo_close_path (cr);

            gdk_cairo_set_source_color(cr, skin_get_color(aud_active_skin, SKIN_PLEDIT_SELECTEDBG));

            cairo_fill(cr);

            in_selection = FALSE;
        }
    }

    list = playlist->entries;
    list = g_list_nth(list, priv->first);

    /* now draw the text */
    for (i = priv->first;
         list && i < priv->first + priv->rows;
         list = g_list_next(list), i++) {
        entry = list->data;

        /* FIXME: entry->title should NEVER be NULL, and there should
           NEVER be a need to do a UTF-8 conversion. Playlist title
           strings should be kept properly. */

        if (!entry->title) {
            gchar *realfn = g_filename_from_uri(entry->filename, NULL, NULL);
            gchar *basename = g_path_get_basename(realfn ? realfn : entry->filename);
            title = aud_filename_to_utf8(basename);
            g_free(basename); g_free(realfn);
        }
        else
            title = aud_str_to_utf8(entry->title);

        title = aud_convert_title_text(title);

        pos = aud_playlist_get_queue_position(playlist, entry);

        tail[0] = 0;
        queuepos[0] = 0;
        length[0] = 0;

        if (pos != -1)
            g_snprintf(queuepos, sizeof(queuepos), "%d", pos + 1);

        if (entry->length != -1)
        {
            g_snprintf(length, sizeof(length), "%d:%-2.2d",
                       entry->length / 60000, (entry->length / 1000) % 60);
        }

        strncat(tail, length, sizeof(tail) - 1);
        tail_len = strlen(tail);

        max_time_len = MAX(max_time_len, tail_len);

        if (pos != -1 && tpadding_dwidth <= 0)
            tail_width = width - (width_approx_digits * (strlen(queuepos) + 2.25));
        else if (pos != -1)
            tail_width = width - (width_approx_digits * (tpadding_dwidth + strlen(queuepos) + 4));
        else if (tpadding_dwidth > 0)
            tail_width = width - (width_approx_digits * (tpadding_dwidth + 2.5));
        else
            tail_width = width;

        if (i == aud_playlist_get_position_nolock(playlist))
            gdk_cairo_set_source_color(cr, skin_get_color(aud_active_skin, SKIN_PLEDIT_CURRENT));
        else
            gdk_cairo_set_source_color(cr, skin_get_color(aud_active_skin, SKIN_PLEDIT_NORMAL));

        playlist_list_draw_string(cr, pl, playlist_list_font,
                                  i - priv->first, tail_width, title,
                                  i + 1);

        x = width - width_approx_digits * 2;
        y = ((i - priv->first) - 1) * priv->row_height + ascent;

        frags = NULL;
        frag0 = NULL;

        if ((strlen(tail) > 0) && (tail != NULL)) {
            frags = g_strsplit(tail, ":", 0);
            frag0 = g_strconcat(frags[0], ":", NULL);

            layout = gtk_widget_create_pango_layout(playlistwin, frags[1]);
            pango_layout_set_font_description(layout, playlist_list_font);
            pango_layout_set_width(layout, tail_len * 100);
            pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

            cairo_new_path(cr);
            cairo_move_to(cr, x - (0.5 * width_approx_digits), y + abs(descent));
            pango_cairo_show_layout(cr, layout);
            g_object_unref(layout);

            layout = gtk_widget_create_pango_layout(playlistwin, frag0);
            pango_layout_set_font_description(layout, playlist_list_font);
            pango_layout_set_width(layout, tail_len * 100);
            pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);

            cairo_move_to(cr, x - (0.75 * width_approx_digits), y + abs(descent));
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
                              width_approx_digits) +
                             (width_approx_digits / 4)),
                            y + abs(descent),
                            (strlen(queuepos)) *
                            width_approx_digits +
                            (width_approx_digits / 2),
                            priv->row_height - 2);

            layout =
                gtk_widget_create_pango_layout(playlistwin, queuepos);
            pango_layout_set_font_description(layout, playlist_list_font);
            pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

            cairo_move_to(cr,
                            x -
                            ((queue_tailpadding +
                              strlen(queuepos)) * width_approx_digits) +
                            (width_approx_digits / 4),
                            y + abs(descent));
            pango_cairo_show_layout(cr, layout);

            g_object_unref(layout);
        }

        cairo_stroke(cr);

        g_free(title);
    }

    if (priv->focused >= priv->first && priv->focused <= priv->first +
     priv->rows - 1)
    {
        double left, top, right, bottom;

        cairo_set_line_width (cr, 1);
        gdk_cairo_set_source_color (cr, skin_get_color (aud_active_skin,
         SKIN_PLEDIT_NORMAL));

        left = 0.5;
        top = priv->row_height * (priv->focused - priv->first) + 0.5;
        right = width - 0.5;
        bottom = top + priv->row_height - 1;

        cairo_new_path (cr);
        cairo_move_to (cr, left, top + rounding_offset * 2);
        cairo_curve_to (cr, left, top + rounding_offset, left, top, left
         + rounding_offset, top);
        cairo_line_to (cr, right - rounding_offset * 2, top);
        cairo_curve_to (cr, right - rounding_offset, top, right, top, right, top
         + rounding_offset);
        cairo_line_to (cr, right, bottom - rounding_offset * 2);
        cairo_curve_to (cr, right, bottom - rounding_offset, right, bottom,
         right - rounding_offset, bottom);
        cairo_line_to (cr, left + rounding_offset * 2, bottom);
        cairo_curve_to (cr, left + rounding_offset, bottom, left, bottom, left,
         bottom - rounding_offset);
        cairo_close_path (cr);

        cairo_stroke (cr);
    }

    gdk_cairo_set_source_color(cr, skin_get_color(aud_active_skin, SKIN_PLEDIT_NORMAL));
    cairo_set_line_width(cr, 1);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

    if (config.show_numbers_in_pl)
    {
        padding_plength = aud_playlist_get_length(playlist);

        if (padding_plength == 0) {
            padding_dwidth = 0;
        }
        else {
            padding_dwidth = gint_count_digits(aud_playlist_get_length(playlist));
        }

        padding =
            (padding_dwidth *
             width_approx_digits) + width_approx_digits;


        /* For italic or oblique fonts we add another half of the
         * approximate width */
        if (has_slant)
            padding += width_approx_digits_half;

        if (config.show_separator_in_pl) {
            cairo_new_path(cr);

            cairo_move_to(cr, padding, 0);
            cairo_rel_line_to(cr, 0, priv->height - 1);

            cairo_stroke(cr);
        }
    }

    if (tpadding_dwidth != 0)
    {
        tpadding = (tpadding_dwidth * width_approx_digits) + (width_approx_digits * 1.5);

        if (has_slant)
            tpadding += width_approx_digits_half;

        if (config.show_separator_in_pl) {
            cairo_new_path(cr);

            cairo_move_to(cr, priv->width - tpadding, 0);
            cairo_rel_line_to(cr, 0, priv->height - 1);

            cairo_stroke(cr);
        }
    }

    PLAYLIST_UNLOCK(playlist);

    cairo_destroy(cr);

    return FALSE;
}

static int calc_position (UiSkinnedPlaylistPrivate * private, int length, int y)
{
    int position;

    if (y < 0)
        return -1;

    position = private->first + y / private->row_height;

    if (position > private->first + private->rows - 1 || position >
     length - 1)
        return length;

    return position;
}

static int adjust_position (UiSkinnedPlaylistPrivate * private, int length, char
 relative, int position)
{
    if (length == 0)
        return -1;

    if (relative)
    {
        if (private->focused == -1)
            return 0;

        position += private->focused;
    }

    if (position < 0)
        return 0;
    if (position > length - 1)
        return length - 1;

    return position;
}

static void scroll_to (UiSkinnedPlaylistPrivate * private, int length, int
 position)
{
    if (position < private->first)
    {
        private->first = position - private->rows / 2;

        if (private->first < 0)
            private->first = 0;
    }
    else if (position > private->first + private->rows - 1)
    {
        private->first = position - private->rows / 2;

        if (private->first + private->rows > length)
            private->first = length - private->rows;
    }
}

/* This is very inefficient. Don't use it in any loops. */
static char is_selected (Playlist * playlist, int position)
{
    GList * selected;
    char is;

    selected = aud_playlist_get_selected (playlist);
    is = g_list_find (selected, GINT_TO_POINTER (position)) ? 1 : 0;
    g_list_free (selected);
    return is;
}

static char shift_up_one (Playlist * playlist)
{
    GList * list;

    PLAYLIST_LOCK (playlist);
    list = playlist->entries;

    if (! list || ((PlaylistEntry *) list->data)->selected)
    {
        PLAYLIST_UNLOCK (playlist);
        return 0;
    }

    while (list)
    {
        if (((PlaylistEntry *) list->data)->selected)
            glist_moveup (list);

        list = list->next;
    }

    PLAYLIST_UNLOCK (playlist);
    return 1;
}

static char shift_down_one (Playlist * playlist)
{
    GList * list;

    PLAYLIST_LOCK (playlist);
    list = g_list_last (playlist->entries);

    if (! list || ((PlaylistEntry *) list->data)->selected)
    {
        PLAYLIST_UNLOCK (playlist);
        return 0;
    }

    while (list)
    {
        if (((PlaylistEntry *) list->data)->selected)
            glist_movedown (list);

        list = list->prev;
    }

    PLAYLIST_UNLOCK (playlist);
    return 1;
}

static void select_single (UiSkinnedPlaylistPrivate * private, Playlist *
 playlist, int length, char relative, int position)
{
    position = adjust_position (private, length, relative, position);

    if (position == -1)
        return;

    aud_playlist_select_all (playlist, 0);
    aud_playlist_select_range (playlist, position, position, 1);

    private->focused = position;
    scroll_to (private, length, position);
}

static void select_extend (UiSkinnedPlaylistPrivate * private, Playlist *
 playlist, int length, char relative, int position)
{
    int sign;

    position = adjust_position (private, length, relative, position);

    if (position == -1 || position == private->focused)
        return;

    sign = position > private->focused ? 1 : -1;

    if (is_selected (playlist, private->focused + sign) != is_selected (playlist,
     private->focused))
        aud_playlist_select_range (playlist, private->focused + sign, position,
         is_selected (playlist, private->focused));
    else if (is_selected (playlist, private->focused - sign) != is_selected
     (playlist, private->focused))
        aud_playlist_select_range (playlist, private->focused, position - sign,
         ! is_selected (playlist, private->focused));
    else
        aud_playlist_select_range (playlist, private->focused, position, 1);

    private->focused = position;
    scroll_to (private, length, position);
}

static void select_slide (UiSkinnedPlaylistPrivate * private, int length, char
 relative, int position)
{
    position = adjust_position (private, length, relative, position);

    if (position == -1)
        return;

    private->focused = position;
    scroll_to (private, length, position);
}

static void select_toggle (UiSkinnedPlaylistPrivate * private, Playlist *
 playlist, int length, char relative, int position)
{
    GList * selected;

    position = adjust_position (private, length, relative, position);

    if (position == -1)
        return;

    selected = aud_playlist_get_selected (playlist);
    aud_playlist_select_range (playlist, position, position, ! g_list_find
     (selected, GINT_TO_POINTER (position)));
    g_list_free (selected);

    private->focused = position;
    scroll_to (private, length, position);
}

static void select_move (UiSkinnedPlaylistPrivate * private, Playlist * playlist,
 int length, char relative, int position)
{
    position = adjust_position (private, length, relative, position);

    if (position == -1)
        return;

    for (; private->focused > position; private->focused --)
    {
        if (! shift_up_one (playlist))
            break;
    }

    for (; private->focused < position; private->focused ++)
    {
        if (! shift_down_one (playlist))
            break;
    }

    scroll_to (private, length, private->focused);
}

static void delete_selected (UiSkinnedPlaylistPrivate * private, Playlist *
 playlist, int length)
{
    GList * selected, * list;
    int position, shift;

    position = adjust_position (private, length, 1, 0);

    if (position == -1)
        return;

    shift = 0;
    selected = aud_playlist_get_selected (playlist);

    for (list = selected; list; list = list->next)
    {
        length --;

        if (GPOINTER_TO_INT (list->data) < position)
            shift --;
    }

    g_list_free (selected);

    private->no_update = 1;
    aud_playlist_delete (playlist, 0);
    private->no_update = 0;

    if (private->first + private->rows > length)
        private->first = length - private->rows;
    if (private->first < 0)
        private->first = 0;

    if (length == 0)
        private->focused = -1;
    else
        select_single (private, playlist, length, 0, position + shift);
}

void ui_skinned_playlist_update (GtkWidget * widget)
{
    UiSkinnedPlaylistPrivate * private;
    Playlist * playlist;
    int length;

    private = UI_SKINNED_PLAYLIST_GET_PRIVATE ((UiSkinnedPlaylist *) widget);

    if (private->no_update)
        return;

    playlist = aud_playlist_get_active ();
    length = aud_playlist_get_length (playlist);

    cancel_all (widget, private);

    if (private->first + private->rows > length)
        private->first = length - private->rows;
    if (private->first < 0)
        private->first = 0;

    if (length == 0)
        private->focused = -1;
    else
        select_single (private, playlist, length, 0, aud_playlist_get_position
         (playlist));
}

char ui_skinned_playlist_key (GtkWidget * widget, GdkEventKey * event)
{
    UiSkinnedPlaylistPrivate * private;
    Playlist * playlist;
    int length;

    private = UI_SKINNED_PLAYLIST_GET_PRIVATE ((UiSkinnedPlaylist *) widget);
    playlist = aud_playlist_get_active ();
    length = aud_playlist_get_length (playlist);

    cancel_all (widget, private);

    switch (event->state)
    {
      case 0:
        switch (event->keyval)
        {
          case GDK_space:
            select_single (private, playlist, length, 1, 0);
            break;
          case GDK_Up:
            select_single (private, playlist, length, 1, -1);
            break;
          case GDK_Down:
            select_single (private, playlist, length, 1, 1);
            break;
          case GDK_Page_Up:
            select_single (private, playlist, length, 1, -private->rows);
            break;
          case GDK_Page_Down:
            select_single (private, playlist, length, 1, private->rows);
            break;
          case GDK_Home:
            select_single (private, playlist, length, 0, 0);
            break;
          case GDK_End:
            select_single (private, playlist, length, 0, length - 1);
            break;
          case GDK_Return:
            select_single (private, playlist, length, 1, 0);
            aud_playlist_set_position (playlist, private->focused);
            audacious_drct_initiate ();
            break;
          case GDK_Escape:
            select_single (private, playlist, length, 0,
            aud_playlist_get_position (playlist));
            break;
          case GDK_Delete:
            delete_selected (private, playlist, length);
            break;
          default:
            return 0;
        }
        break;
      case GDK_SHIFT_MASK:
        switch (event->keyval)
        {
          case GDK_Up:
            select_extend (private, playlist, length, 1, -1);
            break;
          case GDK_Down:
            select_extend (private, playlist, length, 1, 1);
            break;
          case GDK_Page_Up:
            select_extend (private, playlist, length, 1, -private->rows);
            break;
          case GDK_Page_Down:
            select_extend (private, playlist, length, 1, private->rows);
            break;
          case GDK_Home:
            select_extend (private, playlist, length, 0, 0);
            break;
          case GDK_End:
            select_extend (private, playlist, length, 0, length - 1);
            break;
          default:
            return 0;
        }
        break;
      case GDK_CONTROL_MASK:
        switch (event->keyval)
        {
          case GDK_space:
            select_toggle (private, playlist, length, 1, 0);
            break;
          case GDK_Up:
            select_slide (private, length, 1, -1);
            break;
          case GDK_Down:
            select_slide (private, length, 1, 1);
            break;
          case GDK_Page_Up:
            select_slide (private, length, 1, -private->rows);
            break;
          case GDK_Page_Down:
            select_slide (private, length, 1, private->rows);
            break;
          case GDK_Home:
            select_slide (private, length, 0, 0);
            break;
          case GDK_End:
            select_slide (private, length, 0, length - 1);
            break;
          default:
            return 0;
        }
        break;
      case GDK_MOD1_MASK:
        switch (event->keyval)
        {
          case GDK_Up:
            select_move (private, playlist, length, 1, -1);
            break;
          case GDK_Down:
            select_move (private, playlist, length, 1, 1);
            break;
          case GDK_Page_Up:
            select_move (private, playlist, length, 1, -private->rows);
            break;
          case GDK_Page_Down:
            select_move (private, playlist, length, 1, private->rows);
            break;
          case GDK_Home:
            select_move (private, playlist, length, 0, 0);
            break;
          case GDK_End:
            select_move (private, playlist, length, 0, length - 1);
            break;
          default:
            return 0;
        }
        break;
      default:
        return 0;
    }

    playlistwin_update_list (playlist);
    return 1;
}

void ui_skinned_playlist_row_info (GtkWidget * widget, int * rows, int * first)
{
    UiSkinnedPlaylistPrivate * private;
    private = UI_SKINNED_PLAYLIST_GET_PRIVATE ((UiSkinnedPlaylist *) widget);

    * rows = private->rows;
    * first = private->first;
}

void ui_skinned_playlist_scroll_to (GtkWidget * widget, int row)
{
    UiSkinnedPlaylistPrivate * private;
    Playlist * playlist;
    int length;

    private = UI_SKINNED_PLAYLIST_GET_PRIVATE ((UiSkinnedPlaylist *) widget);
    playlist = aud_playlist_get_active ();
    length = aud_playlist_get_length (playlist);

    cancel_all (widget, private);

    private->first = row;

    if (private->first + private->rows > length)
        private->first = length - private->rows;
    if (private->first < 0)
        private->first = 0;

    playlistwin_update_list (playlist);
}

static gboolean ui_skinned_playlist_button_press (GtkWidget * widget, GdkEventButton * event)
{
    UiSkinnedPlaylistPrivate * private;
    Playlist * playlist;
    int length, position;

    private = UI_SKINNED_PLAYLIST_GET_PRIVATE ((UiSkinnedPlaylist *) widget);
    playlist = aud_playlist_get_active ();
    length = aud_playlist_get_length (playlist);

    cancel_all (widget, private);

    position = calc_position (private, length, event->y);

    switch (event->type)
    {
      case GDK_BUTTON_PRESS:
        switch (event->button)
        {
          case 1:
            if (position == -1 || position == length)
                return 1;

            if (event->state & GDK_SHIFT_MASK)
            {
                select_extend (private, playlist, length, 0, position);
                private->drag = DRAG_SELECT;
            }
            else if (event->state & GDK_CONTROL_MASK)
            {
                select_toggle (private, playlist, length, 0, position);
            }
            else
            {
                if (is_selected (playlist, position))
                {
                    select_slide (private, length, 0, position);
                    private->drag = DRAG_MOVE;
                }
                else
                {
                    select_single (private, playlist, length, 0, position);
                    private->drag = DRAG_SELECT;
                }
                private->drag = DRAG_SELECT;
            }

            break;
          case 3:
            if (event->state)
                return 1;

            if (position != -1 && position != length)
            {
                if (is_selected (playlist, position))
                    select_slide (private, length, 0, position);
                else
                    select_single (private, playlist, length, 0, position);
            }

            ui_manager_popup_menu_show ((GtkMenu *) playlistwin_popup_menu, event->x_root, event->y_root, 3, event->time);
            break;
          default:
            return 1;
        }

        break;
      case GDK_2BUTTON_PRESS:
        if (event->button != 1 || position == -1 || position == length)
            return 1;

        aud_playlist_set_position (playlist, position);
        audacious_drct_initiate ();
        break;
      default:
        return 1;
    }

    playlistwin_update_list (playlist);
    return 1;
}

static gboolean ui_skinned_playlist_button_release (GtkWidget * widget,
 GdkEventButton * event)
{
    cancel_all (widget, UI_SKINNED_PLAYLIST_GET_PRIVATE ((UiSkinnedPlaylist *)
     widget));
    return 1;
}

static gboolean scroll_cb (void * data)
{
    UiSkinnedPlaylistPrivate * private;
    Playlist * playlist;
    int length, position;

    private = (UiSkinnedPlaylistPrivate *) data;
    playlist = aud_playlist_get_active ();
    length = aud_playlist_get_length (playlist);

    position = adjust_position (private, length, 1, private->scroll);

    if (position == -1)
        return 1;

    switch (private->drag)
    {
      case DRAG_SELECT:
        select_extend (private, playlist, length, 0, position);
        break;
      case DRAG_MOVE:
        select_move (private, playlist, length, 0, position);
        break;
    }

    playlistwin_update_list (playlist);
    return 1;
}

static gboolean ui_skinned_playlist_motion_notify (GtkWidget * widget,
 GdkEventMotion * event)
{
    UiSkinnedPlaylistPrivate * private;
    Playlist * playlist;
    int length, position, new_scroll;

    private = UI_SKINNED_PLAYLIST_GET_PRIVATE ((UiSkinnedPlaylist *) widget);
    playlist = aud_playlist_get_active ();
    length = aud_playlist_get_length (playlist);

    position = calc_position (private, length, event->y);

    if (private->drag)
    {
        if (position == -1 || position == length)
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
                select_extend (private, playlist, length, 0, position);
                break;
              case DRAG_MOVE:
                select_move (private, playlist, length, 0, position);
                break;
            }

            playlistwin_update_list (playlist);
        }
    }
    else
    {
        if (position == -1 || position == length)
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

    return 1;
}

static gboolean ui_skinned_playlist_leave_notify (GtkWidget * widget,
 GdkEventCrossing * event)
{
    UiSkinnedPlaylistPrivate * private;

    private = UI_SKINNED_PLAYLIST_GET_PRIVATE ((UiSkinnedPlaylist *) widget);

    if (! private->drag)
        cancel_all (widget, private);

    return 1;
}

void ui_skinned_playlist_set_font(const gchar * font) {
    /* Welcome to bad hack central 2k3 */
    gchar *font_lower;
    gint width_temp;
    gint width_temp_0;

    playlist_list_font = pango_font_description_from_string(font);

    text_get_extents(font,
                     "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz ",
                     &width_approx_letters, NULL, &ascent, &descent);

    width_approx_letters = (width_approx_letters / 53);

    /* Experimental: We don't weigh the 1 into total because it's width is almost always
     * very different from the rest
     */
    text_get_extents(font, "023456789", &width_approx_digits, NULL, NULL,
                     NULL);
    width_approx_digits = (width_approx_digits / 9);

    /* Precache some often used calculations */
    width_approx_digits_half = width_approx_digits / 2;

    /* FIXME: We assume that any other number is broader than the "1" */
    text_get_extents(font, "1", &width_temp, NULL, NULL, NULL);
    text_get_extents(font, "2", &width_temp_0, NULL, NULL, NULL);

    if (abs(width_temp_0 - width_temp) < 2) {
        width_delta_digit_one = 0;
    }
    else {
        width_delta_digit_one = ((width_temp_0 - width_temp) / 2) + 2;
    }

    text_get_extents(font, ":", &width_colon, NULL, NULL, NULL);
    width_colon_third = width_colon / 4;

    font_lower = g_utf8_strdown(font, strlen(font));
    /* This doesn't take any i18n into account, but i think there is none with TTF fonts
     * FIXME: This can probably be retrieved trough Pango too
     */
    has_slant = g_strstr_len(font_lower, strlen(font_lower), "oblique")
        || g_strstr_len(font_lower, strlen(font_lower), "italic");

    g_free(font_lower);
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
        Tuple *tuple;
        Playlist *pl_active = aud_playlist_get_active();
        GtkWidget *popup = g_object_get_data(G_OBJECT(widget), "popup");

        tuple = aud_playlist_get_tuple(pl_active, pos);
        if ((tuple == NULL) || (aud_tuple_get_int(tuple, FIELD_LENGTH, NULL) < 1)) {
           gchar *title = aud_playlist_get_songtitle(pl_active, pos);
           audacious_fileinfopopup_show_from_title(popup, title);
           g_free(title);
        } else {
           audacious_fileinfopopup_show_from_tuple(popup , tuple);
        }
        g_object_set_data(G_OBJECT(widget), "popup_active" , GINT_TO_POINTER(1));
    }

    ui_skinned_playlist_popup_timer_stop(widget);
    return FALSE;
}

static void ui_skinned_playlist_popup_hide(GtkWidget *widget) {
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "popup_active")) == 1) {
        GtkWidget *popup = g_object_get_data(G_OBJECT(widget), "popup");
        g_object_set_data(G_OBJECT(widget), "popup_active", GINT_TO_POINTER(0));
        audacious_fileinfopopup_hide(popup, NULL);
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
