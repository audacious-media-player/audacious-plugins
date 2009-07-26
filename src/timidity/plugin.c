/*
    xmms-timidity - MIDI Plugin for XMMS
    Copyright (C) 2004 Konstantin Korikov <lostclus@ua.fm>
    Copyright (C) 2009 Matti Hämäläinen <ccr@tnsp.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>
#include "libtimidity/timidity.h"
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include "interface.h"

#define TIMID_CFGID "timidity"

static struct
{
    gchar *config_file;
    gint rate;
    gint bits;
    gint channels;
    gint buffer_size;
} xmmstimid_cfg;


static gboolean xmmstimid_initialized = FALSE;
static GMutex *seek_mutex = NULL;
static GCond *seek_cond = NULL;
static gint seek_value = -1;

static GtkWidget *xmmstimid_conf_wnd = NULL;
static GtkEntry *xmmstimid_conf_config_file;
static GtkToggleButton *xmmstimid_conf_rate_11000, *xmmstimid_conf_rate_22000, *xmmstimid_conf_rate_44100;
static GtkToggleButton *xmmstimid_conf_bits_8, *xmmstimid_conf_bits_16;
static GtkToggleButton *xmmstimid_conf_channels_1, *xmmstimid_conf_channels_2;


void xmmstimid_init(void)
{
    mcs_handle_t *db;

    seek_mutex = g_mutex_new();
    seek_cond = g_cond_new();

    xmmstimid_cfg.config_file = NULL;
    xmmstimid_cfg.rate = 44100;
    xmmstimid_cfg.bits = 16;
    xmmstimid_cfg.channels = 2;
    xmmstimid_cfg.buffer_size = 512;

    db = aud_cfg_db_open();

    if (!aud_cfg_db_get_string(db, TIMID_CFGID, "config_file", &xmmstimid_cfg.config_file))
        xmmstimid_cfg.config_file = g_strdup("/etc/timidity/timidity.cfg");

    aud_cfg_db_get_int(db, TIMID_CFGID, "samplerate", &xmmstimid_cfg.rate);
    aud_cfg_db_get_int(db, TIMID_CFGID, "bits", &xmmstimid_cfg.bits);
    aud_cfg_db_get_int(db, TIMID_CFGID, "channels", &xmmstimid_cfg.channels);
    aud_cfg_db_close(db);

    if (mid_init(xmmstimid_cfg.config_file) != 0)
    {
        xmmstimid_initialized = FALSE;
        return;
    }
    xmmstimid_initialized = TRUE;
}

static void xmmstimid_cleanup(void)
{
    g_mutex_free(seek_mutex);
    g_cond_free(seek_cond);

    g_free(xmmstimid_cfg.config_file);
    xmmstimid_cfg.config_file = NULL;

    if (xmmstimid_initialized)
        mid_exit();
}

static void xmmstimid_about(void)
{
    static GtkWidget *aboutbox = NULL;
    if (aboutbox == NULL)
    {
        gchar *title, *desc;
        title = g_strdup_printf(_("TiMidity Plugin %s"), PACKAGE_VERSION);
        desc = g_strjoin("", _("TiMidity Plugin\nhttp://libtimidity.sourceforge.net\nby Konstantin Korikov"), NULL);
        aboutbox = audacious_info_dialog(title, desc, _("Ok"), FALSE, NULL, NULL);
        g_signal_connect(G_OBJECT(aboutbox), "destroy", (GCallback) gtk_widget_destroyed, &aboutbox);
        g_free(title);
        g_free(desc);
    }
    else
        gtk_window_present(GTK_WINDOW(aboutbox));
}

static void xmmstimid_conf_ok(GtkButton * button, gpointer user_data);

#define get_conf_wnd_item(type, name) type (g_object_get_data(G_OBJECT(xmmstimid_conf_wnd), name))

static void xmmstimid_configure(void)
{
    GtkToggleButton *tb;

    if (xmmstimid_conf_wnd == NULL)
    {
        xmmstimid_conf_wnd = create_xmmstimid_conf_wnd();

        xmmstimid_conf_config_file = get_conf_wnd_item(GTK_ENTRY, "config_file");
        xmmstimid_conf_rate_11000 = get_conf_wnd_item(GTK_TOGGLE_BUTTON, "rate_11000");
        xmmstimid_conf_rate_22000 = get_conf_wnd_item(GTK_TOGGLE_BUTTON, "rate_22000");
        xmmstimid_conf_rate_44100 = get_conf_wnd_item(GTK_TOGGLE_BUTTON, "rate_44100");
        xmmstimid_conf_bits_8 = get_conf_wnd_item(GTK_TOGGLE_BUTTON, "bits_8");
        xmmstimid_conf_bits_16 = get_conf_wnd_item(GTK_TOGGLE_BUTTON, "bits_16");
        xmmstimid_conf_channels_1 = get_conf_wnd_item(GTK_TOGGLE_BUTTON, "channels_1");
        xmmstimid_conf_channels_2 = get_conf_wnd_item(GTK_TOGGLE_BUTTON, "channels_2");

        gtk_signal_connect_object(get_conf_wnd_item(GTK_OBJECT, "conf_ok"),
            "clicked", G_CALLBACK(xmmstimid_conf_ok), NULL);
    }

    gtk_entry_set_text(xmmstimid_conf_config_file, xmmstimid_cfg.config_file);

    switch (xmmstimid_cfg.rate)
    {
      case 11025:
          tb = xmmstimid_conf_rate_11000;
          break;
      case 22050:
          tb = xmmstimid_conf_rate_22000;
          break;
      case 44100:
      default:
          xmmstimid_cfg.rate = 44100;
          tb = xmmstimid_conf_rate_44100;
          break;
    }

    if (tb != NULL)
        gtk_toggle_button_set_active(tb, TRUE);

    switch (xmmstimid_cfg.bits)
    {
      case 8:
          tb = xmmstimid_conf_bits_8;
          break;
      case 16:
      default:
          xmmstimid_cfg.bits = 16;
          tb = xmmstimid_conf_bits_16;
          break;
    }

    if (tb != NULL)
        gtk_toggle_button_set_active(tb, TRUE);

    switch (xmmstimid_cfg.channels)
    {
      case 1:
          tb = xmmstimid_conf_channels_1;
          break;
      case 2:
      default:
          xmmstimid_cfg.channels = 2;
          tb = xmmstimid_conf_channels_2;
          break;
    }

    if (tb != NULL)
        gtk_toggle_button_set_active(tb, TRUE);

    gtk_widget_show(xmmstimid_conf_wnd);
    gtk_window_present(GTK_WINDOW(xmmstimid_conf_wnd));
}

static void xmmstimid_conf_ok(GtkButton * button, gpointer user_data)
{
    mcs_handle_t *db;

    if (gtk_toggle_button_get_active(xmmstimid_conf_rate_11000))
        xmmstimid_cfg.rate = 11025;
    else if (gtk_toggle_button_get_active(xmmstimid_conf_rate_22000))
        xmmstimid_cfg.rate = 22050;
    else
        xmmstimid_cfg.rate = 44100;

    if (gtk_toggle_button_get_active(xmmstimid_conf_bits_8))
        xmmstimid_cfg.bits = 8;
    else
        xmmstimid_cfg.bits = 16;

    if (gtk_toggle_button_get_active(xmmstimid_conf_channels_1))
        xmmstimid_cfg.channels = 1;
    else
        xmmstimid_cfg.channels = 2;

    db = aud_cfg_db_open();

    g_free(xmmstimid_cfg.config_file);
    xmmstimid_cfg.config_file = g_strdup(gtk_entry_get_text(xmmstimid_conf_config_file));

    aud_cfg_db_set_string(db, TIMID_CFGID, "config_file", xmmstimid_cfg.config_file);
    aud_cfg_db_set_int(db, TIMID_CFGID, "samplerate", xmmstimid_cfg.rate);
    aud_cfg_db_set_int(db, TIMID_CFGID, "bits", xmmstimid_cfg.bits);
    aud_cfg_db_set_int(db, TIMID_CFGID, "channels", xmmstimid_cfg.channels);
    aud_cfg_db_close(db);

    gtk_widget_hide(xmmstimid_conf_wnd);
}

static gint xmmstimid_is_our_fd(const gchar * filename, VFSFile * fp)
{
    gchar magic_bytes[4];

    aud_vfs_fread(magic_bytes, 1, 4, fp);

    if (!memcmp(magic_bytes, "MThd", 4))
        return TRUE;

    if (!memcmp(magic_bytes, "RIFF", 4))
    {
        /* skip the four bytes after RIFF, then read the next four */
        aud_vfs_fseek(fp, 4, SEEK_CUR);
        aud_vfs_fread(magic_bytes, 1, 4, fp);
        if (!memcmp(magic_bytes, "RMID", 4))
            return TRUE;
    }

    return FALSE;
}

static Tuple *xmmstimid_get_song_tuple(const gchar * filename)
{
    Tuple *tuple = aud_tuple_new_from_filename(filename);
    const gchar *tmp;
    MidIStream *stream;
    MidSongOptions opts;
    MidSong *song;

    if (tuple == NULL)
        return NULL;

    if (!xmmstimid_initialized)
    {
        xmmstimid_init();
        if (!xmmstimid_initialized)
            return tuple;
    }

    stream = mid_istream_open_file(filename);
    if (stream == NULL)
        return tuple;

    opts.rate = xmmstimid_cfg.rate;
    opts.format = (xmmstimid_cfg.bits == 16) ? MID_AUDIO_S16LSB : MID_AUDIO_S8;
    opts.channels = xmmstimid_cfg.channels;
    opts.buffer_size = 8;

    song = mid_song_load(stream, &opts);
    mid_istream_close(stream);

    if (song == NULL)
        return tuple;

    if ((tmp = mid_song_get_meta(song, MID_SONG_TRACK_NAME)) == NULL)
        tmp = aud_tuple_get_string(tuple, FIELD_FILE_NAME, NULL);
    aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, tmp);

    if ((tmp = mid_song_get_meta(song, MID_SONG_COPYRIGHT)) != NULL)
        aud_tuple_associate_string(tuple, FIELD_COPYRIGHT, NULL, tmp);

    if ((tmp = mid_song_get_meta(song, MID_SONG_TEXT)) != NULL)
        aud_tuple_associate_string(tuple, FIELD_COMMENT, NULL, tmp);

    aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, mid_song_get_total_time(song));
    aud_tuple_associate_string(tuple, FIELD_CODEC, NULL, "TiMidity MIDI");
    aud_tuple_associate_string(tuple, FIELD_QUALITY, NULL, "sequenced");

    mid_song_free(song);
    return tuple;
}


static void xmmstimid_play_file(InputPlayback * playback)
{
    MidIStream *stream;
    MidSong *xmmstimid_song;
    MidSongOptions xmmstimid_opts;
    Tuple *tuple;
    gsize buffer_size;
    guint8 *buffer;
    AFormat fmt;

    if (!xmmstimid_initialized)
    {
        xmmstimid_init();
        if (!xmmstimid_initialized)
            return;
    }

    stream = mid_istream_open_file(playback->filename);
    if (stream == NULL)
        return;

    xmmstimid_opts.rate = xmmstimid_cfg.rate;
    xmmstimid_opts.format = (xmmstimid_cfg.bits == 16) ? MID_AUDIO_S16LSB : MID_AUDIO_S8;
    xmmstimid_opts.channels = xmmstimid_cfg.channels;
    xmmstimid_opts.buffer_size = xmmstimid_cfg.buffer_size;

    xmmstimid_song = mid_song_load(stream, &xmmstimid_opts);
    mid_istream_close(stream);

    if (xmmstimid_song == NULL)
    {
        playback->set_title(playback, _("Couldn't load MIDI file"));
        return;
    }

    fmt = (xmmstimid_opts.format == MID_AUDIO_S16LSB) ? FMT_S16_LE : FMT_S8;
    if (playback->output->open_audio(fmt, xmmstimid_opts.rate, xmmstimid_opts.channels) == 0)
    {
        playback->error = TRUE;
        mid_song_free(xmmstimid_song);
        return;
    }

    tuple = xmmstimid_get_song_tuple(playback->filename);
    playback->set_params(playback, aud_tuple_get_string(tuple, FIELD_FILE_NAME, NULL), mid_song_get_total_time(xmmstimid_song), 0, xmmstimid_opts.rate, xmmstimid_opts.channels);
    tuple_free(tuple);

    buffer_size = ((xmmstimid_opts.format == MID_AUDIO_S16LSB) ? 16 : 8) * xmmstimid_opts.channels / 8 * xmmstimid_opts.buffer_size;

    if ((buffer = g_malloc(buffer_size)) == NULL)
    {
        mid_song_free(xmmstimid_song);
        return;
    }

    mid_song_start(xmmstimid_song);

    playback->playing = TRUE;
    playback->eof = FALSE;
    playback->set_pb_ready(playback);

    while (playback->playing)
    {
        gsize bytes_read;

        g_mutex_lock(seek_mutex);
        if (seek_value >= 0)
        {
            mid_song_seek(xmmstimid_song, seek_value * 1000);
            playback->output->flush(seek_value * 1000);
            seek_value = -1;
            bytes_read = 0;
            g_cond_signal(seek_cond);
        }
        g_mutex_unlock(seek_mutex);

        bytes_read = mid_song_read_wave(xmmstimid_song, buffer, buffer_size);

        if (bytes_read > 0)
        {
            playback->pass_audio(playback, fmt, xmmstimid_opts.channels, bytes_read, buffer, &playback->playing);
        }
        else
        {
            playback->eof = TRUE;
            playback->playing = FALSE;
        }

    }

    playback->output->close_audio();
    playback->playing = FALSE;
    mid_song_free(xmmstimid_song);
    g_free(buffer);
}

static void xmmstimid_stop(InputPlayback * playback)
{
    playback->playing = FALSE;
}

static void xmmstimid_pause(InputPlayback * playback, gshort p)
{
    playback->output->pause(p);
}

static void xmmstimid_seek(InputPlayback * playback, gint time)
{
    g_mutex_lock(seek_mutex);
    seek_value = time;
    playback->eof = FALSE;
    g_cond_wait(seek_cond, seek_mutex);
    g_mutex_unlock(seek_mutex);
}

static gchar *midi_exts[] = { "mid", "midi", "rmi", NULL };

static InputPlugin xmmstimid_ip = {
    .description = "TiMidity Audio Plugin",
    .init = xmmstimid_init,
    .about = xmmstimid_about,
    .configure = xmmstimid_configure,
    .play_file = xmmstimid_play_file,
    .stop = xmmstimid_stop,
    .pause = xmmstimid_pause,
    .seek = xmmstimid_seek,
    .cleanup = xmmstimid_cleanup,
    .is_our_file_from_vfs = xmmstimid_is_our_fd,
    .get_song_tuple = xmmstimid_get_song_tuple,
    .vfs_extensions = midi_exts,
};

InputPlugin *timidity_iplist[] = { &xmmstimid_ip, NULL };

DECLARE_PLUGIN(timidity, NULL, NULL, timidity_iplist, NULL, NULL, NULL, NULL, NULL);
