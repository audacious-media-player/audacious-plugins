#include <audacious/plugin.h>

void console_init(void);
void console_aboutbox(void);
void console_cfg_ui(void);
void play_file(InputPlayback *playback);
void console_stop(InputPlayback *playback);
void console_pause(InputPlayback * playback, gshort p);
void seek(InputPlayback * data, gint time);
Tuple *get_song_tuple(gchar *path);
Tuple *probe_for_tuple(gchar *filename, VFSFile *fd);

static const gchar *gme_fmts[] = { "ay", "gbs", "gym", "hes", "kss", "nsf", "nsfe", 
		      "sap", "spc", "vgm", "vgz", NULL };

static InputPlugin console_ip =
{
	.description = "Game console audio module decoder",
	.init = console_init,
	.about = console_aboutbox,
	.configure = console_cfg_ui,
	.enabled = FALSE,
	.play_file = play_file,
	.stop = console_stop,
	.pause = console_pause,
	.seek = seek,
	.get_song_tuple = get_song_tuple,
	.vfs_extensions = (gchar **)gme_fmts,
	.probe_for_tuple = probe_for_tuple,
	.have_subtune = TRUE
};

InputPlugin *console_iplist[] = { &console_ip, NULL };

SIMPLE_INPUT_PLUGIN(console, console_iplist);
