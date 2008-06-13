/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

// #define AUD_DEBUG 1

#include "gui/main.h"
extern "C" {
#include <audacious/plugin.h>
}

extern InputPlugin gModPlug;

static void Init(void)
{
    gModplugXMMS.SetInputPlugin(gModPlug);
    gModplugXMMS.Init();
}

static int CanPlayFileFromVFS(char* aFilename, VFSFile *VFSFile)
{
    AUDDBG("aFilename=%s\n", aFilename);
    if(gModplugXMMS.CanPlayFileFromVFS(aFilename, VFSFile))
        return 1;
    return 0;
}

static void PlayFile(InputPlayback *data)
{
    char* aFilename = data->filename;
    gModplugXMMS.SetOutputPlugin(*data->output);
    gModplugXMMS.PlayFile(aFilename, data);
}

static void Stop(InputPlayback *data)
{
    gModplugXMMS.Stop();
}

static void Pause(InputPlayback *data, short aPaused)
{
    gModplugXMMS.Pause((bool)aPaused);
}

static void Seek(InputPlayback *data, int aTime)
{
    gModplugXMMS.Seek(float32(aTime));
}

static int GetTime(InputPlayback *data)
{
    float32 lTime;
    
    lTime = gModplugXMMS.GetTime();
    if(lTime == -1)
        return -1;
    else
        return (int)(lTime * 1000);
}

static Tuple* GetSongTuple(char* aFilename)
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

void ShowFileInfoBox(char* aFilename)
{
    ShowInfoWindow(aFilename);
}

const gchar *fmts[] =
    { "amf", "ams", "dbm", "dbf", "dsm", "far", "mdl", "stm", "ult", "mt2",
      "mdz", "mdr", "mdgz", "mdbz", "mod", "s3z", "s3r", "s3gz", "s3m", "xmz", "xmr", "xmgz",
      "itz", "itr", "itgz", "dmf", "umx", "it", "669", "xm", "mtm", "psm", "ft2",
      "zip", "gz", "bz2", "rar", "rb",
      NULL };

InputPlugin gModPlug =
{
    /* Common plugin fields */
    NULL,
    NULL,
    (gchar *)"ModPlug Audio Plugin",
    Init,
    NULL,
    ShowAboutBox,
    ShowConfigureBox,
    FALSE,

    /* Input plugin fields */
    NULL,
    NULL,
    PlayFile,
    Stop,
    Pause,
    Seek,
    NULL,
    GetTime,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    ShowFileInfoBox,
    NULL,   // output
    GetSongTuple,
    CanPlayFileFromVFS, // vfs
    (gchar **)fmts,
    NULL,
    NULL,
    TRUE,   // subtune. to exclude .zip etc which doesn't contain any mod file --yaz
    NULL
};

InputPlugin *modplug_iplist[] = { &gModPlug, NULL };

SIMPLE_INPUT_PLUGIN(modplug, modplug_iplist);
