//
// Audio Overload SDK
//
// Fake ao.h to set up the general Audio Overload style environment
//

#ifndef __AO_H
#define __AO_H

#include <stdint.h>

#define WANT_AUD_BSWAP
#include <libaudcore/audio.h>
#include <libaudcore/index.h>

#define AO_SUCCESS					1
#define AO_FAIL						0
#define AO_FAIL_DECOMPRESSION		-1

#define MAX_DISP_INFO_LENGTH		256
#define AUDIO_RATE					(44100)

enum
{
	COMMAND_NONE = 0,
	COMMAND_PREV,
	COMMAND_NEXT,
	COMMAND_RESTART,
	COMMAND_HAS_PREV,
	COMMAND_HAS_NEXT,
	COMMAND_GET_MIN,
	COMMAND_GET_MAX,
	COMMAND_JUMP
};

Index<char> ao_get_lib(char *filename);

#endif // AO_H
