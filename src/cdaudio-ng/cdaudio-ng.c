
/*
	todo:
		- about dialog
*/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

#include <cdio/cdio.h>
#include <cdio/cdtext.h>
#include <cdio/track.h>
#include <cdio/cdda.h>
#include <cdio/audio.h>
#include <cdio/sector.h>
#include <cdio/cd_types.h>
#include <cddb/cddb.h>

#include <glib.h>

#include <audacious/i18n.h>
#include <audacious/configdb.h>
#include <audacious/plugin.h>
//#include <audacious/playback.h>	// todo: this should be available soon (by 1.4)
#include <audacious/util.h>
#include <audacious/output.h>
#include "config.h"

#include "cdaudio-ng.h"
#include "configure.h"


static int				firsttrackno = -1;
static int				lasttrackno = -1;
static CdIo_t			*pcdio = NULL;
static trackinfo_t		*trackinfo = NULL;

static gboolean			use_dae = TRUE;
static gboolean			use_cdtext = TRUE;
static gboolean			use_cddb = TRUE;
static char				device[DEF_STRING_LEN];
static int				limitspeed = 1;
static gboolean			is_paused = FALSE;
static int				playing_track = -1;
static dae_params_t		*pdae_params = NULL;
static gboolean			debug = FALSE;
static char				cddb_server[DEF_STRING_LEN];
static int				cddb_port;
static InputPlayback	*pglobalinputplayback = NULL;

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
static TitleInput		*cdaudio_get_song_tuple(gchar *filename);

static TitleInput		*create_tuple_from_trackinfo(char *filename);
static void				dae_play_loop(dae_params_t *pdae_params);
static int				calculate_track_length(int startlsn, int endlsn);
static int				find_trackno_from_filename(char *filename);
static void				cleanup_on_error();


static InputPlugin inputplugin = {
	NULL,
	NULL,
	"CD Audio Plugin NG",
	cdaudio_init,
	cdaudio_about,
	cdaudio_configure,
	cdaudio_is_our_file,
	cdaudio_scan_dir,
	cdaudio_play_file,
	cdaudio_stop,
	cdaudio_pause,
	cdaudio_seek,
	NULL,
	cdaudio_get_time,
	cdaudio_get_volume,
	cdaudio_set_volume,
	cdaudio_cleanup,
	NULL,
	NULL,
	NULL,
	NULL,
	cdaudio_get_song_info,
	NULL,
	NULL,
	cdaudio_get_song_tuple
};

InputPlugin *cdaudio_iplist[] = { &inputplugin, NULL };

DECLARE_PLUGIN(cdaudio, NULL, NULL, cdaudio_iplist, NULL, NULL, NULL, NULL);


void cdaudio_init()
{
	if (debug)
		printf("cdaudio-ng: cdaudio_init()\n");

	if (!cdio_init()) {
		fprintf(stderr, "cdaudio-ng: failed to initialize cdio subsystem\n");
		cleanup_on_error();
		return;
	}

	libcddb_init();

	ConfigDb *db = bmp_cfg_db_open();
	gchar *string = NULL;

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
	if (!bmp_cfg_db_get_string(db, "CDDA", "cddbserver", &string))
		strcpy(cddb_server, CDDA_DEFAULT_CDDB_SERVER);
	else
		strcpy(cddb_server, string);
	if (!bmp_cfg_db_get_int(db, "CDDA", "cddbport", &cddb_port))
		cddb_port = CDDA_DEFAULT_CDDB_PORT;
	if (!bmp_cfg_db_get_string(db, "CDDA", "device", &string))
		strcpy(device, "");
	else
		strcpy(device, string);
	if (!bmp_cfg_db_get_bool(db, "CDDA", "debug", &debug))
		debug = FALSE;

	bmp_cfg_db_close(db);

	if (debug)
		printf("cdaudio-ng: configuration: "/*use_dae = %d, */"limitspeed = %d, use_cdtext = %d, use_cddb = %d, cddbserver = \"%s\", cddbport = %d, device = \"%s\", debug = %d\n", /*use_dae, */limitspeed, use_cdtext, use_cddb, cddb_server, cddb_port, device, debug);

	configure_set_variables(/*&use_dae, */&limitspeed, &use_cdtext, &use_cddb, device, &debug, cddb_server, &cddb_port);
	configure_create_gui();
}

void cdaudio_about()
{
	if (debug)
		printf("cdaudio-ng: cdaudio_about()\n");

	static GtkWidget* about_window = NULL;

    if (about_window) {
        gdk_window_raise(about_window->window);
    }

    char about_text[1000];
	sprintf(about_text, _("Copyright (c) 2007, by Calin Crisan <ccrisan@gmail.com> and The Audacious Team.\n\n"
						"Many thanks to libcdio developers <http://www.gnu.org/software/libcdio/>\n\tand to libcddb developers <http://libcddb.sourceforge.net/>.\n\n"
						"Also thank you Tony Vroon for mentoring & guiding me.\n\n"
 						"This was a Google Summer of Code 2007 project."));

    about_window = xmms_show_message(_("About CD Audio Plugin NG"), about_text, _("OK"), FALSE, NULL, NULL);

    g_signal_connect(G_OBJECT(about_window), "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &about_window);
}

void cdaudio_configure()
{
	if (debug)
		printf("cdaudio-ng: cdaudio_configure()\n");

	/*
	if (playing_track != -1)
		playback_stop();
	*/

	configure_show_gui();
}

gint cdaudio_is_our_file(gchar *filename)
{
	if (debug)
		printf("cdaudio-ng: cdaudio_is_our_file(\"%s\")\n", filename);

	if ((filename != NULL) && strlen(filename) > 4 && (!strcasecmp(filename + strlen(filename) - 4, ".cda"))) {
			/* no CD information yet */
		if (pcdio == NULL) {
			if (debug)
				printf("cdaudio-ng: no cd information, scanning\n");
			cdaudio_scan_dir(CDDA_DEFAULT);
		}

			/* reload the cd information if the media has changed */
		if (cdio_get_media_changed(pcdio) && pcdio != NULL) {
			if (debug)
				printf("cdaudio-ng: cd changed, rescanning\n");
			cdaudio_scan_dir(CDDA_DEFAULT);
		}

		if (pcdio == NULL) {
			if (debug)
				printf("cdaudio-ng: \"%s\" is not our file\n", filename);
			return FALSE;
		}

			/* check if the requested track actually exists on the current audio cd */
		int trackno = find_trackno_from_filename(filename);
		if (trackno < firsttrackno || trackno > lasttrackno) {
			if (debug)
				printf("cdaudio-ng: \"%s\" is not our file\n", filename);
			return FALSE;
		}

		if (debug)
			printf("cdaudio-ng: \"%s\" is our file\n", filename);
		return TRUE;
	}
	else {
		if (debug)
			printf("cdaudio-ng: \"%s\" is not our file\n", filename);
		return FALSE;
	}
}

GList *cdaudio_scan_dir(gchar *dirname)
{
	if (debug)
		printf("cdaudio-ng: cdaudio_scan_dir(\"%s\")\n", dirname);

		/* if the given dirname does not belong to us, we return NULL */
	if (strstr(dirname, CDDA_DEFAULT) == NULL) {
		if (debug)
			printf("cdaudio-ng: \"%s\" directory does not belong to us\n", dirname);
		return NULL;
	}

		/* find an available, audio capable, cd drive  */
	if (device != NULL && strlen(device) > 0) {
		pcdio = cdio_open(device, DRIVER_UNKNOWN);
		if (pcdio == NULL) {
			fprintf(stderr, "cdaudio-ng: failed to open cd device \"%s\"\n", device);
			return NULL;
		}
	}
	else {
		char **ppcd_drives = cdio_get_devices_with_cap(NULL, CDIO_FS_AUDIO, false);
		pcdio = NULL;
		if (ppcd_drives != NULL && *ppcd_drives != NULL) { /* we have at least one audio capable cd drive */
			pcdio = cdio_open(*ppcd_drives, DRIVER_UNKNOWN);
			if (pcdio == NULL) {
				fprintf(stderr, "cdaudio-ng: failed to open cd\n");
				cleanup_on_error();
				return NULL;
			}
			if (debug)
				printf("cdaudio-ng: found cd drive \"%s\" with audio capable media\n", *ppcd_drives);
		}
		else {
			fprintf(stderr, "cdaudio-ng: unable find or access a cdda capable drive\n");
			cleanup_on_error();
			return NULL;
		}
		cdio_free_device_list(ppcd_drives);
	}

		/* limit read speed */
	if (limitspeed > 0 && use_dae) {
		if (debug)
			printf("cdaudio-ng: setting drive speed limit to %dx\n", limitspeed);
		if (cdio_set_speed(pcdio, limitspeed) != DRIVER_OP_SUCCESS)
			fprintf(stderr, "cdaudio-ng: failed to set drive speed to %dx\n", limitspeed);
	}

		/* get general track initialization */
	cdrom_drive_t *pcdrom_drive = cdio_cddap_identify_cdio(pcdio, 1, NULL);	// todo : check return / NULL
	firsttrackno = cdio_get_first_track_num(pcdrom_drive->p_cdio);
	lasttrackno = cdio_get_last_track_num(pcdrom_drive->p_cdio);
	if (firsttrackno == CDIO_INVALID_TRACK || lasttrackno == CDIO_INVALID_TRACK) {
		fprintf(stderr, "cdaudio-ng: failed to retrieve first/last track number\n");
		cleanup_on_error();
		return NULL;
	}
	if (debug)
		printf("cdaudio-ng: first track is %d and last track is %d\n", firsttrackno, lasttrackno);

	if (trackinfo != NULL) /* if a previously allocated track information exists, we free it */
		free(trackinfo);
	trackinfo = (trackinfo_t *) malloc(sizeof(trackinfo_t) * (lasttrackno + 1));
	int trackno;

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
			fprintf(stderr, "cdaudio-ng: failed to retrieve stard/end lsn for track %d\n", trackno);
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
			fprintf(stderr, "cdaudio-ng: failed to create the cddb connection\n");
		else {
			if (debug)
				printf("cdaudio-ng: getting cddb info\n");

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
				fprintf(stderr, "cdaudio-ng: failed to query the cddb server: %s\n", cddb_error_str(cddb_errno(pcddb_conn)));
				cddb_disc_destroy(pcddb_disc);
				pcddb_disc = NULL;
			}
			else {
				if (debug)
					printf("cdaudio-ng: discid = %X, category = \"%s\"\n", cddb_disc_get_discid(pcddb_disc), cddb_disc_get_category_str(pcddb_disc));

				cddb_read(pcddb_conn, pcddb_disc);
				if (cddb_errno(pcddb_conn) != CDDB_ERR_OK) {
					fprintf(stderr, "cdaudio-ng: failed to read the cddb info: %s\n", cddb_error_str(cddb_errno(pcddb_conn)));
					cddb_disc_destroy(pcddb_disc);
					pcddb_disc = NULL;
				}
				else {
					if (debug)
						printf("cdaudio-ng: we have got the cddb info\n");

					strcpy(trackinfo[0].performer, cddb_disc_get_artist(pcddb_disc));
					strcpy(trackinfo[0].name, cddb_disc_get_title(pcddb_disc));
					strcpy(trackinfo[0].genre, cddb_disc_get_genre(pcddb_disc));
				}
			}
		}
	}

		/* adding trackinfo[0] information (the disc) */
	if (use_cdtext) {
		if (debug)
			printf("cdaudio-ng: getting cd-text information for disc\n");
		cdtext_t *pcdtext = cdio_get_cdtext(pcdrom_drive->p_cdio, 0);
		if (pcdtext == NULL || pcdtext->field[CDTEXT_TITLE] == NULL) {
			if (debug)
				printf("cdaudio-ng: no cd-text available for disc\n");
		}
		else {
			strcpy(trackinfo[0].performer, pcdtext->field[CDTEXT_PERFORMER] != NULL ? pcdtext->field[CDTEXT_PERFORMER] : "");
			strcpy(trackinfo[0].name, pcdtext->field[CDTEXT_TITLE] != NULL ? pcdtext->field[CDTEXT_TITLE] : "");
			strcpy(trackinfo[0].genre, pcdtext->field[CDTEXT_GENRE] != NULL ? pcdtext->field[CDTEXT_GENRE] : "");
		}
	}

		/* add track "file" names to the list */
	GList *list = NULL;
	for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
		list = g_list_append(list, g_strdup_printf("track%02u.cda", trackno));
		cdtext_t *pcdtext = NULL;
		if (use_cdtext) {
			if (debug)
				printf("cdaudio-ng: getting cd-text information for track %d\n", trackno);
			pcdtext = cdio_get_cdtext(pcdrom_drive->p_cdio, trackno);
			if (pcdtext == NULL || pcdtext->field[CDTEXT_PERFORMER] == NULL) {
				if (debug)
					printf("cdaudio-ng: no cd-text available for track %d\n", trackno);
				pcdtext = NULL;
			}
		}

		if (pcdtext != NULL) {
			strcpy(trackinfo[trackno].performer, pcdtext->field[CDTEXT_PERFORMER] != NULL ? pcdtext->field[CDTEXT_PERFORMER] : "");
			strcpy(trackinfo[trackno].name, pcdtext->field[CDTEXT_TITLE] != NULL ? pcdtext->field[CDTEXT_TITLE] : "");
			strcpy(trackinfo[trackno].genre, pcdtext->field[CDTEXT_GENRE] != NULL ? pcdtext->field[CDTEXT_GENRE] : "");
		}
		else
			if (pcddb_disc != NULL) {
				cddb_track_t *pcddb_track = cddb_disc_get_track(pcddb_disc, trackno - 1);
				strcpy(trackinfo[trackno].performer, cddb_track_get_artist(pcddb_track));
				strcpy(trackinfo[trackno].name, cddb_track_get_title(pcddb_track));
				strcpy(trackinfo[trackno].genre, cddb_disc_get_genre(pcddb_disc));
			}
			else {
				strcpy(trackinfo[trackno].performer, "");
				strcpy(trackinfo[trackno].name, "");
				strcpy(trackinfo[trackno].genre, "");
			}

		if (strlen(trackinfo[trackno].name) == 0)
			sprintf(trackinfo[trackno].name, "CD Audio Track %02u", trackno);

	}

	if (debug) {
		printf("cdaudio-ng: disc has : performer = \"%s\", name = \"%s\", genre = \"%s\"\n",
			   trackinfo[0].performer, trackinfo[0].name, trackinfo[0].genre);
		for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
			printf("cdaudio-ng: track %d has : performer = \"%s\", name = \"%s\", genre = \"%s\", startlsn = %d, endlsn = %d\n",
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
	if (debug)
		printf("cdaudio-ng: cdaudio_play_file(\"%s\")\n", pinputplayback->filename);

	pglobalinputplayback = pinputplayback;

	if (trackinfo == NULL) {
		if (debug)
			printf("cdaudio-ng: no cd information, scanning\n");
		cdaudio_scan_dir(CDDA_DEFAULT);
	}

	if (cdio_get_media_changed(pcdio)) {
		if (debug)
			printf("cdaudio-ng: cd changed, rescanning\n");
		cdaudio_scan_dir(CDDA_DEFAULT);
	}

	int trackno = find_trackno_from_filename(pinputplayback->filename);
	if (trackno < firsttrackno || trackno > lasttrackno) {
		fprintf(stderr, "cdaudio-ng: trackno %d is out of range [%d..%d]\n", trackno, firsttrackno, lasttrackno);
		cleanup_on_error();
		return;
	}

	pinputplayback->playing = TRUE;
	playing_track = trackno;
	is_paused = FALSE;

	if (use_dae) {
		if (debug)
			printf("cdaudio-ng: using digital audio extraction\n");

		if (pdae_params != NULL) {
			fprintf(stderr, "cdaudio-ng: dae playback seems to be already started\n");
			return;
		}

		if (pinputplayback->output->open_audio(FMT_S16_LE, 44100, 2) == 0) {
			fprintf(stderr, "cdaudio-ng: failed open audio output\n");
			cleanup_on_error();
			return;
		}

		/*
		if (debug)
			printf("cdaudio-ng: starting dae thread...\n");
		*/
		pdae_params = (dae_params_t *) malloc(sizeof(dae_params_t));
		pdae_params->startlsn = trackinfo[trackno].startlsn;
		pdae_params->endlsn = trackinfo[trackno].endlsn;
		pdae_params->pplayback = pinputplayback;
		pdae_params->seektime = -1;
		pdae_params->currlsn = trackinfo[trackno].startlsn;
		pdae_params->thread = g_thread_self();
		dae_play_loop(pdae_params);
	}
	else {
		if (debug)
			printf("cdaudio-ng: not using digital audio extraction\n");

		msf_t startmsf, endmsf;
		cdio_lsn_to_msf(trackinfo[trackno].startlsn, &startmsf);
		cdio_lsn_to_msf(trackinfo[trackno].endlsn, &endmsf);
		if (cdio_audio_play_msf(pcdio, &startmsf, &endmsf) != DRIVER_OP_SUCCESS) {
			fprintf(stderr, "cdaudio-ng: failed to play analog audio cd\n");
			cleanup_on_error();
			return;
		}
	}

	char *title = xmms_get_titlestring(xmms_get_gentitle_format(), create_tuple_from_trackinfo(pinputplayback->filename));

	inputplugin.set_info(title, calculate_track_length(trackinfo[trackno].startlsn, trackinfo[trackno].endlsn), 128000, 44100, 2);
	free(title);
}

void cdaudio_stop(InputPlayback *pinputplayback)
{
	if (debug)
		printf("cdaudio-ng: cdaudio_stop(\"%s\")\n", pinputplayback != NULL ? pinputplayback->filename : "N/A");

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
			free(pdae_params);
			pdae_params = NULL;
		}
	}
	else {
		if (cdio_audio_stop(pcdio) != DRIVER_OP_SUCCESS) {
			fprintf(stderr, "cdaudio-ng: failed to stop analog cd\n");
			return;
		}
	}
}

void cdaudio_pause(InputPlayback *pinputplayback, gshort paused)
{
	if (debug)
		printf("cdaudio-ng: cdaudio_pause(\"%s\", %d)\n", pinputplayback->filename, paused);

	if (!is_paused) {
		is_paused = TRUE;
		if (!use_dae)
			if (cdio_audio_pause(pcdio) != DRIVER_OP_SUCCESS) {
				fprintf(stderr, "cdaudio-ng: failed to pause analog cd\n");
				cleanup_on_error();
				return;
			}
	}
	else {
		is_paused = FALSE;
		if (!use_dae)
			if (cdio_audio_resume(pcdio) != DRIVER_OP_SUCCESS) {
				fprintf(stderr, "cdaudio-ng: failed to resume analog cd\n");
				cleanup_on_error();
				return;
			}
	}
}

void cdaudio_seek(InputPlayback *pinputplayback, gint time)
{
	if (debug)
		printf("cdaudio-ng: cdaudio_seek(\"%s\", %d)\n", pinputplayback->filename, time);

	if (playing_track == -1)
		return;

	if (use_dae) {
		if (pdae_params != NULL) {
			pdae_params->seektime = time * 1000;
		}
	}
	else {
		int newstartlsn = trackinfo[playing_track].startlsn + time * 75;
		msf_t startmsf, endmsf;
		cdio_lsn_to_msf(newstartlsn, &startmsf);
		cdio_lsn_to_msf(trackinfo[playing_track].endlsn, &endmsf);

		if (cdio_audio_play_msf(pcdio, &startmsf, &endmsf) != DRIVER_OP_SUCCESS) {
			fprintf(stderr, "cdaudio-ng: failed to play analog cd\n");
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
			fprintf(stderr, "cdaudio-ng: failed to read analog cd subchannel\n");
			cleanup_on_error();
			return 0;
		}
		int currlsn = cdio_msf_to_lsn(&subchannel.abs_addr);

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
			fprintf(stderr, "cdaudio-ng: failed to retrieve analog cd volume\n");
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
	if (debug)
		printf("cdaudio-ng: cdaudio_set_volume(%d, %d)\n", l, r);

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
	if (debug)
		printf("cdaudio-ng: cdaudio_cleanup()\n");

	libcddb_shutdown();

	if (pcdio!= NULL) {
		if (playing_track != -1 && !use_dae)
			cdio_audio_stop(pcdio);
		cdio_destroy(pcdio);
		pcdio = NULL;
	}
	if (trackinfo != NULL) {
		free(trackinfo);
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
	bmp_cfg_db_set_string(db, "CDDA", "device", device);
	bmp_cfg_db_set_bool(db, "CDDA", "debug", debug);
	bmp_cfg_db_close(db);
}

void cdaudio_get_song_info(gchar *filename, gchar **title, gint *length)
{
	if (debug)
		printf("cdaudio-ng: cdaudio_get_song_info(\"%s\")\n", filename);

	int trackno = find_trackno_from_filename(filename);

	*title = xmms_get_titlestring(xmms_get_gentitle_format(), create_tuple_from_trackinfo(filename));
	*length = calculate_track_length(trackinfo[trackno].startlsn, trackinfo[trackno].endlsn);
}

TitleInput *cdaudio_get_song_tuple(gchar *filename)
{
	if (debug)
		printf("cdaudio-ng: cdaudio_get_song_tuple(\"%s\")\n", filename);

	return create_tuple_from_trackinfo(filename);
}


	/* auxiliar functions */

TitleInput *create_tuple_from_trackinfo(char *filename)
{
	TitleInput *tuple = bmp_title_input_new();
	int trackno = find_trackno_from_filename(filename);

	if (trackno < firsttrackno || trackno > lasttrackno)
		return NULL;

	tuple->performer = strlen(trackinfo[trackno].performer) > 0 ? g_strdup(trackinfo[trackno].performer) : NULL;
	tuple->album_name = strlen(trackinfo[0].name) > 0 ? g_strdup(trackinfo[0].name) : NULL;
	tuple->track_name = strlen(trackinfo[trackno].name) > 0 ? g_strdup(trackinfo[trackno].name) : NULL;
	tuple->track_number = trackno;
	tuple->file_name = g_strdup(basename(filename));
	tuple->file_path = g_strdup(basename(filename));
	tuple->file_ext = g_strdup("cda");
	tuple->length = calculate_track_length(trackinfo[trackno].startlsn, trackinfo[trackno].endlsn);
	tuple->genre = strlen(trackinfo[trackno].genre) > 0 ? g_strdup(trackinfo[trackno].genre) : NULL;
	//tuple->year = 0; todo: set the year

	return tuple;
}

void dae_play_loop(dae_params_t *pdae_params)
{
	unsigned char *buffer = (unsigned char *) malloc(CDDA_DAE_FRAMES * CDIO_CD_FRAMESIZE_RAW);

	if (debug)
		printf("cdaudio-ng: dae started\n");
	cdio_lseek(pcdio, pdae_params->startlsn * CDIO_CD_FRAMESIZE_RAW, SEEK_SET);

	gboolean output_paused = FALSE;
	int read_error_counter = 0;

	while (pdae_params->pplayback->playing) {
			/* handle pause status */
		if (is_paused) {
			if (!output_paused) {
				if (debug)
					printf("cdaudio-ng: playback was not paused, pausing\n");
				pdae_params->pplayback->output->pause(TRUE);
				output_paused = TRUE;
			}
			usleep(1000);
			continue;
		}
		else {
			if (output_paused) {
				if (debug)
					printf("cdaudio-ng: playback was paused, resuming\n");
				pdae_params->pplayback->output->pause(FALSE);
				output_paused = FALSE;
			}
		}

			/* check if we have to seek */
		if (pdae_params->seektime != -1) {
			if (debug)
				printf("cdaudio-ng: requested seek to %d ms\n", pdae_params->seektime);
			int newlsn = pdae_params->startlsn + pdae_params->seektime * 75 / 1000;
			cdio_lseek(pcdio, newlsn * CDIO_CD_FRAMESIZE_RAW, SEEK_SET);
			pdae_params->pplayback->output->flush(pdae_params->seektime);
			pdae_params->currlsn = newlsn;
			pdae_params->seektime = -1;
		}

			/* compute the actual number of sectors to read */
		int lsncount = CDDA_DAE_FRAMES <= (pdae_params->endlsn - pdae_params->currlsn + 1) ? CDDA_DAE_FRAMES : (pdae_params->endlsn - pdae_params->currlsn + 1);
			/* check too see if we have reached the end of the song */
		if (lsncount <= 0)
			break;

		if (cdio_read_audio_sectors(pcdio, buffer, pdae_params->currlsn, lsncount) != DRIVER_OP_SUCCESS) {
			fprintf(stderr, "cdaudio-ng: failed to read audio sector\n");
			read_error_counter++;
			if (read_error_counter >= 2) {
				fprintf(stderr, "cdaudio-ng: this cd can no longer be played, stopping\n");
				break;
			}
		}
		else
			read_error_counter = 0;

		int remainingbytes = lsncount * CDIO_CD_FRAMESIZE_RAW;
		unsigned char *bytebuff = buffer;
		while (pdae_params->pplayback->playing && remainingbytes > 0 && pdae_params->seektime == -1) {
				/* compute the actual number of bytes to play */
			int bytecount = CDIO_CD_FRAMESIZE_RAW <= remainingbytes ? CDIO_CD_FRAMESIZE_RAW : remainingbytes;
				/* wait until the output buffer has enough room */
			while (pdae_params->pplayback->playing && pdae_params->pplayback->output->buffer_free() < bytecount && pdae_params->seektime == -1)
				usleep(1000);
				/* play the sound :) */
			if (pdae_params->pplayback->playing && pdae_params->seektime == -1)
				produce_audio(pdae_params->pplayback->output->written_time(), FMT_S16_LE, 2, bytecount, bytebuff, &pdae_params->pplayback->playing);
			remainingbytes -= bytecount;
			bytebuff += bytecount;
		}
		pdae_params->currlsn += lsncount;
	}
	if (debug)
		printf("cdaudio-ng: dae ended\n");

	pdae_params->pplayback->playing = FALSE;
	pdae_params->pplayback->output->close_audio();
	is_paused = FALSE;

	pdae_params->pplayback->output->close_audio();
	free(buffer);
}

int calculate_track_length(int startlsn, int endlsn)
{
	return ((endlsn - startlsn + 1) * 1000) / 75;
}

int find_trackno_from_filename(char *filename)
{
	if ((filename == NULL) || strlen(filename) <= 6)
		return -1;

	char tracknostr[3];
	strncpy(tracknostr, filename + strlen(filename) - 6, 2);
	tracknostr[2] = '\0';
	return strtol(tracknostr, NULL, 10);
}

void cleanup_on_error()
{
	if (playing_track != -1) {
		playing_track = -1;
		playback_stop();
	}

	if (trackinfo != NULL) {
		free(trackinfo);
		trackinfo = NULL;
	}
}

