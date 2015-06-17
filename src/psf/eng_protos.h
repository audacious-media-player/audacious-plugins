#include <stdint.h>
#include <libaudcore/tuple.h>

int32_t psf2_start(uint8_t *, uint32_t length);
int32_t psf2_execute(void (*update)(const void *, int));
int32_t psf2_stop(void);
int32_t psf2_command(int32_t, int32_t);
int32_t psf2_fill_info(Tuple *);
int   psf2_seek(uint32_t);

int32_t psf_start(uint8_t *buffer, uint32_t length);
int32_t psf_execute(void (*update)(const void *, int));
int   psf_seek(uint32_t);
int32_t psf_stop(void);

int32_t spx_start(uint8_t *buffer, uint32_t length);
int32_t spx_execute(void (*update)(const void *, int));
int   spx_seek(uint32_t);
int32_t spx_stop(void);

extern bool stop_flag;
