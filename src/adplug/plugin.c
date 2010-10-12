#include <audacious/plugin.h>

#include "adplug-xmms.h"

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
  .probe_for_tuple = adplug_get_tuple,
  .is_our_file_from_vfs = adplug_is_our_fd,
  .vfs_extensions = fmts,
};

InputPlugin *adplug_iplist[] = { &adplug_ip, NULL };

DECLARE_PLUGIN(adplug, NULL, NULL, adplug_iplist, NULL, NULL, NULL, NULL,NULL);
