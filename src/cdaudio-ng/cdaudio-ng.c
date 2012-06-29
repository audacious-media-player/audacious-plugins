/*
 * Audacious CD Digital Audio plugin
 *
 * Copyright (c) 2007 Calin Crisan <ccrisan@gmail.com>
 * Copyright 2009 John Lindgren
 * Copyright 2009 Tomasz Moń <desowin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <cdio/cdio.h>
#include <cdio/cdtext.h>
#include <cdio/track.h>
#include <cdio/cdda.h>
#include <cdio/audio.h>
#include <cdio/sector.h>
#include <cdio/cd_types.h>

/* libcdio's header files #define these */
#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION

#include <cddb/cddb.h>

#include <glib.h>

#include <audacious/debug.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "cdaudio-ng.h"
#include "configure.h"

#define MAX_RETRIES 10
#define MAX_SKIPS 10

#define warn(...) fprintf(stderr, "cdaudio-ng: " __VA_ARGS__)

typedef struct
{
    gchar performer[DEF_STRING_LEN];
    gchar name[DEF_STRING_LEN];
    gchar genre[DEF_STRING_LEN];
    gint startlsn;
    gint endlsn;
}
trackinfo_t;

static GMutex *mutex;
static gint seek_time;
static gboolean stop_flag;

/* lock mutex to read / set these variables */
cdng_cfg_t cdng_cfg;
static gint firsttrackno = -1;
static gint lasttrackno = -1;
static gint n_audio_tracks;
static cdrom_drive_t *pcdrom_drive = NULL;
static trackinfo_t *trackinfo = NULL;
static gint monitor_source = 0;

/* read / set these variables in main thread only */

static gboolean cdaudio_init (void);
static void cdaudio_about (void);
static void cdaudio_configure (void);
static gint cdaudio_is_our_file (const gchar * filename, VFSFile * file);
static gboolean cdaudio_play (InputPlayback * p, const gchar * name, VFSFile *
 file, gint start, gint stop, gboolean pause);
static void cdaudio_stop (InputPlayback * pinputplayback);
static void cdaudio_pause (InputPlayback * p, gboolean paused);
static void cdaudio_mseek (InputPlayback * p, gint time);
static void cdaudio_cleanup (void);
static Tuple * make_tuple (const gchar * filename, VFSFile * file);
static void scan_cd (void);
static void refresh_trackinfo (gboolean warning);
static gint calculate_track_length (gint startlsn, gint endlsn);
static gint find_trackno_from_filename (const gchar * filename);

static const gchar * const schemes[] = {"cdda", NULL};

AUD_INPUT_PLUGIN
(
    .name = "Audio CD Support",
    .init = cdaudio_init,
    .about = cdaudio_about,
    .configure = cdaudio_configure,
    .is_our_file_from_vfs = cdaudio_is_our_file,
    .play = cdaudio_play,
    .stop = cdaudio_stop,
    .pause = cdaudio_pause,
    .mseek = cdaudio_mseek,
    .cleanup = cdaudio_cleanup,
    .probe_for_tuple = make_tuple,
    .schemes = schemes,
    .have_subtune = TRUE,
)

static void cdaudio_error (const gchar * message_format, ...)
{
    va_list args;
    gchar *msg = NULL;

    va_start (args, message_format);
    msg = g_markup_vprintf_escaped (message_format, args);
    va_end (args);

    aud_interface_show_error (msg);
    g_free (msg);
}

/* main thread only */
static void purge_playlist (gint playlist)
{
    gint length = aud_playlist_entry_count (playlist);

    for (gint count = 0; count < length; count ++)
    {
        gchar * filename = aud_playlist_entry_get_filename (playlist, count);

        if (cdaudio_is_our_file (filename, NULL))
        {
            aud_playlist_entry_delete (playlist, count, 1);
            count--;
            length--;
        }

        str_unref (filename);
    }
}

/* main thread only */
static void purge_all_playlists (void)
{
    gint playlists = aud_playlist_count ();
    gint count;

    for (count = 0; count < playlists; count++)
        purge_playlist (count);
}

/* main thread only */
static gboolean monitor (gpointer unused)
{
    g_mutex_lock (mutex);

    if (trackinfo != NULL)
        refresh_trackinfo (FALSE);

    if (trackinfo != NULL)
    {
        g_mutex_unlock (mutex);
        return TRUE;
    }

    monitor_source = 0;
    g_mutex_unlock (mutex);

    purge_all_playlists ();
    return FALSE;
}

/* mutex must be locked */
static void trigger_monitor (void)
{
    if (monitor_source)
        return;

#if GLIB_CHECK_VERSION (2, 14, 0)
    monitor_source = g_timeout_add_seconds (1, monitor, NULL);
#else
    monitor_source = g_timeout_add (1000, monitor, NULL);
#endif
}

static const gchar * const cdaudio_defaults[] = {
 "use_cdtext", "TRUE",
 "use_cddb", "TRUE",
 "cddbserver", "freedb.org",
 "cddbport", "8880",
 "cddbhttp", "FALSE",
 "disc_speed", "2",
 NULL};

/* main thread only */
static gboolean cdaudio_init (void)
{
    mutex = g_mutex_new ();

    aud_config_set_defaults ("CDDA", cdaudio_defaults);

    cdng_cfg.use_cdtext = aud_get_bool ("CDDA", "use_cdtext");
    cdng_cfg.use_cddb = aud_get_bool ("CDDA", "use_cddb");
    cdng_cfg.device = aud_get_string ("CDDA", "device");
    cdng_cfg.cddb_server = aud_get_string ("CDDA", "cddbserver");
    cdng_cfg.cddb_path = aud_get_string ("CDDA", "cddbpath");
    cdng_cfg.cddb_port = aud_get_int ("CDDA", "cddbport");
    cdng_cfg.cddb_http = aud_get_bool ("CDDA", "cddbhttp");
    cdng_cfg.disc_speed = aud_get_int ("CDDA", "disc_speed");
    cdng_cfg.disc_speed = CLAMP (cdng_cfg.disc_speed, MIN_DISC_SPEED, MAX_DISC_SPEED);

    cdng_cfg.use_proxy = aud_get_bool (NULL, "use_proxy");
    cdng_cfg.proxy_host = aud_get_string (NULL, "proxy_host");
    cdng_cfg.proxy_port = aud_get_int (NULL, "proxy_port");
    cdng_cfg.proxy_username = aud_get_string (NULL, "proxy_user");
    cdng_cfg.proxy_password = aud_get_string (NULL, "proxy_pass");

    if (!cdio_init ())
    {
        cdaudio_error ("Failed to initialize cdio subsystem.");
        return FALSE;
    }

    libcddb_init ();

    return TRUE;
}

/* main thread only */
static void cdaudio_about (void)
{
    static GtkWidget * about_window = NULL;

    audgui_simple_message (& about_window, GTK_MESSAGE_INFO,
     _("About Audio CD Plugin"),
     _("Copyright (c) 2007, by Calin Crisan <ccrisan@gmail.com> and The Audacious Team.\n\n"
     "Many thanks to libcdio developers <http://www.gnu.org/software/libcdio/>\n"
     "\tand to libcddb developers <http://libcddb.sourceforge.net/>.\n\n"
     "Also thank you Tony Vroon for mentoring & guiding me.\n\n"
     "This was a Google Summer of Code 2007 project.\n\n"
     "Copyright 2009 John Lindgren"));
}

/* main thread only */
static void cdaudio_configure ()
{
    configure_show_gui ();
}

/* thread safe (mutex may be locked) */
static gint cdaudio_is_our_file (const gchar * filename, VFSFile * file)
{
    return !strncmp (filename, "cdda://", 7);
}

/* thread safe (mutex may be locked) */
static void cdaudio_set_strinfo (trackinfo_t * t,
                                 const gchar * performer, const gchar * name,
                                 const gchar * genre)
{
    g_strlcpy (t->performer, performer, DEF_STRING_LEN);
    g_strlcpy (t->name, name, DEF_STRING_LEN);
    g_strlcpy (t->genre, genre, DEF_STRING_LEN);
}

/* thread safe (mutex may be locked) */
static void cdaudio_set_fullinfo (trackinfo_t * t,
                                  const lsn_t startlsn, const lsn_t endlsn,
                                  const gchar * performer, const gchar * name,
                                  const gchar * genre)
{
    t->startlsn = startlsn;
    t->endlsn = endlsn;
    cdaudio_set_strinfo (t, performer, name, genre);
}

/* play thread only */
static gboolean cdaudio_play (InputPlayback * p, const gchar * name, VFSFile *
 file, gint start, gint stop, gboolean pause)
{
    g_mutex_lock (mutex);

    if (trackinfo == NULL)
    {
        refresh_trackinfo (TRUE);

        if (trackinfo == NULL)
        {
ERR:
            g_mutex_unlock (mutex);
            return FALSE;
        }
    }

    gint trackno = find_trackno_from_filename (name);

    if (trackno < 0)
    {
        cdaudio_error ("Invalid URI %s.", name);
        goto ERR;
    }

    if (trackno < firsttrackno || trackno > lasttrackno)
    {
        cdaudio_error ("Track %d not found.", trackno);
        goto ERR;
    }

    /* don't play any data tracks */
    if (!cdda_track_audiop (pcdrom_drive, trackno))
    {
        cdaudio_error ("Track %d is a data track.\n", trackno);
        goto ERR;
    }

    gint startlsn = trackinfo[trackno].startlsn;
    gint endlsn = trackinfo[trackno].endlsn;

    if (! p->output->open_audio (FMT_S16_LE, 44100, 2))
    {
        cdaudio_error ("Failed to open audio output.");
        goto ERR;
    }

    seek_time = (start > 0) ? start : -1;
    stop_flag = FALSE;

    if (stop >= 0)
        endlsn = MIN (endlsn, startlsn + stop * 75 / 1000);

    if (pause)
        p->output->pause (TRUE);

    p->set_params (p, 1411200, 44100, 2);
    p->set_pb_ready (p);

    g_mutex_unlock (mutex);

    gint buffer_size = aud_get_int (NULL, "output_buffer_size");
    gint sectors = CLAMP (buffer_size / 2, 50, 250) * cdng_cfg.disc_speed * 75 / 1000;
    guchar buffer[2352 * sectors];
    gint currlsn = startlsn;
    gint retry_count = 0, skip_count = 0;

    while (1)
    {
        g_mutex_lock (mutex);

        if (stop_flag)
        {
            g_mutex_unlock (mutex);
            goto CLOSE;
        }

        if (seek_time >= 0)
        {
            p->output->flush (seek_time);
            currlsn = startlsn + (seek_time * 75 / 1000);
            seek_time = -1;
        }

        g_mutex_unlock (mutex);

        sectors = MIN (sectors, endlsn + 1 - currlsn);
        if (sectors < 1)
            break;

        if (cdio_read_audio_sectors (pcdrom_drive->p_cdio, buffer, currlsn, sectors) ==
         DRIVER_OP_SUCCESS)
        {
            retry_count = 0;
            skip_count = 0;
        }
        else if (sectors > 16)
        {
            warn ("Read failed; reducing read size.\n");
            sectors /= 2;
            continue;
        }
        else if (retry_count < MAX_RETRIES)
        {
            warn ("Read failed; retrying.\n");
            retry_count ++;
            continue;
        }
        else if (skip_count < MAX_SKIPS)
        {
            warn ("Read failed; skipping.\n");
            currlsn = MIN (currlsn + 75, endlsn + 1);
            skip_count ++;
            continue;
        }
        else
        {
            cdaudio_error ("Too many read errors; giving up.");
            break;
        }

        p->output->write_audio (buffer, 2352 * sectors);
        currlsn += sectors;
    }

    while (p->output->buffer_playing ())
        g_usleep (20000);

    g_mutex_lock (mutex);
    stop_flag = FALSE;
    g_mutex_unlock (mutex);

CLOSE:
    p->output->close_audio ();
    return TRUE;
}

/* main thread only */
static void cdaudio_stop (InputPlayback * p)
{
    g_mutex_lock (mutex);

    if (! stop_flag)
    {
        stop_flag = TRUE;
        p->output->abort_write();
    }

    g_mutex_unlock (mutex);
}

/* main thread only */
static void cdaudio_pause (InputPlayback * p, gboolean pause)
{
    g_mutex_lock (mutex);

    if (! stop_flag)
        p->output->pause (pause);

    g_mutex_unlock (mutex);
}

/* main thread only */
static void cdaudio_mseek (InputPlayback * p, gint time)
{
    g_mutex_lock (mutex);

    if (! stop_flag)
    {
        seek_time = time;
        p->output->abort_write();
    }

    g_mutex_unlock (mutex);
}

/* main thread only */
static void cdaudio_cleanup (void)
{
    g_mutex_lock (mutex);

    if (monitor_source)
    {
        g_source_remove (monitor_source);
        monitor_source = 0;
    }

    if (pcdrom_drive != NULL)
    {
        cdda_close (pcdrom_drive);
        pcdrom_drive = NULL;
    }
    if (trackinfo != NULL)
    {
        g_free (trackinfo);
        trackinfo = NULL;
    }

    libcddb_shutdown ();

    // todo: destroy the gui

    aud_set_bool ("CDDA", "use_cdtext", cdng_cfg.use_cdtext);
    aud_set_bool ("CDDA", "use_cddb", cdng_cfg.use_cddb);
    aud_set_string ("CDDA", "device", cdng_cfg.device);
    aud_set_string ("CDDA", "cddbserver", cdng_cfg.cddb_server);
    aud_set_string ("CDDA", "cddbpath", cdng_cfg.cddb_path);
    aud_set_int ("CDDA", "cddbport", cdng_cfg.cddb_port);
    aud_set_bool ("CDDA", "cddbhttp", cdng_cfg.cddb_http);
    aud_set_int ("CDDA", "disc_speed", cdng_cfg.disc_speed);

    g_free (cdng_cfg.device);
    g_free (cdng_cfg.cddb_server);
    g_free (cdng_cfg.cddb_path);
    g_free (cdng_cfg.proxy_host);
    g_free (cdng_cfg.proxy_username);
    g_free (cdng_cfg.proxy_password);

    g_mutex_unlock (mutex);
    g_mutex_free (mutex);
}

/* thread safe */
static Tuple * make_tuple (const gchar * filename, VFSFile * file)
{
    Tuple *tuple = NULL;
    gint trackno;

    g_mutex_lock (mutex);

    if (trackinfo == NULL)
        refresh_trackinfo (TRUE);
    if (trackinfo == NULL)
        goto DONE;

    if (!strcmp (filename, "cdda://"))
    {
        tuple = tuple_new_from_filename (filename);

        gint subtunes[n_audio_tracks];
        gint i = 0;

        /* only add the audio tracks to the playlist */
        for (trackno = firsttrackno; trackno <= lasttrackno; trackno++)
            if (cdda_track_audiop (pcdrom_drive, trackno))
                subtunes[i ++] = trackno;

        tuple_set_subtunes (tuple, n_audio_tracks, subtunes);

        goto DONE;
    }

    trackno = find_trackno_from_filename (filename);

    if (trackno < firsttrackno || trackno > lasttrackno)
    {
        warn ("Track %d not found.\n", trackno);
        goto DONE;
    }

    if (!cdda_track_audiop (pcdrom_drive, trackno))
    {
        warn ("Track %d is a data track.\n", trackno);
        goto DONE;
    }

    tuple = tuple_new_from_filename (filename);
    tuple_set_format (tuple, _("Audio CD"), 2, 44100, 1411);

    if (strlen (trackinfo[trackno].performer))
    {
        tuple_set_str (tuple, FIELD_ARTIST, NULL,
                                    trackinfo[trackno].performer);
    }
    if (strlen (trackinfo[0].name))
    {
        tuple_set_str (tuple, FIELD_ALBUM, NULL,
                                    trackinfo[0].name);
    }
    if (strlen (trackinfo[trackno].name))
    {
        tuple_set_str (tuple, FIELD_TITLE, NULL,
                                    trackinfo[trackno].name);
    }

    tuple_set_int (tuple, FIELD_TRACK_NUMBER, NULL, trackno);

    tuple_set_int (tuple, FIELD_LENGTH, NULL,
                             calculate_track_length (trackinfo[trackno].
                                                     startlsn,
                                                     trackinfo[trackno].
                                                     endlsn));

    if (strlen (trackinfo[trackno].genre))
    {
        tuple_set_str (tuple, FIELD_GENRE, NULL,
                                    trackinfo[trackno].genre);
    }

  DONE:
    g_mutex_unlock (mutex);
    return tuple;
}

/* mutex must be locked */
static void open_cd (void)
{
    AUDDBG ("Opening CD drive.\n");
    g_return_if_fail (pcdrom_drive == NULL);

    /* find an available, audio capable, cd drive  */
    if (cdng_cfg.device != NULL && strlen (cdng_cfg.device) > 0)
    {
        pcdrom_drive = cdda_identify (cdng_cfg.device, 1, NULL);
        if (pcdrom_drive == NULL)
        {
            cdaudio_error ("Failed to open CD device \"%s\".", cdng_cfg.device);
            return;
        }
    }
    else
    {
        gchar **ppcd_drives =
            cdio_get_devices_with_cap (NULL, CDIO_FS_AUDIO, false);

        if (ppcd_drives != NULL && *ppcd_drives != NULL)
        {                       /* we have at least one audio capable cd drive */
            pcdrom_drive = cdda_identify (*ppcd_drives, 1, NULL);
            if (pcdrom_drive == NULL)
            {
                cdaudio_error ("Failed to open CD.");
                return;
            }
            AUDDBG ("found cd drive \"%s\" with audio capable media\n",
                   *ppcd_drives);
        }
        else
        {
            cdaudio_error ("No audio capable CD drive found.");
            return;
        }

        if (ppcd_drives != NULL && *ppcd_drives != NULL)
            cdio_free_device_list (ppcd_drives);
    }
}

/* mutex must be locked */
static void scan_cd (void)
{
    AUDDBG ("Scanning CD drive.\n");
    g_return_if_fail (pcdrom_drive != NULL);
    g_return_if_fail (trackinfo == NULL);

    gint trackno;

    /* general track initialization */

    /* skip endianness detection (because it only affects cdda_read, and we use
     * cdio_read_audio_sectors instead) */
    pcdrom_drive->bigendianp = 0;

    /* finish initialization of drive/disc (performs disc TOC sanitization) */
    if (cdda_open (pcdrom_drive) != 0)
    {
        cdaudio_error ("Failed to finish initializing opened CD drive.");
        goto ERR;
    }

    if (cdda_speed_set (pcdrom_drive, cdng_cfg.disc_speed) != DRIVER_OP_SUCCESS)
        warn ("Cannot set drive speed.\n");

    firsttrackno = cdio_get_first_track_num (pcdrom_drive->p_cdio);
    lasttrackno = cdio_get_last_track_num (pcdrom_drive->p_cdio);
    if (firsttrackno == CDIO_INVALID_TRACK || lasttrackno == CDIO_INVALID_TRACK)
    {
        cdaudio_error ("Failed to retrieve first/last track number.");
        goto ERR;
    }
    AUDDBG ("first track is %d and last track is %d\n", firsttrackno,
           lasttrackno);

    trackinfo = (trackinfo_t *) g_new (trackinfo_t, (lasttrackno + 1));

    cdaudio_set_fullinfo (&trackinfo[0],
                          cdda_track_firstsector (pcdrom_drive, 0),
                          cdda_track_lastsector (pcdrom_drive, lasttrackno),
                          "", "", "");

    n_audio_tracks = 0;

    for (trackno = firsttrackno; trackno <= lasttrackno; trackno++)
    {
        cdaudio_set_fullinfo (&trackinfo[trackno],
                              cdda_track_firstsector (pcdrom_drive, trackno),
                              cdda_track_lastsector (pcdrom_drive, trackno),
                              "", "", "");

        if (trackinfo[trackno].startlsn == CDIO_INVALID_LSN
            || trackinfo[trackno].endlsn == CDIO_INVALID_LSN)
        {
            cdaudio_error ("Cannot read start/end LSN for track %d.", trackno);
            goto ERR;
        }

        /* count how many tracks are audio tracks */
        if (cdda_track_audiop (pcdrom_drive, trackno))
            n_audio_tracks++;
    }

    /* get trackinfo[0] cdtext information (the disc) */
    if (cdng_cfg.use_cdtext)
    {
        AUDDBG ("getting cd-text information for disc\n");
        cdtext_t *pcdtext = cdio_get_cdtext (pcdrom_drive->p_cdio, 0);
        if (pcdtext == NULL || pcdtext->field[CDTEXT_TITLE] == NULL)
        {
            AUDDBG ("no cd-text available for disc\n");
        }
        else
        {
            cdaudio_set_strinfo (&trackinfo[0],
                                 pcdtext->field[CDTEXT_PERFORMER] ? pcdtext->
                                 field[CDTEXT_PERFORMER] : "",
                                 pcdtext->field[CDTEXT_TITLE] ? pcdtext->
                                 field[CDTEXT_TITLE] : "",
                                 pcdtext->field[CDTEXT_GENRE] ? pcdtext->
                                 field[CDTEXT_GENRE] : "");
        }
    }

    /* get track information from cdtext */
    gboolean cdtext_was_available = FALSE;
    for (trackno = firsttrackno; trackno <= lasttrackno; trackno++)
    {
        cdtext_t *pcdtext = NULL;
        if (cdng_cfg.use_cdtext)
        {
            AUDDBG ("getting cd-text information for track %d\n", trackno);
            pcdtext = cdio_get_cdtext (pcdrom_drive->p_cdio, trackno);
            if (pcdtext == NULL || pcdtext->field[CDTEXT_PERFORMER] == NULL)
            {
                AUDDBG ("no cd-text available for track %d\n", trackno);
                pcdtext = NULL;
            }
        }

        if (pcdtext != NULL)
        {
            cdaudio_set_strinfo (&trackinfo[trackno],
                                 pcdtext->field[CDTEXT_PERFORMER] ? pcdtext->
                                 field[CDTEXT_PERFORMER] : "",
                                 pcdtext->field[CDTEXT_TITLE] ? pcdtext->
                                 field[CDTEXT_TITLE] : "",
                                 pcdtext->field[CDTEXT_GENRE] ? pcdtext->
                                 field[CDTEXT_GENRE] : "");
            cdtext_was_available = TRUE;
        }
        else
        {
            cdaudio_set_strinfo (&trackinfo[trackno], "", "", "");
            snprintf (trackinfo[trackno].name, DEF_STRING_LEN,
                      "Track %d", trackno);
        }
    }

    if (!cdtext_was_available)
    {
        /* initialize de cddb subsystem */
        cddb_conn_t *pcddb_conn = NULL;
        cddb_disc_t *pcddb_disc = NULL;
        cddb_track_t *pcddb_track = NULL;
        lba_t lba;              /* Logical Block Address */

        if (cdng_cfg.use_cddb)
        {
            pcddb_conn = cddb_new ();
            if (pcddb_conn == NULL)
                cdaudio_error ("Failed to create the cddb connection.");
            else
            {
                AUDDBG ("getting CDDB info\n");

                cddb_cache_enable (pcddb_conn);
                // cddb_cache_set_dir(pcddb_conn, "~/.cddbslave");

                if (cdng_cfg.use_proxy)
                {
                    cddb_http_proxy_enable (pcddb_conn);
                    cddb_set_http_proxy_server_name (pcddb_conn,
                                                     cdng_cfg.proxy_host);
                    cddb_set_http_proxy_server_port (pcddb_conn,
                                                     cdng_cfg.proxy_port);
                    cddb_set_http_proxy_username (pcddb_conn,
                                                  cdng_cfg.proxy_username);
                    cddb_set_http_proxy_password (pcddb_conn,
                                                  cdng_cfg.proxy_password);
                    cddb_set_server_name (pcddb_conn, cdng_cfg.cddb_server);
                    cddb_set_server_port (pcddb_conn, cdng_cfg.cddb_port);
                }
                else if (cdng_cfg.cddb_http)
                {
                    cddb_http_enable (pcddb_conn);
                    cddb_set_server_name (pcddb_conn, cdng_cfg.cddb_server);
                    cddb_set_server_port (pcddb_conn, cdng_cfg.cddb_port);
                    cddb_set_http_path_query (pcddb_conn, cdng_cfg.cddb_path);
                }
                else
                {
                    cddb_set_server_name (pcddb_conn, cdng_cfg.cddb_server);
                    cddb_set_server_port (pcddb_conn, cdng_cfg.cddb_port);
                }

                pcddb_disc = cddb_disc_new ();

                lba = cdio_get_track_lba (pcdrom_drive->p_cdio,
                                          CDIO_CDROM_LEADOUT_TRACK);
                cddb_disc_set_length (pcddb_disc, FRAMES_TO_SECONDS (lba));

                for (trackno = firsttrackno; trackno <= lasttrackno; trackno++)
                {
                    pcddb_track = cddb_track_new ();
                    cddb_track_set_frame_offset (pcddb_track,
                                                 cdio_get_track_lba (
                                                     pcdrom_drive->p_cdio,
                                                     trackno));
                    cddb_disc_add_track (pcddb_disc, pcddb_track);
                }

                cddb_disc_calc_discid (pcddb_disc);

#if DEBUG
                guint discid = cddb_disc_get_discid (pcddb_disc);
                AUDDBG ("CDDB disc id = %x\n", discid);
#endif

                gint matches;
                if ((matches = cddb_query (pcddb_conn, pcddb_disc)) == -1)
                {
                    if (cddb_errno (pcddb_conn) == CDDB_ERR_OK)
                        cdaudio_error ("Failed to query the CDDB server");
                    else
                        cdaudio_error ("Failed to query the CDDB server: %s",
                                       cddb_error_str (cddb_errno
                                                       (pcddb_conn)));

                    cddb_disc_destroy (pcddb_disc);
                    pcddb_disc = NULL;
                }
                else
                {
                    if (matches == 0)
                    {
                        AUDDBG ("no cddb info available for this disc\n");

                        cddb_disc_destroy (pcddb_disc);
                        pcddb_disc = NULL;
                    }
                    else
                    {
                        AUDDBG ("CDDB disc category = \"%s\"\n",
                               cddb_disc_get_category_str (pcddb_disc));

                        cddb_read (pcddb_conn, pcddb_disc);
                        if (cddb_errno (pcddb_conn) != CDDB_ERR_OK)
                        {
                            cdaudio_error ("failed to read the cddb info: %s",
                                           cddb_error_str (cddb_errno
                                                           (pcddb_conn)));
                            cddb_disc_destroy (pcddb_disc);
                            pcddb_disc = NULL;
                        }
                        else
                        {
                            cdaudio_set_strinfo (&trackinfo[0],
                                                 cddb_disc_get_artist
                                                 (pcddb_disc),
                                                 cddb_disc_get_title
                                                 (pcddb_disc),
                                                 cddb_disc_get_genre
                                                 (pcddb_disc));

                            gint trackno;
                            for (trackno = firsttrackno; trackno <= lasttrackno;
                                 trackno++)
                            {
                                cddb_track_t *pcddb_track =
                                    cddb_disc_get_track (pcddb_disc,
                                                         trackno - 1);
                                cdaudio_set_strinfo (&trackinfo[trackno],
                                                     cddb_track_get_artist
                                                     (pcddb_track),
                                                     cddb_track_get_title
                                                     (pcddb_track),
                                                     cddb_disc_get_genre
                                                     (pcddb_disc));
                            }
                        }
                    }
                }
            }
        }

        if (pcddb_disc != NULL)
            cddb_disc_destroy (pcddb_disc);

        if (pcddb_conn != NULL)
            cddb_destroy (pcddb_conn);
    }

    return;

  ERR:
    g_free (trackinfo);
    trackinfo = NULL;
}

/* mutex must be locked */
static void refresh_trackinfo (gboolean warning)
{
    trigger_monitor ();

    if (pcdrom_drive == NULL)
    {
        open_cd ();
        if (pcdrom_drive == NULL)
            return;
    }

    int mode = cdio_get_discmode (pcdrom_drive->p_cdio);
#ifdef _WIN32 /* cdio_get_discmode reports the wrong disk type sometimes */
    if (mode == CDIO_DISC_MODE_NO_INFO || mode == CDIO_DISC_MODE_ERROR)
#else
    if (mode != CDIO_DISC_MODE_CD_DA && mode != CDIO_DISC_MODE_CD_MIXED)
#endif
    {
        if (warning)
        {
            if (mode == CDIO_DISC_MODE_NO_INFO)
                cdaudio_error (_("Drive is empty."));
            else
                cdaudio_error (_("Unsupported disk type."));
        }

        /* reset libcdio, else it will not read a new disk correctly */
        if (pcdrom_drive)
        {
            cdda_close (pcdrom_drive);
            pcdrom_drive = NULL;
        }

        g_free (trackinfo);
        trackinfo = NULL;
        return;
    }

    if (trackinfo == NULL || cdio_get_media_changed (pcdrom_drive->p_cdio))
    {
        g_free (trackinfo);
        trackinfo = NULL;
        scan_cd ();
    }
}

/* thread safe (mutex may be locked) */
static gint calculate_track_length (gint startlsn, gint endlsn)
{
    return ((endlsn - startlsn + 1) * 1000) / 75;
}

/* thread safe (mutex may be locked) */
static gint find_trackno_from_filename (const gchar * filename)
{
    gint track;

    if (strncmp (filename, "cdda://?", 8) || sscanf (filename + 8, "%d",
                                                     &track) != 1)
        return -1;

    return track;
}
