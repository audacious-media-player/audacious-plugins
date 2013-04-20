/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#include "plugin.h"
#include "modplugbmp.h"

static ModplugXMMS gModplugXMMS;

extern "C" {

void InitSettings (const ModplugSettings * settings)
{
    gModplugXMMS.SetModProps (* settings);
}

int CanPlayFileFromVFS(const char* aFilename, VFSFile *VFSFile)
{
    if(gModplugXMMS.CanPlayFileFromVFS(aFilename, VFSFile))
        return 1;
    return 0;
}

bool_t PlayFile(InputPlayback * data, const char * filename, VFSFile * file, int start_time, int stop_time, bool_t pause)
{
    return gModplugXMMS.PlayFile(filename, data);
}

void Stop(InputPlayback *data)
{
    gModplugXMMS.Stop(data);
}

void Pause (InputPlayback * playback, bool_t pause)
{
    gModplugXMMS.pause (playback, pause);
}

void mseek (InputPlayback * playback, int time)
{
    gModplugXMMS.mseek (playback, time);
}

Tuple* GetSongTuple(const char* aFilename, VFSFile *fd)
{
    return gModplugXMMS.GetSongTuple(aFilename);
}

} /* extern "C" */
