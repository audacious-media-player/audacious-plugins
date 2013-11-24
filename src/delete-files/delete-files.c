/*
 * delete-files.c
 * Copyright 2013 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */


#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudgui/libaudgui-gtk.h>

static const int menus[] = {AUD_MENU_MAIN, AUD_MENU_PLAYLIST_RCLICK};

static GtkWidget * dialog = NULL;

static void confirm_delete (void)
{
    int playlist = aud_playlist_get_active ();
    int entry_count = aud_playlist_entry_count (playlist);

    for (int i = 0; i < entry_count; i ++)
    {
        if (! aud_playlist_entry_get_selected (playlist, i))
            continue;

        char * uri = aud_playlist_entry_get_filename (playlist, i);
        char * filename = uri_to_filename (uri);

        if (! filename)
        {
            SPRINTF (error, _("Error deleting %s: not a local file."), uri);
            aud_interface_show_error (error);
        }
        else if (unlink (filename) < 0)
        {
            SPRINTF (error, _("Error deleting %s: %s."), filename, strerror (errno));
            aud_interface_show_error (error);
        }

        str_unref (uri);
        free (filename);
    }

    aud_playlist_delete_selected (playlist);
}

static void start_delete (void)
{
    if (dialog)
    {
        gtk_window_present ((GtkWindow *) dialog);
        return;
    }

    GtkWidget * button1 = audgui_button_new (_("Delete"), "edit-delete",
     (AudguiCallback) confirm_delete, NULL);
    GtkWidget * button2 = audgui_button_new (_("Cancel"), "window-close", NULL, NULL);

    dialog = audgui_dialog_new (GTK_MESSAGE_QUESTION,
     _("Delete Files"), _("Do you want to permanently delete the selected "
     "files from the filesystem?"), button1, button2);

    g_signal_connect (dialog, "destroy", (GCallback) gtk_widget_destroyed, & dialog);
    gtk_widget_show_all (dialog);
}

static bool_t delete_files_init (void)
{
    for (int i = 0; i < G_N_ELEMENTS (menus); i ++)
        aud_plugin_menu_add (menus[i], start_delete, _("Delete from Filesystem"), "edit-delete");

    return TRUE;
}

static void delete_files_cleanup (void)
{
    if (dialog)
        gtk_widget_destroy (dialog);

    for (int i = 0; i < G_N_ELEMENTS (menus); i ++)
        aud_plugin_menu_remove (menus[i], start_delete);
}

AUD_GENERAL_PLUGIN
(
    .name = N_("Delete from Filesystem"),
    .domain = PACKAGE,
    .init = delete_files_init,
    .cleanup = delete_files_cleanup,
)
