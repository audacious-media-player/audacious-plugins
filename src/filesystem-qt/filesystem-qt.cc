/*
 * playlist-manager-qt.cc
 * Copyright 2015 John Lindgren
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

//#define FS_TEST_VERSION

#ifdef FS_TEST_VERSION
#define PACKAGE "audacious-test"
#define EXPORT
#endif

#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QFont>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QMouseEvent>
#include <QPointer>
#include <QToolButton>
#include <QString>
#include <QDir>
#include <QMenu>
#include <QUrl>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QSpacerItem>
#include <QScrollBar>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#ifndef FS_TEST_VERSION
#include <libaudqt/libaudqt.h>
#endif
#include <libaudqt/treeview.h>

//#include "../ui-common/qt-compat.h"


class FilesystemQt : public GeneralPlugin
{
public:
    static const char * const defaults[];
    static const char about[];

    static constexpr PluginInfo info = {N_("Filesystem Manager"), PACKAGE,
                                        about, // about
                                        nullptr, // prefs
                                        PluginQtOnly};

    constexpr FilesystemQt() : GeneralPlugin(info, false) {}

    void * get_qt_widget();
    int take_message(const char * code, const void *, int);
};

const char FilesystemQt::about[] =
    N_("(p) 2025 Hans Dijkema\n\n"
       "The FilesystemQt plugin gives a dockable widget that can be used\n"
       "to browse folders for music files (mp3|flac|ogg)\n"
       "\n"
       "Right mouse on a folder or file and these can be added to, or replace\n"
       "the current playlist."
       );

EXPORT FilesystemQt aud_plugin_instance;

#ifdef FS_TEST_VERSION
QWidget *pluginWidget()
{
    return (QWidget *) aud_plugin_instance.get_qt_widget();
}
#endif

class FilesystemTree
{
    public:
        static const int DIR = 1;
        static const int FILE = 2;
        static QString _sep;
    private:
        QFileInfo                       _fi;
        int                             _kind;
        QList<FilesystemTree *>         _entries;
        FilesystemTree                 *_parent;
        int                             _parent_index;
        bool                            _loaded;
    private:
        void clearList() {
            int i;
            for(i = 0; i < _entries.size(); i++) {
                FilesystemTree *t = _entries[i];
                delete t;
            }
            _entries.clear();
        }
    public:
        FilesystemTree(int kind, const QString & p, int idx, FilesystemTree *parent)
        {
            _sep = "/";
            _fi = QFileInfo(p);
            _kind = kind;
            _parent_index = idx;
            _parent = parent;
            _loaded = false;
            //qDebug() << _fi << " - " << path() << " - " << _kind;
        }

        ~FilesystemTree() {
            clearList();
        }
    public:
        int nItems() { load();return _entries.size(); }
        QString name() { load();return _fi.fileName(); }
        QString path() const{ return _fi.absoluteFilePath(); }
        int kind() const { return _kind; }
        int isDir() { return _kind == DIR; }
        int isFile() { return _kind == FILE; }
        FilesystemTree *entry(int row) {
            load();
            if (row >= _entries.size()) {
                qDebug() << "Unexpected: " << row << " - " << _entries.size();
                return nullptr;
            }
            return _entries[row];
        }

    public:
        void setPath(const QString &p)
        {
            _fi = QFileInfo(p);
            reload();
        }
    public:
        void reload() {
            _loaded = false;
            load();
        }
        void load() {
            if (_loaded) { return; }

            _loaded = true;
            if (_kind == DIR) {
                clearList();

                QDir d(_fi.absoluteFilePath());

                QStringList filters;
                filters << "*.mp3" << "*.flac" << "*.ogg";
                d.setNameFilters(filters);

                QDir::Filters d_filters = QDir::AllDirs | QDir::NoDot | QDir::NoDotDot | QDir::Files | QDir::Readable;
                d.setFilter(d_filters);

                QStringList l = d.entryList(filters, d_filters, QDir::SortFlag::IgnoreCase);

                QString p = path();
                int i;
                for(i = 0; i < l.size(); i++) {
                    QString new_file = p + _sep + l[i];
                    QFileInfo f(new_file);
                    int k = (f.isDir()) ? DIR : FILE;
                    //qDebug() << i << " - " << new_file << " - " << k;
                    _entries.append(new FilesystemTree(k, new_file, i, this));
                }
            }
        }
    public:
        FilesystemTree *parent() const { return _parent; }
        int             parentIndex() const { return _parent_index; }

};

QString FilesystemTree::_sep;


class FilesystemModel : public QAbstractItemModel
{
private:
	QString 		 _base_dir;
        FilesystemTree          *_tree;
	
public:
    enum
    {
        ColumnTitle,
        NColumns
    };

    FilesystemModel()
    {
        _base_dir = "";
        _tree = new FilesystemTree(FilesystemTree::DIR, _base_dir, -1, nullptr);
    }
    
    ~FilesystemModel()
    {
        delete _tree;
    }

public:
    void setLibraryPath(const QString &library_path)
    {
        _base_dir = library_path;
        if (_base_dir != _tree->path()) {
            this->beginResetModel();
            _tree->setPath(_base_dir);
            this->endResetModel();
        }
    }

    QModelIndex index(int row, int column, const QModelIndex &parent) const
    {
        if (parent.isValid()) {
            FilesystemTree *t = (FilesystemTree *) parent.internalPointer();
            //qDebug() << "index: " << row << " - " << column << " - " << t->path();
            int parent_row = parent.row();
            return createIndex(row, column, t->entry(parent_row));
        } else {
            return createIndex(row, column, _tree);
        }
    }

    QList<FilesystemTree *> get(QModelIndexList s)
    {
        QList<FilesystemTree *> l;
        int i;
        for(i = 0; i < s.size(); i++) {
            QModelIndex idx = s[i];
            if (idx.isValid()) {
                int row = idx.row();
                FilesystemTree *t = (FilesystemTree *) idx.internalPointer();
                FilesystemTree *entry = t->entry(row);
                l.append(entry);
            }
        }
        return l;
    }

    bool hasChildren(const QModelIndex &parent) const
    {
        if (parent.isValid()) {
            FilesystemTree * t;
            t = (FilesystemTree *) parent.internalPointer();
            int row = parent.row();
            return t->entry(row)->nItems() > 0;
            //return t->nItems() > 0;
        } else {
            return _tree->nItems() > 0;
        }
    }

    int rowCount(const QModelIndex &parent) const
    {
        if (parent.isValid()) {
            FilesystemTree * t = (FilesystemTree *) parent.internalPointer();
            int row = parent.row();
            return t->entry(row)->nItems();
        } else {
            return _tree->nItems();
        }
    }

    int columnCount(const QModelIndex &parent) const
    {
        return NColumns;
    }

    QVariant data(const QModelIndex &index, int role) const
    {
        if (index.isValid()) {
            if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
                int row = index.row();
                int column = index.column();
                FilesystemTree *t = (FilesystemTree *) index.internalPointer();
                FilesystemTree *entry = t->entry(row);
                if (entry == nullptr) {
                    return QVariant();
                }
                if (column == ColumnTitle) {
                    QString name = entry->name();
                    if (entry->kind() == FilesystemTree::DIR) {
                        name += QString::asprintf(" (%d)", entry->nItems());
                    }
                    return name;
                }
            }
        }

        return QVariant();
    }

    QModelIndex parent(const QModelIndex &child) const
    {
        if (child.isValid()) {
            FilesystemTree *t = (FilesystemTree *) child.internalPointer();
            if (t->parent() == nullptr) {
                return QModelIndex();
            } else {
                return createIndex(t->parentIndex(), 0, t->parent());
            }
        } else {
            return QModelIndex();
        }
    }
};

class FilesystemView : public audqt::TreeView
{
private:
    QList<FilesystemTree *> current_selected;

    // Preferences
private:
    int max_files_to_add;
    QString library_path;

public:
    FilesystemView();

public slots:
    void configMusicLibrary();
    void configMaxFiles();

public:
    void connectConfigButtons(QPushButton *configLib, QPushButton *configMaxFiles);

private:
    FilesystemModel m_model;

protected:
    void contextMenuEvent(QContextMenuEvent *event);

public:
    void playThis(bool checked);
    void addThis(bool checked);
    void insertEntries(Playlist &list, bool play);

protected slots:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
};

void FilesystemView::contextMenuEvent(QContextMenuEvent *evt)
{
    QMenu *menu = new QMenu(this);
    QString play = QString("Replace current playlist && play");
    QString add = QString("Add to current playlist");
    QAction *action_play = new QAction(play);
    QAction *action_add = new QAction(add);
    connect(action_play, &QAction::triggered, this, &FilesystemView::playThis);
    connect(action_add, &QAction::triggered, this, &FilesystemView::addThis);
    menu->addAction(action_play);
    menu->addAction(action_add);
    menu->popup(evt->globalPos());
}

void assembleFiles(FilesystemTree *e, QStringList &l, int & count, int max)
{
    if (e->isFile()) {
        l.append(e->path());
        count += 1;
    } else {
        int i, N;
        for(i = 0, N = e->nItems(); i < N && count < max; i++) {
            assembleFiles(e->entry(i), l, count, max);
        }
    }
}

void FilesystemView::playThis(bool checked)
{
    //qDebug() << "REPLACE";
#ifndef FS_TEST_VERSION
    auto list = Playlist::active_playlist();
    list.remove_all_entries();
    insertEntries(list, true);
#endif
}

void FilesystemView::addThis(bool checked)
{
#ifndef FS_TEST_VERSION
    //qDebug() << "ADD";
    auto list = Playlist::active_playlist();
    insertEntries(list, false);
#endif
}

void FilesystemView::insertEntries(Playlist &list, bool do_play)
{
    QItemSelection sel = this->selectionModel()->selection();
    QModelIndexList l = sel.indexes();
    current_selected = m_model.get(l);

    QStringList files;
    int count = 0;
    int i;
    for(i = 0; i < current_selected.size() && count < max_files_to_add; i++) {
        assembleFiles(current_selected[i], files, count, max_files_to_add);
    }

#ifndef FS_TEST_VERSION
    for(i = 0; i < files.size(); i++) {
        QUrl u;
        u = u.fromLocalFile(files[i]);
        QString f = u.toString();
        list.insert_entry(-1, f.toUtf8(), Tuple(), do_play);
        if (do_play) { do_play = false; }
    }
#endif
}

void FilesystemView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QTreeView::selectionChanged(selected, deselected);
    //QModelIndexList l = selected.indexes();
    //current_selected = m_model.get(l);
}


FilesystemView::FilesystemView()
{
    setModel(&m_model);

    QSettings s("filesystem", "audacious");

    QString std_music_loc;
    QStringList std_music_locs = QStandardPaths::standardLocations(QStandardPaths::MusicLocation);
    if (std_music_locs.size() > 0) {
        std_music_loc = std_music_locs[0];
    } else {
        QStringList l = QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
        if (l.size() > 0) { std_music_loc = l[0]; }
    }

    max_files_to_add = s.value("max_files_to_add", 100).toInt();
    library_path = s.value("library_path", std_music_loc).toString();
    m_model.setLibraryPath(library_path);

    setAllColumnsShowFocus(true);
    setFrameShape(QFrame::NoFrame);

    horizontalScrollBar()->setEnabled(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header()->hide();

    setSelectionBehavior(SelectRows);
    setSelectionMode(ExtendedSelection);
}

void FilesystemView::configMusicLibrary()
{
    QString n_lib_path = QFileDialog::getExistingDirectory(this, N_("Select a folder as Music Library"), library_path, QFileDialog::ShowDirsOnly);
    if (n_lib_path != "") {
        QSettings s("filesystem", "audacious");
        library_path = n_lib_path;
        s.setValue("library_path", library_path);
        m_model.setLibraryPath(library_path);
    }
}

void FilesystemView::configMaxFiles()
{
    QSettings s("filesystem", "audacious");
    QDialog *dlg = new QDialog(this);
    dlg->setModal(true);
    dlg->setWindowTitle(N_("Maximum Files to a Playlist"));
    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->addWidget(new QLabel(N_("Maximum number of files to add from library to playlist at once:")));

    QSpinBox *sp = new QSpinBox();
    sp->setMaximum(500);
    sp->setMinimum(10);
    sp->setValue(s.value("max_files_to_add", max_files_to_add).toInt());
    hbox->addWidget(sp, 1);

    connect(sp, &QSpinBox::valueChanged, [this](int v) {
                                                max_files_to_add = v;
                                                QSettings s("filesystem", "audacious");
                                                s.setValue("max_files_to_add", max_files_to_add);
                                         });

    QPushButton *ok = new QPushButton(N_("Close"));
    connect(ok, &QPushButton::clicked, dlg, &QDialog::close);
    QHBoxLayout *hbox1 = new QHBoxLayout();
    hbox1->addStretch(1);
    hbox1->addWidget(ok);

    QVBoxLayout *vbox = new QVBoxLayout();
    vbox->addLayout(hbox);
    vbox->addLayout(hbox1);

    dlg->setLayout(vbox);
    dlg->exec();
}

void FilesystemView::connectConfigButtons(QPushButton *configLib, QPushButton *configMaxFiles)
{
    connect(configLib, &QPushButton::clicked, this, &FilesystemView::configMusicLibrary);
    connect(configMaxFiles, &QPushButton::clicked,this, &FilesystemView::configMaxFiles);
}


static QPointer<FilesystemView> s_filesystem_view;

void * FilesystemQt::get_qt_widget()
{
    s_filesystem_view = new FilesystemView;

#ifndef FS_TEST_VERSION
    auto hbox = audqt::make_hbox(nullptr);
    hbox->setContentsMargins(audqt::margins.TwoPt);

    QPushButton *fbtn = new QPushButton(N_("Max files to add"));
    QPushButton *btn = new QPushButton(N_("Set Music Library Folder"));
    s_filesystem_view->connectConfigButtons(btn, fbtn);
    hbox->addWidget(fbtn);
    hbox->addWidget(btn);

    auto widget = new QWidget;
    auto vbox = audqt::make_vbox(widget, 0);
    vbox->addWidget(s_filesystem_view, 1);
    vbox->addLayout(hbox);
#else
    QWidget *widget = s_filesystem_view;
#endif
    return widget;
}

int FilesystemQt::take_message(const char * code, const void *p, int n)
{
    qDebug() << code << " - " << n;
    if (!strcmp(code, "grab focus") && s_filesystem_view)
    {
        s_filesystem_view->setFocus(Qt::OtherFocusReason);
        return 0;
    }

    return -1;
}
