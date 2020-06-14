#pragma once

#include <QBoxLayout>
#include <QLineEdit>
#include <QPointer>
#include <QWidget>

#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#define CFG_ID "library-view"

constexpr auto DEFAULT_LIBRARY_PATH = "~";
constexpr auto DEFAULT_PLAYLIST_TITLE = "Library view";
constexpr auto DEFAULT_PLAYING_TITLE = "Playing";

class LibraryTreeView;
class LibraryViewQt : public GeneralPlugin {
  public:
    static const char* const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;
    static constexpr PluginInfo info = {N_("Library View"), PACKAGE,
					nullptr, // about
					&prefs, PluginQtOnly};
    constexpr LibraryViewQt() : GeneralPlugin(info, false) {}
    static LibraryTreeView* get_tree_widget();
    bool init();
    void* get_qt_widget();
};

class PreferenceWidget : public QWidget {
  public:
    PreferenceWidget(QWidget* parent = nullptr);
    ~PreferenceWidget();
    static void* create_instance();
    static void init();
    static void apply();
    static QPointer<PreferenceWidget> instance;

  private:
    QHBoxLayout* box_layout;
    QLineEdit* file_entry;
    QPalette default_palette;
    QPalette error_palette;
    void update_text_color(QString new_path);
};

void set_pref_uri(QString filename);
String get_pref_uri();
bool test_uri(QString path);
inline bool test_uri(String path) {
    return test_uri(static_cast<const char*>(path));
}
Playlist find_playlist(const char* title);