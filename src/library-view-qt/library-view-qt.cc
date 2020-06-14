#include <glib.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

#include "library-view-qt.h"
#include "tree-view.h"

static QPointer<LibraryTreeView> tree_widget;
EXPORT LibraryViewQt aud_plugin_instance;

const char* const LibraryViewQt::defaults[] = {
    "library_path",   DEFAULT_LIBRARY_PATH,
    "playlist_title", DEFAULT_PLAYLIST_TITLE,
    "playing_title", DEFAULT_PLAYING_TITLE,
	"auto_send", "TRUE",
    nullptr};

const PreferencesWidget LibraryViewQt::widgets[] = {
    WidgetLabel(N_("<b>Library Path</b>")),
    WidgetCustomQt(PreferenceWidget::create_instance),

    WidgetLabel(N_("<b>Playlist title</b>")),
    WidgetEntry(N_("Library playlist"),
		WidgetString(CFG_ID, "playlist_title")),
    WidgetEntry(N_("Playing playlist"),
		WidgetString(CFG_ID, "playing_title")),
    WidgetSeparator(),
	WidgetCheck(N_("Auto-send to playlist"), WidgetBool(CFG_ID, "auto_send"))
};

const PluginPreferences LibraryViewQt::prefs = {
    {widgets}, PreferenceWidget::init, PreferenceWidget::apply, nullptr};

LibraryTreeView* LibraryViewQt::get_tree_widget() { return tree_widget; }

bool LibraryViewQt::init() {
	aud_config_set_defaults(CFG_ID, defaults);
	return true;
}

void* LibraryViewQt::get_qt_widget() {
	if(!tree_widget)
	tree_widget = new LibraryTreeView;

	return tree_widget;
}

void set_pref_uri(QString filename) {
    auto uri = filename_to_uri(
	filename_expand(filename_normalize(str_copy(filename.toUtf8()))));
    aud_set_str(CFG_ID, "library_path", uri);
}

String get_pref_uri() {
    auto to_uri = [](const char* path) {
	return String(filename_to_uri(path));
    };

    auto path = aud_get_str(CFG_ID, "library_path");
    if(path[0] && test_uri(static_cast<const char*>(path)))
	return path;
    else
	return to_uri(g_get_home_dir());
}

bool test_uri(QString path) {
    StringBuf full_path = str_copy(path.toUtf8());
    full_path = filename_expand(filename_normalize(uri_to_filename(full_path)));
    return g_file_test(full_path, G_FILE_TEST_IS_DIR);
}

Playlist find_playlist(const char* title) {
    auto playlist = Playlist();
    for(int p = 0; p < Playlist::n_playlists(); p++) {
	auto current = Playlist::by_index(p);
	if(!strcmp(current.get_title(), title)) {
	    playlist = current;
	    break;
	}
    }
    return playlist;
}