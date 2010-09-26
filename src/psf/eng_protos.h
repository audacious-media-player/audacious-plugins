#include <audacious/plugin.h>

int32 psf2_start(uint8 *, uint32 length);
int32 psf2_execute(InputPlayback *playback);
int32 psf2_stop(void);
int32 psf2_command(int32, int32);
int32 psf2_fill_info(Tuple *);
int   psf2_seek(uint32);

int32 psf_start(uint8 *buffer, uint32 length);
int32 psf_execute(InputPlayback *playback);
int   psf_seek(uint32);
int32 psf_stop(void);

int32 spx_start(uint8 *buffer, uint32 length);
int32 spx_execute(InputPlayback *playback);
int   spx_seek(uint32);
int32 spx_stop(void);

