/*
 * filesystem-qt.cc
 * Produced 2025 Hans Dijkema
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
#include <QDesktopServices>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudqt/libaudqt.h>
#include <libaudqt/treeview.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

/////////////////////////////////////////////////////////////////////////////////////////
// FilesystemQt Plugin Class
/////////////////////////////////////////////////////////////////////////////////////////

#define CFG_ID              "FilesystemQt"

#define CFG_MAX_FILES       "max_files_to_add"
#define CFG_MUSIC_LIBRARY   "music_library_folder"
#define CFG_MUSIC_EXTS      "music_file_extenstions"
#define CFG_COVER_FILES     "music_cover_files"
#define CFG_BOOKLET_FILE    "music_booklet"

#define CFG_DEFAULT_BOOKLET "booklet.pdf"
#define CFG_DEFAULT_COVERS  "cover.jpg|folder.jpg|cover.png|folder.png"
#define CFG_DEFAULT_EXTS    "mp3|flac|ogg|m4a|ape|wav|aac|aiff|opus|dsf"

#define PLUGIN_BASENAME     "filesystem-qt"

static void callback_folder();
static void callback_max_files_to_add();
static void callback_exts();
static void callback_booklet();
static void callback_covers();

#define AUD_LOGGING
//#define EXPORT

#ifdef AUD_LOGGING
#define LOGINFO AUDINFO
#define LOGDBG  AUDDBG
#define LOGWARN AUDWARN
#else
#define LOGBASE(kind, ...) fprintf(stderr, "%s:%d (%s) - %s:", __FILE__, __LINE__, __FUNCTION__, kind)
#define LOGINFO(...) { LOGBASE("Info   ");fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
#define LOGDBG(...)  { LOGBASE("Debug  ");fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
#define LOGWARN(...) { LOGBASE("Warning");fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
#endif

class FilesystemQt : public GeneralPlugin
{
public:
    static const char * const defaults[];
    static const char about[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {N_("Filesystem Manager"), PACKAGE,
                                        about, // about
                                        &prefs, // prefs
                                        PluginQtOnly};

    constexpr FilesystemQt() : GeneralPlugin(info, true)
    {
    }

    void * get_qt_widget();
    int take_message(const char * code, const void *, int);
};

const char FilesystemQt::about[] =
    N_("(p) 2025 Hans Dijkema\n\n"
       "The FilesystemQt plugin gives a dockable widget that can be used\n"
       "to browse folders for music files (mp3|flac|ogg|etc.)\n"
       "\n"
       "Right mouse on a folder or file and these can be added to, or replace\n"
       "the current playlist."
       );

const static WidgetVFileEntry dir_entry = { FileSelectMode::Folder };

const PreferencesWidget FilesystemQt::widgets[] = {
    WidgetLabel(N_("Standard options")),
    WidgetLabel("--------------------------------------------------------------------------------------------------------------------------------------------------"),
    WidgetFileEntry (N_("Music folder:"), WidgetString(CFG_ID, CFG_MUSIC_LIBRARY, callback_folder), dir_entry),
    WidgetSpin(N_("Maximum files to add to playlist:"), WidgetInt(CFG_ID, CFG_MAX_FILES, callback_max_files_to_add), { 10, 500, 1, N_("files") }),
    WidgetLabel(""),
    WidgetLabel(N_("Advanced options")),
    WidgetLabel("--------------------------------------------------------------------------------------------------------------------------------------------------"),
    WidgetEntry(N_("Music file extensions to browse                       :"), WidgetString(CFG_ID, CFG_MUSIC_EXTS, callback_exts)),
    WidgetEntry(N_("Booklet files to recognize                                  :"), WidgetString(CFG_ID, CFG_BOOKLET_FILE, callback_booklet)),
    WidgetEntry(N_("Cover art files (e.g. folder.jpg) to recognize :"), WidgetString(CFG_ID, CFG_COVER_FILES, callback_covers))
};

const PluginPreferences FilesystemQt::prefs = {{widgets}};


EXPORT FilesystemQt aud_plugin_instance;

QWidget *pluginWidget()
{
    return (QWidget *) aud_plugin_instance.get_qt_widget();
}

/////////////////////////////////////////////////////////////////////////////////////////
// Internal datastructure to hold directories and files. 
/////////////////////////////////////////////////////////////////////////////////////////

class FilesystemTree
{
    public:
        static const int DIR = 1;
        static const int FILE = 2;
        static QString _sep;
        static QString _exts;
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
        FilesystemTree(int kind, const QString & p, const QString & exts, int idx, FilesystemTree *parent)
        {
            _sep = "/";					// This might also work on Windows in Qt.
            _exts = exts;
            _fi = QFileInfo(p);
            _kind = kind;
            _parent_index = idx;
            _parent = parent;
            _loaded = false;
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
                qDebug() << "Unexpected!: " << row << " - " << _entries.size();
                return nullptr;
            }
            return _entries[row];
        }
        QString validExts() { return _exts; }

    public:
        void setPath(const QString &p)
        {
            _fi = QFileInfo(p);
            reload();
        }

        void setValidExts(const QString &e)
        {
            _exts = e;
            reload();
        }

        void setPathAndExts(const QString &p, const QString &e)
        {
            _fi = QFileInfo(p);
            _exts = e;
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
                {
                    QStringList exts = validExts().split("|");
                    int i;
                    for(i = 0; i < exts.size(); i++) {
                        filters.append(QString("*.") + exts[i].trimmed().toLower());
                    }
                }
                d.setNameFilters(filters);

                QDir::Filters d_filters = QDir::AllDirs | QDir::NoDot | QDir::NoDotDot | QDir::Files | QDir::Readable;
                d.setFilter(d_filters);

                QStringList l = d.entryList(filters, d_filters, QDir::SortFlag::IgnoreCase);

                QString p = path();
                int i;
                for(i = 0; i < l.size(); i++) {
                    if (l[i] != "lost+found") {
                        QString new_file = p + _sep + l[i];

                        QFileInfo f(new_file);
                        int k = (f.isDir()) ? DIR : FILE;

                        _entries.append(new FilesystemTree(k, new_file, _exts, i, this));
                    }
                }
            }
        }
    public:
        FilesystemTree *parent() const { return _parent; }
        int             parentIndex() const { return _parent_index; }

};

QString FilesystemTree::_sep;
QString FilesystemTree::_exts;


/////////////////////////////////////////////////////////////////////////////////////////
// FilesystemModel interfaces with the QTreeView framework of Qt
/////////////////////////////////////////////////////////////////////////////////////////

class FilesystemModel : public QAbstractItemModel
{
private:
    QString          _base_dir;
    QString          _valid_exts;
    FilesystemTree  *_tree;
    bool             _in_config;
	
public:
    enum
    {
        ColumnTitle,
        NColumns
    };

    FilesystemModel(const QString &base_dir, const QString &valid_exts)
    {
        _base_dir = base_dir;
        _valid_exts = valid_exts;
        _in_config = false;
        _tree = new FilesystemTree(FilesystemTree::DIR, _base_dir, _valid_exts, -1, nullptr);
    }
    
    ~FilesystemModel()
    {
        delete _tree;
    }

public:
    void beginConfig()
    {
        _in_config = true;
        this->beginResetModel();
    }

    void endConfig()
    {
        _in_config = false;
        this->endResetModel();
    }

    void refresh()
    {
        beginConfig();
        _tree = new FilesystemTree(FilesystemTree::DIR, _base_dir, _valid_exts, -1, nullptr);
        endConfig();
    }

    void setLibraryPath(const QString &library_path)
    {
        _base_dir = library_path;
        if (_base_dir != _tree->path()) {
            if (!_in_config) { this->beginResetModel(); }
            _tree->setPath(_base_dir);
            if (!_in_config) { this->endResetModel(); }
        }
    }

    void setValidExts(const QString &valid_exts)
    {
        _valid_exts = valid_exts;
        if (_valid_exts != _tree->validExts()) {
            if (!_in_config) { this->beginResetModel(); }
            _tree->setValidExts(_valid_exts);
            if (!_in_config) { this->endResetModel(); }
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

/////////////////////////////////////////////////////////////////////////////////////////
// FilesystemView implements a TreeView for a folder structure
/////////////////////////////////////////////////////////////////////////////////////////

class FilesystemView : public audqt::TreeView
{
private:
    QList<FilesystemTree *> current_selected;

    // Preferences
private:
    int max_files_to_add;
    QString library_path;
    QString valid_exts;
    QString booklet_file;
    QString cover_files;

    QString booklet_file_to_open;
    QString cover_file_to_open;

private:
    QDir folderForSelection(bool &ok);

public slots:
    void config();
    void refresh();

public:
    FilesystemView();

public:
    void setMusicLibrary(const QUrl &folder);
    void setMaxFilesToAdd(int max);
    void setMusicExts(const QString &exts);
    void setCoverFiles(const QString &files);
    void setBookletFile(const QString &file);

private:
    FilesystemModel *m_model;

protected:
    void contextMenuEvent(QContextMenuEvent *event);

public:
    void playThis(bool checked);
    void addThis(bool checked);
    void openThis(bool checked);
    void insertEntries(Playlist &list, bool play);
};

void FilesystemView::contextMenuEvent(QContextMenuEvent *evt)
{
    QMenu *menu = new QMenu(this);
    QString play = QString(N_("Replace current playlist and play"));
    QString add = QString(N_("Add to current playlist"));
    QString open = QString(N_("Open containing folder"));
    QAction *action_play = new QAction(play);
    QAction *action_add = new QAction(add);
    QAction *action_open = new QAction(open);
    connect(action_play, &QAction::triggered, this, &FilesystemView::playThis);
    connect(action_add, &QAction::triggered, this, &FilesystemView::addThis);
    connect(action_open, &QAction::triggered, this, &FilesystemView::openThis);
    menu->addAction(action_play);
    menu->addAction(action_add);
    menu->addSeparator();
    menu->addAction(action_open);

    // Check for booklet
    bool has_selected;
    bool separator_added = false;
    QDir d = this->folderForSelection(has_selected);
    if (has_selected) {
        if (d.exists(booklet_file)) {
            booklet_file_to_open = d.filePath(booklet_file);
            QAction *action_open_booklet = new QAction(N_("Open booklet"));
            connect(action_open_booklet, &QAction::triggered, this, [this]() {
                QUrl f;
                f = f.fromLocalFile(this->booklet_file_to_open);
                QDesktopServices::openUrl(f);
            });
            menu->addSeparator();
            separator_added = true;
            menu->addAction(action_open_booklet);
        }

        QStringList covers = cover_files.split("|");
        QString file = "";
        int i;
        for(i = 0; i < covers.size() && file == ""; i++) {
            QString cover = covers[i].trimmed();
            if (d.exists(cover)) {
                file = d.filePath(cover);
            }
        }
        if (file != "") {
            cover_file_to_open = file;
            QAction *action_open_cover = new QAction(N_("Open Cover Art"));
            connect(action_open_cover, &QAction::triggered, this, [this]() {
                QUrl f;
                f = f.fromLocalFile(this->cover_file_to_open);
                QDesktopServices::openUrl(f);
            });
            if (!separator_added) { menu->addSeparator(); }
            menu->addAction(action_open_cover);
        }
    }

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
    auto list = Playlist::active_playlist();
    list.remove_all_entries();
    insertEntries(list, true);
}

void FilesystemView::addThis(bool checked)
{
    auto list = Playlist::active_playlist();
    insertEntries(list, false);
}

void FilesystemView::openThis(bool checked)
{
    // Get the currently selected files / directories

    QItemSelection sel = this->selectionModel()->selection();
    QModelIndexList l = sel.indexes();
    current_selected = m_model->get(l);

    if (current_selected.size() > 1) {
        QMessageBox::warning(this,
                             N_("Too many selected entries"),
                             N_("Cannot determine where to open the filenmanager / explorer\n"
                                "Please select one entry")
                             );
    } else if (current_selected.size() == 0) {
        QMessageBox::warning(this,
                             N_("No selected entries"),
                             N_("Cannot determine where to open the filenmanager / explorer\n"
                                "Please select one entry")
                             );
    } else {
        FilesystemTree *s = current_selected[0];
        QString where = s->path();
        if (s->kind() == FilesystemTree::FILE) {
            QFileInfo fi(s->path());
            where = fi.canonicalPath();
        }
        QDir w(where);
        if (w.exists()) {
            QUrl wu = QUrl().fromLocalFile(where);
            QDesktopServices::openUrl(wu);
        }
    }
}

void FilesystemView::insertEntries(Playlist &list, bool do_play)
{
    // Get the currently selected files / directories

    QItemSelection sel = this->selectionModel()->selection();
    QModelIndexList l = sel.indexes();
    current_selected = m_model->get(l);

    // Assemble the files to a maximum of 'max_files_to_add', which is configurable.

    QStringList files;
    int count = 0;
    int i;
    for(i = 0; i < current_selected.size() && count < max_files_to_add; i++) {
        assembleFiles(current_selected[i], files, count, max_files_to_add);
    }

   // Add the files to the current playlist or replace the contents of the playlist.
   // if do_play == true, the first added entry will also be played.

    for(i = 0; i < files.size(); i++) {
        QUrl u;
        u = u.fromLocalFile(files[i]);
        QString f = u.toString();
        list.insert_entry(-1, f.toUtf8(), Tuple(), do_play);
        if (do_play) { do_play = false; }
    }
}

static QString correctExts(QString exts, bool &correct);

QDir FilesystemView::folderForSelection(bool &ok)
{
    QItemSelection sel = this->selectionModel()->selection();
    QModelIndexList l = sel.indexes();
    current_selected = m_model->get(l);

    if (current_selected.size() == 1) {
        ok = true;

        FilesystemTree *s = current_selected[0];
        QString where = s->path();
        if (s->kind() == FilesystemTree::FILE) {
            QFileInfo fi(s->path());
            where = fi.canonicalPath();
        }

        return QDir(where);
    } else {
        ok = false;
        return QDir();
    }
}

void FilesystemView::config()
{
    LOGDBG("configuration called");
    PluginHandle *h = aud_plugin_lookup_basename(PLUGIN_BASENAME);
    if (h != nullptr) {
        audqt::plugin_prefs(h);
    } else {
        LOGWARN("Plugin lookup failed for plugin: %s", PLUGIN_BASENAME);
        QString msg;
        msg = msg.asprintf(N_("Plugin lookup for '%s' failed\n\n"
                              "Please configure this plugin via the normale plugin configuration way"),
                           PLUGIN_BASENAME);
        QMessageBox::warning(this,
                             N_("Plugin Lookup Failed"),
                             msg);
    }
}

void FilesystemView::refresh()
{
    m_model->refresh();
}

FilesystemView::FilesystemView()
{
    // Configuration Defaults
    LOGDBG("config defaults");

    QString std_music_loc;
    QStringList std_music_locs = QStandardPaths::standardLocations(QStandardPaths::MusicLocation);
    if (std_music_locs.size() > 0) {
        std_music_loc = std_music_locs[0];
    } else {
        QStringList l = QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
        if (l.size() > 0) { std_music_loc = l[0]; }
        else {
            LOGWARN(N_("No default music location found! Configure it by hand!"));
        }
    }

    const char *default_music_loc = std_music_loc.toUtf8();
    LOGINFO("Standard Music Locatio Found: %s", default_music_loc);

    const char * const cfg_filesystem_defaults [] = {
        CFG_MAX_FILES, "100",
        CFG_MUSIC_LIBRARY, default_music_loc,
        CFG_MUSIC_EXTS, CFG_DEFAULT_EXTS,
        CFG_BOOKLET_FILE, CFG_DEFAULT_BOOKLET,
        CFG_COVER_FILES, CFG_DEFAULT_COVERS,
        nullptr
    };

    aud_config_set_defaults(CFG_ID, cfg_filesystem_defaults);

    // Get configuration settings
    LOGDBG("Config settings");

    max_files_to_add = aud_get_int(CFG_ID, CFG_MAX_FILES);

    const char *aud_music_lib = aud_get_str(CFG_ID, CFG_MUSIC_LIBRARY);
    QString path = QString(aud_music_lib);
    if (!path.startsWith("file:")) {
        library_path = path;
    } else {
        library_path = QUrl(aud_music_lib).toLocalFile();
    }

    const char *aud_valid_exts = aud_get_str(CFG_ID, CFG_MUSIC_EXTS);
    bool correct_exts = false;
    valid_exts = correctExts(QString(aud_valid_exts), correct_exts);

    const char *aud_booklet_file = aud_get_str(CFG_ID, CFG_BOOKLET_FILE);
    booklet_file = QString(aud_booklet_file);

    const char *aud_conver_files = aud_get_str(CFG_ID, CFG_COVER_FILES);
    cover_files = QString(aud_conver_files);

    if (!correct_exts) {
        LOGWARN("configured music file extensions: %s, are not correct", aud_valid_exts);
        LOGWARN("they have been reset to %s", valid_exts.toUtf8().data());
    }

    LOGINFO("max_files_to_add = %d", max_files_to_add);
    LOGINFO("library_path     = %s", library_path.toUtf8().data());
    LOGINFO("valid_exts       = %s", valid_exts.toUtf8().data());
    LOGINFO("booklet file     = %s", booklet_file.toUtf8().data());
    LOGINFO("cover files      = %s", cover_files.toUtf8().data());

    // Initialize Model
    LOGDBG("Model Init");
    m_model = new FilesystemModel(library_path, valid_exts);
    setModel(m_model);

    // Initialize View
    LOGDBG("View Init");

    setAllColumnsShowFocus(true);
    //setFrameShape(QFrame::NoFrame);

    horizontalScrollBar()->setEnabled(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header()->hide();

    setSelectionBehavior(SelectRows);
    setSelectionMode(ExtendedSelection);
}

void FilesystemView::setMusicLibrary(const QUrl &folder)
{
    library_path = folder.toLocalFile();
    m_model->setLibraryPath(library_path);
}

void FilesystemView::setMaxFilesToAdd(int m)
{
    max_files_to_add = m;
}

void FilesystemView::setMusicExts(const QString &exts)
{
    valid_exts = exts;
    m_model->setValidExts(valid_exts);
}

void FilesystemView::setCoverFiles(const QString &files)
{
    cover_files = files;
}

void FilesystemView::setBookletFile(const QString &file)
{
    booklet_file = file;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Make the Plugin Work.
/////////////////////////////////////////////////////////////////////////////////////////

static QPointer<FilesystemView> s_filesystem_view;

static FilesystemView *getMyWidget()
{
    return s_filesystem_view;
}

void * FilesystemQt::get_qt_widget()
{
    s_filesystem_view = new FilesystemView;

    QWidget *widget = new QWidget();
    QVBoxLayout *vbox = new QVBoxLayout();
    QPushButton *config = new QPushButton(N_("Configure"));
    QPushButton *refresh = new QPushButton(N_("Refresh Library"));
    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->addStretch(1);
    hbox->addWidget(refresh);
    hbox->addWidget(config);
    FilesystemView::connect(config, &QPushButton::clicked, s_filesystem_view, &FilesystemView::config);
    FilesystemView::connect(refresh, &QPushButton::clicked, s_filesystem_view, &FilesystemView::refresh);

    QFrame *frm = new QFrame();
    frm->setFrameStyle(QFrame::NoFrame);
    frm->setLineWidth(1);
    frm->setLayout(hbox);
    hbox->setContentsMargins(0, 0, 0, 0);

    vbox->setContentsMargins(0, 0, 0,0);

    vbox->addWidget(s_filesystem_view, 1);
    vbox->addWidget(frm);

    widget->setLayout(vbox);

    return widget;
}

int FilesystemQt::take_message(const char * code, const void *p, int n)
{
    if (!strcmp(code, "grab focus") && s_filesystem_view)
    {
        s_filesystem_view->setFocus(Qt::OtherFocusReason);
        return 0;
    }

    return -1;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Preferences
/////////////////////////////////////////////////////////////////////////////////////////


static void callback_booklet()
{
    const char *c_booklet = aud_get_str(CFG_ID, CFG_BOOKLET_FILE);
    QString booklet(c_booklet);
    FilesystemView *view = getMyWidget();
    view->setBookletFile(booklet);
}

static void callback_covers()
{
    const char *c_covers = aud_get_str(CFG_ID, CFG_BOOKLET_FILE);
    QString covers(c_covers);
    FilesystemView *view = getMyWidget();
    view->setCoverFiles(covers);
}

static void callback_folder()
{
    const char *c_folder = aud_get_str(CFG_ID, CFG_MUSIC_LIBRARY);
    QString folder(c_folder);
    QUrl folder_url(folder);

    FilesystemView *view = getMyWidget();
    view->setMusicLibrary(folder_url);
}

static void callback_max_files_to_add()
{
    int max_files = aud_get_int(CFG_ID, CFG_MAX_FILES);
    FilesystemView *view = getMyWidget();
    view->setMaxFilesToAdd(max_files);
}


static QString correctExts(QString exts, bool &correct)
{
    QRegularExpression re("^\\s*[A-Za-z0-9]+(\\s*[|]\\s*[A-Za-z0-9]*)*\\s*$");
    QRegularExpressionMatch m = re.match(exts);
    if (m.hasMatch()) {
        QStringList l_exts = exts.split("|");

        int i;
        exts = "";
        for(i = 0; i < l_exts.size(); i++) {
            l_exts[i] = l_exts[i].trimmed().toLower();
        }
        exts = l_exts.join("|");

        correct = true;

        return exts;
    } else {
        correct = false;
        return CFG_DEFAULT_EXTS;
    }
}

static void callback_exts()
{
    FilesystemView *view = getMyWidget();

    const char *c_exts = aud_get_str(CFG_ID, CFG_MUSIC_EXTS);
    QString exts(c_exts);
    bool correct = false;
    QString real_exts = correctExts(exts, correct);

    if (correct) {
        LOGDBG("New music file extensions: %s", exts.toUtf8().data());
        view->setMusicExts(real_exts);
    } else {
        LOGWARN("Music file extensions are not correctly given: %s", exts.toUtf8().data());

        QRegularExpression re("([^a-zA-Z0-9|]+)");
        exts = exts.replace(re, "<font color=\"red\"><b>\\1</b></font>");
        LOGDBG("%s", exts.toUtf8().data());

        QString msg = N_("The given music file extensions:"
                         "<ul><li>%s</li></ul>"
                         "Only characters and numbers are valid (a .. z and 0 .. 9)<br><br>"
                         "The file extensions for browsing will be set to the default:"
                         "<ul><li>%s</li></ul>"
                         );
        QMessageBox box(view);
        box.setWindowTitle(N_("Music file extensions wrong"));
        box.setTextFormat(Qt::TextFormat::RichText);
        box.setText(msg.asprintf(msg.toUtf8(), exts.toUtf8().data(), real_exts.toUtf8().data()));
        box.addButton(QMessageBox::StandardButton::Ok);
        box.setDefaultButton(QMessageBox::StandardButton::Ok);
        box.exec();

        view->setMusicExts(real_exts);
    }
}

