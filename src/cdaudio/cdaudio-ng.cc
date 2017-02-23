/*
 * Audacious CD Digital Audio plugin
 *
 * Copyright (c) 2007 Calin Crisan <ccrisan@gmail.com>
 * Copyright (c) 2009-2012 John Lindgren <john.lindgren@aol.com>
 * Copyright (c) 2009 Tomasz Moń <desowin@gmail.com>
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

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* prevent libcdio from redefining PACKAGE, VERSION, etc. */
#define EXTERNAL_LIBCDIO_CONFIG_H

#include <cdio/cdio.h>
#include <cdio/cdtext.h>
#include <cdio/track.h>
#include <cdio/audio.h>
#include <cdio/sector.h>
#include <cdio/cd_types.h>

#if LIBCDIO_VERSION_NUM >= 90
#include <cdio/paranoia/cdda.h>
#else
#include <cdio/cdda.h>
#endif

#include <cddb/cddb.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/mainloop.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#define MIN_DISC_SPEED 2
#define MAX_DISC_SPEED 24

#define MAX_RETRIES 10
#define MAX_SKIPS 10

static const char * const cdaudio_schemes[] = {"cdda", nullptr};

class CDAudio : public InputPlugin
{
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Audio CD Plugin"),
        PACKAGE,
        about,
        & prefs
    };

    constexpr CDAudio () : InputPlugin (info, InputInfo (FlagSubtunes)
        .with_schemes (cdaudio_schemes)) {}

    bool init ();
    void cleanup ();

    bool is_our_file (const char * filename, VFSFile & file);
    bool read_tag (const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image);
    bool play (const char * filename, VFSFile & file);
};

EXPORT CDAudio aud_plugin_instance;

typedef struct
{
    String performer;
    String name;
    String genre;
    int startlsn;
    int endlsn;
}
trackinfo_t;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static bool playing;

/* lock mutex to read / set these variables */
static int firsttrackno = -1;
static int lasttrackno = -1;
static int n_audio_tracks;
static cdrom_drive_t *pcdrom_drive = nullptr;
static Index<trackinfo_t> trackinfo;
static QueuedFunc purge_func;

static bool scan_cd ();
static bool refresh_trackinfo (bool warning);
static void reset_trackinfo ();
static int calculate_track_length (int startlsn, int endlsn);
static int find_trackno_from_filename (const char * filename);

const char CDAudio::about[] =
 N_("Copyright (C) 2007-2012 Calin Crisan <ccrisan@gmail.com> and others.\n\n"
    "Many thanks to libcdio developers <http://www.gnu.org/software/libcdio/>\n"
    "and to libcddb developers <http://libcddb.sourceforge.net/>.\n\n"
    "Also thank you to Tony Vroon for mentoring and guiding me.\n\n"
    "This was a Google Summer of Code 2007 project.");

const char * const CDAudio::defaults[] = {
 "disc_speed", "2",
 "use_cdtext", "TRUE",
 "use_cddb", "TRUE",
 "cddbhttp", "FALSE",
 "cddbserver", "freedb.org",
 "cddbport", "8880",
 nullptr};

const PreferencesWidget CDAudio::widgets[] = {
    WidgetLabel (N_("<b>Device</b>")),
    WidgetSpin (N_("Read speed:"),
        WidgetInt ("CDDA", "disc_speed"),
        {MIN_DISC_SPEED, MAX_DISC_SPEED, 1}),
    WidgetEntry (N_("Override device:"),
        WidgetString ("CDDA", "device")),
    WidgetLabel (N_("<b>Metadata</b>")),
    WidgetCheck (N_("Use CD-Text"),
        WidgetBool ("CDDA", "use_cdtext")),
    WidgetCheck (N_("Use CDDB"),
        WidgetBool ("CDDA", "use_cddb")),
    WidgetCheck (N_("Use HTTP instead of CDDBP"),
        WidgetBool ("CDDA", "cddbhttp"),
        WIDGET_CHILD),
    WidgetEntry (N_("Server:"),
        WidgetString ("CDDA", "cddbserver"),
        {false},
        WIDGET_CHILD),
    WidgetEntry (N_("Path:"),
        WidgetString ("CDDA", "cddbpath"),
        {false},
        WIDGET_CHILD),
    WidgetSpin (N_("Port:"),
        WidgetInt ("CDDA", "cddbport"),
        {0, 65535, 1},
        WIDGET_CHILD)
};

const PluginPreferences CDAudio::prefs = {{widgets}};

static void cdaudio_error (const char * message_format, ...)
{
    va_list args;
    va_start (args, message_format);
    StringBuf msg = str_vprintf (message_format, args);
    va_end (args);

    aud_ui_show_error (msg);
}

/* main thread only */
static void purge_playlist (Playlist playlist)
{
    int length = playlist.n_entries ();

    for (int i = 0; i < length; i ++)
    {
        String filename = playlist.entry_filename (i);

        if (! strncmp (filename, "cdda://", 7))
        {
            playlist.remove_entry (i);
            i--;
            length--;
        }
    }
}

/* main thread only */
static void purge_all_playlists (void * = nullptr)
{
    int playlists = Playlist::n_playlists ();

    for (int i = 0; i < playlists; i++)
        purge_playlist (Playlist::by_index (i));
}

/* main thread only */
static void monitor (void *)
{
    pthread_mutex_lock (& mutex);

    /* make sure not to close drive handle while playing */
    if (! playing)
        refresh_trackinfo (false);

    pthread_mutex_unlock (& mutex);
}

/* main thread only */
bool CDAudio::init ()
{
    aud_config_set_defaults ("CDDA", defaults);

    if (!cdio_init ())
    {
        cdaudio_error (_("Failed to initialize cdio subsystem."));
        return false;
    }

    libcddb_init ();

    return true;
}

/* thread safe (mutex may be locked) */
bool CDAudio::is_our_file (const char * filename, VFSFile & file)
{
    return !strncmp (filename, "cdda://", 7);
}

/* play thread only */
bool CDAudio::play (const char * name, VFSFile & file)
{
    pthread_mutex_lock (& mutex);

    if (! trackinfo.len () && ! refresh_trackinfo (true))
    {
        pthread_mutex_unlock (& mutex);
        return false;
    }

    bool okay = false;
    int trackno = find_trackno_from_filename (name);

    if (trackno < 0)
        cdaudio_error (_("Invalid URI %s."), name);
    else if (trackno < firsttrackno || trackno > lasttrackno)
        cdaudio_error (_("Track %d not found."), trackno);
    else if (! cdda_track_audiop (pcdrom_drive, trackno))
        cdaudio_error (_("Track %d is a data track."), trackno);
    else
        okay = true;

    if (! okay)
    {
        pthread_mutex_unlock (& mutex);
        return false;
    }

    set_stream_bitrate (1411200);
    open_audio (FMT_S16_LE, 44100, 2);

    int startlsn = trackinfo[trackno].startlsn;
    int endlsn = trackinfo[trackno].endlsn;

    playing = true;

    int buffer_size = aud_get_int (nullptr, "output_buffer_size");
    int speed = aud_get_int ("CDDA", "disc_speed");
    speed = aud::clamp (speed, MIN_DISC_SPEED, MAX_DISC_SPEED);
    int sectors = aud::clamp (buffer_size / 2, 50, 250) * speed * 75 / 1000;
    int currlsn = startlsn;
    int retry_count = 0, skip_count = 0;

    Index<unsigned char> buffer;
    buffer.insert (0, 2352 * sectors);

    while (! check_stop ())
    {
        int seek_time = check_seek ();
        if (seek_time >= 0)
            currlsn = startlsn + (seek_time * 75 / 1000);

        sectors = aud::min (sectors, endlsn + 1 - currlsn);
        if (sectors < 1)
            break;

        /* unlock mutex here to avoid blocking
         * other threads must be careful not to close drive handle */
        pthread_mutex_unlock (& mutex);

        int ret = cdio_read_audio_sectors (pcdrom_drive->p_cdio,
         buffer.begin (), currlsn, sectors);

        if (ret == DRIVER_OP_SUCCESS)
            write_audio (buffer.begin (), 2352 * sectors);

        pthread_mutex_lock (& mutex);

        if (ret == DRIVER_OP_SUCCESS)
        {
            currlsn += sectors;
            retry_count = 0;
            skip_count = 0;
        }
        else if (sectors > 16)
        {
            /* maybe a smaller read size will help */
            sectors /= 2;
        }
        else if (retry_count < MAX_RETRIES)
        {
            /* still failed; retry a few times */
            retry_count ++;
        }
        else if (skip_count < MAX_SKIPS)
        {
            /* maybe the disk is scratched; try skipping ahead */
            currlsn = aud::min (currlsn + 75, endlsn + 1);
            skip_count ++;
        }
        else
        {
            /* still failed; give it up */
            cdaudio_error (_("Error reading audio CD."));
            break;
        }
    }

    playing = false;

    pthread_mutex_unlock (& mutex);
    return true;
}

/* main thread only */
void CDAudio::cleanup ()
{
    pthread_mutex_lock (& mutex);

    reset_trackinfo ();
    purge_func.stop ();

    libcddb_shutdown ();

    pthread_mutex_unlock (& mutex);
}

/* thread safe */
bool CDAudio::read_tag (const char * filename, VFSFile & file, Tuple & tuple,
 Index<char> * image)
{
    bool whole_disk = ! strcmp (filename, "cdda://");
    bool valid = false;

    pthread_mutex_lock (& mutex);

    /* reset cached info when adding CD to the playlist */
    if (whole_disk && ! playing)
        reset_trackinfo ();

    if (! trackinfo.len () && ! refresh_trackinfo (true))
        goto DONE;

    if (whole_disk)
    {
        Index<short> subtunes;

        /* only add the audio tracks to the playlist */
        for (int trackno = firsttrackno; trackno <= lasttrackno; trackno++)
            if (cdda_track_audiop (pcdrom_drive, trackno))
                subtunes.append (trackno);

        tuple.set_subtunes (subtunes.len (), subtunes.begin ());

        valid = true;
    }
    else
    {
        int trackno = find_trackno_from_filename (filename);

        if (trackno < firsttrackno || trackno > lasttrackno)
        {
            AUDERR ("Track %d not found.\n", trackno);
            goto DONE;
        }

        if (!cdda_track_audiop (pcdrom_drive, trackno))
        {
            AUDERR ("Track %d is a data track.\n", trackno);
            goto DONE;
        }

        tuple.set_format (_("Audio CD"), 2, 44100, 1411);
        tuple.set_int (Tuple::Track, trackno);
        tuple.set_int (Tuple::Length, calculate_track_length
         (trackinfo[trackno].startlsn, trackinfo[trackno].endlsn));

        if (trackinfo[trackno].name)
            tuple.set_str (Tuple::Title, trackinfo[trackno].name);
        if (trackinfo[trackno].performer)
            tuple.set_str (Tuple::Artist, trackinfo[trackno].performer);
        if (trackinfo[0].name)
            tuple.set_str (Tuple::Album, trackinfo[0].name);
        if (trackinfo[0].performer)
            tuple.set_str (Tuple::AlbumArtist, trackinfo[0].performer);
        if (trackinfo[trackno].genre)
            tuple.set_str (Tuple::Genre, trackinfo[trackno].genre);

        valid = true;
    }

  DONE:
    pthread_mutex_unlock (& mutex);
    return valid;
}

/* mutex must be locked */
static bool open_cd ()
{
    if (pcdrom_drive)
        return true;

    AUDDBG ("Opening CD drive.\n");
    String device = aud_get_str ("CDDA", "device");

    if (device[0])
    {
        if (! (pcdrom_drive = cdda_identify (device, 1, nullptr)))
            cdaudio_error (_("Failed to open CD device %s."), (const char *) device);
    }
    else
    {
        char * * ppcd_drives = cdio_get_devices_with_cap (nullptr, CDIO_FS_AUDIO, false);

        if (ppcd_drives && ppcd_drives[0])
        {
            if (! (pcdrom_drive = cdda_identify (ppcd_drives[0], 1, nullptr)))
                cdaudio_error (_("Failed to open CD device %s."), ppcd_drives[0]);
        }
        else
            cdaudio_error (_("No audio capable CD drive found."));

        if (ppcd_drives)
            cdio_free_device_list (ppcd_drives);
    }

    return (bool) pcdrom_drive;
}

/* mutex must be locked */
static bool check_disc_mode (bool warning)
{
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

        return false;
    }

    return true;
}

/* mutex must be locked */
static bool scan_cd ()
{
    AUDDBG ("Scanning CD drive.\n");
    trackinfo.clear ();

    /* general track initialization */

    /* skip endianness detection (because it only affects cdda_read, and we use
     * cdio_read_audio_sectors instead) */
    pcdrom_drive->bigendianp = 0;

    /* finish initialization of drive/disc (performs disc TOC sanitization) */
    if (cdda_open (pcdrom_drive) != 0)
    {
        cdaudio_error (_("Failed to finish initializing opened CD drive."));
        return false;
    }

    int speed = aud_get_int ("CDDA", "disc_speed");
    speed = aud::clamp (speed, MIN_DISC_SPEED, MAX_DISC_SPEED);
    if (cdda_speed_set (pcdrom_drive, speed) != DRIVER_OP_SUCCESS)
        AUDERR ("Cannot set drive speed.\n");

    firsttrackno = cdio_get_first_track_num (pcdrom_drive->p_cdio);
    lasttrackno = cdio_get_last_track_num (pcdrom_drive->p_cdio);
    if (firsttrackno == CDIO_INVALID_TRACK || lasttrackno == CDIO_INVALID_TRACK)
    {
        cdaudio_error (_("Failed to retrieve first/last track number."));
        return false;
    }
    AUDDBG ("first track is %d and last track is %d\n", firsttrackno,
           lasttrackno);

    trackinfo.insert (0, lasttrackno + 1);

    trackinfo[0].startlsn = cdda_track_firstsector (pcdrom_drive, 0);
    trackinfo[0].endlsn = cdda_track_lastsector (pcdrom_drive, lasttrackno);

    n_audio_tracks = 0;

    for (int trackno = firsttrackno; trackno <= lasttrackno; trackno++)
    {
        trackinfo[trackno].startlsn = cdda_track_firstsector (pcdrom_drive, trackno);
        trackinfo[trackno].endlsn = cdda_track_lastsector (pcdrom_drive, trackno);

        if (trackinfo[trackno].startlsn == CDIO_INVALID_LSN
            || trackinfo[trackno].endlsn == CDIO_INVALID_LSN)
        {
            cdaudio_error (_("Cannot read start/end LSN for track %d."), trackno);
            return false;
        }

        /* count how many tracks are audio tracks */
        if (cdda_track_audiop (pcdrom_drive, trackno))
            n_audio_tracks++;
    }

    /* get trackinfo[0] cdtext information (the disc) */
    cdtext_t *pcdtext = nullptr;
    if (aud_get_bool ("CDDA", "use_cdtext"))
    {
        AUDDBG ("getting cd-text information for disc\n");
#if LIBCDIO_VERSION_NUM >= 90
        pcdtext = cdio_get_cdtext (pcdrom_drive->p_cdio);
        if (pcdtext == nullptr)
#else
        pcdtext = cdio_get_cdtext (pcdrom_drive->p_cdio, 0);
        if (pcdtext == nullptr || pcdtext->field[CDTEXT_TITLE] == nullptr)
#endif
        {
            AUDDBG ("no cd-text available for disc\n");
        }
        else
        {
#if LIBCDIO_VERSION_NUM >= 90
            trackinfo[0].performer = String (cdtext_get_const (pcdtext, CDTEXT_FIELD_PERFORMER, 0));
            trackinfo[0].name = String (cdtext_get_const (pcdtext, CDTEXT_FIELD_TITLE, 0));
            trackinfo[0].genre = String (cdtext_get_const (pcdtext, CDTEXT_FIELD_GENRE, 0));
#else
            trackinfo[0].performer = String (pcdtext->field[CDTEXT_PERFORMER]);
            trackinfo[0].name = String (pcdtext->field[CDTEXT_TITLE]);
            trackinfo[0].genre = String (pcdtext->field[CDTEXT_GENRE]);
#endif
        }
    }

    /* get track information from cdtext */
    bool cdtext_was_available = false;
    for (int trackno = firsttrackno; trackno <= lasttrackno; trackno++)
    {
#if LIBCDIO_VERSION_NUM < 90
        if (aud_get_bool ("CDDA", "use_cdtext"))
        {
            AUDDBG ("getting cd-text information for track %d\n", trackno);
            pcdtext = cdio_get_cdtext (pcdrom_drive->p_cdio, trackno);
            if (pcdtext == nullptr || pcdtext->field[CDTEXT_PERFORMER] == nullptr)
            {
                AUDDBG ("no cd-text available for track %d\n", trackno);
                pcdtext = nullptr;
            }
        }
#endif

        if (pcdtext != nullptr)
        {
#if LIBCDIO_VERSION_NUM >= 90
            trackinfo[trackno].performer = String (cdtext_get_const (pcdtext, CDTEXT_FIELD_PERFORMER, trackno));
            trackinfo[trackno].name = String (cdtext_get_const (pcdtext, CDTEXT_FIELD_TITLE, trackno));
            trackinfo[trackno].genre = String (cdtext_get_const (pcdtext, CDTEXT_FIELD_GENRE, trackno));
#else
            trackinfo[trackno].performer = String (pcdtext->field[CDTEXT_PERFORMER]);
            trackinfo[trackno].name = String (pcdtext->field[CDTEXT_TITLE]);
            trackinfo[trackno].genre = String (pcdtext->field[CDTEXT_GENRE]);
#endif
            cdtext_was_available = true;
        }
    }

    if (!cdtext_was_available)
    {
        /* initialize de cddb subsystem */
        cddb_conn_t *pcddb_conn = nullptr;
        cddb_disc_t *pcddb_disc = nullptr;
        cddb_track_t *pcddb_track = nullptr;
        lba_t lba;              /* Logical Block Address */

        if (aud_get_bool ("CDDA", "use_cddb"))
        {
            pcddb_conn = cddb_new ();
            if (pcddb_conn == nullptr)
                cdaudio_error (_("Failed to create the cddb connection."));
            else
            {
                AUDDBG ("getting CDDB info\n");

                cddb_cache_enable (pcddb_conn);
                // cddb_cache_set_dir(pcddb_conn, "~/.cddbslave");

                String server = aud_get_str ("CDDA", "cddbserver");
                String path = aud_get_str ("CDDA", "cddbpath");
                int port = aud_get_int ("CDDA", "cddbport");

                if (aud_get_bool (nullptr, "use_proxy"))
                {
                    String prhost = aud_get_str (nullptr, "proxy_host");
                    int prport = aud_get_int (nullptr, "proxy_port");
                    String pruser = aud_get_str (nullptr, "proxy_user");
                    String prpass = aud_get_str (nullptr, "proxy_pass");

                    cddb_http_proxy_enable (pcddb_conn);
                    cddb_set_http_proxy_server_name (pcddb_conn, prhost);
                    cddb_set_http_proxy_server_port (pcddb_conn, prport);
                    cddb_set_http_proxy_username (pcddb_conn, pruser);
                    cddb_set_http_proxy_password (pcddb_conn, prpass);

                    cddb_set_server_name (pcddb_conn, server);
                    cddb_set_server_port (pcddb_conn, port);
                }
                else if (aud_get_bool ("CDDA", "cddbhttp"))
                {
                    cddb_http_enable (pcddb_conn);
                    cddb_set_server_name (pcddb_conn, server);
                    cddb_set_server_port (pcddb_conn, port);
                    cddb_set_http_path_query (pcddb_conn, path);
                }
                else
                {
                    cddb_set_server_name (pcddb_conn, server);
                    cddb_set_server_port (pcddb_conn, port);
                }

                pcddb_disc = cddb_disc_new ();

                lba = cdio_get_track_lba (pcdrom_drive->p_cdio,
                                          CDIO_CDROM_LEADOUT_TRACK);
                cddb_disc_set_length (pcddb_disc, FRAMES_TO_SECONDS (lba));

                for (int trackno = firsttrackno; trackno <= lasttrackno; trackno++)
                {
                    pcddb_track = cddb_track_new ();
                    cddb_track_set_frame_offset (pcddb_track,
                                                 cdio_get_track_lba (
                                                     pcdrom_drive->p_cdio,
                                                     trackno));
                    cddb_disc_add_track (pcddb_disc, pcddb_track);
                }

                cddb_disc_calc_discid (pcddb_disc);

                unsigned discid = cddb_disc_get_discid (pcddb_disc);
                AUDDBG ("CDDB disc id = %x\n", discid);

                int matches;
                if ((matches = cddb_query (pcddb_conn, pcddb_disc)) == -1)
                {
                    if (cddb_errno (pcddb_conn) == CDDB_ERR_OK)
                        cdaudio_error (_("Failed to query the CDDB server"));
                    else
                        cdaudio_error (_("Failed to query the CDDB server: %s"),
                                       cddb_error_str (cddb_errno
                                                       (pcddb_conn)));

                    cddb_disc_destroy (pcddb_disc);
                    pcddb_disc = nullptr;
                }
                else
                {
                    if (matches == 0)
                    {
                        AUDDBG ("no cddb info available for this disc\n");

                        cddb_disc_destroy (pcddb_disc);
                        pcddb_disc = nullptr;
                    }
                    else
                    {
                        AUDDBG ("CDDB disc category = \"%s\"\n",
                               cddb_disc_get_category_str (pcddb_disc));

                        cddb_read (pcddb_conn, pcddb_disc);
                        if (cddb_errno (pcddb_conn) != CDDB_ERR_OK)
                        {
                            cdaudio_error (_("Failed to read the cddb info: %s"),
                                           cddb_error_str (cddb_errno
                                                           (pcddb_conn)));
                            cddb_disc_destroy (pcddb_disc);
                            pcddb_disc = nullptr;
                        }
                        else
                        {
                            trackinfo[0].performer = String (cddb_disc_get_artist (pcddb_disc));
                            trackinfo[0].name = String (cddb_disc_get_title (pcddb_disc));
                            trackinfo[0].genre = String (cddb_disc_get_genre (pcddb_disc));

                            int trackno;
                            for (trackno = firsttrackno; trackno <= lasttrackno;
                                 trackno++)
                            {
                                cddb_track_t *pcddb_track =
                                    cddb_disc_get_track (pcddb_disc,
                                                         trackno - 1);

                                trackinfo[trackno].performer = String (cddb_track_get_artist (pcddb_track));
                                trackinfo[trackno].name = String (cddb_track_get_title (pcddb_track));
                                trackinfo[trackno].genre = String (cddb_disc_get_genre (pcddb_disc));
                            }
                        }
                    }
                }
            }
        }

        if (pcddb_disc != nullptr)
            cddb_disc_destroy (pcddb_disc);

        if (pcddb_conn != nullptr)
            cddb_destroy (pcddb_conn);
    }

    return true;
}

/* mutex must be locked */
static bool refresh_trackinfo (bool warning)
{
    if (! open_cd () || ! check_disc_mode (warning))
        goto fail;

    if (! trackinfo.len () || cdio_get_media_changed (pcdrom_drive->p_cdio))
    {
        if (! scan_cd ())
            goto fail;

        timer_add (TimerRate::Hz1, monitor);
    }

    return true;

fail:
    reset_trackinfo ();
    purge_func.queue (purge_all_playlists, nullptr);
    return false;
}

/* mutex must be locked */
static void reset_trackinfo ()
{
    timer_remove (TimerRate::Hz1, monitor);

    if (pcdrom_drive != nullptr)
    {
        cdda_close (pcdrom_drive);
        pcdrom_drive = nullptr;
    }

    trackinfo.clear ();
}

/* thread safe (mutex may be locked) */
static int calculate_track_length (int startlsn, int endlsn)
{
    return ((endlsn - startlsn + 1) * 1000) / 75;
}

/* thread safe (mutex may be locked) */
static int find_trackno_from_filename (const char * filename)
{
    int track;

    if (strncmp (filename, "cdda://?", 8) || sscanf (filename + 8, "%d",
                                                     &track) != 1)
        return -1;

    return track;
}
