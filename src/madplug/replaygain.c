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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "plugin.h"
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include "replaygain.h"

#define APE_MATCH_BUF (20000)
#define APE_HEADER_SIZE (32)
static const gchar *ape_header_magic_id = "APETAGEX";

typedef struct {
    guchar id[8];
    guint32 version;
    guint32 length;
    guint32 tagCount;
    guint32 flags;
    guchar reserved[8];
} ape_header_t;


static gboolean
fetchLE32(guint32 *res, gchar **ptr, const gchar *end)
{
    if (*ptr + sizeof(guint32) > end)
        return FALSE;
    else {
        *res = ((guint32) (*ptr)[0] ) |
               ((guint32) (*ptr)[1] << 8) |
               ((guint32) (*ptr)[2] << 16) |
               ((guint32) (*ptr)[3] << 24);
        (*ptr) += sizeof(guint32);
        return TRUE;
    }
}

static gdouble
strgain2double(const gchar * s, const size_t len)
{
    gchar *strval = g_strndup(s, len);
    gdouble res = g_strtod(s, NULL);    // gain, in dB.
    g_free(strval);
    return res;
}

/* Check for APE tag header in current file position, and read
 * header data into given structure. Return 0 if OK.
 */
static gint checkAPEHeader(VFSFile * fp, ape_header_t *hdr)
{
    /* Get magic id and check it */
    if (aud_vfs_fread(&hdr->id, sizeof(hdr->id), 1, fp) != 1)
        return 2;
    
    if (memcmp(hdr->id, ape_header_magic_id, sizeof(hdr->id)) != 0)
        return 3;
    
    /* Currently we only support APEv2 */
    if (!aud_vfs_fget_le32(&hdr->version, fp) || hdr->version != 2000)
        return 4;
    
    /* Validate header length */
    if (!aud_vfs_fget_le32(&hdr->length, fp) || hdr->length < APE_HEADER_SIZE)
        return 5;
    
    /* Get other data */
    if (!aud_vfs_fget_le32(&hdr->tagCount, fp) || !aud_vfs_fget_le32(&hdr->flags, fp) ||
        aud_vfs_fread(&hdr->reserved, sizeof(hdr->reserved), 1, fp) != 1)
        return 6;
    
    return 0;
}

/* Reads APE v2.0 tag ending at current pos in fp
 */
static gint
readAPE2Tag(VFSFile * fp, struct mad_info_t *file_info)
{
    gchar *buff, *p, *end;
    gint res;
    ape_header_t hdr;
    
    if (aud_vfs_fseek(fp, -APE_HEADER_SIZE, SEEK_CUR) != 0)
        return 18;
    
    if ((res = checkAPEHeader(fp, &hdr)) != 0)
        return res;
    
    if (aud_vfs_fseek(fp, -hdr.length, SEEK_CUR) != 0)
        return 7;
    
    if ((buff = (gchar *) g_malloc(hdr.length)) == NULL)
        return 8;
    
    if (aud_vfs_fread(buff, hdr.length - APE_HEADER_SIZE, 1, fp) != 1) {
        g_free(buff);
        return 9;
    }

    AUDDBG("ver = %ld\n", hdr.version);
    AUDDBG("taglen = %ld\n", hdr.length);

    end = buff + hdr.length - APE_HEADER_SIZE;
    
    for (p = buff; p + 8 < end && hdr.tagCount--;) {
        guint32 vsize, flags;
        gchar *tmp;
        
        /* Get and check size and string */
        if (!fetchLE32(&vsize, &p, end)) break;
        if (!fetchLE32(&flags, &p, end)) break;
        for (tmp = p; tmp < end && *tmp != 0; tmp++);
        if (*tmp != 0) break;
        tmp++;
        
        if (vsize > 0) {
            gdouble *scale = NULL;
            gchar **str = NULL;
            if (g_ascii_strcasecmp(p, "REPLAYGAIN_ALBUM_GAIN") == 0) {
                scale = &file_info->replaygain_album_scale;
                str = &file_info->replaygain_album_str;
            } else
            if (g_ascii_strcasecmp(p, "REPLAYGAIN_TRACK_GAIN") == 0) {
                scale = &file_info->replaygain_track_scale;
                str = &file_info->replaygain_track_str;
            }
            if (str != NULL) {
                assert(scale != NULL);
                *scale = strgain2double(tmp, vsize);
                *str = g_strndup(tmp, vsize);
            }
            /* case of peak info tags : */
            str = NULL;
            if (g_ascii_strcasecmp(p, "REPLAYGAIN_TRACK_PEAK") == 0) {
                scale = &file_info->replaygain_track_peak;
                str = &file_info->replaygain_track_peak_str;
            } else
            if (g_ascii_strcasecmp(p, "REPLAYGAIN_ALBUM_PEAK") == 0) {
                scale = &file_info->replaygain_album_peak;
                str = &file_info->replaygain_album_peak_str;
            }
            if (str != NULL) {
                *scale = strgain2double(tmp, vsize);
                *str = g_strndup(tmp, vsize);
            }

            /* mp3gain additional tags : 
               the gain tag translates to scale = 2^(gain/4), 
               i.e., in dB : 20*log(2)/log(10)*gain/4
               -> 1.501*gain dB
             */
            if (g_ascii_strcasecmp(p, "MP3GAIN_UNDO") == 0) {
                str = &file_info->mp3gain_undo_str;
                scale = &file_info->mp3gain_undo;
                assert(4 < vsize);  /* this tag is +left,+right */
                *str = g_strndup(tmp, vsize);
                *scale = 1.50515 * atoi(*str);
            } else
            if (g_ascii_strcasecmp(p, "MP3GAIN_MINMAX") == 0) {
                str = &file_info->mp3gain_minmax_str;
                scale = &file_info->mp3gain_minmax;
                *str = g_strndup(tmp, vsize);
                assert(4 < vsize);  /* this tag is min,max */
                *scale = 1.50515 * (atoi((*str) + 4) - atoi(*str));
            }
        }
        
        p = tmp + vsize;
    }

    g_free(buff);

    return 0;
}

static gint
findOffset(VFSFile * fp)
{
    gchar buff[APE_MATCH_BUF];
    gint matched = 0, last_match = -1;
    size_t N = 0, i;
    
    if (aud_vfs_fseek(fp, - APE_MATCH_BUF, SEEK_CUR) != 0);
    if ((N = aud_vfs_fread(buff, sizeof(gchar), APE_MATCH_BUF, fp)) < 16)
        return 1;
    
    for (i = 0; i < N; ++i) {
        if (buff[i] == ape_header_magic_id[matched])
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
    return last_match + 1 - 8 + APE_HEADER_SIZE - N;
}

/**
 * Read ReplayGain information from RVA2 frames.
 *
 * Return TRUE if ReplayGain information was found; otherwise return FALSE.
 */
static gint
readId3v2RVA2(struct mad_info_t *file_info)
{
    gint i;
    struct id3_frame *frame;
    gint ret = FALSE;

    AUDDBG("f: ReadId3v2RVA2\n");

    /* tag must be read before! */
    if (! file_info->tag ) {
        AUDDBG("id3v2 not found\n");
        return FALSE;
    }

    /* scan RVA2 frames */
    for (i = 0; (frame = id3_tag_findframe(file_info->tag, "RVA2", i)); i++) {
        const gchar *key;
        const id3_byte_t *data;
        id3_length_t datalen;
        gdouble *scale = NULL, *peak = NULL;
        guint p;

        if (frame->nfields != 2)
            continue;

        key =  (const gchar*) id3_field_getlatin1(&frame->fields[0]);
        data = id3_field_getbinarydata(&frame->fields[1], &datalen);

        AUDDBG("got RVA2 frame id=(%s)\n", key);

        /* the identification field should be "track" or "album" */
        if (strcasecmp(key, "track") == 0) {
            scale = &file_info->replaygain_track_scale;
            peak =  &file_info->replaygain_track_peak;
        } else if (strcasecmp(key, "album") == 0) {
            scale = &file_info->replaygain_album_scale;
            peak =  &file_info->replaygain_album_peak;
        }

        /* decode gain information */
        p = 0;
        while (p + 3 < datalen) {
            /* p+0 :       channel type
               p+1, p+2 :  16-bit signed BE int = scaledb * 512
               p+3 :       nr of bits representing peak
               p+4 :       unsigned multibyte BE int =  peak * 2**(peakbits-1)
            */
            gint channel, peakbits;
            gdouble chgain, chpeak;

            channel = data[p];
            chgain =  (gdouble)((((signed char)(data[p+1])) << 8) | ((unsigned char)(data[p+2]))) / 512.0;
            peakbits = data[p+3];

            if (p + 4 + (peakbits + 7) / 8 > datalen)
                break;

            chpeak = 0;
            if (peakbits > 0)
                chpeak += (gdouble)(unsigned char)(data[p+4]);
            if (peakbits > 8)
                chpeak += (gdouble)(unsigned char)(data[p+5]) / 256.0;
            if (peakbits > 16)
                chpeak += (gdouble)(unsigned char)(data[p+6]) / 65536.0;
            if (peakbits > 0)
                chpeak = chpeak / (gdouble)(1 << ((peakbits - 1) & 7));
            AUDDBG("channel=%d chgain=%f peakbits=%d chpeak=%f\n", channel, chgain, peakbits, chpeak);
            if (channel == 1) {
                /* channel 1 == master volume */
                if (scale != NULL && peak != NULL) {
                    *scale = chgain;
                    *peak =  chpeak;
                    ret = TRUE;
                }
            }

            p += 4 + (peakbits + 7) / 8;
        }

    }

    return ret;
}

/* Eugene Zagidullin:
 * Read ReplayGain info from foobar2000-style id3v2 frames.
 */
static gint
readId3v2TXXX(struct mad_info_t *file_info)
{
	gint i;
	gchar *key;
	gchar *value;
	struct id3_frame *frame;
	gint ret = FALSE;

	AUDDBG("f: ReadId3v2TXXX\n");

	/* tag must be read before! */
	if (! file_info->tag ) {
		AUDDBG("id3v2 not found\n");
		return FALSE;
	}

	/* Partially based on code from MPD (http://www.musicpd.org/) */
	for (i = 0; (frame = id3_tag_findframe(file_info->tag, "TXXX", i)); i++) {
		if (frame->nfields < 3)
			continue;

		key = (gchar *) id3_ucs4_latin1duplicate(id3_field_getstring(&frame->fields[1]));
		value = (gchar *) id3_ucs4_latin1duplicate(id3_field_getstring(&frame->fields[2]));

		if (strcasecmp(key, "replaygain_track_gain") == 0) {
			file_info->replaygain_track_scale = g_strtod(value, NULL);
			file_info->replaygain_track_str = g_strdup(value);
			ret = TRUE;
		} else if (strcasecmp(key, "replaygain_album_gain") == 0) {
			file_info->replaygain_album_scale = g_strtod(value, NULL);
			file_info->replaygain_album_str = g_strdup(value);
			ret = TRUE;
		} else if (strcasecmp(key, "replaygain_track_peak") == 0) {
			file_info->replaygain_track_peak = g_strtod(value, NULL);
			file_info->replaygain_track_peak_str = g_strdup(value);
			ret = TRUE;
		} else if (strcasecmp(key, "replaygain_album_peak") == 0) {
			file_info->replaygain_album_peak = g_strtod(value, NULL);
			file_info->replaygain_album_peak_str = g_strdup(value);
			ret = TRUE;
		}

		free(key);
		free(value);
	}

	return ret;
}

void
audmad_read_replaygain(struct mad_info_t *file_info)
{
    VFSFile *fp;
    glong curpos = 0;

    AUDDBG("f: read_replaygain\n");

    file_info->replaygain_track_peak = 0.0;
    file_info->replaygain_track_scale = 0.0;
    file_info->replaygain_album_peak = 0.0;
    file_info->replaygain_album_scale = 0.0;
    file_info->mp3gain_undo = -77;
    file_info->mp3gain_minmax = -77;

    if (readId3v2RVA2(file_info) || readId3v2TXXX(file_info)) {
        AUDDBG("found ReplayGain info in id3v2 tag\n");
#ifdef DEBUG
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
    
    glong pos = aud_vfs_ftell(fp);
    gint res = -1, try_pos = 0;
    while (res != 0 && try_pos < 10) {
        // try skipping an id3 tag
        aud_vfs_fseek(fp, pos, SEEK_SET);
        aud_vfs_fseek(fp, try_pos * -128, SEEK_CUR);
        res = readAPE2Tag(fp, file_info);
        ++try_pos;
    }
    if (res != 0) {
        // try brute search (don't want to parse all possible kinds of tags..)
        aud_vfs_fseek(fp, pos, SEEK_SET);
        gint offs = findOffset(fp);
        if (offs <= 0) {        // found !
            aud_vfs_fseek(fp, pos, SEEK_SET);
            aud_vfs_fseek(fp, offs, SEEK_CUR);
            res = readAPE2Tag(fp, file_info);
            if (res != 0) {
                AUDDBG
                    ("hmpf, was supposed to find a tag.. offs=%d, res=%d",
                     offs, res);
            }
        }
        else 
            AUDDBG("replaygain: not found\n");
    }
#ifdef DEBUG
    if (res == 0) {             // got APE tags, show the result
        gchar *tmp = g_filename_to_utf8(file_info->filename, -1, NULL, NULL, NULL);        
        AUDDBG("RG album scale= %g, RG track scale = %g, in %s\n",
		  file_info->replaygain_album_scale,
		  file_info->replaygain_track_scale, tmp);
        g_free(tmp);
    }
#endif

    if (file_info->infile)
        aud_vfs_fseek(fp, curpos, SEEK_SET);

    aud_vfs_fclose(fp);

    AUDDBG("e: read_replaygain\n");
}
