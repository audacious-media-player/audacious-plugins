#include <audacious/plugin.h>

void adplug_init(void);
void adplug_quit(void);
void adplug_about(void);
void adplug_config(void);
void adplug_stop(InputPlayback * data);
void adplug_play(InputPlayback * data);
void adplug_pause(InputPlayback * playback, short paused);
void adplug_seek(InputPlayback * data, int time);
int adplug_get_time(InputPlayback * data);
void adplug_song_info(char *filename, char **title, int *length);
void adplug_info_box(char *filename);
Tuple* adplug_get_tuple(char *filename);
int adplug_is_our_fd(gchar * filename, VFSFile * fd);

static const gchar *fmts[] =
    { "a2m", "adl", "amd", "bam", "cff", "cmf", "d00", "dfm", "dmo", "dro",
      "dtm", "hsc", "hsp", "ins", "jbm", "ksm", "laa", "lds", "m", "mad",
      "mkj", "msc", "rad", "raw", "rix", "rol", "s3m", "sa2", "sat", "sci",
      "sng", "wlf", "xad", "xsm",
      NULL };

InputPlugin adplug_ip = {
  .description = "AdPlug (AdLib Sound Player)",
  .init = adplug_init,
  .cleanup = adplug_quit,
  .about = adplug_about,
  .configure = adplug_config,
  .enabled = FALSE,
  .play_file = adplug_play,
  .stop = adplug_stop,
  .pause = adplug_pause,
  .seek = adplug_seek,
  .get_time = adplug_get_time,
  .get_song_info = adplug_song_info,
  .file_info_box = adplug_info_box,
  .get_song_tuple = adplug_get_tuple,
  .is_our_file_from_vfs = adplug_is_our_fd,
  .vfs_extensions = (gchar **)fmts,
};

InputPlugin *adplug_iplist[] = { &adplug_ip, NULL };

DECLARE_PLUGIN(adplug, NULL, NULL, adplug_iplist, NULL, NULL, NULL, NULL,NULL);
