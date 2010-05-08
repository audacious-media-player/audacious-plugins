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

static gchar *
extname(const char *filename)
{
	gchar *ext = strrchr(filename, '.');

	if (ext != NULL)
		++ext;

	return ext;
}

/* TODO: do a real probe function
 * (well, actually, we should just port the one from old madplug)
 */
static gint madplug_probe(const gchar *filename, VFSFile *fin)
{
	gchar *ext = extname(filename);

	if (!g_ascii_strcasecmp(ext, "mp3"))
		return TRUE;

	return FALSE;
}

static Tuple *madplug_probe_tuple(const gchar *filename, VFSFile *fin)
{
	Tuple *tuple;
	gchar *ext = extname(filename);

	if (g_ascii_strcasecmp(ext, "mp3"))
		return NULL;

	tuple = aud_tuple_new_from_filename(filename);
	aud_vfs_fseek(fin, 0, SEEK_SET);

	tag_tuple_read(tuple, fin);

	return tuple;
}

static void madplug_play(InputPlayback *ip)
{
	VFSFile *fd = aud_vfs_fopen(ip->filename, "r");

	madplug_decode(ip, fd);

	aud_vfs_fclose(fd);
}

static const gchar *fmts[] = { "mp3", "mp2", "mpg", "bmu", NULL };

InputPlugin madplug_ip = {
	.description = "MPEG Audio Plugin (rewrite; unsupported)",
	.vfs_extensions = (gchar **) fmts,
	.is_our_file_from_vfs = madplug_probe,
	.probe_for_tuple = madplug_probe_tuple,
	.play_file = madplug_play,
};

InputPlugin *madplug_iplist[] = { &madplug_ip, NULL };

SIMPLE_INPUT_PLUGIN(madplug, madplug_iplist);
