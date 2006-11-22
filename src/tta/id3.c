#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "ttalib.h"
#include "id3genre.h"
#include "audacious/util.h"
#include <stdlib.h>
#include <audacious/titlestring.h>

/***********************************************************************
 * ID3 tags manipulation routines
 *
 * Provides read access to ID3v1 tags v1.1, ID3v2 tags v2.3.x and above
 * Supported ID3v2 frames: Title, Artist, Album, Track, Year,
 *                         Genre, Comment.
 *
 **********************************************************************/

static unsigned int unpack_sint28 (const char *ptr) {
	unsigned int value = 0;

	if (ptr[0] & 0x80) return 0;

	value =  value       | (ptr[0] & 0x7f);
	value = (value << 7) | (ptr[1] & 0x7f);
	value = (value << 7) | (ptr[2] & 0x7f);
	value = (value << 7) | (ptr[3] & 0x7f);

	return value;
}

static unsigned int unpack_sint32 (const char *ptr) {
	unsigned int value = 0;

	if (ptr[0] & 0x80) return 0;

	value = (value << 8) | ptr[0];
	value = (value << 8) | ptr[1];
	value = (value << 8) | ptr[2];
	value = (value << 8) | ptr[3];

	return value;
}

static int get_frame_id (const char *id) {
	if (!memcmp(id, "TIT2", 4)) return TIT2;	// Title
	if (!memcmp(id, "TPE1", 4)) return TPE1;	// Artist
	if (!memcmp(id, "TALB", 4)) return TALB;	// Album
	if (!memcmp(id, "TRCK", 4)) return TRCK;	// Track
	if (!memcmp(id, "TYER", 4)) return TYER;	// Year
	if (!memcmp(id, "TCON", 4)) return TCON;	// Genre
	if (!memcmp(id, "COMM", 4)) return COMM;	// Comment
	return 0;
}

int skip_v2_header(tta_info *ttainfo) {
	id3v2_tag id3v2;
	id3v2_frame frame_header;
	int id3v2_size;
	unsigned char *buffer, *ptr;
	gchar *utf;
	gchar tmp[MAX_LINE];
	gchar *tmpptr;
	int tmplen;

	if (!fread(&id3v2, 1, sizeof (id3v2_tag), ttainfo->HANDLE) || 
	    memcmp(id3v2.id, "ID3", 3))
	 {
		fseek (ttainfo->HANDLE, 0, SEEK_SET);
		return 0;
	}

	id3v2_size = unpack_sint28(id3v2.size) + 10;

	fseek (ttainfo->HANDLE, id3v2_size, SEEK_SET);
	ttainfo->id3v2.size = id3v2_size;

	return id3v2_size;
}

/* eof */
