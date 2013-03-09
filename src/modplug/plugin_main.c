#include <audacious/i18n.h>

#include "plugin.h"

static const char * fmts[] =
    { "amf", "ams", "dbm", "dbf", "dsm", "far", "mdl", "stm", "ult", "mt2",
      "mod", "s3m", "dmf", "umx", "it", "669", "xm", "mtm", "psm", "ft2",
      NULL };

AUD_INPUT_PLUGIN
(
    .name = N_("ModPlug (Module Player)"),
    .domain = PACKAGE,
    .init = Init,
    .play = PlayFile,
    .stop = Stop,
    .pause = Pause,
    .mseek = mseek,
    .probe_for_tuple = GetSongTuple,
    .is_our_file_from_vfs = CanPlayFileFromVFS,
    .extensions = fmts,
    .have_subtune = TRUE,   // to exclude .zip etc which doesn't contain any mod file --yaz
)
