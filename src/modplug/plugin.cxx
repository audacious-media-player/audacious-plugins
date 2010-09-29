/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

// #define AUD_DEBUG 1

#include "gui/main.h"
extern "C" {
#include <audacious/debug.h>
#include <audacious/plugin.h>


extern InputPlugin gModPlug;

gboolean Init (void)
{
    gModplugXMMS.SetInputPlugin(gModPlug);
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
    char* aFilename = data->filename;
    return gModplugXMMS.PlayFile(aFilename, data);
}

void Stop(InputPlayback *data)
{
    gModplugXMMS.Stop(data);
}

void Pause (InputPlayback * playback, gshort paused)
{
    gModplugXMMS.pause (playback, paused);
}

void mseek (InputPlayback * playback, gulong time)
{
    gModplugXMMS.mseek (playback, time);
}

Tuple* GetSongTuple(const gchar* aFilename)
{
    return gModplugXMMS.GetSongTuple(aFilename);
}

void ShowAboutBox(void)
{
    ShowAboutWindow();
}

void ShowConfigureBox(void)
{
    ShowConfigureWindow(gModplugXMMS.GetModProps());
}

void ShowFileInfoBox(const gchar* aFilename)
{
    ShowInfoWindow(aFilename);
}

}
