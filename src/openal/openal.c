/*
 * Copyright (c) 2011 William Pitcock <nenolod@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <gtk/gtk.h>

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/debug.h>

#include <AL/al.h>
#include <AL/alc.h>

#define NUM_BUFFERS		100

gboolean
openal_init(void)
{
	ALCdevice *device;

	device = alcOpenDevice(NULL);
	if (device == NULL)
		return FALSE;

	alcCloseDevice(device);
	return TRUE;
}

static struct {
	ALCdevice *device;
	ALCcontext *context;

	ALuint source;
	ALuint buffers[NUM_BUFFERS];

	ALenum format;
	ALuint rate;

	ALint basetime;
	ALint bits;
	ALint nch;

	gboolean preroll;
	gint bufid;
} state;

gint
openal_open(gint fmt, int rate, int nch)
{
	state.rate = rate;
	state.nch = nch;

	if (fmt == FMT_S16_NE)
	{
		state.format = nch == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
		state.bits = 16;
	}
	else if (fmt == FMT_S8)
	{
		state.format = nch == 1 ? AL_FORMAT_MONO8 : AL_FORMAT_STEREO8;
		state.bits = 8;
	}

	if (state.device)
	{
		AUDDBG("WTF: device is already running\n");
		return FALSE;
	}

	state.device = alcOpenDevice(NULL);
	if (state.device == NULL)
	{
		AUDDBG("failed to open device\n");
		return FALSE;
	}

	state.context = alcCreateContext(state.device, NULL);
	if (state.context == NULL)
	{
		if (state.device)
		{
			alcCloseDevice(state.device);
			state.device = NULL;
		}

		AUDDBG("failed to create context\n");
		return FALSE;
	}

	alcMakeContextCurrent(state.context);

	alGenBuffers(NUM_BUFFERS, state.buffers);
	alGenSources(1, &state.source);

	/* disable EAX effects */
	alSourcei(state.source, AL_SOURCE_RELATIVE, AL_TRUE);
	alSourcei(state.source, AL_ROLLOFF_FACTOR, 0);

	state.bufid = 0;
	state.preroll = TRUE;

	return TRUE;
}

void
openal_close(void)
{
	alDeleteSources(1, &state.source);
	alDeleteBuffers(NUM_BUFFERS, state.buffers);

	alcDestroyContext(state.context);
	state.context = NULL;

	alcCloseDevice(state.device);
	state.device = NULL;
}

void
openal_write(void *ptr, int length)
{
	ALuint buf = 0;

	g_return_if_fail(state.device != NULL);

	/* wait for a buffer to become available */
	if (state.preroll == TRUE)
	{
		ALint st;

		printf("preroll bufid %d\n", state.bufid);

		alSourceUnqueueBuffers(state.source, 1, &state.buffers[state.bufid]);

		alBufferData(state.buffers[state.bufid], state.format, ptr, length, state.rate);
		alSourceQueueBuffers(state.source, 1, &state.buffers[state.bufid]);

		alGetSourcei(state.source, AL_BUFFERS_PROCESSED, &st);

		if (state.bufid >= (NUM_BUFFERS - 1))
		{
			printf("done preroll proc %d?\n", st);

			alSourcePlay(state.source);
			state.preroll = FALSE;
		}

		state.bufid++;

		return;
	}

	ALint processed = 0;
	alGetSourcei(state.source, AL_BUFFERS_PROCESSED, &processed);
	g_print("lol:%d\n", processed);

	if(processed < (NUM_BUFFERS / 2))
	{
		ALint st;

		alGetSourcei(state.source, AL_SOURCE_STATE, &st);
		if (st != AL_PLAYING)
			alSourcePlay(state.source);
		else
			g_usleep(10000);
	}

	alSourceUnqueueBuffers(state.source, 1, &buf);
	alBufferData(buf, state.format, ptr, length, state.rate);
	alSourceQueueBuffers(state.source, 1, &buf);

	state.basetime += length;
}

int
openal_written_time(void)
{
	ALint offset;

	if (state.device == NULL)
		return 0;

	alGetSourcei(state.source, AL_SAMPLE_OFFSET, &offset);
	offset += (state.basetime / (state.nch * 8) / state.bits);

	return offset;
}

int
openal_buffer_size(void)
{
	return 19000;	
}

void
openal_flush(gint time)
{
	if (state.device == NULL)
		return;

	alSourceRewind(state.source);
	alSourcei(state.source, AL_BUFFER, 0);
}

OutputPlugin openal_op = {
        .description = "OpenAL Output Plugin",
	.probe_priority = 3,
	.init = openal_init,
	.open_audio = openal_open,
	.close_audio = openal_close,
	.write_audio = openal_write,
	.output_time = openal_written_time,
	.written_time = openal_written_time,
	.buffer_free = openal_buffer_size,
	.flush = openal_flush,
};

OutputPlugin *openal_oplist[] = { &openal_op, NULL };

SIMPLE_OUTPUT_PLUGIN(openal, openal_oplist);
