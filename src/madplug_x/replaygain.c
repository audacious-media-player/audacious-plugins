/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
 *
 * Portions derived from xmms-mad:
 * Copyright (C) 2001-2002 Sam Clegg - See COPYING
 * Copyright (C) 2001-2007 Samuel Krempp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "plugin.h"
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include "replaygain.h"

static unsigned long
Read_LE_Uint32(const unsigned char *p)
{
    return ((unsigned long) p[0] << 0) |
        ((unsigned long) p[1] << 8) |
        ((unsigned long) p[2] << 16) | ((unsigned long) p[3] << 24);
}

static int
uncase_strcmp(const char *s1, const char *s2)
{
    int l1 = strlen(s1);
    int l2 = strlen(s2);
    int i;
    for (i = 0; i < l1 && i < l2; ++i) {
        if (toupper(s1[i]) < toupper(s2[i]))
            return -1;
    }
    if (l1 == l2)
        return 0;
    return (l1 < l2) ? -1 : +1;
}

static gdouble
strgain2double(gchar * s, int len)
{
    gdouble res = g_strtod(s, NULL);    // gain, in dB.
    if (res == 0)
        return 1;
    return pow(10, res / 20);
}

// Reads APE v2.0 tag ending at current pos in fp

static int
ReadAPE2Tag(VFSFile * fp, struct mad_info_t *file_info)
{
    unsigned long vsize;
    unsigned long isize;
    unsigned long flags;
    char *buff;
    char *p;
    char *end;
    struct APETagFooterStruct T, *tp;
    unsigned long TagLen;
    unsigned long TagCount;

    tp = &T;

    if (aud_vfs_fseek(fp, -sizeof(T), SEEK_CUR) != 0)
        return 18;
    if (aud_vfs_fread(tp, 1, sizeof(T), fp) != sizeof T)
        return 2;
    if (memcmp(tp->ID, "APETAGEX", sizeof(tp->ID)) != 0)
        return 3;
    if (Read_LE_Uint32(tp->Version) != 2000)
        return 4;
    TagLen = Read_LE_Uint32(tp->Length);
    if (TagLen < sizeof(T))
        return 5;
    if (aud_vfs_fseek(fp, -TagLen, SEEK_CUR) != 0)
        return 6;
    if ((buff = (char *) malloc(TagLen)) == NULL) {
        return 7;
    }
    if (aud_vfs_fread(buff, 1, TagLen - sizeof(T), fp) != TagLen - sizeof(T)) {
        free(buff);
        return 8;
    }

    AUDDBG("ver = %ld\n", Read_LE_Uint32(tp->Version));
    AUDDBG("taglen = %ld\n", TagLen);

    TagCount = Read_LE_Uint32(tp->TagCount);
    end = buff + TagLen - sizeof(T);
    for (p = buff; p < end && TagCount--;) {
        vsize = Read_LE_Uint32((unsigned char *)p);
        p += 4;
        flags = Read_LE_Uint32((unsigned char *)p);
        p += 4;
        isize = strlen((char *) p);

        if (isize > 0 && vsize > 0) {
            gdouble *scale = NULL;
            gchar **str = NULL;
            if (uncase_strcmp(p, "REPLAYGAIN_ALBUM_GAIN") == 0) {
                scale = &file_info->replaygain_album_scale;
                str = &file_info->replaygain_album_str;
            }
            if (uncase_strcmp(p, "REPLAYGAIN_TRACK_GAIN") == 0) {
                scale = &file_info->replaygain_track_scale;
                str = &file_info->replaygain_track_str;
            }
            if (str != NULL) {
                assert(scale != NULL);
                *scale = strgain2double(p + isize + 1, vsize);
                *str = g_strndup(p + isize + 1, vsize);
            }
            //* case of peak info tags : */
            str = NULL;
            if (uncase_strcmp(p, "REPLAYGAIN_TRACK_PEAK") == 0) {
                scale = &file_info->replaygain_track_peak;
                str = &file_info->replaygain_track_peak_str;
            }
            if (uncase_strcmp(p, "REPLAYGAIN_ALBUM_PEAK") == 0) {
                scale = &file_info->replaygain_album_peak;
                str = &file_info->replaygain_album_peak_str;
            }
            if (str != NULL) {
                *scale = g_strtod(p + isize + 1, NULL);
                *str = g_strndup(p + isize + 1, vsize);
            }

            /* mp3gain additional tags : 
               the gain tag translates to scale = 2^(gain/4), 
               i.e., in dB : 20*log(2)/log(10)*gain/4
               -> 1.501*gain dB
             */
            if (uncase_strcmp(p, "MP3GAIN_UNDO") == 0) {
                str = &file_info->mp3gain_undo_str;
                scale = &file_info->mp3gain_undo;
                assert(4 < vsize);  /* this tag is +left,+right */
                *str = g_strndup(p + isize + 1, vsize);
                *scale = 1.50515 * atoi(*str);
            }
            if (uncase_strcmp(p, "MP3GAIN_MINMAX") == 0) {
                str = &file_info->mp3gain_minmax_str;
                scale = &file_info->mp3gain_minmax;
                *str = g_strndup(p + isize + 1, vsize);
                assert(4 < vsize);  /* this tag is min,max */
                *scale = 1.50515 * (atoi((*str) + 4) - atoi(*str));
            }
        }
        p += isize + 1 + vsize;
    }

    free(buff);

    return 0;
}

static int
find_offset(VFSFile * fp)
{
    static const char *key = "APETAGEX";
    char buff[20000];
    int N = 0;
    if (aud_vfs_fseek(fp, -20000, SEEK_CUR) != 0);
    if ((N = aud_vfs_fread(buff, 1, 20000, fp)) < 16)
        return 1;
    int matched = 0;
    int i, last_match = -1;
    for (i = 0; i < N; ++i) {
        if (buff[i] == key[matched])
            ++matched;
        else {
            if (matched == 5 && buff[i] == 'P')
                matched = 2;    // got "APET" + "AP"
            else
                matched = 0;
        }
        if (matched == 8) {
            last_match = i;
            matched = 0;
        }
    }
    if (last_match == -1)
        return 1;
    return last_match + 1 - 8 + sizeof(struct APETagFooterStruct) - N;
}

/* Eugene Zagidullin:
 * Read ReplayGain info from foobar2000-style id3v2 frames */

static int
ReadId3v2TXXX(struct mad_info_t *file_info)
{
	int i;
	char *key;
	char *value;
	struct id3_frame *frame;

	AUDDBG("f: ReadId3v2TXXX\n");

	/* tag must be read before! */
	if (! file_info->tag ) {
		AUDDBG("id3v2 not found\n");
		return 0;
	}

	/* Partially based on code from MPD (http://www.musicpd.org/) */
	for (i = 0; (frame = id3_tag_findframe(file_info->tag, "TXXX", i)); i++) {
		if (frame->nfields < 3)
			continue;

		key = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[1]));
		value = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[2]));

		if (strcasecmp(key, "replaygain_track_gain") == 0) {
			file_info->replaygain_track_scale = strgain2double(value, strlen(value));
			file_info->replaygain_track_str = g_strdup(value);
		} else if (strcasecmp(key, "replaygain_album_gain") == 0) {
			file_info->replaygain_album_scale = strgain2double(value, strlen(value));
			file_info->replaygain_album_str = g_strdup(value);
		} else if (strcasecmp(key, "replaygain_track_peak") == 0) {
			file_info->replaygain_track_peak = g_strtod(value, NULL);
			file_info->replaygain_track_peak_str = g_strdup(value);
		} else if (strcasecmp(key, "replaygain_album_peak") == 0) {
			file_info->replaygain_album_peak = g_strtod(value, NULL);
			file_info->replaygain_album_peak_str = g_strdup(value);
		}

		free(key);
		free(value);
	}

	if (file_info->replaygain_track_scale != -1 || file_info->replaygain_album_scale != -1)
	{
		file_info->has_replaygain = TRUE;
		return 1;
	}

	return 0;
}

void
read_replaygain(struct mad_info_t *file_info)
{
    VFSFile *fp;
    glong curpos = 0;

    AUDDBG("f: read_replaygain\n");

    file_info->has_replaygain = FALSE;
    file_info->replaygain_album_scale = -1;
    file_info->replaygain_track_scale = -1;
    file_info->mp3gain_undo = -77;
    file_info->mp3gain_minmax = -77;

    if (ReadId3v2TXXX(file_info)) {
        AUDDBG("found ReplayGain info in id3v2 tag\n");
#ifdef AUD_DEBUG
	gchar *tmp = g_filename_to_utf8(file_info->filename, -1, NULL, NULL, NULL);

        AUDDBG("RG album scale= %g, RG track scale = %g, in %s\n",
		  file_info->replaygain_album_scale,
		  file_info->replaygain_track_scale, tmp);
        g_free(tmp);
#endif
	return;
    }


    /* APEv2 stuff */
    if (file_info->infile) {
        fp = aud_vfs_dup(file_info->infile);
        curpos = aud_vfs_ftell(fp);
    }
    else {
        if ((fp = aud_vfs_fopen(file_info->filename, "rb")) == NULL)
            return;
    }

    if (aud_vfs_fseek(fp, 0L, SEEK_END) != 0) {
        aud_vfs_fclose(fp);
        return;
    }
    
    long pos = aud_vfs_ftell(fp);
    int res = -1;
    int try = 0;
    while (res != 0 && try < 10) {
        // try skipping an id3 tag
        aud_vfs_fseek(fp, pos, SEEK_SET);
        aud_vfs_fseek(fp, try * -128, SEEK_CUR);
        res = ReadAPE2Tag(fp, file_info);
        ++try;
    }
    if (res != 0) {
        // try brute search (don't want to parse all possible kinds of tags..)
        aud_vfs_fseek(fp, pos, SEEK_SET);
        int offs = find_offset(fp);
        if (offs <= 0) {        // found !
            aud_vfs_fseek(fp, pos, SEEK_SET);
            aud_vfs_fseek(fp, offs, SEEK_CUR);
            res = ReadAPE2Tag(fp, file_info);
            if (res != 0) {
                g_message
                    ("hmpf, was supposed to find a tag.. offs=%d, res=%d",
                     offs, res);
            }
        }
        else 
            AUDDBG("replaygain: not found\n");
    }
#ifdef AUD_DEBUG
    if (res == 0) {             // got APE tags, show the result
        gchar *tmp = g_filename_to_utf8(file_info->filename, -1, NULL, NULL, NULL);        
        AUDDBG("RG album scale= %g, RG track scale = %g, in %s\n",
		  file_info->replaygain_album_scale,
		  file_info->replaygain_track_scale, tmp);
        g_free(tmp);
    }
#endif

    if (file_info->replaygain_album_scale != -1
        || file_info->replaygain_track_scale != -1)
        file_info->has_replaygain = TRUE;

    if (file_info->infile)
        aud_vfs_fseek(fp, curpos, SEEK_SET);

    aud_vfs_fclose(fp);

    AUDDBG("e: read_replaygain\n");
}
