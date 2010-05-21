/*
 * Copyright (C) 2010 Audacious development team
 * Copyright (C) 2009 Richard Kettlewell
 *
 * Note that this license ONLY applies to multidrag.c and multidrag.h, not to
 * the rest of DisOrder.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/** @file disobedience/multidrag.c
 * @brief Drag multiple rows of a GtkTreeView
 *
 * Normally when you start a drag, GtkTreeView sets the selection to just row
 * you dragged from (because it can't cope with dragging more than one row at a
 * time).
 *
 * Disobedience needs more.
 *
 * Firstly it intercepts button-press-event and button-release event and for
 * clicks that might be the start of drags, suppresses changes to the
 * selection.  A consequence of this is that it needs to intercept
 * button-release-event too, to restore the effect of the click, if it turns
 * out not to be drag after all.
 *
 * The location of the initial click is stored in object data called @c
 * multidrag-where.
 *
 * Secondly it intercepts drag-begin and constructs an icon from the rows to be
 * dragged.
 *
 * Inspired by similar code in <a
 * href="http://code.google.com/p/quodlibet/">Quodlibet</a> (another software
 * jukebox, albeit as far as I can see a single-user one).
 */

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "multidrag.h"

static gboolean multidrag_selection_block(GtkTreeSelection __attribute__((unused)) *selection,
                                          GtkTreeModel __attribute__((unused)) *model,
                                          GtkTreePath __attribute__((unused)) *path,
                                          gboolean __attribute__((unused)) path_currently_selected,
                                          gpointer data)
{
    return *(const gboolean *)data;
}

static void block_selection(GtkWidget *w, gboolean block, gint x, gint y)
{
    static const gboolean which[] = {FALSE, TRUE};
    GtkTreeSelection *s = gtk_tree_view_get_selection(GTK_TREE_VIEW(w));
    gtk_tree_selection_set_select_function(s, multidrag_selection_block, (gboolean *)&which[!!block], NULL);

    /* Remember the pointer location */
    gint *where = g_object_get_data(G_OBJECT(w), "multidrag-where");

    if (!where)
    {
        where = g_new0(gint, 2);
        g_object_set_data_full(G_OBJECT(w), "multidrag-where", where, g_free);
    }

    where[0] = x;
    where[1] = y;
}

static gboolean multidrag_button_press_event(GtkWidget *w, GdkEventButton *event,
                                             gpointer __attribute__((unused)) user_data)
{
    /* By default we assume that anything this button press does should act as normal */
    block_selection(w, TRUE, -1, -1);

    /* We are only interested in left-button behavior */
    if (event->button != 1)
        return FALSE;

    /* We are only interested in unmodified clicks (not SHIFT etc) */
    /*if(event->state & GDK_MODIFIER_MASK)
        return FALSE;*/

    /* We are only interested if a well-defined path is clicked */
    GtkTreePath *path = NULL;

    if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(w), event->x, event->y, &path, NULL, NULL, NULL))
        return FALSE;

    /* We are only interested if a selected row is clicked */
    GtkTreeSelection *s = gtk_tree_view_get_selection(GTK_TREE_VIEW(w));

    if(gtk_tree_selection_path_is_selected(s, path))
    {
        /* We block subsequent selection changes and remember where the click was */
        block_selection(w, FALSE, event->x, event->y);
    }

    if(path)
        gtk_tree_path_free(path);

    return FALSE;
}

static gboolean multidrag_button_release_event(GtkWidget *w, GdkEventButton *event,
                                               gpointer __attribute__((unused)) user_data)
{
    gint *where = g_object_get_data(G_OBJECT(w), "multidrag-where");

    /* Did button-press-event do anything?  We just check the outcome rather than
     * going through all the conditions it tests. */
    if (where && where[0] != -1)
    {
        /* Remember where the down-click was */
        const gint x = where[0], y = where[1];

        /* Re-allow selections */
        block_selection(w, TRUE, -1, -1);

        if(x == event->x && y == event->y)
        {
            /* If the up-click is at the same location as the down-click,
             * it's not a drag. */
            GtkTreePath *path = NULL;
            GtkTreeViewColumn *col;

            if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(w), event->x, event->y, &path, &col, NULL, NULL))
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(w), path, col, FALSE);

            if(path)
                gtk_tree_path_free(path);
        }
    }
    return FALSE;
}

void make_treeview_multidrag(GtkWidget *w)
{
    g_signal_connect(w, "button-press-event",
                     G_CALLBACK(multidrag_button_press_event), NULL);

    g_signal_connect(w, "button-release-event",
                     G_CALLBACK(multidrag_button_release_event), NULL);
}

