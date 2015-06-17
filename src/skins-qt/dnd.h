/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team
 *
 *  Based on BMP:
 *  Copyright (C) 2003-2004  BMP development team
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#ifndef SKINS_DND_H
#define SKINS_DND_H

/* Designate dropped data types that we know and care about */
enum {
    DROP_STRING,
    DROP_PLAINTEXT,
    DROP_URLENCODED,
    DROP_SKIN,
    DROP_FONT
};

#if 0
/* Drag data format listing for gtk_drag_dest_set() */
static const GtkTargetEntry drop_types[] = {
    {(char *) "text/plain", 0, DROP_PLAINTEXT},
    {(char *) "text/uri-list", 0, DROP_URLENCODED},
    {(char *) "STRING", 0, DROP_STRING},
    {(char *) "interface/x-winamp-skin", 0, DROP_SKIN},
    {(char *) "application/x-font-ttf", 0, DROP_FONT},
};

static inline void drag_dest_set (GtkWidget * widget)
{
    gtk_drag_dest_set (widget, (GtkDestDefaults) (GTK_DEST_DEFAULT_MOTION |
     GTK_DEST_DEFAULT_DROP), drop_types, aud::n_elems (drop_types),
     (GdkDragAction) (GDK_ACTION_COPY | GDK_ACTION_MOVE));
}
#endif

#endif /* SKINS_DND_H */
