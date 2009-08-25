/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

// #define AUD_DEBUG 1

#include "gui/main.h"
extern "C" {
#include <audacious/plugin.h>


extern InputPlugin gModPlug;

void Init(void)
{
    gModplugXMMS.SetInputPlugin(gModPlug);
    gModplugXMMS.Init();
}

gint CanPlayFileFromVFS(const char* aFilename, VFSFile *VFSFile)
{
    AUDDBG("aFilename=%s\n", aFilename);
    if(gModplugXMMS.CanPlayFileFromVFS(aFilename, VFSFile))
        return 1;
    return 0;
}

void PlayFile(InputPlayback *data)
{
    char* aFilename = data->filename;
    gModplugXMMS.SetOutputPlugin(*data->output);
    gModplugXMMS.PlayFile(aFilename, data);
}

void Stop(InputPlayback *data)
{
    gModplugXMMS.Stop(data);
}

void Pause(InputPlayback *data, short aPaused)
{
    gModplugXMMS.Pause((bool)aPaused);
}

void Seek(InputPlayback *data, int aTime)
{
    gModplugXMMS.Seek(float32(aTime));
}

int GetTime(InputPlayback *data)
{
    float32 lTime;
    
    lTime = gModplugXMMS.GetTime();
    if(lTime == -1)
        return -1;
    else
        return (int)(lTime * 1000);
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
