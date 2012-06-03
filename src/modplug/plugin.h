#ifndef MODPLUG_PLUGIN_H
#define MODPLUG_PLUGIN_H

#include <audacious/plugin.h>

bool_t Init (void);
int CanPlayFileFromVFS (const char * filename, VFSFile * file);
bool_t PlayFile (InputPlayback * data, const char * filename, VFSFile * file,
 int start_time, int stop_time, bool_t pause);
void Stop (InputPlayback * data);
void Pause (InputPlayback * data, bool_t pause);
void mseek (InputPlayback * data, int time);
Tuple * GetSongTuple (const char * filename, VFSFile * file);

extern InputPlugin _aud_plugin_self;

#endif /* MODPLUG_PLUGIN_H */
