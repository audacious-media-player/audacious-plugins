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
#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/output.h>
#include <audacious/playlist.h>
#include <audacious/ui_plugin_menu.h>
#include <audacious/util.h>

#include "cdaudio-ng.h"
#include "configure.h"


static int				firsttrackno = -1;
static int				lasttrackno = -1;
static CdIo_t			*pcdio = NULL;
static trackinfo_t		*trackinfo = NULL;

static gboolean			use_dae = TRUE;
static gboolean			use_cdtext = TRUE;
static gboolean			use_cddb = TRUE;
static gchar				*cd_device = NULL;
static gint				limitspeed = 1;
static gboolean			is_paused = FALSE;
static gint				playing_track = -1;
static dae_params_t		*pdae_params = NULL;
static gboolean			use_debug = FALSE;
static gchar				*cddb_server = NULL;
static gint				cddb_port;
static InputPlayback	*pglobalinputplayback = NULL;
static GtkWidget		*main_menu_item, *playlist_menu_item;

static void				cdaudio_init();
static void				cdaudio_about();
static void				cdaudio_configure();
static gint				cdaudio_is_our_file(gchar *filename);
static GList			*cdaudio_scan_dir(gchar *dirname);
static void				cdaudio_play_file(InputPlayback *pinputplayback);
static void				cdaudio_stop(InputPlayback *pinputplayback);
static void				cdaudio_pause(InputPlayback *pinputplayback, gshort paused);
static void				cdaudio_seek(InputPlayback *pinputplayback, gint time);
static gint				cdaudio_get_time(InputPlayback *pinputplayback);
static gint				cdaudio_get_volume(gint *l, gint *r);
static gint				cdaudio_set_volume(gint l, gint r);
static void				cdaudio_cleanup();
static void				cdaudio_get_song_info(gchar *filename, gchar **title, gint *length);
static Tuple			*cdaudio_get_song_tuple(gchar *filename);

static void				menu_click();
static Tuple			*create_aud_tuple_from_trackinfo(char *filename);
static void				dae_play_loop(dae_params_t *pdae_params);
static int				calculate_track_length(int startlsn, int endlsn);
static int				find_trackno_from_filename(char *filename);
static void				cleanup_on_error();


static InputPlugin inputplugin = {
	.description = "CD Audio Plugin NG",
	.init = cdaudio_init,
	.about = cdaudio_about,
	.configure = cdaudio_configure,
	.is_our_file = cdaudio_is_our_file,
	.scan_dir = cdaudio_scan_dir,
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


void cdaudio_error(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "cdaudio-ng: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}


void CDDEBUG(const char *fmt, ...)
{
	if (use_debug) {
		va_list ap;
		fprintf(stderr, "cdaudio-ng: ");
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}


static void cdaudio_init()
{
	ConfigDb *db;
	gchar *menu_item_text;
	gchar *tmpstr;
	
	CDDEBUG("cdaudio_init()\n");

	if ((db = bmp_cfg_db_open()) == NULL) {
		cdaudio_error("Failed to read configuration.\n");
		cleanup_on_error();
		return;
	}

	if (!cdio_init()) {
		cdaudio_error("Failed to initialize cdio subsystem.\n");
		cleanup_on_error();
		return;
	}

	libcddb_init();

	/*
	if (!bmp_cfg_db_get_bool(db, "CDDA", "use_dae", &use_dae))
		use_dae = TRUE;
	*/
	if (!bmp_cfg_db_get_int(db, "CDDA", "limitspeed", &limitspeed))
		limitspeed = 1;
	if (!bmp_cfg_db_get_bool(db, "CDDA", "use_cdtext", &use_cdtext))
		use_cdtext = TRUE;
	if (!bmp_cfg_db_get_bool(db, "CDDA", "use_cddb", &use_cddb))
		use_cddb = TRUE;
	if (!bmp_cfg_db_get_string(db, "CDDA", "cddbserver", &cddb_server))
		cddb_server = g_strdup(CDDA_DEFAULT_CDDB_SERVER);
	if (!bmp_cfg_db_get_int(db, "CDDA", "cddbport", &cddb_port))
		cddb_port = CDDA_DEFAULT_CDDB_PORT;
	if (!bmp_cfg_db_get_string(db, "CDDA", "device", &cd_device))
		cd_device = g_strdup("");
	if (!bmp_cfg_db_get_bool(db, "CDDA", "debug", &use_debug))
		use_debug = FALSE;

	bmp_cfg_db_close(db);

	CDDEBUG(/*use_dae = %d, */"limitspeed = %d, use_cdtext = %d, use_cddb = %d, cddbserver = \"%s\", cddbport = %d, device = \"%s\", debug = %d\n", /*use_dae, */limitspeed, use_cdtext, use_cddb, cddb_server, cddb_port, cd_device, use_debug);

	configure_set_variables(/*&use_dae, */&limitspeed, &use_cdtext, &use_cddb, cd_device, &use_debug, cddb_server, &cddb_port);
	configure_create_gui();

	menu_item_text = _("Add CD");
	main_menu_item = gtk_image_menu_item_new_with_label(menu_item_text);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(main_menu_item), gtk_image_new_from_stock(GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU));
	gtk_widget_show(main_menu_item);
	audacious_menu_plugin_item_add(AUDACIOUS_MENU_MAIN, main_menu_item);
	g_signal_connect(G_OBJECT(main_menu_item), "button_press_event", G_CALLBACK(menu_click), NULL);  
	
	playlist_menu_item = gtk_image_menu_item_new_with_label(menu_item_text);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(playlist_menu_item), gtk_image_new_from_stock(GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU));
	gtk_widget_show(playlist_menu_item);
	audacious_menu_plugin_item_add(AUDACIOUS_MENU_PLAYLIST, playlist_menu_item);
	g_signal_connect(G_OBJECT(playlist_menu_item), "button_press_event", G_CALLBACK(menu_click), NULL);  
	
	aud_uri_set_plugin("cdda://", &inputplugin);
}

static void cdaudio_about()
{
	CDDEBUG("cdaudio_about()\n");

	static GtkWidget* about_window = NULL;

    if (about_window) {
        gdk_window_raise(about_window->window);
    }

    about_window = audacious_info_dialog(_("About CD Audio Plugin NG"),
    _("Copyright (c) 2007, by Calin Crisan <ccrisan@gmail.com> and The Audacious Team.\n\n"
    "Many thanks to libcdio developers <http://www.gnu.org/software/libcdio/>\n"
    "\tand to libcddb developers <http://libcddb.sourceforge.net/>.\n\n"
    "Also thank you Tony Vroon for mentoring & guiding me.\n\n"
    "This was a Google Summer of Code 2007 project."), _("OK"), FALSE, NULL, NULL);

    g_signal_connect(G_OBJECT(about_window), "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &about_window);
}

static void cdaudio_configure()
{
	CDDEBUG("cdaudio_configure()\n");

	/*
	if (playing_track != -1)
		playback_stop();
	*/

	configure_show_gui();
}

static gint cdaudio_is_our_file(gchar *filename)
{
	CDDEBUG("cdaudio_is_our_file(\"%s\")\n", filename);

	if ((filename != NULL) && strlen(filename) > 4 && (!strcasecmp(filename + strlen(filename) - 4, ".cda"))) {
			/* no CD information yet */
		if (pcdio == NULL) {
			CDDEBUG("no CD information, scanning\n");
			cdaudio_scan_dir(CDDA_DEFAULT);
		}

			/* reload the cd information if the media has changed */
		if (cdio_get_media_changed(pcdio) && pcdio != NULL) {
			CDDEBUG("CD changed, rescanning\n");
			if (cdaudio_scan_dir(CDDA_DEFAULT) == NULL)
				pcdio = NULL;
		}

		if (pcdio == NULL) {
			CDDEBUG("\"%s\" is not our file\n", filename);
			return FALSE;
		}

			/* check if the requested track actually exists on the current audio cd */
		gint trackno = find_trackno_from_filename(filename);
		if (trackno < firsttrackno || trackno > lasttrackno) {
			CDDEBUG("\"%s\" is not our file\n", filename);
			return FALSE;
		}
		
		CDDEBUG("\"%s\" is our file\n", filename);
		return TRUE;
	}
	else {
		CDDEBUG("\"%s\" is not our file\n", filename);
		return FALSE;
	}
}

GList *cdaudio_scan_dir(gchar *dirname)
{
	CDDEBUG("cdaudio_scan_dir(\"%s\")\n", dirname);

		/* if the given dirname does not belong to us, we return NULL */
	if (strstr(dirname, CDDA_DEFAULT) == NULL) {
		CDDEBUG("\"%s\" directory does not belong to us\n", dirname);
		return NULL;
	}

		/* find an available, audio capable, cd drive  */
	if (cd_device != NULL && strlen(cd_device) > 0) {
		pcdio = cdio_open(cd_device, DRIVER_UNKNOWN);
		if (pcdio == NULL) {
			cdaudio_error("Failed to open CD device \"%s\".\n", cd_device);
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
				return NULL;
			}
			CDDEBUG("found cd drive \"%s\" with audio capable media\n", *ppcd_drives);
		}
		else {
			cdaudio_error("Unable to find or access a CDDA capable drive.\n");
			cleanup_on_error();
			return NULL;
		}
		if (ppcd_drives != NULL && *ppcd_drives != NULL)
			cdio_free_device_list(ppcd_drives);
	}

		/* limit read speed */
	if (limitspeed > 0 && use_dae) {
		CDDEBUG("setting drive speed limit to %dx\n", limitspeed);
		if (cdio_set_speed(pcdio, limitspeed) != DRIVER_OP_SUCCESS)
			cdaudio_error("Failed to set drive speed to %dx.\n", limitspeed);
	}

		/* get general track initialization */
	cdrom_drive_t *pcdrom_drive = cdio_cddap_identify_cdio(pcdio, 1, NULL);	// todo : check return / NULL
	firsttrackno = cdio_get_first_track_num(pcdrom_drive->p_cdio);
	lasttrackno = cdio_get_last_track_num(pcdrom_drive->p_cdio);
	if (firsttrackno == CDIO_INVALID_TRACK || lasttrackno == CDIO_INVALID_TRACK) {
		cdaudio_error("Failed to retrieve first/last track number.\n");
		cleanup_on_error();
		return NULL;
	}
	CDDEBUG("first track is %d and last track is %d\n", firsttrackno, lasttrackno);

	g_free(trackinfo);
	trackinfo = (trackinfo_t *) g_new(trackinfo_t, (lasttrackno + 1));
	gint trackno;

	trackinfo[0].startlsn = cdio_get_track_lsn(pcdrom_drive->p_cdio, 0);
	trackinfo[0].endlsn = cdio_get_track_last_lsn(pcdrom_drive->p_cdio, CDIO_CDROM_LEADOUT_TRACK);
	strcpy(trackinfo[0].performer, "");
	strcpy(trackinfo[0].name, "");
	strcpy(trackinfo[0].genre, "");
	for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
		trackinfo[trackno].startlsn = cdio_get_track_lsn(pcdrom_drive->p_cdio, trackno);
		trackinfo[trackno].endlsn = cdio_get_track_last_lsn(pcdrom_drive->p_cdio, trackno);
		strcpy(trackinfo[trackno].performer, "");
		strcpy(trackinfo[trackno].name, "");
		strcpy(trackinfo[trackno].genre, "");

		if (trackinfo[trackno].startlsn == CDIO_INVALID_LSN || trackinfo[trackno].endlsn == CDIO_INVALID_LSN) {
			cdaudio_error("Failed to retrieve stard/end lsn for track %d.\n", trackno);
			cleanup_on_error();
			return NULL;
		}
	}

		/* initialize de cddb subsystem */
	cddb_conn_t *pcddb_conn = NULL;
	cddb_disc_t *pcddb_disc = NULL;
	cddb_track_t *pcddb_track = NULL;

	if (use_cddb) {
		pcddb_conn = cddb_new();
		if (pcddb_conn == NULL)
			cdaudio_error("Failed to create the cddb connection.\n");
		else {
			CDDEBUG("getting CDDB info\n");

			cddb_set_server_name(pcddb_conn, cddb_server);
			cddb_set_server_port(pcddb_conn, cddb_port);

			pcddb_disc = cddb_disc_new();
			for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
				pcddb_track = cddb_track_new();
				cddb_track_set_frame_offset(pcddb_track, trackinfo[trackno].startlsn);
				cddb_disc_add_track(pcddb_disc, pcddb_track);
			}

			msf_t startmsf, endmsf;
			cdio_get_track_msf(pcdio, 1, &startmsf);
			cdio_get_track_msf(pcdio, CDIO_CDROM_LEADOUT_TRACK, &endmsf);
			cddb_disc_set_length(pcddb_disc, cdio_audio_get_msf_seconds(&endmsf) - cdio_audio_get_msf_seconds(&startmsf));

			int matches;
			if ((matches = cddb_query(pcddb_conn, pcddb_disc)) == -1) {
				cdaudio_error("Failed to query the CDDB server: %s\n", cddb_error_str(cddb_errno(pcddb_conn)));
				cddb_disc_destroy(pcddb_disc);
				pcddb_disc = NULL;
			}
			else {
				CDDEBUG("discid = %X, category = \"%s\"\n", cddb_disc_get_discid(pcddb_disc), cddb_disc_get_category_str(pcddb_disc));

				cddb_read(pcddb_conn, pcddb_disc);
				if (cddb_errno(pcddb_conn) != CDDB_ERR_OK) {
					fprintf(stderr, "cdaudio-ng: failed to read the cddb info: %s\n", cddb_error_str(cddb_errno(pcddb_conn)));
					cddb_disc_destroy(pcddb_disc);
					pcddb_disc = NULL;
				}
				else {
					CDDEBUG("we have got the cddb info\n");

					strncpy(trackinfo[0].performer, cddb_disc_get_artist(pcddb_disc), strlen(cddb_disc_get_artist(pcddb_disc)) + 1);
					strncpy(trackinfo[0].name, cddb_disc_get_title(pcddb_disc), strlen(cddb_disc_get_title(pcddb_disc)) + 1);
					strncpy(trackinfo[0].genre, cddb_disc_get_genre(pcddb_disc), strlen(cddb_disc_get_genre(pcddb_disc)) + 1);
				}
			}
		}
	}

		/* adding trackinfo[0] information (the disc) */
	if (use_cdtext) {
		CDDEBUG("getting cd-text information for disc\n");
		cdtext_t *pcdtext = cdio_get_cdtext(pcdrom_drive->p_cdio, 0);
		if (pcdtext == NULL || pcdtext->field[CDTEXT_TITLE] == NULL) {
			CDDEBUG("no cd-text available for disc\n");
		}
		else {
			strncpy(trackinfo[0].performer, pcdtext->field[CDTEXT_PERFORMER] != NULL ? pcdtext->field[CDTEXT_PERFORMER] : "", strlen(pcdtext->field[CDTEXT_PERFORMER]) + 1);
			strncpy(trackinfo[0].name, pcdtext->field[CDTEXT_TITLE] != NULL ? pcdtext->field[CDTEXT_TITLE] : "", strlen(pcdtext->field[CDTEXT_TITLE]) + 1);
			strncpy(trackinfo[0].genre, pcdtext->field[CDTEXT_GENRE] != NULL ? pcdtext->field[CDTEXT_GENRE] : "", strlen(pcdtext->field[CDTEXT_GENRE]) + 1);
		}
	}

		/* add track "file" names to the list */
	GList *list = NULL;
	for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
		list = g_list_append(list, g_strdup_printf("track%02u.cda", trackno));
		cdtext_t *pcdtext = NULL;
		if (use_cdtext) {
			CDDEBUG("getting cd-text information for track %d\n", trackno);
			pcdtext = cdio_get_cdtext(pcdrom_drive->p_cdio, trackno);
			if (pcdtext == NULL || pcdtext->field[CDTEXT_PERFORMER] == NULL) {
				CDDEBUG("no cd-text available for track %d\n", trackno);
				pcdtext = NULL;
			}
		}

		if (pcdtext != NULL) {
			strncpy(trackinfo[trackno].performer, pcdtext->field[CDTEXT_PERFORMER] != NULL ? pcdtext->field[CDTEXT_PERFORMER] : "", strlen(pcdtext->field[CDTEXT_PERFORMER]) + 1);
			strncpy(trackinfo[trackno].name, pcdtext->field[CDTEXT_TITLE] != NULL ? pcdtext->field[CDTEXT_TITLE] : "", strlen(pcdtext->field[CDTEXT_TITLE]) + 1);
			strncpy(trackinfo[trackno].genre, pcdtext->field[CDTEXT_GENRE] != NULL ? pcdtext->field[CDTEXT_GENRE] : "", strlen(pcdtext->field[CDTEXT_GENRE]) + 1);
		}
		else
			if (pcddb_disc != NULL) {
				cddb_track_t *pcddb_track = cddb_disc_get_track(pcddb_disc, trackno - 1);
				strncpy(trackinfo[trackno].performer, cddb_track_get_artist(pcddb_track), strlen(cddb_track_get_artist(pcddb_track)) + 1);
				strncpy(trackinfo[trackno].name, cddb_track_get_title(pcddb_track), strlen(cddb_track_get_title(pcddb_track)) + 1);
				strncpy(trackinfo[trackno].genre, cddb_disc_get_genre(pcddb_disc), strlen(cddb_disc_get_genre(pcddb_disc)) + 1);
			}
			else {
				strcpy(trackinfo[trackno].performer, "");
				strcpy(trackinfo[trackno].name, "");
				strcpy(trackinfo[trackno].genre, "");
			}

		if (strlen(trackinfo[trackno].name) == 0)
			snprintf(trackinfo[trackno].name, sizeof(trackinfo[trackno].name), "CD Audio Track %02u", trackno);

	}

	if (use_debug) {
		CDDEBUG("disc has : performer = \"%s\", name = \"%s\", genre = \"%s\"\n",
			   trackinfo[0].performer, trackinfo[0].name, trackinfo[0].genre);
		for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
			CDDEBUG("track %d has : performer = \"%s\", name = \"%s\", genre = \"%s\", startlsn = %d, endlsn = %d\n",
				trackno, trackinfo[trackno].performer, trackinfo[trackno].name, trackinfo[trackno].genre, trackinfo[trackno].startlsn, trackinfo[trackno].endlsn);
		}
	}

	if (pcddb_disc != NULL)
		cddb_disc_destroy(pcddb_disc);
	if (pcddb_conn != NULL)
		cddb_destroy(pcddb_conn);

	return list;
}

void cdaudio_play_file(InputPlayback *pinputplayback)
{
	Tuple *tuple;
	gchar *title;
	
	CDDEBUG("cdaudio_play_file(\"%s\")\n", pinputplayback->filename);

	pglobalinputplayback = pinputplayback;

	if (trackinfo == NULL) {
		CDDEBUG("no CD information, scanning\n");
		cdaudio_scan_dir(CDDA_DEFAULT);
	}

	if (cdio_get_media_changed(pcdio)) {
		CDDEBUG("CD changed, rescanning\n");
		cdaudio_scan_dir(CDDA_DEFAULT);
	}

	if (trackinfo == NULL) {
		CDDEBUG("no CD information can be retrieved, aborting\n");
		pinputplayback->playing = FALSE;
		return;
	}

	int trackno = find_trackno_from_filename(pinputplayback->filename);
	if (trackno < firsttrackno || trackno > lasttrackno) {
		cdaudio_error("Track #%d is out of range [%d..%d]\n", trackno, firsttrackno, lasttrackno);
		cleanup_on_error();
		return;
	}

	pinputplayback->playing = TRUE;
	playing_track = trackno;
	is_paused = FALSE;

	tuple = create_aud_tuple_from_trackinfo(pinputplayback->filename);
	title = aud_tuple_formatter_make_title_string(tuple, get_gentitle_format());

	pinputplayback->set_params(pinputplayback, title, calculate_track_length(trackinfo[trackno].startlsn, trackinfo[trackno].endlsn), 1411200, 44100, 2);
	g_free(title);
	aud_tuple_free(tuple);

	if (use_dae) {
		CDDEBUG("using digital audio extraction\n");

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
		CDDEBUG("starting dae thread...\n");
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
		CDDEBUG("not using digital audio extraction\n");

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

void cdaudio_stop(InputPlayback *pinputplayback)
{
	CDDEBUG("cdaudio_stop(\"%s\")\n", pinputplayback != NULL ? pinputplayback->filename : "N/A");

	pglobalinputplayback = NULL;

	if (playing_track == -1)
		return;

	if (pinputplayback != NULL)
		pinputplayback->playing = FALSE;
	playing_track = -1;
	is_paused = FALSE;

	if (use_dae) {
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

void cdaudio_pause(InputPlayback *pinputplayback, gshort paused)
{
	CDDEBUG("cdaudio_pause(\"%s\", %d)\n", pinputplayback->filename, paused);

	if (!is_paused) {
		is_paused = TRUE;
		if (!use_dae)
			if (cdio_audio_pause(pcdio) != DRIVER_OP_SUCCESS) {
				cdaudio_error("Failed to pause analog CD!\n");
				cleanup_on_error();
				return;
			}
	}
	else {
		is_paused = FALSE;
		if (!use_dae)
			if (cdio_audio_resume(pcdio) != DRIVER_OP_SUCCESS) {
				cdaudio_error("Failed to resume analog CD!\n");
				cleanup_on_error();
				return;
			}
	}
}

void cdaudio_seek(InputPlayback *pinputplayback, gint time)
{
	CDDEBUG("cdaudio_seek(\"%s\", %d)\n", pinputplayback->filename, time);

	if (playing_track == -1)
		return;

	if (use_dae) {
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

gint cdaudio_get_time(InputPlayback *pinputplayback)
{
	//printf("cdaudio-ng: cdaudio_get_time(\"%s\")\n", pinputplayback->filename); // annoying!

	if (playing_track == -1)
		return -1;

	if (!use_dae) {
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

gint cdaudio_get_volume(gint *l, gint *r)
{
	//printf("cdaudio-ng: cdaudio_get_volume()\n"); // annoying!

	if (use_dae) {
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

gint cdaudio_set_volume(gint l, gint r)
{
	CDDEBUG("cdaudio_set_volume(%d, %d)\n", l, r);

	if (use_dae) {
		return FALSE;
	}
	else {
		cdio_audio_volume_t volume = {{l, r, 0, 0}};
		if (cdio_audio_set_volume(pcdio, &volume) != DRIVER_OP_SUCCESS) {
			fprintf(stderr, "cdaudio-ng: failed to set analog cd volume\n");
			cleanup_on_error();
			return FALSE;
		}

		return TRUE;
	}
}

void cdaudio_cleanup()
{
	CDDEBUG("cdaudio_cleanup()\n");

	libcddb_shutdown();

	if (pcdio!= NULL) {
		if (playing_track != -1 && !use_dae)
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

	ConfigDb *db = bmp_cfg_db_open();
	/*bmp_cfg_db_set_bool(db, "CDDA", "use_dae", use_dae);*/
	bmp_cfg_db_set_int(db, "CDDA", "limitspeed", limitspeed);
	bmp_cfg_db_set_bool(db, "CDDA", "use_cdtext", use_cdtext);
	bmp_cfg_db_set_bool(db, "CDDA", "use_cddb", use_cddb);
	bmp_cfg_db_set_string(db, "CDDA", "cddbserver", cddb_server);
	bmp_cfg_db_set_int(db, "CDDA", "cddbport", cddb_port);
	bmp_cfg_db_set_string(db, "CDDA", "device", cd_device);
	bmp_cfg_db_set_bool(db, "CDDA", "debug", use_debug);
	bmp_cfg_db_close(db);
}

void cdaudio_get_song_info(gchar *filename, gchar **title, gint *length)
{
	CDDEBUG("cdaudio_get_song_info(\"%s\")\n", filename);

	gint trackno = find_trackno_from_filename(filename);
	Tuple *tuple = create_aud_tuple_from_trackinfo(filename);

	if(tuple) {
		*title = aud_tuple_formatter_process_string(tuple, get_gentitle_format());
		aud_tuple_free(tuple);
		tuple = NULL;
	}
	*length = calculate_track_length(trackinfo[trackno].startlsn, trackinfo[trackno].endlsn);
}

Tuple *cdaudio_get_song_tuple(gchar *filename)
{
	CDDEBUG("cdaudio_get_song_tuple(\"%s\")\n", filename);

	return create_aud_tuple_from_trackinfo(filename);
}


	/* auxiliar functions */

void menu_click()
{
    GList *list, *node;
    gchar *filename;
	
	if (!(list = cdaudio_scan_dir(CDDA_DEFAULT))) {
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
		return;
	}

	for (node = list; node; node = g_list_next(node)) {
		filename = g_build_filename(CDDA_DEFAULT, node->data, NULL);
		playlist_add(playlist_get_active(), filename);
		g_free(filename);
		g_free(node->data);
	}

	g_list_free(list);
}

Tuple *create_aud_tuple_from_trackinfo(char *filename)
{
	if (trackinfo == NULL)
		return NULL;

	Tuple *tuple = aud_tuple_new_from_filename(filename);
	gint trackno = find_trackno_from_filename(filename);

	if (trackno < firsttrackno || trackno > lasttrackno)
		return NULL;

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

	aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, calculate_track_length(trackinfo[trackno].startlsn, trackinfo[trackno].endlsn));
	if(strlen(trackinfo[trackno].genre)) {
		aud_tuple_associate_string(tuple, FIELD_GENRE, NULL,  trackinfo[trackno].genre);
	}
	//tuple->year = 0; todo: set the year

	return tuple;
}

void dae_play_loop(dae_params_t *pdae_params)
{
	guchar *buffer = g_new(guchar, CDDA_DAE_FRAMES * CDIO_CD_FRAMESIZE_RAW);

	CDDEBUG("dae started\n");
	cdio_lseek(pcdio, pdae_params->startlsn * CDIO_CD_FRAMESIZE_RAW, SEEK_SET);

	gboolean output_paused = FALSE;
	gint read_error_counter = 0;

	//pdae_params->endlsn += 75 * 3;

	while (pdae_params->pplayback->playing) {
			/* handle pause status */
		if (is_paused) {
			if (!output_paused) {
				CDDEBUG("playback was not paused, pausing\n");
				pdae_params->pplayback->output->pause(TRUE);
				output_paused = TRUE;
			}
			g_usleep(1000);
			continue;
		}
		else {
			if (output_paused) {
				CDDEBUG("playback was paused, resuming\n");
				pdae_params->pplayback->output->pause(FALSE);
				output_paused = FALSE;
			}
		}

			/* check if we have to seek */
		if (pdae_params->seektime != -1) {
			CDDEBUG("requested seek to %d ms\n", pdae_params->seektime);
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
			CDDEBUG("failed to read audio sector\n");
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
				usleep(1000);
				/* play the sound :) */
			if (pdae_params->pplayback->playing && pdae_params->seektime == -1)
				pdae_params->pplayback->pass_audio(pdae_params->pplayback, FMT_S16_LE, 2, 
					bytecount, bytebuff, &pdae_params->pplayback->playing);
			remainingbytes -= bytecount;
			bytebuff += bytecount;
		}
		pdae_params->currlsn += lsncount;
	}
	CDDEBUG("dae ended\n");

	pdae_params->pplayback->playing = FALSE;
	pdae_params->pplayback->output->close_audio();
	is_paused = FALSE;

	pdae_params->pplayback->output->close_audio();
	g_free(buffer);
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

