/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#include "audacious/plugin.h"
#include "modplug.h"
#include "gui/main.h"

extern InputPlugin gModPlug;

static void Init(void)
{
	gModplugXMMS.SetInputPlugin(gModPlug);
	gModplugXMMS.Init();
}

static int CanPlayFileFromVFS(char* aFilename, VFSFile *VFSFile)
{
	if(gModplugXMMS.CanPlayFileFromVFS(aFilename, VFSFile))
		return 1;
	return 0;
}

static void PlayFile(InputPlayback *data)
{
        char* aFilename = data->filename;
	gModplugXMMS.SetOutputPlugin(*data->output);
	gModplugXMMS.PlayFile(aFilename);
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

static void GetSongInfo(char* aFilename, char** aTitle, int* aLength)
{
	gModplugXMMS.GetSongInfo(aFilename, *aTitle, *aLength);
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
	{ "amf", "ams", "dbm", "dbf", "dsm", "far", "mdl", "stm", "ult", "j2b", "mt2",
	  "mdz", "mdr", "mdgz", "mdbz", "mod", "s3z", "s3r", "s3gz", "s3m", "xmz", "xmr", "xmgz",
	  "itz", "itr", "itgz", "dmf", "umx", "it", "669", "xm", "mtm", "psm", "ft2", NULL };

InputPlugin gModPlug =
{
	NULL,
	NULL,
	(gchar *)"ModPlug Player",
	Init,
	ShowAboutBox,
	ShowConfigureBox,
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
	GetSongInfo,
	ShowFileInfoBox,
	NULL,	// output
	NULL,   // tuple
	NULL,
	NULL,
	CanPlayFileFromVFS,	// vfs
	(gchar **)fmts,
};

extern "C"
{
	InputPlugin* get_iplugin_info (void)
	{
		return &gModPlug;
	}
}
