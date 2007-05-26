
/*
	todo: 
		- move stuff into cdaudio-ng.h
		- vis_pcm...?!
		- limit cd read speed
		- cddb
		- dialogs
		- remove //'s & todo's
		- additional comments
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

#include <glib.h>

#include <audacious/i18n.h>
#include <audacious/configdb.h>
#include <audacious/plugin.h>
#include <audacious/util.h>
#include <audacious/output.h>

#define DEF_STRING_LEN	256
#define CDROM_DIR		"cdda://default"
#define CDDA_DAE_FRAMES	8


typedef struct {

	char				performer[DEF_STRING_LEN];
	char				name[DEF_STRING_LEN];
	char				genre[DEF_STRING_LEN];
	lsn_t				startlsn;
	lsn_t				endlsn;

} trackinfo_t;

typedef struct {

	lsn_t				startlsn;
	lsn_t				endlsn;
	lsn_t				currlsn;
	lsn_t				seektime;	/* in miliseconds */
	InputPlayback		*pplayback;
	GThread				*thread;

} dae_params_t;


static int				firsttrackno = -1;
static int				lasttrackno = -1;
static CdIo_t			*pcdio = NULL;
static trackinfo_t		*trackinfo = NULL;
static char				album_name[DEF_STRING_LEN];
static gboolean			use_dae = TRUE;
static gboolean			is_paused = FALSE;
static int				playing_track = -1;
static dae_params_t		*pdae_params = NULL;


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
static void				cdaudio_file_info_box(gchar *filename);
static TitleInput		*cdaudio_get_song_tuple(gchar *filename);

static void				*dae_playing_thread_core(dae_params_t *pdae_params);
static int				calculate_track_length(int startlsn, int endlsn);
static int				find_trackno_from_filename(char *filename);
static void				cleanup_on_error();

/*
static int				calculate_digit_sum(int n);
static unsigned long	calculate_cddb_discid();
*/


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
	cdaudio_file_info_box,
	NULL,
	cdaudio_get_song_tuple
};

InputPlugin *cdaudio_iplist[] = { &inputplugin, NULL };

DECLARE_PLUGIN(cdaudio, NULL, NULL, cdaudio_iplist, NULL, NULL, NULL, NULL);


void cdaudio_init()
{
	if (!cdio_init()) {
		fprintf(stderr, "cdaudio-ng: failed to initialize cdio subsystem\n");
		cleanup_on_error();
		return;
	}
}

void cdaudio_about()
{
}

void cdaudio_configure()
{
}

gint cdaudio_is_our_file(gchar *filename)
{
	if ((filename != NULL) && strlen(filename) > 4 && (!strcasecmp(filename + strlen(filename) - 4, ".cda"))) {
			/* no CD information yet */
		if (pcdio == NULL) {
			printf("cdaudio-ng: no cd information, scanning\n");
			cdaudio_scan_dir(CDROM_DIR);
		}

			/* reload the cd information if the media has changed */
		if (cdio_get_media_changed(pcdio)) {
			printf("cdaudio-ng: cd changed, rescanning\n");
			cdaudio_scan_dir(CDROM_DIR);
		}

			/* check if the requested track actually exists on the current audio cd */
		int trackno = find_trackno_from_filename(filename);
		if (trackno < firsttrackno || trackno > lasttrackno)
			return FALSE;

		return TRUE;
	}
	else
		return FALSE;
}

GList *cdaudio_scan_dir(gchar *dirname)
{
		/* if the given dirname does not belong to us, we return NULL */
	if (strstr(dirname, CDROM_DIR) == NULL)
		return NULL;

		/* find the first available, audio capable, cd drive  */
	char **ppcd_drives = cdio_get_devices_with_cap(NULL, CDIO_FS_AUDIO, false);
	if (ppcd_drives != NULL) { /* we have at least one audio capable cd drive */
		pcdio = cdio_open(*ppcd_drives, DRIVER_UNKNOWN);
		if (pcdio == NULL) {
			fprintf(stderr, "cdaudio-ng: failed to open cd\n");
			cleanup_on_error();
			return NULL;
		}
	}
	else {
		fprintf(stderr, "cdaudio-ng: unable find or access a cdda capable drive\n");
		cleanup_on_error();
		return NULL;
	}
	cdio_free_device_list(ppcd_drives);

		/* get track information */
	cdrom_drive_t *pcdrom_drive = cdio_cddap_identify_cdio(pcdio, 1, NULL);	// todo : check return / NULL
	firsttrackno = cdio_get_first_track_num(pcdrom_drive->p_cdio);
	lasttrackno = cdio_get_last_track_num(pcdrom_drive->p_cdio);
	if (firsttrackno == CDIO_INVALID_TRACK || lasttrackno == CDIO_INVALID_TRACK) {
		fprintf(stderr, "cdaudio-ng: failed to retrieve first/last track number");
		cleanup_on_error();
		return NULL;
	}

		/* add track "file" names to the list */
	GList *list = NULL;
	if (trackinfo != NULL) /* if a previously allocated track information exists, we free it */
		free(trackinfo);
	trackinfo = (trackinfo_t *) malloc(sizeof(trackinfo_t) * (lasttrackno + 1));
	int trackno;
	for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
		list = g_list_append(list, g_strdup_printf("track%02u.cda", trackno));	
		cdtext_t *pcdtext = cdio_get_cdtext(pcdrom_drive->p_cdio, trackno);

		if (pcdtext != NULL) {
			strcpy(trackinfo[trackno].performer, pcdtext->field[CDTEXT_PERFORMER] != NULL ? pcdtext->field[CDTEXT_PERFORMER] : "");
			strcpy(trackinfo[trackno].name, pcdtext->field[CDTEXT_TITLE] != NULL ? pcdtext->field[CDTEXT_TITLE] : "");
			strcpy(trackinfo[trackno].genre, pcdtext->field[CDTEXT_GENRE] != NULL ? pcdtext->field[CDTEXT_GENRE] : "");
		}
		else {
			strcpy(trackinfo[trackno].performer, "");
			strcpy(trackinfo[trackno].name, "");
			strcpy(trackinfo[trackno].genre, "");
		}
		
		if (strlen(trackinfo[trackno].name) == 0)
			sprintf(trackinfo[trackno].name, "CD Audio Track %02u", trackno);

		trackinfo[trackno].startlsn = cdio_get_track_lsn(pcdrom_drive->p_cdio, trackno);
		trackinfo[trackno].endlsn = cdio_get_track_last_lsn(pcdrom_drive->p_cdio, trackno);
		
		if (trackinfo[trackno].startlsn == CDIO_INVALID_LSN || trackinfo[trackno].endlsn == CDIO_INVALID_LSN) {
			fprintf(stderr, "cdaudio-ng: failed to retrieve stard/end lsn for track %d\n", trackno);
			g_list_free(list);
			cleanup_on_error();
			return NULL;
		}
	}

	return list;
}

void cdaudio_play_file(InputPlayback *pinputplayback)
{	
	if (trackinfo == NULL) {
		printf("cdaudio-ng: no cd information, scanning\n");
		cdaudio_scan_dir(CDROM_DIR);
	}

	if (cdio_get_media_changed(pcdio)) {
		printf("cdaudio-ng: cd changed, rescanning\n");
		cdaudio_scan_dir(CDROM_DIR);
	}

	int trackno = find_trackno_from_filename(pinputplayback->filename);
	if (trackno < firsttrackno || trackno > lasttrackno) {
		fprintf(stderr, "cdaudio-ng: trackno %d should be between %d and %d\n", trackno, firsttrackno, lasttrackno);
		cleanup_on_error();
		return;
	}

	pinputplayback->playing = TRUE;
	playing_track = trackno;
	is_paused = FALSE;

	if (use_dae) {
		if (pdae_params != NULL) {
			fprintf(stderr, "cdaudio-ng: dae playback seems to be already started\n");
			return;
		}

		if (pinputplayback->output->open_audio(FMT_S16_LE, 44100, 2) == 0) {
			fprintf(stderr, "cdaudio-ng: failed open audio output\n");
			cleanup_on_error();
			return;
		}

		pdae_params = (dae_params_t *) malloc(sizeof(dae_params_t));
		pdae_params->startlsn = trackinfo[trackno].startlsn;
		pdae_params->endlsn = trackinfo[trackno].endlsn;
		pdae_params->pplayback = pinputplayback;
		pdae_params->seektime = -1;
		pdae_params->currlsn = trackinfo[trackno].startlsn;
		pdae_params->thread = g_thread_create((GThreadFunc) dae_playing_thread_core, pdae_params, TRUE, NULL);
	}
	else {
		msf_t startmsf, endmsf;
		cdio_lsn_to_msf(trackinfo[trackno].startlsn, &startmsf);
		cdio_lsn_to_msf(trackinfo[trackno].endlsn, &endmsf);
		if (cdio_audio_play_msf(pcdio, &startmsf, &endmsf) != DRIVER_OP_SUCCESS) {
			fprintf(stderr, "cdaudio-ng: failed to play analog audio cd\n");
			cleanup_on_error();
			return;
		}
	}

	char title[DEF_STRING_LEN];

	if (strlen(trackinfo[trackno].performer) > 0) {
		strcpy(title, trackinfo[trackno].performer);
		strcat(title, " - ");
	}
	else
		strcpy(title, "");
	strcat(title, trackinfo[trackno].name);

	inputplugin.set_info(title, calculate_track_length(trackinfo[trackno].startlsn, trackinfo[trackno].endlsn), 128000, 44100, 2);
}

void cdaudio_stop(InputPlayback *pinputplayback)
{ 
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
			cleanup_on_error();
			return;
		}
	}
}

void cdaudio_pause(InputPlayback *pinputplayback, gshort paused)
{
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
	if (playing_track == -1)
		return -1;

	if (!use_dae) {
		cdio_subchannel_t subchannel;
		if (cdio_audio_read_subchannel(pcdio, &subchannel) != DRIVER_OP_SUCCESS) {
			fprintf(stderr, "cdaudio-ng: failed to read analog cd subchannel\n");
			cleanup_on_error();
			return -1;
		}
		int currlsn = cdio_msf_to_lsn(&subchannel.abs_addr);

			/* check to see if we have reached the end of the song */
		if (currlsn == trackinfo[playing_track].endlsn) {
			cdaudio_stop(pinputplayback);
			return -1;
		}

		return calculate_track_length(trackinfo[playing_track].startlsn, currlsn);
	}
	else {
		if (pdae_params != NULL)
			return pinputplayback->output->output_time();
		else
			return -1;
	}
}

gint cdaudio_get_volume(gint *l, gint *r)
{
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
}

void cdaudio_get_song_info(gchar *filename, gchar **title, gint *length)
{
	fprintf(stderr, "DEBUG: get_song_info(\"%s\")\n", filename);
}

void cdaudio_file_info_box(gchar *filename)
{
	fprintf(stderr, "DEBUG: file_info_box(\"%s\")\n", filename);	
}

TitleInput *cdaudio_get_song_tuple(gchar *filename)
{
	TitleInput *tuple = bmp_title_input_new();

		/* return information about the requested track */
	int trackno = find_trackno_from_filename(filename);
	if (trackno < firsttrackno || trackno > lasttrackno)
		return NULL;

	tuple->performer = strlen(trackinfo[trackno].performer) > 0 ? g_strdup(trackinfo[trackno].performer) : NULL;
	tuple->album_name = strlen(album_name) > 0 ? g_strdup(album_name) : NULL;
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


	/* auxiliar functions */

/*
static int calculate_digit_sum(int n)
{
	int ret = 0;

	while (1) {
		ret += n % 10;
		n    = n / 10;
		if (n == 0)
			return ret;
	}
}
*/

/*
static unsigned long calculate_cddb_discid()
{
	int trackno, t, n = 0;
	msf_t startmsf;
	msf_t msf;
	
	for (trackno = firsttrackno; trackno <= lasttrackno; trackno++) {
		cdio_get_track_msf(pcdrom_drive->p_cdio, trackno, &msf);
		n += calculate_digit_sum(cdio_audio_get_msf_seconds(&msf));
	}
	
	cdio_get_track_msf(pcdrom_drive->p_cdio, 1, &startmsf);
	cdio_get_track_msf(pcdrom_drive->p_cdio, CDIO_CDROM_LEADOUT_TRACK, &msf);
	
	t = cdio_audio_get_msf_seconds(&msf) - cdio_audio_get_msf_seconds(&startmsf);
	
	return ((n % 0xFF) << 24 | t << 8 | (lasttrackno - firsttrackno + 1));
}
*/

void *dae_playing_thread_core(dae_params_t *pdae_params)
{
	unsigned char *buffer = (unsigned char *) malloc(CDDA_DAE_FRAMES * CDIO_CD_FRAMESIZE_RAW);

	cdio_lseek(pcdio, pdae_params->startlsn * CDIO_CD_FRAMESIZE_RAW, SEEK_SET);

	gboolean output_paused = FALSE;
	
	while (pdae_params->pplayback->playing) {
			/* handle pause status */
		if (is_paused) {
			if (!output_paused) {
				pdae_params->pplayback->output->pause(TRUE);
				output_paused = TRUE;
			}
			usleep(1000);
			continue;
		}
		else
			if (output_paused) {
				pdae_params->pplayback->output->pause(FALSE);
				output_paused = FALSE;
			}

			/* check if we have to seek */
		if (pdae_params->seektime != -1) {
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
			/* ok, that's it, we go on */
		}

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

	pdae_params->pplayback->playing = FALSE;
	playing_track = -1;
	is_paused = FALSE;

	pdae_params->pplayback->output->close_audio();
	free(buffer);

	g_thread_exit(NULL);
	return NULL;
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
	if (pcdio != NULL) {
		if (playing_track != -1 && !use_dae)
			cdio_audio_stop(pcdio);
	}
	if (trackinfo != NULL) {
		free(trackinfo);
		trackinfo = NULL;
	}
	playing_track = -1;
}
