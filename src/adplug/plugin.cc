#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>

#include "adplug-xmms.h"
static const char * fmts[] =
    { "a2m", "adl", "amd", "bam", "cff", "cmf", "d00", "dfm", "dmo", "dro",
      "dtm", "hsc", "hsp", "ins", "jbm", "ksm", "laa", "lds", "m", "mad",
      "mkj", "msc", "rad", "raw", "rix", "rol", "s3m", "sa2", "sat", "sci",
      "sng", "wlf", "xad", "xsm",
      nullptr };

#define AUD_PLUGIN_NAME        N_("AdPlug (AdLib Player)")
#define AUD_PLUGIN_INIT        adplug_init
#define AUD_PLUGIN_CLEANUP     adplug_quit
#define AUD_INPUT_PLAY         adplug_play
#define AUD_INPUT_READ_TUPLE   adplug_get_tuple
#define AUD_INPUT_IS_OUR_FILE  adplug_is_our_fd
#define AUD_INPUT_EXTS         fmts

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
