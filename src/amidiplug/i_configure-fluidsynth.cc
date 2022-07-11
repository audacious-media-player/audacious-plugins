/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#include "i_configure-fluidsynth.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/index.h>

#include <glib/gstdio.h>

#include "i_configure.h"

#ifdef USE_GTK

#include <gtk/gtk.h>

enum
{
    LISTSFONT_FILENAME_COLUMN = 0,
    LISTSFONT_FILESIZE_COLUMN,
    LISTSFONT_N_COLUMNS
};

static void i_configure_ev_sflist_commit (void * sfont_lv);

void i_configure_ev_sflist_add (void * sfont_lv)
{
    GtkWidget * parent_window = gtk_widget_get_toplevel ((GtkWidget *) sfont_lv);

    if (gtk_widget_is_toplevel (parent_window))
    {
        GtkTreeSelection * listsel = gtk_tree_view_get_selection (GTK_TREE_VIEW (sfont_lv));
        GtkTreeIter itersel, iterapp;
        GtkWidget * browse_dialog = gtk_file_chooser_dialog_new (_("AMIDI-Plug - select SoundFont file"),
                                    GTK_WINDOW (parent_window),
                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                    _("_Cancel"), GTK_RESPONSE_CANCEL,
                                    _("_Open"), GTK_RESPONSE_ACCEPT, nullptr);

        if (gtk_tree_selection_get_selected (listsel, nullptr, &itersel))
        {
            char * selfilename = nullptr, *selfiledir = nullptr;
            GtkTreeModel * store = gtk_tree_view_get_model (GTK_TREE_VIEW (sfont_lv));
            gtk_tree_model_get (GTK_TREE_MODEL (store), &itersel, LISTSFONT_FILENAME_COLUMN, &selfilename, -1);
            selfiledir = g_path_get_dirname (selfilename);
            gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (browse_dialog), selfiledir);
            g_free (selfiledir);
            g_free (selfilename);
        }

        if (gtk_dialog_run (GTK_DIALOG (browse_dialog)) == GTK_RESPONSE_ACCEPT)
        {
            GStatBuf finfo;
            GtkTreeModel * store = gtk_tree_view_get_model (GTK_TREE_VIEW (sfont_lv));
            char * filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (browse_dialog));
            int filesize = -1;

            if (g_stat (filename, &finfo) == 0)
                filesize = finfo.st_size;

            gtk_list_store_append (GTK_LIST_STORE (store), &iterapp);
            gtk_list_store_set (GTK_LIST_STORE (store), &iterapp,
                                LISTSFONT_FILENAME_COLUMN, filename,
                                LISTSFONT_FILESIZE_COLUMN, filesize, -1);

            g_free (filename);
        }

        gtk_widget_destroy (browse_dialog);
    }

    i_configure_ev_sflist_commit (sfont_lv);
}


void i_configure_ev_sflist_rem (void * sfont_lv)
{
    GtkTreeModel * store;
    GtkTreeIter iter;
    GtkTreeSelection * listsel = gtk_tree_view_get_selection (GTK_TREE_VIEW (sfont_lv));

    if (gtk_tree_selection_get_selected (listsel, &store, &iter))
        gtk_list_store_remove (GTK_LIST_STORE (store), &iter);

    i_configure_ev_sflist_commit (sfont_lv);
}


void i_configure_ev_sflist_swap (GtkWidget * button, void * sfont_lv)
{
    GtkTreeModel * store;
    GtkTreeIter iter;
    GtkTreeSelection * listsel = gtk_tree_view_get_selection (GTK_TREE_VIEW (sfont_lv));

    if (gtk_tree_selection_get_selected (listsel, &store, &iter))
    {
        unsigned swapdire = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "swapdire"));

        if (swapdire == 0)   /* move up */
        {
            GtkTreePath * treepath = gtk_tree_model_get_path (store, &iter);

            if (gtk_tree_path_prev (treepath))
            {
                GtkTreeIter iter_prev;

                if (gtk_tree_model_get_iter (store, &iter_prev, treepath))
                    gtk_list_store_swap (GTK_LIST_STORE (store), &iter, &iter_prev);
            }

            gtk_tree_path_free (treepath);
        }
        else /* move down */
        {
            GtkTreeIter iter_prev = iter;

            if (gtk_tree_model_iter_next (store, &iter))
                gtk_list_store_swap (GTK_LIST_STORE (store), &iter, &iter_prev);
        }
    }

    i_configure_ev_sflist_commit (sfont_lv);
}


void i_configure_ev_sflist_commit (void * sfont_lv)
{
    GtkTreeIter iter;
    GtkTreeModel * store = gtk_tree_view_get_model (GTK_TREE_VIEW (sfont_lv));
    GString * sflist_string = g_string_new ("");

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter) == true)
    {
        gboolean iter_is_valid = false;

        do
        {
            char * fname;
            gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                                LISTSFONT_FILENAME_COLUMN, &fname, -1);
            g_string_prepend_c (sflist_string, ';');
            g_string_prepend (sflist_string, fname);
            g_free (fname);
            iter_is_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
        }
        while (iter_is_valid == true);
    }

    if (sflist_string->len > 0)
        g_string_truncate (sflist_string, sflist_string->len - 1);

    aud_set_str ("amidiplug", "fsyn_soundfont_file", sflist_string->str);

    g_string_free (sflist_string, true);

    /* reset backend at beginning of next song to apply changes */
    __sync_bool_compare_and_swap (& backend_settings_changed, false, true);
}


void * create_soundfont_list ()
{
        GtkListStore * soundfont_file_store;
        GtkCellRenderer * soundfont_file_lv_text_rndr;
        GtkTreeViewColumn * soundfont_file_lv_fname_col, *soundfont_file_lv_fsize_col;
        GtkWidget * soundfont_file_hbox, *soundfont_file_lv, *soundfont_file_lv_sw;
        GtkTreeSelection * soundfont_file_lv_sel;
        GtkWidget * soundfont_file_bbox_vbox, *soundfont_file_bbox_addbt, *soundfont_file_bbox_rembt;
        GtkWidget * soundfont_file_bbox_mvupbt, *soundfont_file_bbox_mvdownbt;

        /* soundfont settings - soundfont files - listview */
        soundfont_file_store = gtk_list_store_new (LISTSFONT_N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

        String soundfont_file = aud_get_str ("amidiplug", "fsyn_soundfont_file");

        if (soundfont_file[0])
        {
            /* fill soundfont list with fsyn_soundfont_file information */
            char ** sffiles = g_strsplit (soundfont_file, ";", 0);
            GtkTreeIter iter;
            int i = 0;

            while (sffiles[i] != nullptr)
            {
                int filesize = -1;
                GStatBuf finfo;

                if (g_stat (sffiles[i], &finfo) == 0)
                    filesize = finfo.st_size;

                gtk_list_store_prepend (GTK_LIST_STORE (soundfont_file_store), &iter);
                gtk_list_store_set (GTK_LIST_STORE (soundfont_file_store), &iter,
                                    LISTSFONT_FILENAME_COLUMN, sffiles[i],
                                    LISTSFONT_FILESIZE_COLUMN, filesize, -1);
                i++;
            }

            g_strfreev (sffiles);
        }

        soundfont_file_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
        soundfont_file_lv = gtk_tree_view_new_with_model (GTK_TREE_MODEL (soundfont_file_store));
        g_object_unref (soundfont_file_store);
        soundfont_file_lv_text_rndr = gtk_cell_renderer_text_new();
        soundfont_file_lv_fname_col = gtk_tree_view_column_new_with_attributes (
                                          _("File name"), soundfont_file_lv_text_rndr, "text",
                                          LISTSFONT_FILENAME_COLUMN, nullptr);
        gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (soundfont_file_lv_fname_col), true);
        soundfont_file_lv_fsize_col = gtk_tree_view_column_new_with_attributes (
                                          _("Size (bytes)"), soundfont_file_lv_text_rndr, "text",
                                          LISTSFONT_FILESIZE_COLUMN, nullptr);
        gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (soundfont_file_lv_fsize_col), false);
        gtk_tree_view_append_column (GTK_TREE_VIEW (soundfont_file_lv), soundfont_file_lv_fname_col);
        gtk_tree_view_append_column (GTK_TREE_VIEW (soundfont_file_lv), soundfont_file_lv_fsize_col);
        soundfont_file_lv_sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (soundfont_file_lv));
        gtk_tree_selection_set_mode (GTK_TREE_SELECTION (soundfont_file_lv_sel), GTK_SELECTION_SINGLE);

        soundfont_file_lv_sw = gtk_scrolled_window_new (nullptr, nullptr);
        gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) soundfont_file_lv_sw, GTK_SHADOW_IN);
        gtk_scrolled_window_set_policy ((GtkScrolledWindow *) soundfont_file_lv_sw,
                                         GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add (GTK_CONTAINER (soundfont_file_lv_sw), soundfont_file_lv);

        /* soundfont settings - soundfont files - buttonbox */
        soundfont_file_bbox_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        soundfont_file_bbox_addbt = gtk_button_new();
        gtk_button_set_image (GTK_BUTTON (soundfont_file_bbox_addbt),
                              gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_MENU));
        g_signal_connect_swapped (G_OBJECT (soundfont_file_bbox_addbt), "clicked",
                                  G_CALLBACK (i_configure_ev_sflist_add), soundfont_file_lv);
        gtk_box_pack_start (GTK_BOX (soundfont_file_bbox_vbox), soundfont_file_bbox_addbt, false, false, 0);
        soundfont_file_bbox_rembt = gtk_button_new();
        gtk_button_set_image (GTK_BUTTON (soundfont_file_bbox_rembt),
                              gtk_image_new_from_icon_name ("list-remove", GTK_ICON_SIZE_MENU));
        g_signal_connect_swapped (G_OBJECT (soundfont_file_bbox_rembt), "clicked",
                                  G_CALLBACK (i_configure_ev_sflist_rem), soundfont_file_lv);
        gtk_box_pack_start (GTK_BOX (soundfont_file_bbox_vbox), soundfont_file_bbox_rembt, false, false, 0);
        soundfont_file_bbox_mvupbt = gtk_button_new();
        gtk_button_set_image (GTK_BUTTON (soundfont_file_bbox_mvupbt),
                              gtk_image_new_from_icon_name ("go-up", GTK_ICON_SIZE_MENU));
        g_object_set_data (G_OBJECT (soundfont_file_bbox_mvupbt), "swapdire", GUINT_TO_POINTER (0));
        g_signal_connect (G_OBJECT (soundfont_file_bbox_mvupbt), "clicked",
                          G_CALLBACK (i_configure_ev_sflist_swap), soundfont_file_lv);
        gtk_box_pack_start (GTK_BOX (soundfont_file_bbox_vbox), soundfont_file_bbox_mvupbt, false, false, 0);
        soundfont_file_bbox_mvdownbt = gtk_button_new();
        gtk_button_set_image (GTK_BUTTON (soundfont_file_bbox_mvdownbt),
                              gtk_image_new_from_icon_name ("go-down", GTK_ICON_SIZE_MENU));
        g_object_set_data (G_OBJECT (soundfont_file_bbox_mvdownbt), "swapdire", GUINT_TO_POINTER (1));
        g_signal_connect (G_OBJECT (soundfont_file_bbox_mvdownbt), "clicked",
                          G_CALLBACK (i_configure_ev_sflist_swap), soundfont_file_lv);
        gtk_box_pack_start (GTK_BOX (soundfont_file_bbox_vbox), soundfont_file_bbox_mvdownbt, false, false, 0);
        gtk_box_pack_start (GTK_BOX (soundfont_file_hbox), soundfont_file_lv_sw, true, true, 0);
        gtk_box_pack_start (GTK_BOX (soundfont_file_hbox), soundfont_file_bbox_vbox, false, false, 0);

        return soundfont_file_hbox;
}

#endif // USE_GTK

#ifdef USE_QT

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QAbstractListModel>
#include <QTreeView>
#include <QPushButton>

#include <libaudqt/libaudqt.h>

/* soundfont settings - soundfont files - listview */
class SoundFontListModel : public QAbstractListModel
{
public:
    enum {
        FileName = 0,
        FileSize,
        NColumns
    };

    void update ();
    void commit ();
    void delete_selected (QModelIndexList);
    void shift_selected (QModelIndexList, int);
    void append (const char *);

    SoundFontListModel (QObject * parent = nullptr) : QAbstractListModel (parent) { update (); }
    ~SoundFontListModel ();

protected:
    int rowCount (const QModelIndex & parent) const
    {
        return parent.isValid() ? 0 : m_file_names.len();
    }

    int columnCount (const QModelIndex &) const { return NColumns; }
    QVariant data (const QModelIndex & index, int role) const;
    QVariant headerData (int section, Qt::Orientation, int role) const;

    Qt::ItemFlags flags (const QModelIndex & index) const
    {
        if (index.isValid ())
            return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
        else
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }

private:
    Index <String> m_file_names;
    Index <int> m_file_sizes;
};

SoundFontListModel::~SoundFontListModel ()
{
    m_file_names.clear ();
    m_file_sizes.clear ();
}

void SoundFontListModel::append (const char * filename)
{
    QAbstractItemModel::beginResetModel ();

    int filesize = -1;
    GStatBuf finfo;

    if (g_stat (filename, &finfo) == 0)
        filesize = finfo.st_size;

    m_file_names.append (String (filename));
    m_file_sizes.append (filesize);

    commit ();
    QAbstractItemModel::endResetModel ();
}

void SoundFontListModel::update ()
{
    String soundfont_file = aud_get_str ("amidiplug", "fsyn_soundfont_file");

    if (soundfont_file[0])
    {
        char ** sffiles = g_strsplit (soundfont_file, ";", 0);
        int i = 0;

        while (sffiles[i] != nullptr)
        {
            append (sffiles[i]);
            i++;
        }

        g_strfreev (sffiles);
    }
}

void SoundFontListModel::commit ()
{
    std::string sflist_string;

    for (auto str : m_file_names)
    {
        if (sflist_string[0])
            sflist_string.append (";");

        sflist_string.append (str);
    }

    aud_set_str ("amidiplug", "fsyn_soundfont_file", sflist_string.c_str ());

    /* reset backend at beginning of next song to apply changes */
    __sync_bool_compare_and_swap (& backend_settings_changed, false, true);
}

void SoundFontListModel::delete_selected (QModelIndexList selections)
{
    if (selections.empty ())
        return;

    QAbstractItemModel::beginResetModel ();

    auto index = selections.first ();

    m_file_names.remove (index.row (), 1);
    m_file_sizes.remove (index.row (), 1);

    commit ();
    QAbstractItemModel::endResetModel ();
}

void SoundFontListModel::shift_selected (QModelIndexList selections, int direction)
{
    if (selections.empty ())
        return;

    QAbstractItemModel::beginResetModel ();

    auto index = selections.first ();

    int from_row = index.row ();
    int to_row = index.row () + direction;

    if (to_row < 0)
        return;

    String filename[2] = { m_file_names[from_row], m_file_names[to_row] };
    int filesize[2] = { m_file_sizes[from_row], m_file_sizes[to_row] };

    m_file_names[from_row] = filename[1];
    m_file_names[to_row] = filename[0];

    m_file_sizes[from_row] = filesize[1];
    m_file_sizes[to_row] = filesize[0];

    commit ();
    QAbstractItemModel::endResetModel ();
}

QVariant SoundFontListModel::data (const QModelIndex & index, int role) const
{
    int col = index.column ();
    if (col < 0 || col >= NColumns)
        return QVariant ();

    switch (role)
    {
    case Qt::DisplayRole:
        switch (col)
        {
        case FileName:
            return QString ((const char *) m_file_names[index.row ()]);

        case FileSize:
            return QString (int_to_str (m_file_sizes[index.row ()]));

        default:
            return QVariant ();
        }

        break;
    default:
        break;
    }

    return QVariant ();
}

QVariant SoundFontListModel::headerData (int section, Qt::Orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant ();

    switch (section) {
    case FileName:
        return QString (_("File name"));

    case FileSize:
        return QString (_("Size (bytes)"));

    default:
        return QVariant ();
    }
}

class SoundFontWidget : public QWidget
{
public:
    SoundFontWidget (QWidget * parent = nullptr);

private:
    QVBoxLayout * m_vbox_layout;
    QTreeView * m_view;
    SoundFontListModel * m_model;
    QWidget * m_bbox;
    QHBoxLayout * m_bbox_layout;
    QPushButton * m_button_sf_add;
    QPushButton * m_button_sf_del;
    QPushButton * m_button_sf_up;
    QPushButton * m_button_sf_down;
};

SoundFontWidget::SoundFontWidget (QWidget * parent) :
    QWidget (parent),
    m_vbox_layout (audqt::make_vbox (this)),
    m_view (new QTreeView (this)),
    m_model (new SoundFontListModel (m_view)),
    m_bbox (new QWidget (this)),
    m_bbox_layout (audqt::make_hbox (m_bbox)),
    m_button_sf_add (new QPushButton (m_bbox)),
    m_button_sf_del (new QPushButton (m_bbox)),
    m_button_sf_up (new QPushButton (m_bbox)),
    m_button_sf_down (new QPushButton (m_bbox))
{
    m_button_sf_add->setIcon (QIcon::fromTheme ("list-add"));
    m_button_sf_del->setIcon (QIcon::fromTheme ("list-remove"));
    m_button_sf_up->setIcon (QIcon::fromTheme ("go-up"));
    m_button_sf_down->setIcon (QIcon::fromTheme ("go-down"));

    m_bbox_layout->addWidget (m_button_sf_add);
    m_bbox_layout->addWidget (m_button_sf_del);
    m_bbox_layout->addWidget (m_button_sf_up);
    m_bbox_layout->addWidget (m_button_sf_down);

    m_bbox->setLayout (m_bbox_layout);

    m_view->setModel (m_model);
    m_view->setRootIsDecorated (false);

    m_vbox_layout->addWidget (m_view);
    m_vbox_layout->addWidget (m_bbox);

    setLayout (m_vbox_layout);

    QObject::connect (m_button_sf_add, & QPushButton::clicked, [this] () {
        auto dialog = new QFileDialog (this, _("AMIDI-Plug - select SoundFont file"));
        dialog->setFileMode (QFileDialog::ExistingFiles);
        audqt::window_bring_to_front (dialog);

        QObject::connect (dialog, & QFileDialog::accepted, [this, dialog] () {
            for (auto & fn : dialog->selectedFiles ())
                m_model->append (fn.toUtf8 ());
        });
    });

    QObject::connect (m_button_sf_del, & QPushButton::clicked, [=] () {
        m_model->delete_selected (m_view->selectionModel ()->selectedIndexes ());
    });

    QObject::connect (m_button_sf_up, & QPushButton::clicked, [=] () {
        m_model->shift_selected (m_view->selectionModel ()->selectedIndexes (), -1);
    });

    QObject::connect (m_button_sf_down, & QPushButton::clicked, [=] () {
        m_model->shift_selected (m_view->selectionModel ()->selectedIndexes (), 1);
    });
}

void * create_soundfont_list_qt ()
{
    return new SoundFontWidget;
}

#endif // USE_QT
