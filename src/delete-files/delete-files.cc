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

static bool delete_uri (const char * uri, bool use_trash)
{
    GFile * gfile = g_file_new_for_uri (uri);
    GError * gerror = nullptr;

    bool success = use_trash ?
     g_file_trash (gfile, nullptr, & gerror) :
     g_file_delete (gfile, nullptr, & gerror);

    if (! success)
    {
        aud_ui_show_error (gerror->message);
        g_error_free (gerror);
    }

    g_object_unref ((GObject *) gfile);
    return success;
}

class DeleteOperation
{
public:
    DeleteOperation (Playlist playlist) :
        m_playlist (playlist),
        m_use_trash (aud_get_bool ("delete_files", "use_trash"))
    {
        int num_entries = playlist.n_entries ();
        for (int i = 0; i < num_entries; i ++)
        {
            if (playlist.entry_selected (i))
                m_files.append (playlist.entry_filename (i));
        }
    }

    StringBuf prompt () const
    {
        StringBuf buf;

        if (! m_files.len ())
        {
            buf.insert (-1, _("No files are selected."));
        }
        else if (m_files.len () == 1)
        {
            const char * format = m_use_trash ?
             _("Do you want to move %s to the trash?") :
             _("Do you want to permanently delete %s?");
            StringBuf display = uri_to_display (m_files[0]);
            str_append_printf (buf, format, (const char *) display);
        }
        else
        {
            const char * format = m_use_trash ?
             _("Do you want to move %d files to the trash?") :
             _("Do you want to permanently delete %d files?");
            str_append_printf (buf, format, m_files.len ());
        }

        return buf;
    }

    const char * action () const
    {
        if (! m_files.len ())
            return nullptr;

        return m_use_trash ? _("Move to trash") : _("Delete");
    }

    const char * icon () const
    {
        if (! m_files.len ())
            return nullptr;

        return m_use_trash ? "user-trash" : "edit-delete";
    }

    void run () const
    {
        /* work around -Wignored-attributes on MinGW */
        auto string_compare = [] (const String & a, const String & b)
            { return strcmp (a, b); };

        Index<String> deleted;

        for (auto & uri: m_files)
        {
            if (delete_uri (uri, m_use_trash))
                deleted.append (uri);
        }

        deleted.sort (string_compare);

        /* make sure selection matches what we actually deleted */
        int num_entries = m_playlist.n_entries ();
        for (int i = 0; i < num_entries; i++)
        {
            int j = deleted.bsearch (m_playlist.entry_filename (i), string_compare);
            m_playlist.select_entry (i, (j >= 0));
        }

        m_playlist.remove_selected ();
    }

private:
    const Playlist m_playlist;
    const bool m_use_trash;
    Index<String> m_files;
};

static void start_delete ()
{
    auto op = new DeleteOperation (Playlist::active_playlist ());

    StringBuf prompt = op->prompt ();
    const char * action = op->action ();
    const char * icon = op->icon ();

    if (! action)
    {
        aud_ui_show_error (prompt);
        delete op;
        return;
    }

#ifdef USE_GTK
    if (aud_get_mainloop_type () == MainloopType::GLib)
    {
        if (dialog)
            gtk_widget_destroy (dialog);

        auto run_cb = aud::obj_member<DeleteOperation, & DeleteOperation::run>;
        auto button1 = audgui_button_new (action, icon, run_cb, op);
        auto button2 = audgui_button_new (_("Cancel"), "process-stop", nullptr, nullptr);

        dialog = audgui_dialog_new (GTK_MESSAGE_QUESTION, _("Delete Files"), prompt, button1, button2);

        g_signal_connect (dialog, "destroy", (GCallback) gtk_widget_destroyed, & dialog);
        g_signal_connect_swapped (dialog, "destroy", (GCallback) aud::delete_obj<DeleteOperation>, op);
        gtk_widget_show_all (dialog);
    }
#endif
#ifdef USE_QT
    if (aud_get_mainloop_type () == MainloopType::Qt)
    {
        delete qdialog;

        qdialog = new QMessageBox;
        qdialog->setAttribute (Qt::WA_DeleteOnClose);
        qdialog->setIcon (QMessageBox::Question);
        qdialog->setWindowTitle (_("Delete Files"));
        qdialog->setText ((const char *) prompt);

        auto remove = new QPushButton (action, qdialog);
        auto cancel = new QPushButton (_("Cancel"), qdialog);

        remove->setIcon (QIcon::fromTheme (icon));
        cancel->setIcon (QIcon::fromTheme ("process-stop"));

        qdialog->addButton (remove, QMessageBox::AcceptRole);
        qdialog->addButton (cancel, QMessageBox::RejectRole);

        QObject::connect (remove, & QPushButton::clicked, [op] () {
            op->run ();
        });
        QObject::connect (qdialog, & QObject::destroyed, [op] () {
            qdialog = nullptr;
            delete op;
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
