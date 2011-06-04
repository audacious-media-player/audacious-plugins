/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#include "modplugbmp.h"
extern "C" {
#include <audacious/debug.h>
#include <audacious/plugin.h>

extern InputPlugin _aud_plugin_self;

gboolean Init (void)
{
    gModplugXMMS.SetInputPlugin (_aud_plugin_self);
    gModplugXMMS.Init();
    return TRUE;
}

gint CanPlayFileFromVFS(const char* aFilename, VFSFile *VFSFile)
{
    AUDDBG("aFilename=%s\n", aFilename);
    if(gModplugXMMS.CanPlayFileFromVFS(aFilename, VFSFile))
        return 1;
    return 0;
}

gboolean PlayFile(InputPlayback * data, const gchar * filename, VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
    return gModplugXMMS.PlayFile(filename, data);
}

void Stop(InputPlayback *data)
{
    gModplugXMMS.Stop(data);
}

void Pause (InputPlayback * playback, gboolean pause)
{
    gModplugXMMS.pause (playback, pause);
}

void mseek (InputPlayback * playback, gint time)
{
    gModplugXMMS.mseek (playback, time);
}

Tuple* GetSongTuple(const gchar* aFilename, VFSFile *fd)
{
    return gModplugXMMS.GetSongTuple(aFilename);
}

}
