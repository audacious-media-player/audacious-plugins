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
#include <cddb/cddb.h>

#include <glib.h>

#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <audacious/output.h>
#include <audacious/ui_plugin_menu.h>

#include "cdaudio-ng.h"
#include "configure.h"


#define DEBUG FALSE

#if DEBUG
    #define debug(...) printf (__VA_ARGS__)
#else
    #define debug(...)
#endif

#define warn(...) fprintf(stderr, "cdaudio-ng: " __VA_ARGS__)

static GMutex * mutex;
static GCond * control_cond;

/* lock mutex to read / set these variables */
cdng_cfg_t			cdng_cfg;
static gint			firsttrackno = -1;
static gint			lasttrackno = -1;
static CdIo_t			*pcdio = NULL;
static trackinfo_t		*trackinfo = NULL;
static volatile gboolean	is_paused = FALSE;
static gint			playing_track = -1;
static dae_params_t		*pdae_params = NULL;

/* read / set these variables in main thread only */
#define N_MENUS 3
static const int menus [N_MENUS] = {AUDACIOUS_MENU_MAIN,
 AUDACIOUS_MENU_PLAYLIST_ADD, AUDACIOUS_MENU_PLAYLIST_RCLICK};
static GtkWidget * menu_items [2 * N_MENUS];
static int monitor_source;


static void			cdaudio_init(void);
static void			cdaudio_about(void);
static void			cdaudio_configure(void);
static gint			cdaudio_is_our_file(const gchar *filename);
static void			cdaudio_play_file(InputPlayback *pinputplayback);
static void			cdaudio_stop(InputPlayback *pinputplayback);
static void			cdaudio_pause(InputPlayback *pinputplayback, gshort paused);
static void			cdaudio_seek(InputPlayback *pinputplayback, gint time);
static gint			cdaudio_get_time(InputPlayback *pinputplayback);
static gint			cdaudio_get_volume(gint *l, gint *r);
static gint			cdaudio_set_volume(gint l, gint r);
static void			cdaudio_cleanup(void);
static Tuple * create_tuple_from_trackinfo_and_filename (const gchar * filename);
static void			dae_play_loop(dae_params_t *pdae_params);
static void scan_cd (void);
static void			refresh_trackinfo(void);
static gint			calculate_track_length(gint startlsn, gint endlsn);
static gint find_trackno_from_filename (const gchar * filename);


static InputPlugin inputplugin = {
	.description = "Audio CD Plugin",
	.init = cdaudio_init,
	.about = cdaudio_about,
	.configure = cdaudio_configure,
	.is_our_file = cdaudio_is_our_file,
	.play_file = cdaudio_play_file,
	.stop = cdaudio_stop,
	.pause = cdaudio_pause,
	.seek = cdaudio_seek,
	.get_time = cdaudio_get_time,
	.get_volume = cdaudio_get_volume,
	.set_volume = cdaudio_set_volume,
	.cleanup = cdaudio_cleanup,
	.get_song_tuple = create_tuple_from_trackinfo_and_filename,
	.have_subtune = TRUE,
};

InputPlugin *cdaudio_iplist[] = { &inputplugin, NULL };

DECLARE_PLUGIN(cdaudio, NULL, NULL, cdaudio_iplist, NULL, NULL, NULL, NULL, NULL);


static void cdaudio_error (const gchar *message_format, ...)
{
    va_list args;
    gchar *msg = NULL;

    va_start (args, message_format);
    msg = g_markup_vprintf_escaped (message_format, args);
    va_end (args);

    aud_event_queue_with_data_free("interface show error", msg);
}

/* mutex must be locked */
static void check_disk (void)
{
    if (trackinfo == NULL)
        refresh_trackinfo ();

    if (trackinfo == NULL)
        cdaudio_error ("No audio CD found.");
}

/* thread safe */
static gboolean check_disk_safe (void)
{
    gboolean disk;

    g_mutex_lock (mutex);

    check_disk ();
    disk = (trackinfo != NULL);

    g_mutex_unlock (mutex);
    return disk;
}

/* main thread only */
static void purge_playlist (gint playlist)
{
    gint length = aud_playlist_entry_count (playlist);
    gint count;
    const gchar * filename;

    for (count = 0; count < length; count ++)
    {
        filename = aud_playlist_entry_get_filename (playlist, count);

        if (cdaudio_is_our_file (filename))
        {
            aud_playlist_entry_delete (playlist, count, 1);
            count --;
            length --;
        }
    }
}

/* main thread only */
static void purge_all_playlists (void)
{
    gint playlists = aud_playlist_count ();
    gint count;

    for (count = 0; count < playlists; count ++)
        purge_playlist (count);
}

/* main thread only */
static gboolean monitor (gpointer unused)
{
    g_mutex_lock (mutex);

    if (trackinfo != NULL)
    {
        refresh_trackinfo ();

        if (trackinfo == NULL)
        {
            g_mutex_unlock (mutex);
            purge_all_playlists ();
            return FALSE;
        }
    }

    g_mutex_unlock (mutex);
    return TRUE;
}

/* main thread only */
static void play_cd (GtkMenuItem * item, gpointer user_data)
{
    gint playlist = aud_playlist_get_active ();

    if (! check_disk_safe ())
        return;

    aud_playlist_entry_delete (playlist, 0, aud_playlist_entry_count (playlist));
    aud_playlist_entry_insert (playlist, 0, g_strdup ("cdda://"), NULL);
    aud_playlist_set_playing (playlist);
    audacious_drct_play ();
}

/* main thread only */
static void add_cd (GtkMenuItem * item, gpointer user_data)
{
    if (! check_disk_safe ())
        return;

    aud_playlist_entry_insert (aud_playlist_get_active (), -1, g_strdup
     ("cdda://"), NULL);
}

/* main thread only */
static void cdaudio_init()
{
	mcs_handle_t *db;
        gint count;
        GtkWidget * item;

        mutex = g_mutex_new ();
        control_cond = g_cond_new ();

	cdng_cfg.use_dae = TRUE;
	cdng_cfg.use_cdtext = TRUE;
	cdng_cfg.use_cddb = TRUE;
	cdng_cfg.device = g_strdup("");
	cdng_cfg.cddb_server = g_strdup(CDDA_DEFAULT_CDDB_SERVER);
	cdng_cfg.cddb_path = g_strdup("");
	cdng_cfg.cddb_port = CDDA_DEFAULT_CDDB_PORT;
	cdng_cfg.cddb_http = FALSE;
	cdng_cfg.limitspeed = CDDA_DEFAULT_LIMIT_SPEED;
	cdng_cfg.use_proxy = FALSE;
	cdng_cfg.proxy_host = g_strdup("");
	cdng_cfg.proxy_port = CDDA_DEFAULT_PROXY_PORT;
	cdng_cfg.proxy_username = g_strdup("");
	cdng_cfg.proxy_password = g_strdup("");

	if ((db = aud_cfg_db_open()) == NULL) {
		cdaudio_error("Failed to read configuration.");
                return;
	}

	aud_cfg_db_get_bool(db, "CDDA", "use_dae", &cdng_cfg.use_dae);
	aud_cfg_db_get_bool(db, "CDDA", "use_cdtext", &cdng_cfg.use_cdtext);
	aud_cfg_db_get_bool(db, "CDDA", "use_cddb", &cdng_cfg.use_cddb);
	aud_cfg_db_get_string(db, "CDDA", "device", &cdng_cfg.device);
	aud_cfg_db_get_string(db, "CDDA", "cddbserver", &cdng_cfg.cddb_server);
	aud_cfg_db_get_string(db, "CDDA", "cddbpath", &cdng_cfg.cddb_path);
	aud_cfg_db_get_int(db, "CDDA", "cddbport", &cdng_cfg.cddb_port);
	aud_cfg_db_get_bool(db, "CDDA", "cddbhttp", &cdng_cfg.cddb_http);
	aud_cfg_db_get_int(db, "CDDA", "limitspeed", &cdng_cfg.limitspeed);
	aud_cfg_db_get_bool(db, "audacious", "use_proxy", &cdng_cfg.use_proxy);
	aud_cfg_db_get_string(db, "audacious", "proxy_host", &cdng_cfg.proxy_host);
	aud_cfg_db_get_int(db, "audacious", "proxy_port", &cdng_cfg.proxy_port);
	aud_cfg_db_get_string(db, "audacious", "proxy_user", &cdng_cfg.proxy_username);
	aud_cfg_db_get_string(db, "audacious", "proxy_pass", &cdng_cfg.proxy_password);

	aud_cfg_db_close(db);

	debug ("use_dae = %d, limitspeed = %d, use_cdtext = %d, use_cddb = %d, "
	 "cddbserver = \"%s\", cddbpath = \"%s\", cddbport = %d, cddbhttp = %d, "
	 "device = \"%s\"\n",
		cdng_cfg.use_dae, cdng_cfg.limitspeed, cdng_cfg.use_cdtext, cdng_cfg.use_cddb,
         cdng_cfg.cddb_server, cdng_cfg.cddb_path, cdng_cfg.cddb_port,
         cdng_cfg.cddb_http, cdng_cfg.device);

	if (!cdio_init()) {
		cdaudio_error("Failed to initialize cdio subsystem.");
                return;
	}

	libcddb_init();

        for (count = 0; count < N_MENUS; count ++)
        {
            item = gtk_image_menu_item_new_with_label (_ ("Play CD"));
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
             gtk_image_new_from_stock (GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU));
            gtk_widget_show (item);
            menu_items [2 * count] = item;
            audacious_menu_plugin_item_add (menus [count], item);
            g_signal_connect (G_OBJECT (item), "activate",
             G_CALLBACK (play_cd), NULL);

            item = gtk_image_menu_item_new_with_label (_ ("Add CD"));
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
             gtk_image_new_from_stock (GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU));
            gtk_widget_show (item);
            menu_items [2 * count + 1] = item;
            audacious_menu_plugin_item_add (menus [count], item);
            g_signal_connect (G_OBJECT (item), "activate",
             G_CALLBACK (add_cd), NULL);
        }

	aud_uri_set_plugin("cdda://", &inputplugin);

        trackinfo = NULL;
        monitor_source = g_timeout_add (1000, monitor, NULL);
}

/* main thread only */
static void cdaudio_about()
{
	static GtkWidget* about_window = NULL;

	if (about_window) {
            gtk_window_present(GTK_WINDOW(about_window));
	} else {
            about_window = audacious_info_dialog(_("About Audio CD Plugin"),
	    _("Copyright (c) 2007, by Calin Crisan <ccrisan@gmail.com> and The Audacious Team.\n\n"
	    "Many thanks to libcdio developers <http://www.gnu.org/software/libcdio/>\n"
	    "\tand to libcddb developers <http://libcddb.sourceforge.net/>.\n\n"
	    "Also thank you Tony Vroon for mentoring & guiding me.\n\n"
	     "This was a Google Summer of Code 2007 project.\n\n"
	     "Copyright 2009 John Lindgren"), _("Close"), FALSE, NULL, NULL);

	    g_signal_connect(G_OBJECT(about_window), "destroy",	G_CALLBACK(gtk_widget_destroyed), &about_window);
        }
}

/* main thread only */
static void cdaudio_configure()
{
	configure_show_gui();

	debug ("use_dae = %d, limitspeed = %d, use_cdtext = %d, use_cddb = %d, "
	 "cddbserver = \"%s\", cddbpath = \"%s\", cddbport = %d, cddbhttp = %d, "
	 "device = \"%s\"\n",
		cdng_cfg.use_dae, cdng_cfg.limitspeed, cdng_cfg.use_cdtext, cdng_cfg.use_cddb,
	 cdng_cfg.cddb_server, cdng_cfg.cddb_path, cdng_cfg.cddb_port,
	 cdng_cfg.cddb_http, cdng_cfg.device);
}

/* thread safe (mutex may be locked) */
static gint cdaudio_is_our_file(const gchar *filename)
{
    return ! strncmp (filename, "cdda://", 7);
}

/* thread safe (mutex may be locked) */
static void cdaudio_set_strinfo(trackinfo_t *t,
		const gchar *performer, const gchar *name, const gchar *genre)
{
	g_strlcpy(t->performer, performer, DEF_STRING_LEN);
	g_strlcpy(t->name, name, DEF_STRING_LEN);
	g_strlcpy(t->genre, genre, DEF_STRING_LEN);
}

/* thread safe (mutex may be locked) */
static void cdaudio_set_fullinfo(trackinfo_t *t,
		const lsn_t startlsn, const lsn_t endlsn,
		const gchar *performer, const gchar *name, const gchar *genre)
{
	t->startlsn = startlsn;
	t->endlsn = endlsn;
	cdaudio_set_strinfo(t, performer, name, genre);
}

/* play thread only */
static void cdaudio_play_file(InputPlayback *pinputplayback)
{
    gint trackno = find_trackno_from_filename (pinputplayback->filename);
    Tuple * tuple;
    gchar * title;

    if (trackno == -1)
    {
        cdaudio_error ("Invalid URI %s.", pinputplayback->filename);
        return;
    }

    /* calls refresh_trackinfo if needed */
    tuple = create_tuple_from_trackinfo_and_filename (pinputplayback->filename);

    if (tuple == NULL)
    {
        cdaudio_error ("Track %d not found.", trackno);
        return;
    }

    title = aud_tuple_formatter_make_title_string (tuple,
     aud_get_gentitle_format ());
    aud_tuple_free (tuple);

    g_mutex_lock (mutex);

    /* slim chance that these have changed since create_tuple */
    if (trackinfo == NULL || trackno < firsttrackno || trackno > lasttrackno)
        goto UNLOCK;

    pinputplayback->set_params (pinputplayback, title, calculate_track_length
     (trackinfo[trackno].startlsn, trackinfo[trackno].endlsn), 1411200, 44100, 2);

    pinputplayback->playing = TRUE;
    playing_track = trackno;
    is_paused = FALSE;

	if (cdng_cfg.use_dae)
	{
		if (pinputplayback->output->open_audio(FMT_S16_LE, 44100, 2) == 0) {
			cdaudio_error("Failed to open audio output.");
                        goto UNLOCK;
		}

		pdae_params = (dae_params_t *) g_new(dae_params_t, 1);
		pdae_params->startlsn = trackinfo[trackno].startlsn;
		pdae_params->endlsn = trackinfo[trackno].endlsn;
		pdae_params->pplayback = pinputplayback;
		pdae_params->seektime = -1;
		pdae_params->currlsn = trackinfo[trackno].startlsn;
		pdae_params->thread = g_thread_self();
		pinputplayback->set_pb_ready(pinputplayback);

		dae_play_loop(pdae_params);

		g_free (pdae_params);
	}
	else {
		msf_t startmsf, endmsf;
		cdio_lsn_to_msf(trackinfo[trackno].startlsn, &startmsf);
		cdio_lsn_to_msf(trackinfo[trackno].endlsn, &endmsf);
		if (cdio_audio_play_msf(pcdio, &startmsf, &endmsf) != DRIVER_OP_SUCCESS) {
			cdaudio_error("Failed to play analog audio CD.");
                        goto UNLOCK;
		}
	}

UNLOCK:
    g_mutex_unlock (mutex);
    g_free (title);
}

/* main thread only */
static void cdaudio_stop (InputPlayback * playback)
{
    g_mutex_lock (mutex);

    if (! playback->playing)
        goto UNLOCK;

    playback->playing = 0;

    if (cdng_cfg.use_dae)
    {
        g_mutex_unlock (mutex);
        g_thread_join (playback->thread);
        g_mutex_lock (mutex);
    }
    else
    {
        if (cdio_audio_stop (pcdio) != DRIVER_OP_SUCCESS)
            cdaudio_error ("Cannot stop analog CD.");
    }

UNLOCK:
    g_mutex_unlock (mutex);
}

/* main thread only */
static void cdaudio_pause(InputPlayback *pinputplayback, gshort paused)
{
    g_mutex_lock (mutex);

    is_paused = paused;

    if (cdng_cfg.use_dae)
        g_cond_broadcast (control_cond);
    else
    {
        if (paused)
        {
            if (cdio_audio_pause (pcdio) != DRIVER_OP_SUCCESS)
                cdaudio_error ("Cannot pause analog CD.");
        }
        else
        {
            if (cdio_audio_resume (pcdio) != DRIVER_OP_SUCCESS)
                cdaudio_error ("Cannot resume analog CD.");
        }
    }

    g_mutex_unlock (mutex);
}

/* main thread only */
static void cdaudio_seek (InputPlayback * playback, gint time)
{
    g_mutex_lock (mutex);

    if (cdng_cfg.use_dae)
    {
        pdae_params->seektime = time * 1000;
        g_cond_broadcast (control_cond);
    }
    else
    {
		gint newstartlsn = trackinfo[playing_track].startlsn + time * 75;
		msf_t startmsf, endmsf;
		cdio_lsn_to_msf(newstartlsn, &startmsf);
		cdio_lsn_to_msf(trackinfo[playing_track].endlsn, &endmsf);

		if (cdio_audio_play_msf(pcdio, &startmsf, &endmsf) != DRIVER_OP_SUCCESS)
			cdaudio_error("Failed to play analog CD");
    }

    g_mutex_unlock (mutex);
}

/* main thread only */
static gint cdaudio_get_time (InputPlayback * playback)
{
    int time;

    g_mutex_lock (mutex);

    time = 0;

    if (! playback->playing)
        goto UNLOCK;

    if (cdng_cfg.use_dae)
        time = playback->output->output_time ();
    else
    {
		cdio_subchannel_t subchannel;
		if (cdio_audio_read_subchannel(pcdio, &subchannel) != DRIVER_OP_SUCCESS) {
			cdaudio_error("Failed to read analog CD subchannel.");
			goto UNLOCK;
		}
		gint currlsn = cdio_msf_to_lsn(&subchannel.abs_addr);

                time = calculate_track_length
                 (trackinfo [playing_track].startlsn, currlsn);
    }

UNLOCK:
    g_mutex_unlock (mutex);
    return time;
}

/* main thread only */
static gint cdaudio_get_volume(gint *l, gint *r)
{
    g_mutex_lock (mutex);

	if (cdng_cfg.use_dae) {
                g_mutex_unlock (mutex);
		return FALSE;
	}
	else {
		cdio_audio_volume_t volume;
		if (cdio_audio_get_volume(pcdio, &volume) != DRIVER_OP_SUCCESS) {
			cdaudio_error("Failed to retrieve analog CD volume.");

                        g_mutex_unlock (mutex);
			return FALSE;
		}
		*l = volume.level[0];
		*r = volume.level[1];

                g_mutex_unlock (mutex);
		return TRUE;
	}
}

/* main thread only */
static gint cdaudio_set_volume(gint l, gint r)
{
    g_mutex_lock (mutex);

	if (cdng_cfg.use_dae) {
                g_mutex_unlock (mutex);
		return FALSE;
	}
	else {
		cdio_audio_volume_t volume = {{l, r, 0, 0}};
		if (cdio_audio_set_volume(pcdio, &volume) != DRIVER_OP_SUCCESS) {
			cdaudio_error("cdaudio-ng: failed to set analog cd volume");

                        g_mutex_unlock (mutex);
			return FALSE;
		}

                g_mutex_unlock (mutex);
		return TRUE;
	}
}

/* main thread only */
static void cdaudio_cleanup(void)
{
        int count;

        g_mutex_lock (mutex);

        for (count = 0; count < N_MENUS; count ++)
        {
            audacious_menu_plugin_item_remove (menus [count],
             menu_items [2 * count]);
            audacious_menu_plugin_item_remove (menus [count],
             menu_items [2 * count + 1]);
        }

        g_source_remove (monitor_source);

	if (pcdio != NULL) {
		cdio_destroy(pcdio);
		pcdio = NULL;
	}
	if (trackinfo != NULL) {
		g_free(trackinfo);
		trackinfo = NULL;
	}

	libcddb_shutdown();

	// todo: destroy the gui

	mcs_handle_t *db = aud_cfg_db_open();
	aud_cfg_db_set_bool(db, "CDDA", "use_dae", cdng_cfg.use_dae);
	aud_cfg_db_set_int(db, "CDDA", "limitspeed", cdng_cfg.limitspeed);
	aud_cfg_db_set_bool(db, "CDDA", "use_cdtext", cdng_cfg.use_cdtext);
	aud_cfg_db_set_bool(db, "CDDA", "use_cddb", cdng_cfg.use_cddb);
	aud_cfg_db_set_string(db, "CDDA", "cddbserver", cdng_cfg.cddb_server);
	aud_cfg_db_set_string(db, "CDDA", "cddbpath", cdng_cfg.cddb_path);
	aud_cfg_db_set_int(db, "CDDA", "cddbport", cdng_cfg.cddb_port);
	aud_cfg_db_set_bool(db, "CDDA", "cddbhttp", cdng_cfg.cddb_http);
	aud_cfg_db_set_string(db, "CDDA", "device", cdng_cfg.device);
	aud_cfg_db_close(db);

        g_mutex_unlock (mutex);
        g_mutex_free (mutex);
        g_cond_free (control_cond);
}

/* thread safe */
static Tuple * create_tuple_from_trackinfo_and_filename (const gchar * filename)
{
    Tuple * tuple = NULL;
    gint trackno;

    g_mutex_lock (mutex);

    if (trackinfo == NULL)
        refresh_trackinfo ();

    if (trackinfo == NULL)
    {
        warn ("No audio CD found.\n");
        goto DONE;
    }

    if (! strcmp (filename, "cdda://"))
    {
        tuple = aud_tuple_new_from_filename (filename);
        tuple->nsubtunes = 1 + lasttrackno - firsttrackno;
        tuple->subtunes = g_malloc (sizeof * tuple->subtunes * tuple->nsubtunes);

        for (trackno = firsttrackno; trackno <= lasttrackno; trackno ++)
            tuple->subtunes[trackno - firsttrackno] = trackno;

        goto DONE;
    }

    trackno = find_trackno_from_filename (filename);

    if (trackno < firsttrackno || trackno > lasttrackno)
    {
        warn ("Track %d not found.\n", trackno);
        goto DONE;
    }

    tuple = aud_tuple_new_from_filename (filename);

	if(strlen(trackinfo[trackno].performer)) {
		aud_tuple_associate_string(tuple, FIELD_ARTIST, NULL, trackinfo[trackno].performer);
	}
	if(strlen(trackinfo[0].name)) {
		aud_tuple_associate_string(tuple, FIELD_ALBUM, NULL, trackinfo[0].name);
	}
	if(strlen(trackinfo[trackno].name)) {
		aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, trackinfo[trackno].name);
	}

	aud_tuple_associate_int(tuple, FIELD_TRACK_NUMBER, NULL, trackno);

	aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL,
		calculate_track_length(trackinfo[trackno].startlsn, trackinfo[trackno].endlsn));

	if(strlen(trackinfo[trackno].genre)) {
		aud_tuple_associate_string(tuple, FIELD_GENRE, NULL,  trackinfo[trackno].genre);
	}

DONE:
    g_mutex_unlock (mutex);
    return tuple;
}

/* play thread only, mutex must be locked */
static void do_seek (void)
{
	pdae_params->pplayback->output->flush(pdae_params->seektime);
	pdae_params->currlsn = pdae_params->startlsn + (pdae_params->seektime * 75 / 1000);
	cdio_lseek(pcdio, pdae_params->currlsn * CDIO_CD_FRAMESIZE_RAW, SEEK_SET);
	pdae_params->seektime = -1;
}

/* play thread only, mutex must be locked */
/* unlocks mutex temporarily */
static void dae_play_loop(dae_params_t *pdae_params)
{
        InputPlayback * playback;
	guchar *buffer = g_new(guchar, CDDA_DAE_FRAMES * CDIO_CD_FRAMESIZE_RAW);

	cdio_lseek(pcdio, pdae_params->startlsn * CDIO_CD_FRAMESIZE_RAW, SEEK_SET);

	gint read_error_counter = 0;

	//pdae_params->endlsn += 75 * 3;

        while (pdae_params->pplayback->playing)
        {
            if (is_paused)
            {
                pdae_params->pplayback->output->pause (1);

                while (is_paused)
                {
                    if (pdae_params->seektime != -1)
                        do_seek ();

                    g_cond_wait (control_cond, mutex);
                }

                pdae_params->pplayback->output->pause (0);
            }

            if (pdae_params->seektime != -1)
                do_seek ();

		/* compute the actual number of sectors to read */
		gint lsncount = CDDA_DAE_FRAMES <= (pdae_params->endlsn - pdae_params->currlsn + 1) ? CDDA_DAE_FRAMES : (pdae_params->endlsn - pdae_params->currlsn + 1);

		/* check too see if we have reached the end of the song */
		if (lsncount <= 0) {
			sleep(3);
			break;
		}

		if (cdio_read_audio_sectors(pcdio, buffer, pdae_params->currlsn, lsncount) != DRIVER_OP_SUCCESS) {
			cdaudio_error ("Cannot read sector.");
			read_error_counter++;
			if (read_error_counter >= 2) {
				read_error_counter = 0;
				cdaudio_error("This CD can no longer be played, stopping.");
				break;
			}
		}
		else
			read_error_counter = 0;

                playback = pdae_params->pplayback;

                g_mutex_unlock (mutex);

		gint remainingbytes = lsncount * CDIO_CD_FRAMESIZE_RAW;
		guchar *bytebuff = buffer;

		while (remainingbytes > 0)
		{
			/* compute the actual number of bytes to play */
			gint bytecount = CDIO_CD_FRAMESIZE_RAW <= remainingbytes ? CDIO_CD_FRAMESIZE_RAW : remainingbytes;

                        playback->pass_audio (playback, FMT_S16_LE, 2,
                         bytecount, bytebuff, & playback->playing);

			remainingbytes -= bytecount;
			bytebuff += bytecount;
		}

                g_mutex_lock (mutex);

		pdae_params->currlsn += lsncount;
	}

	pdae_params->pplayback->playing = FALSE;
	pdae_params->pplayback->output->close_audio();
	g_free(buffer);
}

/* mutex must be locked */
static void scan_cd (void)
{
    gint trackno;

    g_free (trackinfo);
    trackinfo = NULL;

	/* find an available, audio capable, cd drive  */
	if (cdng_cfg.device != NULL && strlen(cdng_cfg.device) > 0) {
		pcdio = cdio_open(cdng_cfg.device, DRIVER_UNKNOWN);
		if (pcdio == NULL) {
			cdaudio_error("Failed to open CD device \"%s\".", cdng_cfg.device);
			goto ERROR;
		}
	}
	else {
		gchar **ppcd_drives = cdio_get_devices_with_cap(NULL, CDIO_FS_AUDIO, false);
		pcdio = NULL;
		if (ppcd_drives != NULL && *ppcd_drives != NULL) { /* we have at least one audio capable cd drive */
			pcdio = cdio_open(*ppcd_drives, DRIVER_UNKNOWN);
			if (pcdio == NULL) {
				cdaudio_error("Failed to open CD.");
				goto ERROR;
			}
			debug("found cd drive \"%s\" with audio capable media\n", *ppcd_drives);
		}
		else
			goto ERROR;

		if (ppcd_drives != NULL && *ppcd_drives != NULL)
			cdio_free_device_list(ppcd_drives);
	}

	/* limit read speed */
	if (cdng_cfg.limitspeed > 0 && cdng_cfg.use_dae) {
		debug("setting drive speed limit to %dx\n", cdng_cfg.limitspeed);
		if (cdio_set_speed(pcdio, cdng_cfg.limitspeed) != DRIVER_OP_SUCCESS)
			cdaudio_error("Failed to set drive speed to %dx.", cdng_cfg.limitspeed);
	}

	/* general track initialization */
	cdrom_drive_t *pcdrom_drive = cdio_cddap_identify_cdio(pcdio, 1, NULL);	// todo : check return / NULL
	firsttrackno = cdio_get_first_track_num(pcdrom_drive->p_cdio);
	lasttrackno = cdio_get_last_track_num(pcdrom_drive->p_cdio);
	if (firsttrackno == CDIO_INVALID_TRACK || lasttrackno == CDIO_INVALID_TRACK) {
		cdaudio_error("Failed to retrieve first/last track number.");
		goto ERROR;
	}
	debug("first track is %d and last track is %d\n", firsttrackno, lasttrackno);

	trackinfo = (trackinfo_t *) g_new(trackinfo_t, (lasttrackno + 1));

	cdaudio_set_fullinfo(&trackinfo[0],
		cdio_get_track_lsn(pcdrom_drive->p_cdio, 0),
		cdio_get_track_last_lsn(pcdrom_drive->p_cdio, CDIO_CDROM_LEADOUT_TRACK),
		"", "", "");

	for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
		cdaudio_set_fullinfo(&trackinfo[trackno],
			cdio_get_track_lsn(pcdrom_drive->p_cdio, trackno),
			cdio_get_track_last_lsn(pcdrom_drive->p_cdio, trackno),
			"", "", "");

		if (trackinfo[trackno].startlsn == CDIO_INVALID_LSN || trackinfo[trackno].endlsn == CDIO_INVALID_LSN) {
                    cdaudio_error ("Cannot read start/end LSN for track %d.",
                     trackno);
                    goto ERROR;
		}
	}

	/* get trackinfo[0] cdtext information (the disc) */
	if (cdng_cfg.use_cdtext) {
		debug("getting cd-text information for disc\n");
		cdtext_t *pcdtext = cdio_get_cdtext(pcdrom_drive->p_cdio, 0);
		if (pcdtext == NULL || pcdtext->field[CDTEXT_TITLE] == NULL) {
			debug("no cd-text available for disc\n");
		}
		else {
			cdaudio_set_strinfo(&trackinfo[0],
				pcdtext->field[CDTEXT_PERFORMER] ? pcdtext->field[CDTEXT_PERFORMER] : "",
				pcdtext->field[CDTEXT_TITLE] ? pcdtext->field[CDTEXT_TITLE] : "",
				pcdtext->field[CDTEXT_GENRE] ? pcdtext->field[CDTEXT_GENRE] : "");
		}
	}

	/* get track information from cdtext */
	gboolean cdtext_was_available = FALSE;
	for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
		cdtext_t *pcdtext = NULL;
		if (cdng_cfg.use_cdtext) {
			debug("getting cd-text information for track %d\n", trackno);
			pcdtext = cdio_get_cdtext(pcdrom_drive->p_cdio, trackno);
			if (pcdtext == NULL || pcdtext->field[CDTEXT_PERFORMER] == NULL) {
				debug("no cd-text available for track %d\n", trackno);
				pcdtext = NULL;
			}
		}

		if (pcdtext != NULL) {
			cdaudio_set_strinfo(&trackinfo[trackno],
				pcdtext->field[CDTEXT_PERFORMER] ? pcdtext->field[CDTEXT_PERFORMER] : "",
				pcdtext->field[CDTEXT_TITLE] ? pcdtext->field[CDTEXT_TITLE] : "",
				pcdtext->field[CDTEXT_GENRE] ? pcdtext->field[CDTEXT_GENRE] : "");
			cdtext_was_available = TRUE;
		}
		else {
			cdaudio_set_strinfo(&trackinfo[trackno], "", "", "");
			snprintf (trackinfo[trackno].name, DEF_STRING_LEN,
			 "Track %d", trackno);
		}
	}

	if (!cdtext_was_available) {
		/* initialize de cddb subsystem */
		cddb_conn_t *pcddb_conn = NULL;
		cddb_disc_t *pcddb_disc = NULL;
		cddb_track_t *pcddb_track = NULL;
		lba_t lba; /* Logical Block Address */

		if (cdng_cfg.use_cddb) {
			pcddb_conn = cddb_new();
			if (pcddb_conn == NULL)
				cdaudio_error("Failed to create the cddb connection.");
			else {
				debug("getting CDDB info\n");

				cddb_cache_enable(pcddb_conn);
				// cddb_cache_set_dir(pcddb_conn, "~/.cddbslave");

				if (cdng_cfg.use_proxy) {
					cddb_http_proxy_enable(pcddb_conn);
					cddb_set_http_proxy_server_name(pcddb_conn, cdng_cfg.proxy_host);
					cddb_set_http_proxy_server_port(pcddb_conn, cdng_cfg.proxy_port);
					cddb_set_http_proxy_username(pcddb_conn, cdng_cfg.proxy_username);
					cddb_set_http_proxy_password(pcddb_conn, cdng_cfg.proxy_password);
					cddb_set_server_name(pcddb_conn, cdng_cfg.cddb_server);
					cddb_set_server_port(pcddb_conn, cdng_cfg.cddb_port);
				}
				else
					if (cdng_cfg.cddb_http) {
						cddb_http_enable(pcddb_conn);
						cddb_set_server_name(pcddb_conn, cdng_cfg.cddb_server);
						cddb_set_server_port(pcddb_conn, cdng_cfg.cddb_port);
                        cddb_set_http_path_query(pcddb_conn, cdng_cfg.cddb_path);
					}
					else {
						cddb_set_server_name(pcddb_conn, cdng_cfg.cddb_server);
						cddb_set_server_port(pcddb_conn, cdng_cfg.cddb_port);
					}

				pcddb_disc = cddb_disc_new();

				lba = cdio_get_track_lba(pcdio, CDIO_CDROM_LEADOUT_TRACK);
				cddb_disc_set_length(pcddb_disc, FRAMES_TO_SECONDS(lba));

				for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
					pcddb_track = cddb_track_new();
					cddb_track_set_frame_offset(pcddb_track, cdio_get_track_lba(pcdio, trackno));
					cddb_disc_add_track(pcddb_disc, pcddb_track);
				}

				cddb_disc_calc_discid(pcddb_disc);

				#if DEBUG
				guint discid = cddb_disc_get_discid(pcddb_disc);
				debug("CDDB disc id = %x\n", discid);
				#endif

				gint matches;
				if ((matches = cddb_query(pcddb_conn, pcddb_disc)) == -1) {
					if (cddb_errno(pcddb_conn) == CDDB_ERR_OK)
						cdaudio_error("Failed to query the CDDB server");
					else
						cdaudio_error("Failed to query the CDDB server: %s", cddb_error_str(cddb_errno(pcddb_conn)));

					cddb_disc_destroy(pcddb_disc);
					pcddb_disc = NULL;
				}
				else {
					if (matches == 0) {
						debug("no cddb info available for this disc\n");

						cddb_disc_destroy(pcddb_disc);
						pcddb_disc = NULL;
					}
					else {
						debug("CDDB disc category = \"%s\"\n", cddb_disc_get_category_str(pcddb_disc));

						cddb_read(pcddb_conn, pcddb_disc);
						if (cddb_errno(pcddb_conn) != CDDB_ERR_OK) {
							cdaudio_error("failed to read the cddb info: %s", cddb_error_str(cddb_errno(pcddb_conn)));
							cddb_disc_destroy(pcddb_disc);
							pcddb_disc = NULL;
						}
						else {
							cdaudio_set_strinfo(&trackinfo[0],
								cddb_disc_get_artist(pcddb_disc),
								cddb_disc_get_title(pcddb_disc),
								cddb_disc_get_genre(pcddb_disc));

							gint trackno;
							for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
								cddb_track_t *pcddb_track = cddb_disc_get_track(pcddb_disc, trackno - 1);
								cdaudio_set_strinfo(&trackinfo[trackno],
									cddb_track_get_artist(pcddb_track),
									cddb_track_get_title(pcddb_track),
									cddb_disc_get_genre(pcddb_disc));
							}
						}
					}
				}
			}
		}

		if (pcddb_disc != NULL)
			cddb_disc_destroy(pcddb_disc);

		if (pcddb_conn != NULL)
			cddb_destroy(pcddb_conn);
	}

    return;

ERROR:
    g_free (trackinfo);
    trackinfo = NULL;
}

/* mutex must be locked */
static void refresh_trackinfo (void)
{
    if (trackinfo == NULL || pcdio == NULL || cdio_get_media_changed (pcdio))
        scan_cd ();
}

/* thread safe (mutex may be locked) */
static gint calculate_track_length(gint startlsn, gint endlsn)
{
	return ((endlsn - startlsn + 1) * 1000) / 75;
}

/* thread safe (mutex may be locked) */
static gint find_trackno_from_filename (const gchar * filename)
{
    gint track;

    if (strncmp (filename, "cdda://?", 8) || sscanf (filename + 8, "%d",
     & track) != 1)
        return -1;

    return track;
}
