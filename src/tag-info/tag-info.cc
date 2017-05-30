/**
 * Tag Info (GTK)
 * Displays tag information for current song.
 *
 * Copyright (C) 2016  kevinlekiller
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/hook.h>
#include <libaudcore/plugin.h>
#include <libaudgui/list.h>
#include <string.h>

class TagInfo : public GeneralPlugin
{
public:
    static const char about[];
    static constexpr PluginInfo info = {
        N_("Tag Info"),
        PACKAGE,
        about,
        nullptr,
        PluginGLibOnly
    };

    constexpr TagInfo () : GeneralPlugin (info, false) {}

    void * get_gtk_widget();
    int take_message(const char * code, const void * data, int size);
};

EXPORT TagInfo aud_plugin_instance;

static GtkWidget * list;
static Tuple tuple;
static double gain_divisor = 1;
static double peak_divisor = 1;
static Tuple::Field tuple_field;
static GValue * g_value;
static String string_buffer;

static void update_values(void *, void *) {
    audgui_list_delete_rows(list, 0, audgui_list_row_count(list));
    tuple = aud_drct_get_tuple();
    gain_divisor = (double) tuple.get_int(Tuple::GainDivisor);
    peak_divisor = (double) tuple.get_int(Tuple::PeakDivisor);
    audgui_list_insert_rows(list, 0, 29);
}

static void unhook(void *) {
    hook_dissociate("playback ready", update_values);
    list = nullptr;
}

static void set_value_normal() {
    switch (tuple.get_value_type(tuple_field)) {
        case Tuple::String:
            string_buffer = String(tuple.get_str(tuple_field));
            break;
        case Tuple::Int:
            string_buffer = String(int_to_str(tuple.get_int(tuple_field)));
            break;
        default:
            break;
    }
}

static void set_value_rg() {
    double val = (double) tuple.get_int(tuple_field);
    if (val) {
        string_buffer = (
            strcmp_nocase(Tuple::field_get_name(tuple_field), "Peak") ?
            String(double_to_str(val / peak_divisor)) :
            String(str_concat({String(double_to_str(val / gain_divisor)), " dB"}))
        );
    }
}

static void set_value_rate() {
    string_buffer = String(str_concat({int_to_str(tuple.get_int(tuple_field)), " Kbps"}));
}

static void set_value_time() {
    int time = tuple.get_int(tuple_field);
    if (time >= 0) {
        int speed = time <= 1000 ? 1 : 1000;
        string_buffer = String(str_concat({int_to_str(time/speed), speed == 1 ? " Milliseconds" : " Seconds"}));
    }
}

enum val_types {
    normal_type, rg_type, rate_type, time_type
};

static void set_value(const int type = val_types::normal_type) {
    string_buffer = String("");
    if (tuple.is_set(tuple_field)) {
        switch (type) {
            case val_types::normal_type:
                set_value_normal();
                break;
            case val_types::rg_type:
                set_value_rg();
                break;
            case val_types::rate_type:
                set_value_rate();
                break;
            case val_types::time_type:
                set_value_time();
                break;
            default:
                break;
        }
    }
    g_value_set_string(g_value, String(string_buffer));
}

static const char * row_titles[] = {
    "Title", "Artist", "Album", "AlbumArtist", "Comment", "Genre", "Year", "Composer",
    "Performer", "Copyright", "Date", "Track", "Length", "Bitrate", "Codec", "Quality",
    "Basename", "Path", "Suffix", "AudioFile", "Subtune", "NumSubtunes", "StartTime",
    "EndTime", "AlbumGain", "AlbumPeak", "TrackGain", "TrackPeak", "FormattedTitle"
};

static const Tuple::Field row_tuples[] = {
    Tuple::Title, Tuple::Artist, Tuple::Album, Tuple::AlbumArtist, Tuple::Comment, Tuple::Genre,
    Tuple::Year, Tuple::Composer, Tuple::Performer, Tuple::Copyright, Tuple::Date, Tuple::Track,
    Tuple::Length, Tuple::Bitrate, Tuple::Codec, Tuple::Quality, Tuple::Basename, Tuple::Path,
    Tuple::Suffix, Tuple::AudioFile, Tuple::Subtune, Tuple::NumSubtunes, Tuple::StartTime, Tuple::EndTime,
    Tuple::AlbumGain, Tuple::AlbumPeak, Tuple::TrackGain, Tuple::TrackPeak, Tuple::FormattedTitle
};

static void get_value(void * user, int row, int column, GValue * value) {
    if (!tuple.valid()) {
        return;
    }
    if (column == 0){
        g_value_set_string(value, String(row_titles[row]));
        return;
    }
    g_value = value;
    tuple_field = row_tuples[row];
    switch(tuple_field) {
        case Tuple::Length:
        case Tuple::StartTime:
        case Tuple::EndTime:
            set_value(val_types::time_type);
            break;
        case Tuple::Bitrate:
            set_value(val_types::rate_type);
            break;
        case Tuple::AlbumGain:
        case Tuple::AlbumPeak:
        case Tuple::TrackGain:
        case Tuple::TrackPeak:
            set_value(val_types::rg_type);
            break;
        default:
            set_value();
            break;
    }
}

static const AudguiListCallbacks callbacks = {
    get_value
};

void * TagInfo::get_gtk_widget() {
#if GTK_MAJOR_VERSION <= 2
    GtkWidget * vbox = gtk_vbox_new(false, 6);
#else
    GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
#endif

    GtkWidget * scroll_window = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy((GtkScrolledWindow *) scroll_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_box_pack_start((GtkBox *) vbox, scroll_window, true, true, 0);

    list = audgui_list_new(& callbacks, nullptr, 0);
    gtk_tree_view_set_headers_visible((GtkTreeView *) list, true);
    audgui_list_add_column(list, _("Name"), 0, G_TYPE_STRING, 11);
    audgui_list_add_column(list, _("Value"), 1, G_TYPE_STRING, 11);

    gtk_container_add((GtkContainer *) scroll_window, list);

    hook_associate("playback ready", update_values, list);
    g_signal_connect(vbox, "destroy", (GCallback) unhook, nullptr);

    return vbox;
}

int TagInfo::take_message(const char * code, const void * data, int size) {
    if (! strcmp(code, "grab focus")) {
        if (list) {
            gtk_widget_grab_focus(list);
        }
        return 0;
    }
    return -1;
}

const char TagInfo::about[] = N_(
    "Tag Info (GTK)\n\n"
    "Displays tag information for current song.\n\n"
    "Copyright (C) 2016  kevinlekiller\n\n"
    "This program is free software: you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License as published by\n"
    "the Free Software Foundation, either version 3 of the License, or\n"
    "(at your option) any later version.\n\n"
    "This program is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "GNU General Public License for more details.\n\n"
    "You should have received a copy of the GNU General Public License\n"
    "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n"
);
