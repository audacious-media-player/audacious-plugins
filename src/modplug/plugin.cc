/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#include "plugin.h"
#include "modplugbmp.h"

static ModplugXMMS gModplugXMMS;

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

bool_t PlayFile(const char * filename, VFSFile * file)
{
    return gModplugXMMS.PlayFile(filename);
}

Tuple* GetSongTuple(const char* aFilename, VFSFile *fd)
{
    return gModplugXMMS.GetSongTuple(aFilename);
}
