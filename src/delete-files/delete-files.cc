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

#include <gio/gio.h>
#include <glib/gstdio.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#ifdef USE_GTK
#include <libaudgui/libaudgui-gtk.h>
#endif
#ifdef USE_QT
#include <QMessageBox>
#include <QPushButton>
#endif

class DeleteFiles : public GeneralPlugin
{
public:
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Delete Files"),
        PACKAGE,
        nullptr,
        & prefs
    };

    constexpr DeleteFiles () : GeneralPlugin (info, false) {}

    bool init ();
    void cleanup ();
};

EXPORT DeleteFiles aud_plugin_instance;

static constexpr AudMenuID menus[] = {
    AudMenuID::Main,
    AudMenuID::Playlist,
    AudMenuID::PlaylistRemove
};

#ifdef USE_GTK
static GtkWidget * dialog = nullptr;
#endif
#ifdef USE_QT
static QMessageBox * qdialog = nullptr;
#endif

static void move_to_trash (const char * filename)
{
    GFile * gfile = g_file_new_for_path (filename);
    GError * gerror = nullptr;

    if (! g_file_trash (gfile, nullptr, & gerror))
    {
        aud_ui_show_error (str_printf (_("Error moving %s to trash: %s."),
         filename, gerror->message));
        g_error_free (gerror);
    }

    g_object_unref ((GObject *) gfile);
}

static void really_delete (const char * filename)
{
    if (g_unlink (filename) < 0)
        aud_ui_show_error (str_printf (_("Error deleting %s: %s."), filename, strerror (errno)));
}

static void confirm_delete ()
{
    Index<String> files;

    auto playlist = Playlist::active_playlist ();
    int entry_count = playlist.n_entries ();

    for (int i = 0; i < entry_count; i ++)
    {
        if (playlist.entry_selected (i))
            files.append (playlist.entry_filename (i));
    }

    playlist.remove_selected ();

    for (const String & uri : files)
    {
        StringBuf filename = uri_to_filename (uri);

        if (filename)
        {
            if (aud_get_bool ("delete_files", "use_trash"))
                move_to_trash (filename);
            else
                really_delete (filename);
        }
        else
            aud_ui_show_error (str_printf
             (_("Error deleting %s: not a local file."), (const char *) uri));
    }
}

static void start_delete ()
{
    const char * message, * action, * icon;

    if (aud_get_bool ("delete_files", "use_trash"))
    {
        message = _("Do you want to move the selected files to the trash?");
        action = _("Move to Trash");
        icon = "user-trash";
    }
    else
    {
        message = _("Do you want to permanently delete the selected files?");
        action = _("Delete");
        icon = "edit-delete";
    }

#ifdef USE_GTK
    if (aud_get_mainloop_type () == MainloopType::GLib)
    {
        if (dialog)
        {
            gtk_window_present ((GtkWindow *) dialog);
            return;
        }

        auto button1 = audgui_button_new (action, icon, (AudguiCallback) confirm_delete, nullptr);
        auto button2 = audgui_button_new (_("Cancel"), "process-stop", nullptr, nullptr);

        dialog = audgui_dialog_new (GTK_MESSAGE_QUESTION, _("Delete Files"), message, button1, button2);

        g_signal_connect (dialog, "destroy", (GCallback) gtk_widget_destroyed, & dialog);
        gtk_widget_show_all (dialog);
    }
#endif
#ifdef USE_QT
    if (aud_get_mainloop_type () == MainloopType::Qt)
    {
        if (qdialog)
            return;

        qdialog = new QMessageBox;
        qdialog->setAttribute (Qt::WA_DeleteOnClose);
        qdialog->setIcon (QMessageBox::Question);
        qdialog->setWindowTitle (_("Delete Files"));
        qdialog->setText (message);

        auto remove = new QPushButton (action, qdialog);
        auto cancel = new QPushButton (_("Cancel"), qdialog);

        remove->setIcon (QIcon::fromTheme (icon));
        cancel->setIcon (QIcon::fromTheme ("process-stop"));

        qdialog->addButton (remove, QMessageBox::AcceptRole);
        qdialog->addButton (cancel, QMessageBox::RejectRole);

        QObject::connect (remove, & QPushButton::clicked, confirm_delete);
        QObject::connect (qdialog, & QObject::destroyed, [] () {
            qdialog = nullptr;
        });

        qdialog->show ();
    }
#endif
}

const char * const DeleteFiles::defaults[] = {
 "use_trash", "TRUE",
 nullptr};

bool DeleteFiles::init ()
{
#if ! GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    aud_config_set_defaults ("delete_files", defaults);

    for (AudMenuID menu : menus)
        aud_plugin_menu_add (menu, start_delete, _("Delete Selected Files"), "edit-delete");

    return true;
}

void DeleteFiles::cleanup ()
{
#ifdef USE_GTK
    if (dialog)
        gtk_widget_destroy (dialog);
#endif
#ifdef USE_QT
    delete qdialog;
#endif

    for (AudMenuID menu : menus)
        aud_plugin_menu_remove (menu, start_delete);
}

const PreferencesWidget DeleteFiles::widgets[] = {
    WidgetLabel (N_("<b>Delete Method</b>")),
    WidgetCheck (N_("Move to trash instead of deleting immediately"),
        WidgetBool ("delete_files", "use_trash"))
};

const PluginPreferences DeleteFiles::prefs = {{widgets}};
