//
// Audio Overload
// Emulated music player
//
// (C) 2000-2007 Richard F. Bannister
//

#include <audacious/plugin.h>

//
// eng_protos.h
//

int32 psf2_start(uint8 *, uint32 length);
int32 psf2_execute(InputPlayback *playback);
int32 psf2_stop(void);
int32 psf2_command(int32, int32);
int32 psf2_fill_info(ao_display_info *);

