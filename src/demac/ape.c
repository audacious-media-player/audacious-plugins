/*
 * Monkey's Audio APE demuxer, standalone version
 * Copyright (c) 2007 Benjamin Zores <ben@geexbox.org>
 *  based upon libdemac from Dave Chapman.
 * Copyright (c) 2007 Eugene Zagidullin <e.asphyx@gmail.com>
 *   Cleanup libav* depending code, Audacious stuff.
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 

#include <audacious/vfs.h>
#include <audacious/plugin.h> 

#include "ape.h"

#define ENABLE_DEBUG 0

/* The earliest and latest file formats supported by this library */
#define APE_MIN_VERSION 3950
#define APE_MAX_VERSION 3990

#define MAC_FORMAT_FLAG_8_BIT                 1 // is 8-bit [OBSOLETE]
#define MAC_FORMAT_FLAG_CRC                   2 // uses the new CRC32 error detection [OBSOLETE]
#define MAC_FORMAT_FLAG_HAS_PEAK_LEVEL        4 // uint32 nPeakLevel after the header [OBSOLETE]
#define MAC_FORMAT_FLAG_24_BIT                8 // is 24-bit [OBSOLETE]
#define MAC_FORMAT_FLAG_HAS_SEEK_ELEMENTS    16 // has the number of seek elements after the peak level
#define MAC_FORMAT_FLAG_CREATE_WAV_HEADER    32 // create the wave header on decompression (not stored)

#define MAC_SUBFRAME_SIZE 4608

#define APE_EXTRADATA_SIZE 6

/* APE tags */
#define APE_TAG_VERSION               2000
#define APE_TAG_FOOTER_BYTES          32
#define APE_TAG_FLAG_CONTAINS_HEADER  (1 << 31)
#define APE_TAG_FLAG_IS_HEADER        (1 << 29)

#define TAG(name, field)  {name, offsetof(AVFormatContext, field), sizeof(((AVFormatContext *)0)->field)}


static uint16_t get_le16(VFSFile *vfd)
{
    unsigned char tmp[2];
    
    if(aud_vfs_fread(tmp, 1, 2, vfd) != 2) return -1;
    return tmp[0] | (tmp[1] << 8);
}

static uint32_t get_le32(VFSFile *vfd)
{
    unsigned char tmp[4];
    
    if(aud_vfs_fread(tmp, 1, 4, vfd) != 4) return -1;
    return tmp[0] | (tmp[1] << 8) | (tmp[2] << 16) | (tmp[3] << 24);
}

#ifdef DEBUG
static void ape_dumpinfo(APEContext * ape_ctx)
{
    int i;

    av_log(NULL, AV_LOG_DEBUG, "Descriptor Block:\n\n");
    av_log(NULL, AV_LOG_DEBUG, "magic                = \"%c%c%c%c\"\n", ape_ctx->magic[0], ape_ctx->magic[1], ape_ctx->magic[2], ape_ctx->magic[3]);
    av_log(NULL, AV_LOG_DEBUG, "fileversion          = %d\n", ape_ctx->fileversion);
    av_log(NULL, AV_LOG_DEBUG, "descriptorlength     = %d\n", ape_ctx->descriptorlength);
    av_log(NULL, AV_LOG_DEBUG, "headerlength         = %d\n", ape_ctx->headerlength);
    av_log(NULL, AV_LOG_DEBUG, "seektablelength      = %d\n", ape_ctx->seektablelength);
    av_log(NULL, AV_LOG_DEBUG, "wavheaderlength      = %d\n", ape_ctx->wavheaderlength);
    av_log(NULL, AV_LOG_DEBUG, "audiodatalength      = %d\n", ape_ctx->audiodatalength);
    av_log(NULL, AV_LOG_DEBUG, "audiodatalength_high = %d\n", ape_ctx->audiodatalength_high);
    av_log(NULL, AV_LOG_DEBUG, "wavtaillength        = %d\n", ape_ctx->wavtaillength);
    av_log(NULL, AV_LOG_DEBUG, "md5                  = ");
    for (i = 0; i < 16; i++)
         av_log(NULL, AV_LOG_DEBUG, "%02x", ape_ctx->md5[i]);
    av_log(NULL, AV_LOG_DEBUG, "\n");

    av_log(NULL, AV_LOG_DEBUG, "\nHeader Block:\n\n");

    av_log(NULL, AV_LOG_DEBUG, "compressiontype      = %d\n", ape_ctx->compressiontype);
    av_log(NULL, AV_LOG_DEBUG, "formatflags          = %d\n", ape_ctx->formatflags);
    av_log(NULL, AV_LOG_DEBUG, "blocksperframe       = %d\n", ape_ctx->blocksperframe);
    av_log(NULL, AV_LOG_DEBUG, "finalframeblocks     = %d\n", ape_ctx->finalframeblocks);
    av_log(NULL, AV_LOG_DEBUG, "totalframes          = %d\n", ape_ctx->totalframes);
    av_log(NULL, AV_LOG_DEBUG, "bps                  = %d\n", ape_ctx->bps);
    av_log(NULL, AV_LOG_DEBUG, "channels             = %d\n", ape_ctx->channels);
    av_log(NULL, AV_LOG_DEBUG, "samplerate           = %d\n", ape_ctx->samplerate);

    av_log(NULL, AV_LOG_DEBUG, "\nSeektable\n\n");
    if ((ape_ctx->seektablelength / sizeof(uint32_t)) != ape_ctx->totalframes) {
        av_log(NULL, AV_LOG_DEBUG, "No seektable\n");
    } else {
        for (i = 0; i < ape_ctx->seektablelength / sizeof(uint32_t); i++) {
            if (i < ape_ctx->totalframes - 1) {
                av_log(NULL, AV_LOG_DEBUG, "%8d   %d (%d bytes)\n", i, ape_ctx->seektable[i], ape_ctx->seektable[i + 1] - ape_ctx->seektable[i]);
            } else {
                av_log(NULL, AV_LOG_DEBUG, "%8d   %d\n", i, ape_ctx->seektable[i]);
            }
        }
    }

    av_log(NULL, AV_LOG_DEBUG, "\nFrames\n\n");
    for (i = 0; i < ape_ctx->totalframes; i++)
        av_log(NULL, AV_LOG_DEBUG, "%8d   %8lld %8d (%d samples)\n", i, ape_ctx->frames[i].pos, ape_ctx->frames[i].size, ape_ctx->frames[i].nblocks);

    av_log(NULL, AV_LOG_DEBUG, "\nCalculated information:\n\n");
    av_log(NULL, AV_LOG_DEBUG, "junklength           = %d\n", ape_ctx->junklength);
    av_log(NULL, AV_LOG_DEBUG, "firstframe           = %d\n", ape_ctx->firstframe);
    av_log(NULL, AV_LOG_DEBUG, "totalsamples         = %d\n", ape_ctx->totalsamples);
}
#else
#define ape_dumpinfo(a) ;
#endif

int ape_read_header(APEContext *ape, VFSFile *pb, int probe_only)
{
    /*ByteIOContext *pb = &s->pb;
    APEContext *ape = s->priv_data;
    AVStream *st;*/
    uint32_t tag;
    int i;
    //int total_blocks;
    //int64_t pts;

    /* TODO: Skip any leading junk such as id3v2 tags */
    ape->junklength = 0;

    tag = get_le32(pb);
    if (tag != MKTAG('M', 'A', 'C', ' '))
        return -1;

    ape->fileversion = get_le16(pb);

    if (ape->fileversion < APE_MIN_VERSION || ape->fileversion > APE_MAX_VERSION) {
#ifdef DEBUG
        fprintf(stderr, "ape.c: ape_read_header(): Unsupported file version - %d.%02d\n",
	        ape->fileversion / 1000, (ape->fileversion % 1000) / 10);
#endif
        return -1;
    }

    if (ape->fileversion >= 3980) {
        ape->padding1             = get_le16(pb);
        ape->descriptorlength     = get_le32(pb);
        ape->headerlength         = get_le32(pb);
        ape->seektablelength      = get_le32(pb);
        ape->wavheaderlength      = get_le32(pb);
        ape->audiodatalength      = get_le32(pb);
        ape->audiodatalength_high = get_le32(pb);
        ape->wavtaillength        = get_le32(pb);

        aud_vfs_fread(ape->md5, 1, 16, pb);

        /* Skip any unknown bytes at the end of the descriptor.
           This is for future compatibility */
        if (ape->descriptorlength > 52)
	    aud_vfs_fseek(pb, ape->descriptorlength - 52, SEEK_CUR);

        /* Read header data */
        ape->compressiontype      = get_le16(pb);
        ape->formatflags          = get_le16(pb);
        ape->blocksperframe       = get_le32(pb);
        ape->finalframeblocks     = get_le32(pb);
        ape->totalframes          = get_le32(pb);
        ape->bps                  = get_le16(pb);
        ape->channels             = get_le16(pb);
        ape->samplerate           = get_le32(pb);
    } else {
        ape->descriptorlength = 0;
        ape->headerlength = 32;

        ape->compressiontype      = get_le16(pb);
        ape->formatflags          = get_le16(pb);
        ape->channels             = get_le16(pb);
        ape->samplerate           = get_le32(pb);
        ape->wavheaderlength      = get_le32(pb);
        ape->wavtaillength        = get_le32(pb);
        ape->totalframes          = get_le32(pb);
        ape->finalframeblocks     = get_le32(pb);

        if (ape->formatflags & MAC_FORMAT_FLAG_HAS_PEAK_LEVEL) {
	    aud_vfs_fseek(pb, 4, SEEK_CUR); /* Skip the peak level */
            ape->headerlength += 4;
        }

        if (ape->formatflags & MAC_FORMAT_FLAG_HAS_SEEK_ELEMENTS) {
            ape->seektablelength = get_le32(pb);
            ape->headerlength += 4;
            ape->seektablelength *= sizeof(int32_t);
        } else
            ape->seektablelength = ape->totalframes * sizeof(int32_t);

        if (ape->formatflags & MAC_FORMAT_FLAG_8_BIT)
            ape->bps = 8;
        else if (ape->formatflags & MAC_FORMAT_FLAG_24_BIT)
            ape->bps = 24;
        else
            ape->bps = 16;

        if (ape->fileversion >= 3950)
            ape->blocksperframe = 73728 * 4;
        else if (ape->fileversion >= 3900 || (ape->fileversion >= 3800  && ape->compressiontype >= 4000))
            ape->blocksperframe = 73728;
        else
            ape->blocksperframe = 9216;

        /* Skip any stored wav header */
        if (!(ape->formatflags & MAC_FORMAT_FLAG_CREATE_WAV_HEADER))
	    aud_vfs_fseek(pb, ape->wavheaderlength, SEEK_CUR);
    }

    if(ape->totalframes > UINT_MAX / sizeof(APEFrame)){
#ifdef DEBUG
        fprintf(stdout, "Too many frames: %d\n", ape->totalframes);
#endif
        return -1;
    }
    
    if(!probe_only) {
      ape->frames = malloc(ape->totalframes * sizeof(APEFrame));
      if(!ape->frames)
          return AVERROR_NOMEM;
    }

    ape->firstframe   = ape->junklength + ape->descriptorlength + ape->headerlength + ape->seektablelength + ape->wavheaderlength;
    ape->currentframe = 0;

    ape->totalsamples = ape->finalframeblocks;
    if (ape->totalframes > 1)
        ape->totalsamples += ape->blocksperframe * (ape->totalframes - 1);
    
    if (!probe_only) {
        if (ape->seektablelength > 0) {
            ape->seektable = malloc(ape->seektablelength);
            for (i = 0; i < ape->seektablelength / sizeof(uint32_t); i++)
                ape->seektable[i] = get_le32(pb);
        }

        ape->frames[0].pos     = ape->firstframe;
        ape->frames[0].nblocks = ape->blocksperframe;
        ape->frames[0].skip    = 0;
        for (i = 1; i < ape->totalframes; i++) {
            ape->frames[i].pos      = ape->seektable[i]; //ape->frames[i-1].pos + ape->blocksperframe;
            ape->frames[i].nblocks  = ape->blocksperframe;
            ape->frames[i - 1].size = ape->frames[i].pos - ape->frames[i - 1].pos;
            ape->frames[i].skip     = (ape->frames[i].pos - ape->frames[0].pos) & 3;
        }
        ape->frames[ape->totalframes - 1].size    = ape->finalframeblocks * 4;
        ape->frames[ape->totalframes - 1].nblocks = ape->finalframeblocks;

        ape->max_packet_size = 0;
        
        for (i = 0; i < ape->totalframes; i++) {
            if(ape->frames[i].skip){
                ape->frames[i].pos  -= ape->frames[i].skip;
                ape->frames[i].size += ape->frames[i].skip;
            }
            ape->frames[i].size = (ape->frames[i].size + 3) & ~3;
    	    // Eugene:
    	    ape->max_packet_size = MAX(ape->max_packet_size, ape->frames[i].size + 8);
    	    // why +8? look in ape_read_packet()
        }
        ape_dumpinfo(ape);

    } // !probe_only

    //ape->total_blocks = (ape->totalframes == 0) ? 0 : ((ape->totalframes - 1) * ape->blocksperframe) + ape->finalframeblocks;
    ape->frame_size = MAC_SUBFRAME_SIZE;
    //ape->duration = (uint64_t) total_blocks * AV_TIME_BASE / ape->samplerate;
    ape->duration = (uint64_t) ape->totalsamples * AV_TIME_BASE / ape->samplerate;

#ifdef DEBUG
    fprintf(stderr, "Decoding file - v%4.2f, compression level %d, duration %llu msec, %d frames, %d blocks per frame, %s\n",
            ape->fileversion / 1000.0, ape->compressiontype, (unsigned long long)ape->duration,
	    ape->totalframes, ape->blocksperframe, ape->channels == 2 ? "stereo" : "mono");
#endif

    //av_set_pts_info(st, 64, MAC_SUBFRAME_SIZE, ape->samplerate);

    /*pts = 0;
    for (i = 0; i < ape->totalframes; i++) {
        ape->frames[i].pts = pts;
        av_add_index_entry(st, ape->frames[i].pos, ape->frames[i].pts, 0, 0, AVINDEX_KEYFRAME);
        pts += ape->blocksperframe / MAC_SUBFRAME_SIZE;
    }*/

    return 0;
}

/* buffer must be allocated before in conformity to ape->max_packet_size. Eugene. */
int ape_read_packet(APEContext *ape, VFSFile *pb, uint8_t *pkt, int *pkt_size)
{
    int ret;
    int nblocks;
    uint32_t extra_size = 8;

    if (aud_vfs_feof(pb))
        return AVERROR_IO;
    if (ape->currentframe > ape->totalframes)
        return AVERROR_IO;

    aud_vfs_fseek(pb, ape->frames[ape->currentframe].pos, SEEK_SET);

    /* Calculate how many blocks there are in this frame */
    if (ape->currentframe == (ape->totalframes - 1))
        nblocks = ape->finalframeblocks;
    else
        nblocks = ape->blocksperframe;

    /*if (av_new_packet(pkt,  ape->frames[ape->currentframe].size + extra_size) < 0)
        return AVERROR_NOMEM;*/

    AV_WL32(pkt    , nblocks);
    AV_WL32(pkt + 4, ape->frames[ape->currentframe].skip);

    ret = aud_vfs_fread(pkt + extra_size, 1, ape->frames[ape->currentframe].size, pb);

    /*pkt->pts = ape->frames[ape->currentframe].pts;
    pkt->stream_index = 0;*/

    /* note: we need to modify the packet size here to handle the last
       packet */
    /*pkt->size = ret + extra_size;*/

    ape->currentframe++;
    *pkt_size = ape->frames[ape->currentframe].size + extra_size;

    return 0;
}

int ape_read_close(APEContext *ape)
{
    if(ape->frames != NULL) free(ape->frames);
    if(ape->seektable != NULL) free(ape->seektable);
    return 0;
}

/*
static int ape_read_seek(AVFormatContext *s, int stream_index, int64_t timestamp, int flags)
{
    AVStream *st = s->streams[stream_index];
    APEContext *ape = s->priv_data;
    int index = av_index_search_timestamp(st, timestamp, flags);

    if (index < 0)
        return -1;

    ape->currentframe = index;
    return 0;
}*/

