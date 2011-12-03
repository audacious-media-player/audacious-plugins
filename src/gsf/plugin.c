/*
 * highlyadvanced for audacious: GBA (GSF) chiptune plugin
 * Copyright (c) 2005-2007 William Pitcock <nenolod@sacredspiral.co.uk>
 *
 * highlyadvanced engine:
 * Copyright (c) 2005-2007 CaitSith2
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

#include <glib.h>

#include <gtk/gtk.h>

#include <audacious/plugin.h>
#include <libaudgui/libaudgui-gtk.h>

#include "VBA/psftag.h"
#include "gsf.h"

int defvolume=1000;
int relvolume=1000;
int TrackLength=0;
int FadeLength=0;
int IgnoreTrackLength, DefaultLength=150000;
int playforever=0;
int fileoutput=0;
int TrailingSilence=1000;
int DetectSilence=0, silencedetected=0, silencelength=5;

int cpupercent=0, sndSamplesPerSec, sndNumChannels;
int sndBitsPerSample=16;
int didseek = 0;
double playtime = 0;

int deflen=120,deffade=4;

extern unsigned short soundFinalWave[1470];
extern int soundBufferLen;

extern char soundEcho;
extern char soundLowPass;
extern char soundReverse;
extern char soundQuality;

double decode_pos_ms; // current decoding position, in milliseconds
int seek_needed; // if != -1, it is the point that the decode thread should seek to, in ms.

extern InputPlugin gsf_ip;

static gchar *lastfn;
static gboolean stop_flag = FALSE;

GThread *gsf_emulthread;

int LengthFromString(const char * timestring);
int VolumeFromString(const char * volumestring);

static int gsf_is_our_fd(const gchar *filename, VFSFile *file)
{
        gchar magic[4];
	const gchar *tmps;

        // Filter out gsflib [we use them, but we can't play them]
        static const gchar *teststr = "gsflib";
        if (strlen(teststr) < strlen(filename))
	{
                tmps = filename + strlen(filename);
                tmps -= strlen(teststr);
                if (!strcasecmp(tmps, teststr))
                        return 0;
        }

	vfs_fread(magic,1,4,file);

	if (!memcmp(magic,"PSF\"",4))
		return 1;

        return 0;
}

static gboolean gsf_play(InputPlayback *playback, const gchar * filename,
     VFSFile * file, gint start_time, gint stop_time, gboolean pause);

static void gsf_stop(InputPlayback *playback)
{
	stop_flag = TRUE;

	g_thread_join(gsf_emulthread);

	playback->output->close_audio();

	if (lastfn != NULL)
	{
		g_free(lastfn);
		lastfn = NULL;
	}
}

static Tuple *gsf_get_song_tuple(const gchar *filename, VFSFile *fd)
{
	char tag[50001];
	char tmp_str[256];
	gchar *f;
	const gchar *fn;

	f = g_filename_from_uri(filename, NULL, NULL);
	fn = f ? f : filename;

	Tuple *ti;

	ti = tuple_new_from_filename(fn);

	psftag_readfromfile((void*)tag, fn);

	if (!psftag_getvar(tag, "title", tmp_str, sizeof(tmp_str)-1)) {
		tuple_set_str(ti, FIELD_TITLE, NULL, tmp_str);
	}

	if (!psftag_getvar(tag, "artist", tmp_str, sizeof(tmp_str)-1)) {
		tuple_set_str(ti, FIELD_ARTIST, NULL, tmp_str);
	}

	if (!psftag_getvar(tag, "game", tmp_str, sizeof(tmp_str)-1)) {
		tuple_set_str(ti, FIELD_ALBUM, NULL, tmp_str);
	}

	if (!psftag_getvar(tag, "year", tmp_str, sizeof(tmp_str)-1)) {
		tuple_set_str(ti, FIELD_DATE, NULL, tmp_str);
	}

	if (!psftag_getvar(tag, "copyright", tmp_str, sizeof(tmp_str)-1)) {
		tuple_set_str(ti, FIELD_COPYRIGHT, NULL, tmp_str);
	}

	if (!psftag_getvar(tag, "gsfby", tmp_str, sizeof(tmp_str)-1)) {
		tuple_set_str(ti, -1, "gsfby", tmp_str);
	}

	if (!psftag_getvar(tag, "tagger", tmp_str, sizeof(tmp_str)-1)) {
		tuple_set_str(ti, -1, "tagger", tmp_str);
	}

	if (!psftag_raw_getvar(tag, "length", tmp_str, sizeof(tmp_str)-1)) {
		tuple_set_int(ti, FIELD_LENGTH, NULL, LengthFromString(tmp_str) + FadeLength);
	}

	if (!psftag_getvar(tag, "comment", tmp_str, sizeof(tmp_str)-1)) {
		tuple_set_str(ti, FIELD_COMMENT, NULL, tmp_str);
	}

	tuple_set_str(ti, FIELD_CODEC, NULL, "GameBoy Advanced Audio (GSF)");
	tuple_set_str(ti, FIELD_QUALITY, NULL, "sequenced");

	g_free(f);

	return ti;
}

static void gsf_pause(InputPlayback *playback, gboolean p)
{
	playback->output->pause(p);
}

static void gsf_mseek(InputPlayback *playback, gint time)
{
	seek_needed = time;
}

static void gsf_about(void)
{
#if 0
	static GtkWidget *widget = NULL;

	audgui_simple_message(&widget, GTK_MESSAGE_INFO, "About Highly Advanced for Audacious",
			"Highly Advanced for Audacious\nversion: " PACKAGE_VERSION "\n\nAudacious plugin: (C) 2006-2010 William Pitcock <nenolod -at- atheme.org>\n\n"
			"Built from the Highly Advanced emulation core, version " HA_VERSION_STR);
#endif
}

static const gchar *gsf_fmts[] = { "gsf", "minigsf", NULL };

AUD_INPUT_PLUGIN
(
	.description = "Highly Advanced for Audacious",
	.about = gsf_about,
	.play = gsf_play,
	.stop = gsf_stop,
	.pause = gsf_pause,
	.mseek = gsf_mseek,
	.probe_for_tuple = gsf_get_song_tuple,
	.is_our_file_from_vfs = gsf_is_our_fd,
	.extensions = gsf_fmts,
)

static InputPlayback *_playback = NULL;

void end_of_track(void)
{
	stop_flag = TRUE;
}

void writeSound(void)
{
	int tmp;
	int ret = soundBufferLen;
	static int countdown = 20;

	decode_pos_ms += (ret/(2*sndNumChannels) * 1000) / (float)sndSamplesPerSec;

	_playback->output->write_audio(soundFinalWave, soundBufferLen);

	if (--countdown == 0)
	{
		_playback->set_params(_playback, cpupercent * 1000, 44100, 2);
		countdown = 20;
	}

	/* is seek request complete? (-2) */
	if (seek_needed == -2)
	{
		_playback->output->flush(decode_pos_ms);
		seek_needed = -1;
	}

	if (lastfn != NULL && (seek_needed != -1))	//if a seek is initiated
	{
	        if (seek_needed < decode_pos_ms)	//if we are asked to seek backwards.  we have to start from the beginning
		{
			GSFClose();
			GSFRun(lastfn);
			decode_pos_ms = 0;
		}
	}
}

static gboolean gsf_play(InputPlayback *playback, const gchar * filename,
	VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
	int r, tmp, fi, random=0;
	char Buffer[1024];
	char length_str[256], fade_str[256], volume[256], title_str[256];
	char tmp_str[256];
	gchar *f, *fn;

	soundLowPass = 0;
	soundEcho = 0;
	soundQuality = 0;

	DetectSilence=1;
	silencelength=5;
	IgnoreTrackLength=0;
	DefaultLength=150000;
	TrailingSilence=1000;
	playforever=0;

	stop_flag = FALSE;

	_playback = playback;
	gsf_emulthread = g_thread_self();
	playback->set_pb_ready(playback);

	f = g_filename_from_uri(filename, NULL, NULL);
	fn = f ? f : filename;

	Tuple *ti = gsf_get_song_tuple(fn, file);

	r = GSFRun(fn);
	if (!r)
		return FALSE;

	lastfn = g_strdup(fn);

	int i = playback->output->open_audio(FMT_S16_LE, sndSamplesPerSec, sndNumChannels);

	gint length = tuple_get_int(ti, FIELD_LENGTH, NULL);

	playback->set_params(playback, 0, 44100, 2);

	decode_pos_ms = 0;
	seek_needed = -1;
	TrailingSilence=1000;

	while(!stop_flag)
		EmulationLoop();

	GSFClose();

	while (playback->output->buffer_playing())
		g_usleep(10000);

	playback->output->close_audio();

	g_free(lastfn);
	lastfn = NULL;

	g_free(f);

	return TRUE;
}
