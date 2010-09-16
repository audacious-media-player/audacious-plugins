#include <audacious/plugin.h>

void adplug_init(void);
void adplug_quit(void);
void adplug_about(void);
void adplug_config(void);
void adplug_stop(InputPlayback * data);
gboolean adplug_play(InputPlayback * data, const gchar * filename, VFSFile * file, gint start_time, gint stop_time, gboolean pause);
void adplug_pause(InputPlayback * playback, gshort paused);
void adplug_mseek (InputPlayback * playback, gulong time);
void adplug_info_box(const gchar *filename);
Tuple* adplug_get_tuple(const gchar *filename);
int adplug_is_our_fd(const gchar * filename, VFSFile * fd);

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
  .play = adplug_play,
  .stop = adplug_stop,
  .pause = adplug_pause,
  .mseek = adplug_mseek,
  .file_info_box = adplug_info_box,
  .get_song_tuple = adplug_get_tuple,
  .is_our_file_from_vfs = adplug_is_our_fd,
  .vfs_extensions = fmts,
};

InputPlugin *adplug_iplist[] = { &adplug_ip, NULL };

DECLARE_PLUGIN(adplug, NULL, NULL, adplug_iplist, NULL, NULL, NULL, NULL,NULL);
