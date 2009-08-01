/*
 * ttadec.c
 *
 * Description:	 TTAv1 decoder library
 * Developed by: Alexander Djourik <ald@true-audio.com>
 *
 * Copyright (c) 1999-2007 Alexander Djourik. All rights reserved.
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Please see the file COPYING in this directory for full copyright
 * information.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ttadec.h"
#include "ttalib.h"

/******************* static variables and structures *******************/

static unsigned char isobuffers[ISO_BUFFERS_SIZE + 4];
static unsigned char *iso_buffers_end = isobuffers + ISO_BUFFERS_SIZE;
static unsigned int pcm_buffer_size;

static decoder	tta[MAX_NCH];	// decoder state
static int	cache[MAX_NCH];		// decoder cache

tta_info *ttainfo;	// currently playing file info

static unsigned int fframes;	// number of frames in file
static unsigned int framelen;	// the frame length in samples
static unsigned int lastlen;	// the length of the last frame in samples
static unsigned int data_pos;	// currently playing frame index
static unsigned int data_cur;	// the playing position in frame

static int maxvalue;	// output data max value
static unsigned int *seek_table; // the playing position table
static unsigned int st_state; //seek table status

static __uint32_t frame_crc32;
static __uint32_t bit_count;
static __uint32_t bit_cache;
static unsigned char *bitpos;
static unsigned int bitrate;

/************************* crc32 functions *****************************/

#define UPDATE_CRC32(x, crc) crc = \
	(((crc>>8) & 0x00FFFFFF) ^ crc32_table[(crc^x) & 0xFF])

static __uint32_t
crc32 (unsigned char *buffer, unsigned int len) {
	unsigned int	i;
	unsigned int	crc = 0xFFFFFFFF;

	for (i = 0; i < len; i++) UPDATE_CRC32(buffer[i], crc);

	return (crc ^ 0xFFFFFFFF);
}

/************************* bit operations ******************************/

static void init_buffer_read() {
	frame_crc32 = 0xFFFFFFFFUL;
	bit_count = bit_cache = 0;
	bitpos = iso_buffers_end;
}

__inline void get_binary(unsigned int *value, unsigned int bits) {
	while (bit_count < bits) {
		if (bitpos == iso_buffers_end) {
			int res = aud_vfs_fread(isobuffers, 1,
				ISO_BUFFERS_SIZE, ttainfo->HANDLE);
			if (!res) {
				ttainfo->STATE = READ_ERROR;
				return;
			}
			bitpos = isobuffers;
		}

		UPDATE_CRC32(*bitpos, frame_crc32);
		bit_cache |= *bitpos << bit_count;
		bit_count += 8;
		bitpos++;
	}

	*value = bit_cache & bit_mask[bits];
	bit_cache >>= bits;
	bit_count -= bits;
	bit_cache &= bit_mask[bit_count];
}

__inline void get_unary(unsigned int *value) {
	*value = 0;

	while (!(bit_cache ^ bit_mask[bit_count])) {
		if (bitpos == iso_buffers_end) {
			int res = aud_vfs_fread(isobuffers, 1,
				ISO_BUFFERS_SIZE, ttainfo->HANDLE);
			if (!res) {
				ttainfo->STATE = READ_ERROR;
				return;
			}
			bitpos = isobuffers;
		}

		*value += bit_count;
		bit_cache = *bitpos++;
		UPDATE_CRC32(bit_cache, frame_crc32);
		bit_count = 8;
	}

	while (bit_cache & 1) {
		(*value)++;
		bit_cache >>= 1;
		bit_count--;
	}

	bit_cache >>= 1;
	bit_count--;
}

static int done_buffer_read() {
	unsigned int crc32, rbytes, res;
	frame_crc32 ^= 0xFFFFFFFFUL;

	rbytes = iso_buffers_end - bitpos;
	if (rbytes < sizeof(int)) {
		memcpy(isobuffers, bitpos, 4);
		res = aud_vfs_fread(isobuffers + rbytes, 1,
			ISO_BUFFERS_SIZE - rbytes, ttainfo->HANDLE);
		if (!res) {
			ttainfo->STATE = READ_ERROR;
			return 0;
		}
		bitpos = isobuffers;
	}

	memcpy(&crc32, bitpos, 4);
	crc32 = ENDSWAP_INT32(crc32);
	bitpos += sizeof(int);
	res = (crc32 != frame_crc32);

	bit_cache = bit_count = 0;
	frame_crc32 = 0xFFFFFFFFUL;

	// calculate dynamic bitrate
	if (data_pos < fframes) {
		rbytes = seek_table[data_pos] -
			seek_table[data_pos - 1];
		bitrate = (rbytes << 3) / 1070;
	}

	return res;
}

/************************** filter functions ****************************/

///////// Filter Settings //////////
static int flt_set[3] = {10, 9, 10};

__inline void
memshl (register int *pA, register int *pB) {
	*pA++ = *pB++;
	*pA++ = *pB++;
	*pA++ = *pB++;
	*pA++ = *pB++;
	*pA++ = *pB++;
	*pA++ = *pB++;
	*pA++ = *pB++;
	*pA   = *pB;
}

__inline void
hybrid_filter (fltst *fs, int *in) {
	register int *pA = fs->dl;
	register int *pB = fs->qm;
	register int *pM = fs->dx;
	register int sum = fs->round;

	if (!fs->error) {
		sum += *pA++ * *pB, pB++;
		sum += *pA++ * *pB, pB++;
		sum += *pA++ * *pB, pB++;
		sum += *pA++ * *pB, pB++;
		sum += *pA++ * *pB, pB++;
		sum += *pA++ * *pB, pB++;
		sum += *pA++ * *pB, pB++;
		sum += *pA++ * *pB, pB++; pM += 8;
	} else if (fs->error < 0) {
		sum += *pA++ * (*pB -= *pM++), pB++;
		sum += *pA++ * (*pB -= *pM++), pB++;
		sum += *pA++ * (*pB -= *pM++), pB++;
		sum += *pA++ * (*pB -= *pM++), pB++;
		sum += *pA++ * (*pB -= *pM++), pB++;
		sum += *pA++ * (*pB -= *pM++), pB++;
		sum += *pA++ * (*pB -= *pM++), pB++;
		sum += *pA++ * (*pB -= *pM++), pB++;
	} else {
		sum += *pA++ * (*pB += *pM++), pB++;
		sum += *pA++ * (*pB += *pM++), pB++;
		sum += *pA++ * (*pB += *pM++), pB++;
		sum += *pA++ * (*pB += *pM++), pB++;
		sum += *pA++ * (*pB += *pM++), pB++;
		sum += *pA++ * (*pB += *pM++), pB++;
		sum += *pA++ * (*pB += *pM++), pB++;
		sum += *pA++ * (*pB += *pM++), pB++;
	}

	*(pM-0) = ((*(pA-1) >> 30) | 1) << 2;
	*(pM-1) = ((*(pA-2) >> 30) | 1) << 1;
	*(pM-2) = ((*(pA-3) >> 30) | 1) << 1;
	*(pM-3) = ((*(pA-4) >> 30) | 1);

	fs->error = *in;
	*in += (sum >> fs->shift);
	*pA = *in;

	*(pA-1) = *(pA-0) - *(pA-1);
	*(pA-2) = *(pA-1) - *(pA-2);
	*(pA-3) = *(pA-2) - *(pA-3);

	memshl (fs->dl, fs->dl + 1);
	memshl (fs->dx, fs->dx + 1);
}

void
filter_init (fltst *fs, int shift) {
	memset (fs, 0, sizeof(fltst));
	fs->shift = shift;
	fs->round = 1 << (shift - 1);
}

/************************* decoder functions ****************************/

int id3v2_header_length (tta_info *ttainfo) {
	struct {
		unsigned char	id[3];
		unsigned short	version;
		unsigned char	flags;
		unsigned char	size[4];
	} __ATTRIBUTE_PACKED__ id3v2;
	unsigned int len = 0;

	if (!aud_vfs_fread(&id3v2, sizeof(id3v2), 1, ttainfo->HANDLE) || 
	    memcmp(id3v2.id, "ID3", 3) ||
	    id3v2.size[0] & 0x80)
	{
		fseek (ttainfo->HANDLE, 0, SEEK_SET);
		return 0;
	}

	len = (id3v2.size[0] & 0x7f);
	len = (len << 7) | (id3v2.size[1] & 0x7f);
	len = (len << 7) | (id3v2.size[2] & 0x7f);
	len = (len << 7) | (id3v2.size[3] & 0x7f);
	len += 10;
	if (id3v2.flags & (1 << 4)) len += 10;

	return len;
}

int open_tta_file (const char *filename, tta_info *info, unsigned int data_offset) {
	VFSFile *infile;
	tta_hdr ttahdr;
	unsigned int checksum;

	// clear the memory
	memset (info, 0, sizeof(tta_info));

	info->HANDLE = infile = aud_vfs_fopen(filename, "rb");
	if (!infile) return OPEN_ERROR;

	// read id3v2 header
//	if (!data_offset)
//		data_offset = id3v2_header_length(info);

	data_offset = get_id3_tags (filename, info);
	aud_vfs_fseek (infile, data_offset, SEEK_SET);

	// read TTA header
	if (aud_vfs_fread (&ttahdr, 1, sizeof (ttahdr), infile) == 0) {
		aud_vfs_fclose (infile);
		info->STATE = READ_ERROR;
		return -1;
	}

	// check for TTA3 signature
	if (ENDSWAP_INT32(ttahdr.TTAid) != TTA1_SIGN) {
		aud_vfs_fclose (infile);
		info->STATE = FORMAT_ERROR;
		return -1;
	}

	ttahdr.CRC32 = ENDSWAP_INT32(ttahdr.CRC32);
	checksum = crc32((unsigned char *) &ttahdr,
	sizeof(tta_hdr) - sizeof(int));
	if (checksum != ttahdr.CRC32) {
		aud_vfs_fclose (infile);
		info->STATE = FILE_ERROR;
		return -1;
	}

	ttahdr.AudioFormat = ENDSWAP_INT16(ttahdr.AudioFormat);
	ttahdr.NumChannels = ENDSWAP_INT16(ttahdr.NumChannels);
	ttahdr.BitsPerSample = ENDSWAP_INT16(ttahdr.BitsPerSample);
	ttahdr.SampleRate = ENDSWAP_INT32(ttahdr.SampleRate);
	ttahdr.DataLength = ENDSWAP_INT32(ttahdr.DataLength);

	// check for player supported formats
	if (ttahdr.AudioFormat != WAVE_FORMAT_PCM ||
		ttahdr.NumChannels > MAX_NCH ||
		ttahdr.BitsPerSample > MAX_BPS ||(
		ttahdr.SampleRate != 16000 &&
		ttahdr.SampleRate != 22050 &&
		ttahdr.SampleRate != 24000 &&
		ttahdr.SampleRate != 32000 &&
		ttahdr.SampleRate != 44100 &&
		ttahdr.SampleRate != 48000 &&
		ttahdr.SampleRate != 64000 &&
		ttahdr.SampleRate != 88200 &&
		ttahdr.SampleRate != 96000)) {
		aud_vfs_fclose (infile);
		info->STATE = FORMAT_ERROR;
		return FORMAT_ERROR;
	}

	// fill the File Info
	info->HANDLE = infile;
	info->NCH = ttahdr.NumChannels;
	info->BPS = ttahdr.BitsPerSample;
	info->BSIZE = (ttahdr.BitsPerSample + 7)/8;
	info->FORMAT = ttahdr.AudioFormat;
	info->SAMPLERATE = ttahdr.SampleRate;
	info->DATALENGTH = ttahdr.DataLength;
	info->FRAMELEN = (int) (FRAME_TIME * ttahdr.SampleRate);
	info->LENGTH = ttahdr.DataLength / ttahdr.SampleRate;
	info->DATAPOS = data_offset;


	return 0;
}

static void rice_init(adapt *rice, unsigned int k0, unsigned int k1) {
	rice->k0 = k0;
	rice->k1 = k1;
	rice->sum0 = shift_16[k0];
	rice->sum1 = shift_16[k1];
}

static void decoder_init(decoder *tta, int nch, int byte_size) {
	int shift = flt_set[byte_size - 1];
	int i;

	for (i = 0; i < nch; i++) {
		filter_init(&tta[i].fst, shift);
		rice_init(&tta[i].rice, 10, 10);
		tta[i].last = 0;
	}
}

static void seek_table_init (unsigned int *seek_table,
	unsigned int len, unsigned int data_offset) {
	unsigned int *st, frame_len;

	for (st = seek_table; st < (seek_table + len); st++) {
		frame_len = ENDSWAP_INT32(*st);
		*st = data_offset;
		data_offset += frame_len;
	}
}

int set_position (unsigned int pos) {
	unsigned int seek_pos;

	if (pos >= fframes) return 0;
	if (!st_state) {
		ttainfo->STATE = FILE_ERROR;
		return -1;
	}

	seek_pos = ttainfo->DATAPOS + seek_table[data_pos = pos];
	aud_vfs_fseek(ttainfo->HANDLE, seek_pos, SEEK_SET);

	data_cur = 0;
	framelen = 0;

	// init bit reader
	init_buffer_read();

	return 0;
}

int player_init (tta_info *info) {
	unsigned int checksum;
	unsigned int data_offset;
	unsigned int st_size;

	ttainfo = info;

	framelen = 0;
	data_pos = 0;
	data_cur = 0;
	bitrate  = 0;

	lastlen = ttainfo->DATALENGTH % ttainfo->FRAMELEN;
	fframes = ttainfo->DATALENGTH / ttainfo->FRAMELEN + (lastlen ? 1:0);
	st_size = (fframes + 1) * sizeof(int);

	seek_table = (unsigned int *) malloc(st_size);
	if (!seek_table) {
		ttainfo->STATE = MEMORY_ERROR;
		return -1;
	}

	// read seek table
	if (!aud_vfs_fread(seek_table, st_size, 1, ttainfo->HANDLE)) {
		ttainfo->STATE = READ_ERROR;
		return -1;
	}

	checksum = crc32((unsigned char *) seek_table, st_size - sizeof(int));
	st_state = (checksum == ENDSWAP_INT32(seek_table[fframes]));
	data_offset = sizeof(tta_hdr) + st_size;

	// init seek table
	seek_table_init(seek_table, fframes, data_offset);

	// init bit reader
	init_buffer_read();

	pcm_buffer_size = PCM_BUFFER_LENGTH * ttainfo->BSIZE * ttainfo->NCH;
	maxvalue = (1UL << ttainfo->BPS) - 1;

	return 0;
}

void close_tta_file (tta_info *info) {
	if (info->HANDLE) {
		aud_vfs_fclose (info->HANDLE);
		info->HANDLE = NULL;
	}
	
	g_free(info->ID3.name);
	g_free(info->ID3.title);
	g_free(info->ID3.artist);
	g_free(info->ID3.album);
	g_free(info->ID3.comment);
	g_free(info->ID3.year);
	g_free(info->ID3.track);
	g_free(info->ID3.genre);
	info->ID3.id3has = FALSE;
}

void player_stop () {
	if (seek_table) {
		free(seek_table);
		seek_table = NULL;
	}
}

int get_bitrate () {
	return bitrate;
}

int get_samples (byte *buffer) {
	unsigned int k, depth, unary, binary=0;
	byte *p = buffer;
	decoder *dec = tta;
	int *prev = cache;
	int value, value_tmp, res;

	for (res = 0; p < buffer + pcm_buffer_size;) {
		fltst *fst = &dec->fst;
		adapt *rice = &dec->rice;
		int  *last = &dec->last;

		if (data_cur == framelen) {
			if (data_pos == fframes) break;
			if (framelen && done_buffer_read()) {
				if (set_position(data_pos) < 0)
					return -1;
				if (res) break;
			}

			if (data_pos == fframes - 1 && lastlen)
				framelen = lastlen;
			else framelen = ttainfo->FRAMELEN;

			decoder_init(tta, ttainfo->NCH, ttainfo->BSIZE);
			data_pos++; data_cur = 0;
		}

		// decode Rice unsigned
		get_unary(&unary);

		switch (unary) {
		case 0: depth = 0; k = rice->k0; break;
		default:
				depth = 1; k = rice->k1;
				unary--;
		}

		if (k) {
			get_binary(&binary, k);
			value = (unary << k) + binary;
		} else value = unary;

		switch (depth) {
		case 1: 
			rice->sum1 += value - (rice->sum1 >> 4);
			if (rice->k1 > 0 && rice->sum1 < shift_16[rice->k1])
				rice->k1--;
			else if (rice->sum1 > shift_16[rice->k1 + 1])
				rice->k1++;
			value += bit_shift[rice->k0];
		default:
			rice->sum0 += value - (rice->sum0 >> 4);
			if (rice->k0 > 0 && rice->sum0 < shift_16[rice->k0])
				rice->k0--;
			else if (rice->sum0 > shift_16[rice->k0 + 1])
			rice->k0++;
		}
		value_tmp = DEC(value);
		value = value_tmp;

		// decompress stage 1: adaptive hybrid filter
		hybrid_filter(fst, &value);

		// decompress stage 2: fixed order 1 prediction
		switch (ttainfo->BSIZE) {
		case 1: value += PREDICTOR1(*last, 4); break;	// bps 8
		case 2: value += PREDICTOR1(*last, 5); break;	// bps 16
		case 3: value += PREDICTOR1(*last, 5); break;	// bps 24
		case 4: value += *last; break;		// bps 32
		} *last = value;

		// check for errors
		if (abs(value) > maxvalue) {
			unsigned int tail =
				pcm_buffer_size / (ttainfo->BSIZE * ttainfo->NCH) - res;
			memset(buffer, 0, pcm_buffer_size);
			data_cur += tail; res += tail;
			break;
		}

		if (dec < tta + (ttainfo->NCH - 1)) {
			*prev++ = value; dec++;
		} else {
			*prev = value;
			if (ttainfo->NCH > 1) {
				int *r = prev - 1;
				for (*prev += *r/2; r >= cache; r--)
					*r = *(r + 1) - *r;
				for (r = cache; r < prev; r++)
					WRITE_BUFFER(r, ttainfo->BSIZE, p)
			}
			WRITE_BUFFER(prev, ttainfo->BSIZE, p)
			prev = cache;
			data_cur++; res++;
			dec = tta;
		}
	}

	return res;
}

/* end */
