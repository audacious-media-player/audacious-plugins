#include <glib.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

#include "library-view-qt.h"
#include "tree-view.h"

QPointer<PreferenceWidget> PreferenceWidget::instance;
String prev_playlist_title;
String prev_playing_title;

PreferenceWidget::PreferenceWidget(QWidget* parent)
    : QWidget(parent), box_layout(audqt::make_hbox(this)),
      file_entry(audqt::file_entry_new(this, _("Choose Folder"),
                                       QFileDialog::Directory,
                                       QFileDialog::AcceptOpen)) {
    file_entry->setText(static_cast<const char*>(uri_to_display(get_pref_uri())));
    box_layout->addWidget(file_entry);
    default_palette = file_entry->palette();
    error_palette = default_palette;
    error_palette.setColor(QPalette::Text, Qt::red);
    QObject::connect(file_entry, &QLineEdit::textChanged, this,
                     &PreferenceWidget::update_text_color);
}
PreferenceWidget::~PreferenceWidget() { instance.clear(); }

void* PreferenceWidget::create_instance() {
    if(instance.isNull())
        instance = new PreferenceWidget;
    return instance.data();
}

void PreferenceWidget::init() {
    prev_playlist_title = aud_get_str(CFG_ID, "playlist_title");
    prev_playing_title = aud_get_str(CFG_ID, "playing_title");
}

void PreferenceWidget::apply() {
    if(instance.isNull())
        return;
    auto path = audqt::file_entry_get_uri(instance->file_entry);
    if(!test_uri(path)) {
        audqt::simple_message("Error", "Invalid library path.");
        return;
    }
    if(uri_to_filename(get_pref_uri()) != path) {
        set_pref_uri(path);
        LibraryViewQt::get_tree_widget()->set_root_path();
    }

    auto test_pref = [](const char* name, String& prev, const char* def) {
        auto pref = aud_get_str(CFG_ID, name);
        if(!strlen(pref)) {
            auto title = strlen(prev) ? prev : def;
            aud_set_str(CFG_ID, name, title);
        } else {
            auto playlist = find_playlist(prev);
            if(playlist.exists()) {
                playlist.set_title(pref);
            }
        }
    };
    test_pref("playlist_title", prev_playlist_title, DEFAULT_PLAYLIST_TITLE);
    test_pref("playing_title", prev_playing_title, DEFAULT_PLAYING_TITLE);
    return;
}

void PreferenceWidget::update_text_color(QString new_path) {
    auto path = audqt::file_entry_get_uri(instance->file_entry);
    if(test_uri(path)) {
        file_entry->setPalette(default_palette);
    } else {
        file_entry->setPalette(error_palette);
    }
    return;
}
