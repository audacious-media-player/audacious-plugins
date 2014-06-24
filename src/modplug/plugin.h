#ifndef MODPLUG_PLUGIN_H
#define MODPLUG_PLUGIN_H

#include "settings.h"

#include <libaudcore/plugin.h>

void InitSettings (const ModplugSettings * settings);
bool CanPlayFileFromVFS (const char * filename, VFSFile * file);
bool PlayFile (const char * filename, VFSFile * file);
Tuple GetSongTuple (const char * filename, VFSFile * file);

#endif /* MODPLUG_PLUGIN_H */
