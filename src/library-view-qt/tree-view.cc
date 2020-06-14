#include <QEvent>
#include <QKeyEvent>
#include <glib.h>
#include <thread>

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

#include "directory-structure-model.h"
#include "library-view-qt.h"
#include "tree-view.h"

LibraryTreeView::LibraryTreeView(QWidget * parent) : audqt::TreeView(parent)
{
	setHeaderHidden(true);
	setTextElideMode(Qt::ElideNone);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setColumnWidth(0, INT_MAX);
	setSelectionMode(SelectionMode::ExtendedSelection);
	setDragDropMode(QAbstractItemView::DragOnly);
	installEventFilter(this);
	directory_structure_model = new DirectoryStructureModel;
	setModel(directory_structure_model);
	set_root_path();
}

void LibraryTreeView::set_root_path(String root) {
	auto str = static_cast<const char*>(root);
	if(!test_uri(str)) return;
	auto root_index = directory_structure_model->set_root_path(static_cast<const char*>(uri_to_filename(str)));
	setRootIndex(root_index);
}

void LibraryTreeView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) {
    update();
    if(!aud_get_bool(CFG_ID, "auto_send"))
	return;
    /* get playlist  */
    auto playlist = find_playlist(aud_get_str(CFG_ID, "playlist_title"));
    auto playing = find_playlist(aud_get_str(CFG_ID, "playing_title"));
    auto current = Playlist::playing_playlist();
    bool is_new_playlist_needed = !playlist.exists();
    if(playlist == current) {
	if(playing.exists()) {
	    playing.remove_playlist();
	}
	playlist.set_title(aud_get_str(CFG_ID, "playing_title"));
	is_new_playlist_needed = true;
	}
	if(is_new_playlist_needed) {
		playlist = Playlist::blank_playlist();
		playlist.set_title(aud_get_str(CFG_ID, "playlist_title"));
	}

	Index<PlaylistAddItem> add;
	for(auto index : selectedIndexes()) {
		if(!index.isValid())
		continue;
		auto filename = directory_structure_model->filePath(index);
		if(filename.isEmpty())
		continue;
		auto ptr = filename.toUtf8().data();
		auto uri = filename_to_uri(ptr);

		add.append(String(uri));
	}
	if(add.len()) {
		playlist.remove_all_entries();
		playlist.insert_items(-1, std::move(add), false);
	}
	playlist.activate();
}

bool LibraryTreeView::eventFilter(QObject* object, QEvent* event){
	if(object != this) return false;
	if(event->type() == QEvent::KeyPress) {
		auto key = reinterpret_cast<QKeyEvent*>(event);
		if((key->key() == Qt::Key_Enter) ||
		   (key->key() == Qt::Key_Return)) {
		auto playlist = find_playlist(aud_get_str(CFG_ID, "playlist_title"));
		if(playlist != Playlist()){
			playlist.start_playback();
			return true;
		}
		}
	}
	return false;
}
