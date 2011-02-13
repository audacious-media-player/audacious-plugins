/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2008  Audacious development team
 *
 *  Based on BMP:
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
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

#include <gtk/gtk.h>

#include <audacious/gtk-compat.h>
#include <libaudcore/hook.h>

#include "util.h"

GtkWidget *make_filebrowser(const gchar * title, gboolean save)
{
    GtkWidget *dialog;
    GtkWidget *button;

    g_return_val_if_fail(title != NULL, NULL);

    dialog = gtk_file_chooser_dialog_new(title, NULL, save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);

    button = gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

    gtk_button_set_use_stock(GTK_BUTTON(button), TRUE);
    gtk_widget_set_can_default (button, TRUE);

    button = gtk_dialog_add_button(GTK_DIALOG(dialog), save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT);

    gtk_button_set_use_stock(GTK_BUTTON(button), TRUE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);    /* centering */

    return dialog;
}

void check_set(GtkActionGroup * action_group, const gchar * action_name, gboolean is_on)
{
    GtkAction * action = gtk_action_group_get_action (action_group, action_name);

    g_return_if_fail (action != NULL);

    gtk_toggle_action_set_active ((GtkToggleAction *) action, is_on);
    hook_call (action_name, GINT_TO_POINTER (is_on));
}
