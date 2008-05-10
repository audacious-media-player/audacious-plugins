/*
 * Audacious CD Digital Audio plugin
 *
 * Copyright (c) 2007 Calin Crisan <ccrisan@gmail.com>
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


/*
 * todo :
 *	audacious -e cdda://track01.cda no longer works
 * 
 *
 */


#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <stdarg.h>

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
#include <audacious/util.h>

#include "cdaudio-ng.h"
#include "configure.h"


cdng_cfg_t			cdng_cfg;
static gint			firsttrackno = -1;
static gint			lasttrackno = -1;
static CdIo_t			*pcdio = NULL;
static trackinfo_t		*trackinfo = NULL;
static gboolean			is_paused = FALSE;
static gint			playing_track = -1;
static dae_params_t		*pdae_params = NULL;
static InputPlayback    	*pglobalinputplayback = NULL;
static GtkWidget		*main_menu_item, *playlist_menu_item;
static GThread			*scan_cd_thread = NULL;
static gint			first_trackno_to_add_after_scan = -1;
static gint			last_trackno_to_add_after_scan = -1;

static void			cdaudio_init(void);
static void			cdaudio_about(void);
static void			cdaudio_configure(void);
static gint			cdaudio_is_our_file(gchar *filename);
static void			cdaudio_play_file(InputPlayback *pinputplayback);
static void			cdaudio_stop(InputPlayback *pinputplayback);
static void			cdaudio_pause(InputPlayback *pinputplayback, gshort paused);
static void			cdaudio_seek(InputPlayback *pinputplayback, gint time);
static gint			cdaudio_get_time(InputPlayback *pinputplayback);
static gint			cdaudio_get_volume(gint *l, gint *r);
static gint			cdaudio_set_volume(gint l, gint r);
static void			cdaudio_cleanup(void);
static void			cdaudio_get_song_info(gchar *filename, gchar **title, gint *length);
static Tuple            	*cdaudio_get_song_tuple(gchar *filename);

static void			menu_click(void);
static void			rescan_menu_click(void);
static Tuple            	*create_tuple_from_trackinfo_and_filename(gchar *filename);
static void			dae_play_loop(dae_params_t *pdae_params);
static void			*scan_cd(void *nothing);
static void			scan_cd_threaded(int firsttrackno, int lasttrackno);
static void			append_track_to_playlist(int trackno);
static gboolean                 show_noaudiocd_info();
static gint			calculate_track_length(gint startlsn, gint endlsn);
static gint			find_trackno_from_filename(gchar *filename);
static void			cleanup_on_error(void);


static InputPlugin inputplugin = {
	.description = "CD Audio Plugin NG",
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
	.get_song_info = cdaudio_get_song_info,
	.get_song_tuple = cdaudio_get_song_tuple
};

InputPlugin *cdaudio_iplist[] = { &inputplugin, NULL };

DECLARE_PLUGIN(cdaudio, NULL, NULL, cdaudio_iplist, NULL, NULL, NULL, NULL, NULL);


static void cdaudio_error(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "cdaudio-ng: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}


static void debug(const char *fmt, ...)
{
	if (cdng_cfg.debug) {
		va_list ap;
		fprintf(stderr, "cdaudio-ng: ");
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}


static void cdaudio_init()
{
	mcs_handle_t *db;
	gchar *menu_item_text;

	debug("cdaudio_init()\n");

	cdng_cfg.use_dae = TRUE;
	cdng_cfg.use_cdtext = TRUE;
	cdng_cfg.use_cddb = TRUE;
	cdng_cfg.debug = FALSE;
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
		cdaudio_error("Failed to read configuration.\n");
		cleanup_on_error();
		return;
	}

	if (!cdio_init()) {
		cdaudio_error("Failed to initialize cdio subsystem.\n");
		cleanup_on_error();
		aud_cfg_db_close(db);
		return;
	}

	libcddb_init();

	aud_cfg_db_get_bool(db, "CDDA", "use_dae", &cdng_cfg.use_dae);
	aud_cfg_db_get_bool(db, "CDDA", "use_cdtext", &cdng_cfg.use_cdtext);
	aud_cfg_db_get_bool(db, "CDDA", "use_cddb", &cdng_cfg.use_cddb);
	aud_cfg_db_get_bool(db, "CDDA", "debug", &cdng_cfg.debug);
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

	debug("use_dae = %d, limitspeed = %d, use_cdtext = %d, use_cddb = %d, cddbserver = \"%s\", cddbpath = \"%s\", cddbport = %d, cddbhttp = %d, device = \"%s\", debug = %d\n",
		cdng_cfg.use_dae, cdng_cfg.limitspeed, cdng_cfg.use_cdtext, cdng_cfg.use_cddb,
          cdng_cfg.cddb_server, cdng_cfg.cddb_path, cdng_cfg.cddb_port, cdng_cfg.cddb_http, cdng_cfg.device, cdng_cfg.debug);

	menu_item_text = _("Rescan CD");
	main_menu_item = gtk_image_menu_item_new_with_label(menu_item_text);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(main_menu_item), gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
	gtk_widget_show(main_menu_item);
	audacious_menu_plugin_item_add(AUDACIOUS_MENU_MAIN, main_menu_item);
	g_signal_connect(G_OBJECT(main_menu_item), "activate", G_CALLBACK(rescan_menu_click), NULL);

	playlist_menu_item = gtk_image_menu_item_new_with_label(menu_item_text);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(playlist_menu_item), gtk_image_new_from_stock(GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU));
	gtk_widget_show(playlist_menu_item);
	audacious_menu_plugin_item_add(AUDACIOUS_MENU_PLAYLIST_RCLICK, playlist_menu_item);
	g_signal_connect(G_OBJECT(playlist_menu_item), "activate", G_CALLBACK(menu_click), NULL);

	menu_item_text = _("Add CD");
	main_menu_item = gtk_image_menu_item_new_with_label(menu_item_text);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(main_menu_item), gtk_image_new_from_stock(GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU));
	gtk_widget_show(main_menu_item);
	audacious_menu_plugin_item_add(AUDACIOUS_MENU_MAIN, main_menu_item);
	g_signal_connect(G_OBJECT(main_menu_item), "activate", G_CALLBACK(menu_click), NULL);

	playlist_menu_item = gtk_image_menu_item_new_with_label(menu_item_text);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(playlist_menu_item), gtk_image_new_from_stock(GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU));
	gtk_widget_show(playlist_menu_item);
	audacious_menu_plugin_item_add(AUDACIOUS_MENU_PLAYLIST_RCLICK, playlist_menu_item);
	g_signal_connect(G_OBJECT(playlist_menu_item), "activate", G_CALLBACK(menu_click), NULL);

	aud_uri_set_plugin("cdda://", &inputplugin);
}

static void cdaudio_about()
{
	debug("cdaudio_about()\n");

	static GtkWidget* about_window = NULL;

	if (about_window) {
            gtk_window_present(GTK_WINDOW(about_window));
	} else {
            about_window = audacious_info_dialog(_("About CD Audio Plugin NG"),
	    _("Copyright (c) 2007, by Calin Crisan <ccrisan@gmail.com> and The Audacious Team.\n\n"
	    "Many thanks to libcdio developers <http://www.gnu.org/software/libcdio/>\n"
	    "\tand to libcddb developers <http://libcddb.sourceforge.net/>.\n\n"
	    "Also thank you Tony Vroon for mentoring & guiding me.\n\n"
	    "This was a Google Summer of Code 2007 project."), _("OK"), FALSE, NULL, NULL);

	    g_signal_connect(G_OBJECT(about_window), "destroy",	G_CALLBACK(gtk_widget_destroyed), &about_window);
        }
}

static void cdaudio_configure()
{
	debug("cdaudio_configure()\n");

	configure_show_gui();

	debug("use_dae = %d, limitspeed = %d, use_cdtext = %d, use_cddb = %d, cddbserver = \"%s\", cddbpath = \"%s\", cddbport = %d, cddbhttp = %d, device = \"%s\", debug = %d\n",
		cdng_cfg.use_dae, cdng_cfg.limitspeed, cdng_cfg.use_cdtext, cdng_cfg.use_cddb,
		cdng_cfg.cddb_server, cdng_cfg.cddb_path, cdng_cfg.cddb_port, cdng_cfg.cddb_http, cdng_cfg.device, cdng_cfg.debug);
}

static gint cdaudio_is_our_file(gchar *filename)
{
	debug("cdaudio_is_our_file(\"%s\")\n", filename);

	if (filename != NULL && !strcmp(filename, CDDA_DUMMYPATH)) {
		debug("\"%s\" will add the whole audio cd\n", filename);
		menu_click();
		return FALSE;
	}

	if ((filename != NULL) && strlen(filename) > 4 && (!strcasecmp(filename + strlen(filename) - 4, ".cda"))) {
		gint trackno = find_trackno_from_filename(filename);

		/* no CD information yet */
		if (pcdio == NULL) {
			debug("no CD information, scanning\n");
			if (first_trackno_to_add_after_scan == -1)
				scan_cd_threaded(0, 0);
			else
				scan_cd_threaded(trackno, trackno);
		}

		/* reload the cd information if the media has changed */
		if (pcdio != NULL && cdio_get_media_changed(pcdio)) {
			debug("CD changed, rescanning\n");
			scan_cd_threaded(0, 0);
		}
		
		/* check if the requested track actually exists on the current audio cd */
		if (trackno < firsttrackno || trackno > lasttrackno) {
			debug("\"%s\" is not our file (track number is out of the valid range)\n", filename);
			return FALSE;
		}

		debug("\"%s\" is our file\n", filename);
		return TRUE;
	}
	else {
		debug("\"%s\" is not our file (unrecognized file name)\n", filename);
		return FALSE;
	}
}


static void cdaudio_set_strinfo(trackinfo_t *t,
		const gchar *performer, const gchar *name, const gchar *genre)
{
	g_strlcpy(t->performer, performer, DEF_STRING_LEN);
	g_strlcpy(t->name, name, DEF_STRING_LEN);
	g_strlcpy(t->genre, genre, DEF_STRING_LEN);
}


static void cdaudio_set_fullinfo(trackinfo_t *t,
		const lsn_t startlsn, const lsn_t endlsn,
		const gchar *performer, const gchar *name, const gchar *genre)
{
	t->startlsn = startlsn;
	t->endlsn = endlsn;
	cdaudio_set_strinfo(t, performer, name, genre);
}


static void cdaudio_play_file(InputPlayback *pinputplayback)
{
	Tuple *tuple;
	gchar *title;
        
	debug("cdaudio_play_file(\"%s\")\n", pinputplayback->filename);

	pglobalinputplayback = pinputplayback;

	if (trackinfo == NULL) {
		debug("no CD information, scanning\n");
		if (scan_cd_thread != NULL)
			g_thread_join(scan_cd_thread);
		else
			scan_cd(pinputplayback);
	}
	else
		if (cdio_get_media_changed(pcdio)) {
			debug("CD changed, rescanning\n");
			if (scan_cd_thread != NULL)
				g_thread_join(scan_cd_thread);
			else
				scan_cd(pinputplayback);
		}

	if (trackinfo == NULL) {
		debug("no CD information can be retrieved, aborting\n");
		pinputplayback->playing = FALSE;
		return;
	}

	gint trackno = find_trackno_from_filename(pinputplayback->filename);
	if (trackno < firsttrackno || trackno > lasttrackno) {
		cdaudio_error("Track #%d is out of range [%d..%d]\n", trackno, firsttrackno, lasttrackno);
		cleanup_on_error();
		return;
	}

	pinputplayback->playing = TRUE;
	playing_track = trackno;
	is_paused = FALSE;

	tuple = create_tuple_from_trackinfo_and_filename(pinputplayback->filename);
	title = aud_tuple_formatter_make_title_string(tuple, aud_get_gentitle_format());

	pinputplayback->set_params(pinputplayback, title, calculate_track_length(trackinfo[trackno].startlsn, trackinfo[trackno].endlsn), 1411200, 44100, 2);
	g_free(title);
	aud_tuple_free(tuple);

	if (cdng_cfg.use_dae) {
		debug("using digital audio extraction\n");

		if (pdae_params != NULL) {
			cdaudio_error("DAE playback seems to be already started.\n");
			return;
		}

		if (pinputplayback->output->open_audio(FMT_S16_LE, 44100, 2) == 0) {
			cdaudio_error("Failed to open audio output.\n");
			cleanup_on_error();
			return;
		}

		/*
		debug("starting dae thread...\n");
		*/
		pdae_params = (dae_params_t *) g_new(dae_params_t, 1);
		pdae_params->startlsn = trackinfo[trackno].startlsn;
		pdae_params->endlsn = trackinfo[trackno].endlsn;
		pdae_params->pplayback = pinputplayback;
		pdae_params->seektime = -1;
		pdae_params->currlsn = trackinfo[trackno].startlsn;
		pdae_params->thread = g_thread_self();
		pinputplayback->set_pb_ready(pinputplayback);
		dae_play_loop(pdae_params);
	}
	else {
		debug("not using digital audio extraction\n");

		msf_t startmsf, endmsf;
		cdio_lsn_to_msf(trackinfo[trackno].startlsn, &startmsf);
		cdio_lsn_to_msf(trackinfo[trackno].endlsn, &endmsf);
		if (cdio_audio_play_msf(pcdio, &startmsf, &endmsf) != DRIVER_OP_SUCCESS) {
			cdaudio_error("Failed to play analog audio CD.\n");
			cleanup_on_error();
			return;
		}
	}
}

static void cdaudio_stop(InputPlayback *pinputplayback)
{
	debug("cdaudio_stop(\"%s\")\n", pinputplayback != NULL ? pinputplayback->filename : "N/A");

	pglobalinputplayback = NULL;

	if (playing_track == -1)
		return;

	if (pinputplayback != NULL)
		pinputplayback->playing = FALSE;
	playing_track = -1;
	is_paused = FALSE;

	if (cdng_cfg.use_dae) {
		if (pdae_params != NULL) {
			g_thread_join(pdae_params->thread);
			g_free(pdae_params);
			pdae_params = NULL;
		}
	}
	else {
		if (cdio_audio_stop(pcdio) != DRIVER_OP_SUCCESS) {
			cdaudio_error("Failed to stop analog CD.\n");
			return;
		}
	}
}

static void cdaudio_pause(InputPlayback *pinputplayback, gshort paused)
{
	debug("cdaudio_pause(\"%s\", %d)\n", pinputplayback->filename, paused);

	if (!is_paused) {
		is_paused = TRUE;
		if (!cdng_cfg.use_dae)
			if (cdio_audio_pause(pcdio) != DRIVER_OP_SUCCESS) {
				cdaudio_error("Failed to pause analog CD!\n");
				cleanup_on_error();
				return;
			}
	}
	else {
		is_paused = FALSE;
		if (!cdng_cfg.use_dae)
			if (cdio_audio_resume(pcdio) != DRIVER_OP_SUCCESS) {
				cdaudio_error("Failed to resume analog CD!\n");
				cleanup_on_error();
				return;
			}
	}
}

static void cdaudio_seek(InputPlayback *pinputplayback, gint time)
{
	debug("cdaudio_seek(\"%s\", %d)\n", pinputplayback->filename, time);

	if (playing_track == -1)
		return;

	if (cdng_cfg.use_dae) {
		if (pdae_params != NULL) {
			pdae_params->seektime = time * 1000;
		}
	}
	else {
		gint newstartlsn = trackinfo[playing_track].startlsn + time * 75;
		msf_t startmsf, endmsf;
		cdio_lsn_to_msf(newstartlsn, &startmsf);
		cdio_lsn_to_msf(trackinfo[playing_track].endlsn, &endmsf);

		if (cdio_audio_play_msf(pcdio, &startmsf, &endmsf) != DRIVER_OP_SUCCESS) {
			cdaudio_error("Failed to play analog CD\n");
			cleanup_on_error();
			return;
		}
	}
}

static gint cdaudio_get_time(InputPlayback *pinputplayback)
{
	if (playing_track == -1)
		return -1;

	if (!cdng_cfg.use_dae) {
		cdio_subchannel_t subchannel;
		if (cdio_audio_read_subchannel(pcdio, &subchannel) != DRIVER_OP_SUCCESS) {
			cdaudio_error("Failed to read analog CD subchannel.\n");
			cleanup_on_error();
			return 0;
		}
		gint currlsn = cdio_msf_to_lsn(&subchannel.abs_addr);

		/* check to see if we have reached the end of the song */
		if (currlsn == trackinfo[playing_track].endlsn)
			return -1;

		return calculate_track_length(trackinfo[playing_track].startlsn, currlsn);
	}
	else {
		if (pdae_params != NULL)
			if (pdae_params->pplayback->playing)
				return pinputplayback->output->output_time();
			else
				return -1;
		else
			return -1;
	}
}

static gint cdaudio_get_volume(gint *l, gint *r)
{
	if (cdng_cfg.use_dae) {
		*l = *r = 0;
		return FALSE;
	}
	else {
		cdio_audio_volume_t volume;
		if (cdio_audio_get_volume(pcdio, &volume) != DRIVER_OP_SUCCESS) {
			cdaudio_error("Failed to retrieve analog CD volume.\n");
			cleanup_on_error();
			*l = *r = 0;
			return FALSE;
		}
		*l = volume.level[0];
		*r = volume.level[1];

		return TRUE;
	}
}

static gint cdaudio_set_volume(gint l, gint r)
{
	debug("cdaudio_set_volume(%d, %d)\n", l, r);

	if (cdng_cfg.use_dae) {
		return FALSE;
	}
	else {
		cdio_audio_volume_t volume = {{l, r, 0, 0}};
		if (cdio_audio_set_volume(pcdio, &volume) != DRIVER_OP_SUCCESS) {
			cdaudio_error("cdaudio-ng: failed to set analog cd volume\n");
			cleanup_on_error();
			return FALSE;
		}

		return TRUE;
	}
}

static void cdaudio_cleanup(void)
{
	debug("cdaudio_cleanup()\n");

	libcddb_shutdown();

	if (pcdio != NULL) {
		if (playing_track != -1 && !cdng_cfg.use_dae)
			cdio_audio_stop(pcdio);
		cdio_destroy(pcdio);
		pcdio = NULL;
	}
	if (trackinfo != NULL) {
		g_free(trackinfo);
		trackinfo = NULL;
	}
	playing_track = -1;

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
	aud_cfg_db_set_bool(db, "CDDA", "debug", cdng_cfg.debug);
	aud_cfg_db_close(db);

	audacious_menu_plugin_item_remove(AUDACIOUS_MENU_MAIN, main_menu_item);
	audacious_menu_plugin_item_remove(AUDACIOUS_MENU_PLAYLIST, playlist_menu_item);
}

static void cdaudio_get_song_info(gchar *filename, gchar **title, gint *length)
{
	debug("cdaudio_get_song_info(\"%s\")\n", filename);

	Tuple *tuple = create_tuple_from_trackinfo_and_filename(filename);
	int trackno = find_trackno_from_filename(filename);

	if (tuple) {
		*title = aud_tuple_formatter_process_string(tuple, aud_get_gentitle_format());
		aud_tuple_free(tuple);
		tuple = NULL;
	}
	*length = calculate_track_length(trackinfo[trackno].startlsn, trackinfo[trackno].endlsn);
}

static Tuple *cdaudio_get_song_tuple(gchar *filename)
{
	debug("cdaudio_get_song_tuple(\"%s\")\n", filename);

	return create_tuple_from_trackinfo_and_filename(filename);
}


/*
 * auxiliar functions
 */
static void menu_click(void)
{
	debug("plugin services menu option selected\n");

	/* reload the cd information if the media has changed, or no track information is available */
	if (pcdio == NULL || cdio_get_media_changed(pcdio)) {
		if (scan_cd_thread != NULL)
			return;
		else {
			scan_cd_threaded(-1, -1);
			debug("CD changed, rescanning\n");
		}
	}
	else {
		gint trackno;
		for (trackno = firsttrackno; trackno <= lasttrackno; trackno++)
			append_track_to_playlist(trackno);
	}
}

static void rescan_menu_click(void)
{
	debug("plugin services rescan option selected\n");
	
	scan_cd_threaded(0, 0);
}

static Tuple *create_tuple_from_trackinfo_and_filename(gchar *filename)
{
	Tuple *tuple = aud_tuple_new_from_filename(filename);

	if (trackinfo == NULL)
		return tuple;

	gint trackno = find_trackno_from_filename(filename);

	if (trackno < firsttrackno || trackno > lasttrackno)
		return tuple;

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
	aud_tuple_associate_string(tuple, -1, "ext", "cda"); //XXX should do? --yaz

	aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL,
		calculate_track_length(trackinfo[trackno].startlsn, trackinfo[trackno].endlsn));

	if(strlen(trackinfo[trackno].genre)) {
		aud_tuple_associate_string(tuple, FIELD_GENRE, NULL,  trackinfo[trackno].genre);
	}
	//tuple->year = 0; todo: set the year

	return tuple;
}

static void dae_play_loop(dae_params_t *pdae_params)
{
	guchar *buffer = g_new(guchar, CDDA_DAE_FRAMES * CDIO_CD_FRAMESIZE_RAW);

	debug("dae started\n");
	cdio_lseek(pcdio, pdae_params->startlsn * CDIO_CD_FRAMESIZE_RAW, SEEK_SET);

	gboolean output_paused = FALSE;
	gint read_error_counter = 0;

	//pdae_params->endlsn += 75 * 3;

	while (pdae_params->pplayback->playing) {
		/* handle pause status */
		if (is_paused) {
			if (!output_paused) {
				debug("playback was not paused, pausing\n");
				pdae_params->pplayback->output->pause(TRUE);
				output_paused = TRUE;
			}
			g_usleep(1000);
			continue;
		}
		else {
			if (output_paused) {
				debug("playback was paused, resuming\n");
				pdae_params->pplayback->output->pause(FALSE);
				output_paused = FALSE;
			}
		}

		/* check if we have to seek */
		if (pdae_params->seektime != -1) {
			debug("requested seek to %d ms\n", pdae_params->seektime);
			gint newlsn = pdae_params->startlsn + ((pdae_params->seektime * 75) / 1000);
			cdio_lseek(pcdio, newlsn * CDIO_CD_FRAMESIZE_RAW, SEEK_SET);
			pdae_params->pplayback->output->flush(pdae_params->seektime);
			pdae_params->currlsn = newlsn;
			pdae_params->seektime = -1;
		}

		/* compute the actual number of sectors to read */
		gint lsncount = CDDA_DAE_FRAMES <= (pdae_params->endlsn - pdae_params->currlsn + 1) ? CDDA_DAE_FRAMES : (pdae_params->endlsn - pdae_params->currlsn + 1);

		/* check too see if we have reached the end of the song */
		if (lsncount <= 0) {
			sleep(3);
			break;
		}

		if (cdio_read_audio_sectors(pcdio, buffer, pdae_params->currlsn, lsncount) != DRIVER_OP_SUCCESS) {
			debug("failed to read audio sector\n");
			read_error_counter++;
			if (read_error_counter >= 2) {
				read_error_counter = 0;
				cdaudio_error("This CD can no longer be played, stopping.\n");
				break;
			}
		}
		else
			read_error_counter = 0;

		gint remainingbytes = lsncount * CDIO_CD_FRAMESIZE_RAW;
		guchar *bytebuff = buffer;
		while (pdae_params->pplayback->playing && remainingbytes > 0 && pdae_params->seektime == -1) {
			/* compute the actual number of bytes to play */
			gint bytecount = CDIO_CD_FRAMESIZE_RAW <= remainingbytes ? CDIO_CD_FRAMESIZE_RAW : remainingbytes;

			/* wait until the output buffer has enough room */
			while (pdae_params->pplayback->playing && pdae_params->pplayback->output->buffer_free() < bytecount && pdae_params->seektime == -1)
				g_usleep(1000);

			/* play the sound :) */
			if (pdae_params->pplayback->playing && pdae_params->seektime == -1)
				pdae_params->pplayback->pass_audio(pdae_params->pplayback, FMT_S16_LE, 2,
					bytecount, bytebuff, &pdae_params->pplayback->playing);
			remainingbytes -= bytecount;
			bytebuff += bytecount;
		}
		pdae_params->currlsn += lsncount;
	}
	debug("dae ended\n");

	pdae_params->pplayback->playing = FALSE;
	pdae_params->pplayback->output->close_audio();
	is_paused = FALSE;

	pdae_params->pplayback->output->close_audio();
	g_free(buffer);
}

static void scan_cd_threaded(int firsttrackno, int lasttrackno)
{
	if (scan_cd_thread != NULL) {
		debug("A scan_cd thread is already running.\n");
		return;
	}
	
	first_trackno_to_add_after_scan = firsttrackno;
	last_trackno_to_add_after_scan = lasttrackno;

	scan_cd_thread = g_thread_create((GThreadFunc)scan_cd, NULL, TRUE, NULL);
	if (scan_cd_thread == NULL) {
		cdaudio_error("Failed to create the thread for retrieving song information.\n");
		return;
	}
}

static void append_track_to_playlist(int trackno) 
{
	gchar pathname[DEF_STRING_LEN];

	g_snprintf(pathname, DEF_STRING_LEN, "%strack%02u.cda", CDDA_DUMMYPATH, trackno);
	aud_playlist_add(aud_playlist_get_active(), pathname);

	debug("added track \"%s\" to the playlist\n", pathname);		
}

static gboolean show_noaudiocd_info()
{	
	GDK_THREADS_ENTER();
	const gchar *markup =
                N_("<b><big>No playable CD found.</big></b>\n\n"
                   "No CD inserted, or inserted CD is not an audio CD.\n");

        GtkWidget *dialog =
                gtk_message_dialog_new_with_markup(NULL,
			   GTK_DIALOG_DESTROY_WITH_PARENT,
			   GTK_MESSAGE_ERROR,
			   GTK_BUTTONS_OK,
			   _(markup));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        GDK_THREADS_LEAVE();
        return TRUE;
}

static void *scan_cd(void *nothing)
{
	debug("scan_cd started\n");

	gint trackno;

	/* find an available, audio capable, cd drive  */
	if (cdng_cfg.device != NULL && strlen(cdng_cfg.device) > 0) {
		pcdio = cdio_open(cdng_cfg.device, DRIVER_UNKNOWN);
		if (pcdio == NULL) {
			cdaudio_error("Failed to open CD device \"%s\".\n", cdng_cfg.device);
			scan_cd_thread = NULL;
			show_noaudiocd_info();
			return NULL;
		}
	}
	else {
		gchar **ppcd_drives = cdio_get_devices_with_cap(NULL, CDIO_FS_AUDIO, false);
		pcdio = NULL;
		if (ppcd_drives != NULL && *ppcd_drives != NULL) { /* we have at least one audio capable cd drive */
			pcdio = cdio_open(*ppcd_drives, DRIVER_UNKNOWN);
			if (pcdio == NULL) {
				cdaudio_error("Failed to open CD.\n");
				cleanup_on_error();
				scan_cd_thread = NULL;
				show_noaudiocd_info();
				return NULL;
			}
			debug("found cd drive \"%s\" with audio capable media\n", *ppcd_drives);
		}
		else {
			cdaudio_error("Unable to find or access a CDDA capable drive.\n");
			cleanup_on_error();
			scan_cd_thread = NULL;
			show_noaudiocd_info();
			return NULL;
		}
		if (ppcd_drives != NULL && *ppcd_drives != NULL)
			cdio_free_device_list(ppcd_drives);
	}

	/* limit read speed */
	if (cdng_cfg.limitspeed > 0 && cdng_cfg.use_dae) {
		debug("setting drive speed limit to %dx\n", cdng_cfg.limitspeed);
		if (cdio_set_speed(pcdio, cdng_cfg.limitspeed) != DRIVER_OP_SUCCESS)
			cdaudio_error("Failed to set drive speed to %dx.\n", cdng_cfg.limitspeed);
	}

	/* general track initialization */
	cdrom_drive_t *pcdrom_drive = cdio_cddap_identify_cdio(pcdio, 1, NULL);	// todo : check return / NULL
	firsttrackno = cdio_get_first_track_num(pcdrom_drive->p_cdio);
	lasttrackno = cdio_get_last_track_num(pcdrom_drive->p_cdio);
	if (firsttrackno == CDIO_INVALID_TRACK || lasttrackno == CDIO_INVALID_TRACK) {
		cdaudio_error("Failed to retrieve first/last track number.\n");
		cleanup_on_error();
		scan_cd_thread = NULL;
		show_noaudiocd_info();
		return NULL;
	}
	debug("first track is %d and last track is %d\n", firsttrackno, lasttrackno);

	g_free(trackinfo);
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
			cdaudio_error("Failed to retrieve stard/end lsn for track %d.\n", trackno);
			cleanup_on_error();
			scan_cd_thread = NULL;
			show_noaudiocd_info();
			return NULL;
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
			g_snprintf(trackinfo[trackno].name, DEF_STRING_LEN, "CD Audio Track %02u", trackno);
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
				cdaudio_error("Failed to create the cddb connection.\n");
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
				guint discid = cddb_disc_get_discid(pcddb_disc);
				debug("CDDB disc id = %x\n", discid);

				gint matches;
				if ((matches = cddb_query(pcddb_conn, pcddb_disc)) == -1) {
					if (cddb_errno(pcddb_conn) == CDDB_ERR_OK)
						cdaudio_error("Failed to query the CDDB server\n");
					else
						cdaudio_error("Failed to query the CDDB server: %s\n", cddb_error_str(cddb_errno(pcddb_conn)));

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
							cdaudio_error("failed to read the cddb info: %s\n", cddb_error_str(cddb_errno(pcddb_conn)));
							cddb_disc_destroy(pcddb_disc);
							pcddb_disc = NULL;
						}
						else {
							debug("we have got the cddb info\n");
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

	if (cdng_cfg.debug) {
		debug("disc has : performer = \"%s\", name = \"%s\", genre = \"%s\"\n",
			trackinfo[0].performer, trackinfo[0].name, trackinfo[0].genre);

		for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
			debug("track %d has : performer = \"%s\", name = \"%s\", genre = \"%s\", startlsn = %d, endlsn = %d\n",
				trackno, trackinfo[trackno].performer, trackinfo[trackno].name, trackinfo[trackno].genre, trackinfo[trackno].startlsn, trackinfo[trackno].endlsn);
		}
	}

	debug("scan_cd ended\n");

	scan_cd_thread = NULL;

	
	/* add the requested entries to the current playlist */
	
	if (first_trackno_to_add_after_scan == 0 || last_trackno_to_add_after_scan == 0)
		return NULL;
	
	if (first_trackno_to_add_after_scan == -1)
		first_trackno_to_add_after_scan = firsttrackno;
	if (last_trackno_to_add_after_scan == -1)
		last_trackno_to_add_after_scan = lasttrackno;
	
	for (trackno = first_trackno_to_add_after_scan; trackno <= last_trackno_to_add_after_scan; trackno++)
		append_track_to_playlist(trackno);
	
	return NULL;
}

static gint calculate_track_length(gint startlsn, gint endlsn)
{
	return ((endlsn - startlsn + 1) * 1000) / 75;
}

static gint find_trackno_from_filename(gchar *filename)
{
	gchar tracknostr[3];
	if ((filename == NULL) || strlen(filename) <= 6)
		return -1;

	strncpy(tracknostr, filename + strlen(filename) - 6, 2);
	tracknostr[2] = '\0';
	return strtol(tracknostr, NULL, 10);
}

static void cleanup_on_error()
{
	if (playing_track != -1) {
		playing_track = -1;
	}

	if (trackinfo != NULL) {
		g_free(trackinfo);
		trackinfo = NULL;
	}
}

