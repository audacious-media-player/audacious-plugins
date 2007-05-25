
/*
	todo: 
		- if any cdio_* returns an error, stop playing immediately
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
#include <audacious/util.h>
#include <audacious/titlestring.h>
#include <audacious/output.h>

#define DEF_STRING_LEN	256


typedef struct {

	char				performer[DEF_STRING_LEN];
	char				name[DEF_STRING_LEN];
	char				genre[DEF_STRING_LEN];
	int					startlsn;
	int					endlsn;

} trackinfo_t;


static int				firsttrackno = -1;
static int				lasttrackno = -1;
static cdrom_drive_t	*pcdrom_drive = NULL;
static trackinfo_t		*trackinfo = NULL;
static char				album_name[DEF_STRING_LEN];
static gboolean			use_dao = FALSE;
static gboolean			is_paused = FALSE;
static int				playing_track = -1;


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

static int				calculate_track_length(int startlsn, int endlsn);
static int				find_trackno_from_filename(char *filename);

/*
static int				calculate_digit_sum(int n);
static unsigned long	calculate_cddb_discid();
*/


static InputPlugin inputplugin = {
	NULL,
	NULL,
	"Zither's CD Audio Plugin",
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
	cdio_init();
}

void cdaudio_about()
{
}

void cdaudio_configure()
{
}

gint cdaudio_is_our_file(gchar *filename)
{
	printf("is_our_file(\"%s\")\n", filename);
	if ((filename != NULL) && strlen(filename) > 4 && (!strcasecmp(filename + strlen(filename) - 4, ".cda"))) {
		if (pcdrom_drive == NULL) {		/* no CD information yet */
			printf("No CD information, rescanning\n");
			cdaudio_scan_dir("/mnt/cdrom");	// todo: :)
		}
		
		if (cdio_get_media_changed(pcdrom_drive->p_cdio)) {
			printf("CD changed, rescanning\n");
			cdaudio_scan_dir("/mnt/cdrom"); // todo: change the hardcoded path
		}
		
		return 1;
	}
	else
		return 0;
}

GList *cdaudio_scan_dir(gchar *dirname)
{
	printf("scan_dir(\"%s\")\n", dirname);
	
	if (strstr(dirname, "/mnt/cdrom") == NULL)	// todo: replace this with a more standardised string
		return NULL;
	
		/* find the first available, audio capable, cd drive  */
	char **ppcd_drives = cdio_get_devices_with_cap(NULL, CDIO_FS_AUDIO, false);
	if (ppcd_drives != NULL) { /* we have at least one audio capable cd drive */
		pcdrom_drive = cdio_cddap_identify(*ppcd_drives, 1, NULL);
	}
	else {
		printf("Unable find or access a CD-ROM drive with an audio CD in it.\n");
		return NULL;
	}
	cdio_free_device_list(ppcd_drives);

		/* get track information */
	firsttrackno = cdio_get_first_track_num(pcdrom_drive->p_cdio);
	lasttrackno = cdio_get_last_track_num(pcdrom_drive->p_cdio);

		/* add track "file" names to the list */
	GList *list = NULL;
	if (trackinfo != NULL)
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
	}

	return list;
}

void cdaudio_play_file(InputPlayback *pinputplayback)
{	
	printf("play_file(\"%s\")\n", pinputplayback->filename);
	
	if (trackinfo == NULL) {
		printf("No CD information, rescanning\n");
		cdaudio_scan_dir("/mnt/cdrom"); // todo: change the hardcoded path
	}

	if (cdio_get_media_changed(pcdrom_drive->p_cdio)) {
		printf("CD changed, rescanning\n");
		cdaudio_scan_dir("/mnt/cdrom"); // todo: change the hardcoded path
	}
	
	int trackno = find_trackno_from_filename(pinputplayback->filename);
	if (trackno < firsttrackno || trackno > lasttrackno)
		return;

	msf_t startmsf, endmsf;
	cdio_lsn_to_msf(trackinfo[trackno].startlsn, &startmsf);
	cdio_lsn_to_msf(trackinfo[trackno].endlsn, &endmsf);
	cdio_audio_play_msf(pcdrom_drive->p_cdio, &startmsf, &endmsf);

	pinputplayback->playing = TRUE;
	playing_track = trackno;
	
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
	printf("stop(\"%s\")\n", pinputplayback->filename);

	cdio_audio_stop(pcdrom_drive->p_cdio);
	pinputplayback->playing = FALSE;
	playing_track = -1;
}

void cdaudio_pause(InputPlayback *pinputplayback, gshort paused)
{
	if (!is_paused) {
		is_paused = TRUE;
		cdio_audio_pause(pcdrom_drive->p_cdio);
	}
	else {
		is_paused = FALSE;
		cdio_audio_resume(pcdrom_drive->p_cdio);
	}
}

void cdaudio_seek(InputPlayback *pinputplayback, gint time)
{
	printf("seek(%d)\n", time);
	if (playing_track == -1)
		return;

	int lsnoffs = (time * 75);
	int startlsn = trackinfo[playing_track].startlsn + lsnoffs;

	msf_t startmsf, endmsf;
	cdio_lsn_to_msf(startlsn, &startmsf);
	cdio_lsn_to_msf(trackinfo[playing_track].endlsn, &endmsf);
	cdio_audio_play_msf(pcdrom_drive->p_cdio, &startmsf, &endmsf);
}

gint cdaudio_get_time(InputPlayback *pinputplayback)
{
	if (playing_track == -1)
		return -1;

	cdio_subchannel_t subchannel;
	cdio_audio_read_subchannel(pcdrom_drive->p_cdio, &subchannel);
	int currentlsn = cdio_msf_to_lsn(&subchannel.abs_addr);

		/* check to see if we have reached the end of the song */
	if (currentlsn == trackinfo[playing_track].endlsn) {
		cdaudio_stop(pinputplayback);
		return -1;
	}

	int seconds = calculate_track_length(trackinfo[playing_track].startlsn, currentlsn);
	// printf("%d\n", seconds);
	return seconds;
}

gint cdaudio_get_volume(gint *l, gint *r)
{
	// printf("get_volume()\n");

	cdio_audio_volume_t volume;;
	cdio_audio_set_volume(pcdrom_drive->p_cdio, &volume);
	*l = volume.level[0];
	*r = volume.level[1];

	return 0;
}

gint cdaudio_set_volume(gint l, gint r)
{
	printf("set_volume(%d, %d)\n", l, r);

	cdio_audio_volume_t volume = {{l, r, 0, 0}};
	cdio_audio_set_volume(pcdrom_drive->p_cdio, &volume);

	return 0;
}

void cdaudio_cleanup()
{
	cdio_destroy(pcdrom_drive->p_cdio);
}

void cdaudio_get_song_info(gchar *filename, gchar **title, gint *length)
{
	printf("get_song_info(\"%s\")\n", filename);
}

void cdaudio_file_info_box(gchar *filename)
{
	
}

TitleInput *cdaudio_get_song_tuple(gchar *filename)
{
	printf("get_song_tuple(\"%s\")\n", filename);

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
