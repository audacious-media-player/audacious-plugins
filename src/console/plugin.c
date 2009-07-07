#include <audacious/plugin.h>

extern void console_init(void);
extern void console_aboutbox(void);
extern void console_cfg_ui(void);
extern void play_file(InputPlayback *playback);
extern void console_stop(InputPlayback *playback);
extern void console_pause(InputPlayback * playback, gshort p);
extern void seek(InputPlayback * data, gint time);
extern Tuple *get_song_tuple(gchar *path);
extern Tuple *probe_for_tuple(gchar *filename, VFSFile *fd);

const gchar *gme_fmts[] = { "ay", "gbs", "gym", "hes", "kss", "nsf", "nsfe", 
		      "sap", "spc", "vgm", "vgz", NULL };

InputPlugin console_ip =
{
	.description = (gchar *)"Game console audio module decoder",
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

