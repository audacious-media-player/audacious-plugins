/*
 * Copyright (c) 2010 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "stand.h"

typedef struct {
	VFSFile *fd;
	gboolean opened;
	gint channels;
	gint srate;
} MADDecodeContext;

#define BUFFER_SIZE		(16 * 1024)

static enum mad_flow
madplug_input(InputPlayback *ip, struct mad_stream *stream)
{
	MADDecodeContext *ctx = (MADDecodeContext *) ip->data;
	guchar buffer[BUFFER_SIZE];
	gsize len;

	memset(buffer, '\0', BUFFER_SIZE);

	if (!(len = aud_vfs_fread(buffer, 1, BUFFER_SIZE, ctx->fd)))
		return MAD_FLOW_STOP;

	g_print("read %d bytes\n", len);
	mad_stream_buffer(stream, buffer, len);

	return MAD_FLOW_CONTINUE;
}

static enum mad_flow
madplug_header(InputPlayback *ip, const struct mad_header *header)
{
	gint channels = MAD_NCHANNELS(header);
	MADDecodeContext *ctx = (MADDecodeContext *) ip->data;

	if ((ctx->srate != header->samplerate) || (ctx->channels != channels)) {
		if (ctx->opened)
			ip->output->close_audio();

		if (!ip->output->open_audio(FMT_FLOAT, header->samplerate, channels))
			return MAD_FLOW_STOP;

		ctx->opened = TRUE;
		g_print("opened, rate=%d, channels=%d\n", header->samplerate, channels);
	}

	ip->set_params(ip, NULL, 0, header->bitrate, header->samplerate, channels);
	ip->set_pb_ready(ip);

	return MAD_FLOW_CONTINUE;
}

static enum mad_flow
madplug_output(InputPlayback *ip, const struct mad_header *header, struct mad_pcm *pcm)
{
	gint channels = MAD_NCHANNELS(header);
	gfloat *data = g_malloc (sizeof(gfloat) * channels * pcm->length);
	gfloat *end = data + channels * pcm->length;
	gint channel;

	for (channel = 0; channel < channels; channel++)
	{
		const mad_fixed_t *from = pcm->samples[channel];
		gfloat *to = data + channel;

		while (to < end)
		{
			*to = (gfloat) (*from++) / (1 << 28);
			to += channels;
		}
	}

//	g_print("passing data for %d channels\n", channels);
	ip->pass_audio(ip, FMT_FLOAT, channels, sizeof(gfloat) * channels * pcm->length, data, NULL);
	g_free(data);

	return MAD_FLOW_CONTINUE;
}

static enum mad_flow
madplug_error(InputPlayback *ip, struct mad_stream *stream, struct mad_frame *frame)
{
//	g_print("error: %s\n", mad_stream_errorstr(stream));

	return MAD_FLOW_CONTINUE;
}

void
madplug_decode(InputPlayback *ip, VFSFile *fd)
{
	MADDecodeContext ctx = {};
	struct mad_decoder decoder;

	ctx.fd = aud_vfs_dup(fd);

	aud_vfs_fseek(fd, 0, SEEK_SET);

	/* explanation: ctx is on the stack, so it'll be valid memory as long as the decoder is running. */
	ip->data = &ctx;

	mad_decoder_init(&decoder, ip, (void *) madplug_input, (void *) madplug_header, NULL, (void *) madplug_output, (void *) madplug_error, NULL);	
	mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
	mad_decoder_finish(&decoder);

	ip->output->close_audio();

	aud_vfs_fclose(ctx.fd);
}
