#include <audacious/plugin.h>

gboolean Init (void);
void ShowAboutBox(void);
void ShowConfigureBox(void);
gboolean PlayFile(InputPlayback * data, const gchar * filename, VFSFile * file, gint start_time, gint stop_time, gboolean pause);
void Stop(InputPlayback *data);
void Pause(InputPlayback *data, gboolean pause);
void mseek (InputPlayback * playback, gint time);
void ShowFileInfoBox(const gchar* aFilename);
Tuple* GetSongTuple(const gchar* aFilename, VFSFile *fd);
gint CanPlayFileFromVFS(const gchar* aFilename, VFSFile *VFSFile);

static const gchar *fmts[] =
    { "amf", "ams", "dbm", "dbf", "dsm", "far", "mdl", "stm", "ult", "mt2",
      "mod", "s3m", "dmf", "umx", "it", "669", "xm", "mtm", "psm", "ft2",
      NULL };

AUD_INPUT_PLUGIN
(
    .name = "ModPlug",
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
