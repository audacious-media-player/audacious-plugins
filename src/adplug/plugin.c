#include <audacious/i18n.h>
#include <audacious/plugin.h>

#include "adplug-xmms.h"
static const char * fmts[] =
    { "a2m", "adl", "amd", "bam", "cff", "cmf", "d00", "dfm", "dmo", "dro",
      "dtm", "hsc", "hsp", "ins", "jbm", "ksm", "laa", "lds", "m", "mad",
      "mkj", "msc", "rad", "raw", "rix", "rol", "s3m", "sa2", "sat", "sci",
      "sng", "wlf", "xad", "xsm",
      NULL };

AUD_INPUT_PLUGIN
(
  .name = N_("AdPlug (AdLib Player)"),
  .domain = PACKAGE,
  .init = adplug_init,
  .cleanup = adplug_quit,
  .play = adplug_play,
  .stop = adplug_stop,
  .pause = adplug_pause,
  .mseek = adplug_mseek,
  .probe_for_tuple = adplug_get_tuple,
  .is_our_file_from_vfs = adplug_is_our_fd,
  .extensions = fmts,
)
