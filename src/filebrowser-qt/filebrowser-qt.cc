/*
 * File Browser Plugin for Audacious
 * Copyright 2025 Thomas Lange
 *
 * Based on the implementation for Qmmp
 * Copyright 2013-2025 Ilya Kotov
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

#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QToolButton>
#include <QTreeView>

#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

#define CFG_ID "filebrowser-qt"
#define CFG_FILE_PATH "file_path"

class FileBrowserQt : public GeneralPlugin
{
public:
    static const char about[];

    static constexpr PluginInfo info = {N_("File Browser"), PACKAGE, about,
                                        nullptr, PluginQtOnly};

    constexpr FileBrowserQt() : GeneralPlugin(info, false) {}

    void * get_qt_widget();
    int take_message(const char * code, const void * data, int size);
};

EXPORT FileBrowserQt aud_plugin_instance;

const char FileBrowserQt::about[] =
 N_("A dockable plugin that can be used to browse folders for music files. "
    "Right-click on a folder or file for supported actions.\n\n"
    "Copyright 2025 Thomas Lange\n\n"
    "Based on the implementation for Qmmp\n"
    "Copyright 2021-2025 Ilya Kotov");

class FileSystemFilterProxyModel : public QSortFilterProxyModel
{
public:
    explicit FileSystemFilterProxyModel(QObject * parent)
        : QSortFilterProxyModel(parent)
    {
    }

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex & sourceParent) const override
    {
        auto model = qobject_cast<QFileSystemModel *>(sourceModel());
        if (model->index(model->rootPath()) != sourceParent)
            return true;

        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }
};

class FileBrowserWidget : public QWidget
{
public:
    FileBrowserWidget();

protected:
    void contextMenuEvent(QContextMenuEvent * event) override;

private:
    void addSelectedItems(bool play);
    void replacePlaylist();
    void createPlaylist();
    void addToPlaylist();
    void openFolder();
    void openCover();

    bool hasMultiSelection() const;
    bool searchCover(QString & result) const;
    QString selectedPath() const;
    QStringList selectedPaths() const;
    QStringList supportedFileExtensions() const;

    void initMusicDirectory();
    void setCurrentDirectory(const QString & path);
    void onTreeViewActivated(const QModelIndex & index);

    QTreeView * m_treeView;
    QFileSystemModel * m_fileSystemModel;
    FileSystemFilterProxyModel * m_proxyModel;
    QToolButton * m_upButton;
    QLineEdit * m_filterLineEdit;
    QString m_coverPath;
};

static QPointer<FileBrowserWidget> s_widget;

FileBrowserWidget::FileBrowserWidget()
{
    m_treeView = new QTreeView(this);
    m_treeView->setDragEnabled(true);
    m_treeView->setFrameStyle(QFrame::NoFrame);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    m_filterLineEdit = new QLineEdit(this);
    m_filterLineEdit->setContentsMargins(0, 5, 5, 5);
    m_filterLineEdit->setClearButtonEnabled(true);

    m_fileSystemModel = new QFileSystemModel(this);
    m_fileSystemModel->setNameFilterDisables(false);
    m_fileSystemModel->setNameFilters(supportedFileExtensions());
    m_fileSystemModel->setFilter(QDir::AllDirs | QDir::Files |
                                 QDir::NoDotAndDotDot);

    m_proxyModel = new FileSystemFilterProxyModel(this);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setSourceModel(m_fileSystemModel);

    m_treeView->setModel(m_proxyModel);
    m_treeView->setColumnHidden(1, true);
    m_treeView->setColumnHidden(2, true);
    m_treeView->setColumnHidden(3, true);
    m_treeView->setHeaderHidden(true);
    m_treeView->setUniformRowHeights(true);
    m_treeView->header()->setStretchLastSection(false);
    m_treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    auto upAction = new QAction(QIcon::fromTheme("go-up"),
                                QString(_("Show the parent folder")), this);
    m_upButton = new QToolButton(this);
    m_upButton->setAutoRaise(true);
    m_upButton->setDefaultAction(upAction);

    QHBoxLayout * hbox = audqt::make_hbox(nullptr, audqt::sizes.TwoPt);
    hbox->addWidget(m_upButton);
    hbox->addWidget(m_filterLineEdit);

    QVBoxLayout * vbox = audqt::make_vbox(this, 0);
    vbox->addLayout(hbox);
    vbox->addWidget(m_treeView);

    connect(m_treeView, &QTreeView::activated, this,
            &FileBrowserWidget::onTreeViewActivated);

    connect(m_filterLineEdit, &QLineEdit::textChanged,
            [this](const QString & text) {
                m_proxyModel->setFilterFixedString(text);
            });

    connect(upAction, &QAction::triggered, [this]() {
        QDir dir = m_fileSystemModel->rootDirectory();
        if (dir.cdUp())
            setCurrentDirectory(dir.path());
    });

    initMusicDirectory();
}

void FileBrowserWidget::contextMenuEvent(QContextMenuEvent * event)
{
    auto newAction = [this](const char * text, const char * icon,
                            QWidget * parent, auto func) {
        auto action = new QAction(QIcon::fromTheme(icon),
                                  audqt::translate_str(text), parent);
        connect(action, &QAction::triggered, this, func);
        return action;
    };

    auto menu = new QMenu(this);

    menu->addAction(newAction(N_("_Play"), "media-playback-start", menu,
                              &FileBrowserWidget::replacePlaylist));
    menu->addAction(newAction(N_("_Create Playlist"), "document-new", menu,
                              &FileBrowserWidget::createPlaylist));
    menu->addAction(newAction(N_("_Add to Playlist"), "list-add", menu,
                              &FileBrowserWidget::addToPlaylist));
    menu->addSeparator();

    if (!hasMultiSelection())
        menu->addAction(newAction(N_("_Open Folder Externally"), "folder", menu,
                                  &FileBrowserWidget::openFolder));
    if (searchCover(m_coverPath))
        menu->addAction(newAction(N_("Open Co_ver Art"), "image-x-generic",
                                  menu, &FileBrowserWidget::openCover));

    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
}

void FileBrowserWidget::addSelectedItems(bool play)
{
    Playlist list = Playlist::active_playlist();
    Index<PlaylistAddItem> items;

    for (const QString & path : selectedPaths())
    {
        QUrl url = QUrl::fromLocalFile(path);
        items.append(String(url.toEncoded().constData()));
    }

    list.insert_items(-1, std::move(items), play);
}

void FileBrowserWidget::replacePlaylist()
{
    Playlist::temporary_playlist().activate();
    addSelectedItems(true);
}

void FileBrowserWidget::createPlaylist()
{
    Playlist::new_playlist();
    addSelectedItems(false);
}

void FileBrowserWidget::addToPlaylist()
{
    addSelectedItems(false);
}

void FileBrowserWidget::openFolder()
{
    QString path = selectedPath();
    if (QDir(path).exists())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void FileBrowserWidget::openCover()
{
    if (QFile(m_coverPath).exists())
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_coverPath));
}

bool FileBrowserWidget::hasMultiSelection() const
{
    return m_treeView->selectionModel()->selectedRows().size() > 1;
}

bool FileBrowserWidget::searchCover(QString & result) const
{
    QString path = selectedPath();
    if (path.isEmpty())
    {
        result = QString();
        return false;
    }

    String coverNames = aud_get_str("cover_name_include");
    QStringList nameFilter = QString(coverNames).remove(' ').split(',');
    QStringList extFilter = {"*.jpg", "*.jpeg", "*.png", "*.webp"};
    QFileInfoList foundFiles = QDir(path).entryInfoList(extFilter, QDir::Files);

    for (const QFileInfo & info : foundFiles)
    {
        if (nameFilter.contains(info.baseName(), Qt::CaseInsensitive))
        {
            result = info.canonicalFilePath();
            return true;
        }
    }

    result = QString();
    return false;
}

QString FileBrowserWidget::selectedPath() const
{
    QModelIndexList indexes = m_treeView->selectionModel()->selectedRows();
    auto nSelected = indexes.size();

    if (nSelected == 0)
        return m_fileSystemModel->rootDirectory().canonicalPath();

    if (nSelected != 1)
        return QString();

    QModelIndex sourceIndex = m_proxyModel->mapToSource(indexes[0]);
    QFileInfo info = m_fileSystemModel->fileInfo(sourceIndex);
    return info.isDir() ? info.absoluteFilePath() : info.absolutePath();
}

QStringList FileBrowserWidget::selectedPaths() const
{
    QStringList paths;

    for (const auto & index : m_treeView->selectionModel()->selectedIndexes())
    {
        if (!index.isValid() || index.column() != 0)
            continue;

        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        paths << m_fileSystemModel->filePath(sourceIndex);
    }

    return paths;
}

QStringList FileBrowserWidget::supportedFileExtensions() const
{
    QStringList extensions;

    for (const char * ext : aud_plugin_get_supported_extensions())
    {
        if (!ext)
            break;
        extensions << QString("*.%1").arg(ext);
    }

    return extensions;
}

void FileBrowserWidget::initMusicDirectory()
{
    auto musicDir = QString(aud_get_str(CFG_ID, CFG_FILE_PATH));

    if (musicDir.isEmpty() || !QDir(musicDir).exists())
    {
        QStringList locations =
            QStandardPaths::standardLocations(QStandardPaths::MusicLocation);
        musicDir = locations.value(0, QDir::homePath());
    }

    setCurrentDirectory(musicDir);
}

void FileBrowserWidget::setCurrentDirectory(const QString & path)
{
    auto info = QFileInfo(path);
    if (!info.isDir() || !info.isExecutable() || !info.isReadable())
        return;

    QModelIndex index = m_fileSystemModel->setRootPath(path);
    if (!index.isValid())
        return;

    m_upButton->setEnabled(!info.isRoot());
    m_treeView->setRootIndex(m_proxyModel->mapFromSource(index));

    QString dirName = info.baseName();
    QString cleanPath = m_fileSystemModel->rootDirectory().canonicalPath();

    m_filterLineEdit->clear();
    m_filterLineEdit->setPlaceholderText(
        dirName.isEmpty() ? QString(_("Search"))
                          : QString(_("Search in %1")).arg(dirName));

    aud_set_str(CFG_ID, CFG_FILE_PATH, cleanPath.toUtf8().constData());
}

void FileBrowserWidget::onTreeViewActivated(const QModelIndex & index)
{
    if (!index.isValid())
        return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    QFileInfo info = m_fileSystemModel->fileInfo(sourceIndex);

    if (info.isDir())
        setCurrentDirectory(info.filePath());
    else
        replacePlaylist();
}

void * FileBrowserQt::get_qt_widget()
{
    if (!s_widget)
        s_widget = new FileBrowserWidget;

    return s_widget;
}

int FileBrowserQt::take_message(const char * code, const void * data, int size)
{
    if (!strcmp(code, "grab focus") && s_widget)
    {
        s_widget->setFocus(Qt::OtherFocusReason);
        return 0;
    }

    return -1;
}
