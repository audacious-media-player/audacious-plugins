#ifndef AUDACIOUS_LIBMPG123_CONFIG_H
#define AUDACIOUS_LIBMPG123_CONFIG_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HAVE_STRDUP
#define HAVE_STRERROR

#define OPT_GENERIC

#define FIFO 1
#define FRAME_INDEX 1
#define GAPLESS 1
#define IEEE_FLOAT 1
#define INDEX_SIZE 1000

#define NO_8BIT
#define NO_32BIT
#define NO_DOWNSAMPLE
#define NO_NTOM
#define NO_REAL
#define NO_ID3V2

#endif
