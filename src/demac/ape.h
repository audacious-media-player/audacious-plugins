/*
 * Monkey's Audio lossless audio decoder, common header,
 * some libav* compatibility stuff
 * Copyright (c) 2007 Benjamin Zores <ben@geexbox.org>
 *  based upon libdemac from Dave Chapman.
 * Copyright (c) 2007 Eugene Zagidullin <e.asphyx@gmail.com>
 *   Cleanup libav* depending code, Audacious stuff.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 *  
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 *  
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110, USA
 */

#ifndef APE_H
#define APE_H

#define FFMIN(a,b) ((a) < (b) ? (a) : (b))

#ifdef DEBUG
#define av_log(a, b, ...) fprintf(stderr, __VA_ARGS__)
#else
#define av_log(a, b, ...) ;
#endif

#define MKTAG(a,b,c,d)           (a | (b << 8) | (c << 16) | (d << 24))
#define MKTAG64(a,b,c,d,e,f,g,h) ((uint64_t)a | ((uint64_t)b << 8) | ((uint64_t)c << 16) | ((uint64_t)d << 24) | \
                                 ((uint64_t)e << 32) | ((uint64_t)f << 40) | ((uint64_t)g << 48) | ((uint64_t)h << 56))

#define AV_TIME_BASE 1000
#define AV_WL16(a,b) { \
          ((uint8_t*)(a))[0] =  (uint16_t)(b) & 0x00ff;        \
          ((uint8_t*)(a))[1] = ((uint16_t)(b) & 0xff00) >> 8; \
	}
#define AV_WL32(a,b) { \
          ((uint8_t*)(a))[0] = ((uint32_t)(b) & 0x000000ff);       \
          ((uint8_t*)(a))[1] = ((uint32_t)(b) & 0x0000ff00) >> 8;  \
          ((uint8_t*)(a))[2] = ((uint32_t)(b) & 0x00ff0000) >> 16; \
          ((uint8_t*)(a))[3] = ((uint32_t)(b) & 0xff000000) >> 24; \
	}

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define BLOCKS_PER_LOOP     4608
#define MAX_CHANNELS        2
//#define MAX_BYTESPERSAMPLE  3

#define APE_FRAMECODE_MONO_SILENCE    1
#define APE_FRAMECODE_STEREO_SILENCE  3
#define APE_FRAMECODE_PSEUDO_STEREO   4

#define HISTORY_SIZE 512
#define PREDICTOR_ORDER 8
/** Total size of all predictor histories */
#define PREDICTOR_SIZE 50

#define YDELAYA (18 + PREDICTOR_ORDER*4)
#define YDELAYB (18 + PREDICTOR_ORDER*3)
#define XDELAYA (18 + PREDICTOR_ORDER*2)
#define XDELAYB (18 + PREDICTOR_ORDER)

#define YADAPTCOEFFSA 18
#define XADAPTCOEFFSA 14
#define YADAPTCOEFFSB 10
#define XADAPTCOEFFSB 5

#define APE_FILTER_LEVELS 3

#define AVERROR_IO -1
#define AVERROR_NOMEM -1

typedef struct {
    int64_t pos;
    int nblocks;
    int size;
    int skip;
    int64_t pts;
} APEFrame;

typedef struct {
    /* Derived fields */
    uint32_t junklength;
    uint32_t firstframe;
    uint32_t totalsamples;
    int currentframe;
    APEFrame *frames;

    /* Info from Descriptor Block */
    char magic[4];
    int16_t fileversion;
    int16_t padding1;
    uint32_t descriptorlength;
    uint32_t headerlength;
    uint32_t seektablelength;
    uint32_t wavheaderlength;
    uint32_t audiodatalength;
    uint32_t audiodatalength_high;
    uint32_t wavtaillength;
    uint8_t md5[16];

    /* Info from Header Block */
    uint16_t compressiontype;
    uint16_t formatflags;
    uint32_t blocksperframe;
    uint32_t finalframeblocks;
    uint32_t totalframes;
    uint16_t bps;
    uint16_t channels;
    uint32_t samplerate;

    /* Seektable */
    uint32_t *seektable;

    /* Added by Eugene: */
    uint32_t frame_size;
    //uint64_t total_blocks;
    uint64_t duration;
    uint32_t max_packet_size;
} APEContext;

/** Filters applied to the decoded data */
typedef struct APEFilter {
    int16_t *coeffs;        ///< actual coefficients used in filtering
    int16_t *adaptcoeffs;   ///< adaptive filter coefficients used for correcting of actual filter coefficients
    int16_t *historybuffer; ///< filter memory
    int16_t *delay;         ///< filtered values

    int avg;
} APEFilter;

typedef struct APERice {
    uint32_t k;
    uint32_t ksum;
} APERice;

typedef struct APERangecoder {
    uint32_t low;           ///< low end of interval
    uint32_t range;         ///< length of interval
    uint32_t help;          ///< bytes_to_follow resp. intermediate value
    unsigned int buffer;    ///< buffer for input/output
} APERangecoder;

/** Filter histories */
typedef struct APEPredictor {
    int32_t *buf;

    int32_t lastA[2];

    int32_t filterA[2];
    int32_t filterB[2];

    int32_t coeffsA[2][4];  ///< adaption coefficients
    int32_t coeffsB[2][5];  ///< adaption coefficients
    int32_t historybuffer[HISTORY_SIZE + PREDICTOR_SIZE];
} APEPredictor;

/** Decoder context */
typedef struct APEDecoderContext {
    //AVCodecContext *avctx;
    APEContext *apectx;
    //DSPContext dsp;
    int channels;
    int samples;                             ///< samples left to decode in current frame

    int fileversion;                         ///< codec version, very important in decoding process
    int compression_level;                   ///< compression levels
    int fset;                                ///< which filter set to use (calculated from compression level)
    int flags;                               ///< global decoder flags

    uint32_t CRC;                            ///< frame CRC
    int frameflags;                          ///< frame flags
    int currentframeblocks;                  ///< samples (per channel) in current frame
    int blocksdecoded;                       ///< count of decoded samples in current frame
    APEPredictor predictor;                  ///< predictor used for final reconstruction

    int32_t decoded0[BLOCKS_PER_LOOP];       ///< decoded data for the first channel
    int32_t decoded1[BLOCKS_PER_LOOP];       ///< decoded data for the second channel

    int16_t* filterbuf[APE_FILTER_LEVELS];   ///< filter memory

    APERangecoder rc;                        ///< rangecoder used to decode actual values
    APERice riceX;                           ///< rice code parameters for the second channel
    APERice riceY;                           ///< rice code parameters for the first channel
    APEFilter filters[APE_FILTER_LEVELS][2]; ///< filters used for reconstruction

    uint8_t *data;                           ///< current frame data
    uint8_t *data_end;                       ///< frame data end
    uint8_t *ptr;                            ///< current position in frame data
    uint8_t *last_ptr;                       ///< position where last 4608-sample block ended
    /*Eugene:*/
    unsigned int max_packet_size;            // Avoid multiply realloc calls
} APEDecoderContext;


static inline uint8_t bytestream_get_byte(uint8_t** ptr) {
  uint8_t tmp;
  tmp = **ptr;
  *ptr += 1;
  return tmp;
}

/*static inline uint32_t bytestream_get_be32(uint8_t** ptr) {
  uint32_t tmp;
  tmp = *((uint32_t*)*ptr);
  *ptr += 4;
  return tmp;
}*/

static inline uint32_t bytestream_get_be32(uint8_t** ptr) {
  uint32_t tmp;
  tmp = (*ptr)[3] | ((*ptr)[2] << 8) | ((*ptr)[1] << 16) | ((*ptr)[0] << 24);
  *ptr += 4;
  return tmp;
}

#ifdef ARCH_X86_64
#  define LEGACY_REGS "=Q"
#else
#  define LEGACY_REGS "=q"
#endif

static inline uint32_t bswap_32(uint32_t x)
{
#if defined(ARCH_X86)
#if __CPU__ != 386
 __asm("bswap   %0":
      "=r" (x)     :
#else
 __asm("xchgb   %b0,%h0\n"
      "         rorl    $16,%0\n"
      "         xchgb   %b0,%h0":
      LEGACY_REGS (x)                :
#endif
      "0" (x));
#elif defined(ARCH_SH4)
        __asm__(
        "swap.b %0,%0\n"
        "swap.w %0,%0\n"
        "swap.b %0,%0\n"
        :"=r"(x):"0"(x));
#elif defined(ARCH_ARM)
    uint32_t t;
    __asm__ (
      "eor %1, %0, %0, ror #16 \n\t"
      "bic %1, %1, #0xFF0000   \n\t"
      "mov %0, %0, ror #8      \n\t"
      "eor %0, %0, %1, lsr #8  \n\t"
      : "+r"(x), "+r"(t));
#elif defined(ARCH_BFIN)
    unsigned tmp;
    asm("%1 = %0 >> 8 (V);\n\t"
        "%0 = %0 << 8 (V);\n\t"
        "%0 = %0 | %1;\n\t"
        "%0 = PACK(%0.L, %0.H);\n\t"
        : "+d"(x), "=&d"(tmp));
#else
    x= ((x<<8)&0xFF00FF00) | ((x>>8)&0x00FF00FF);
    x= (x>>16) | (x<<16);
#endif
    return x;
}

static inline void bswap_buf(uint32_t *dst, uint32_t *src, int w){
    int i;

    for(i=0; i+8<=w; i+=8){
        dst[i+0]= bswap_32(src[i+0]);
        dst[i+1]= bswap_32(src[i+1]);
        dst[i+2]= bswap_32(src[i+2]);
        dst[i+3]= bswap_32(src[i+3]);
        dst[i+4]= bswap_32(src[i+4]);
        dst[i+5]= bswap_32(src[i+5]);
        dst[i+6]= bswap_32(src[i+6]);
        dst[i+7]= bswap_32(src[i+7]);
    }
    for(;i<w; i++){
        dst[i+0]= bswap_32(src[i+0]);
    }
}

static inline int16_t av_clip_int16(int a) {
    if ((a+32768) & ~65535) return (a>>31) ^ 32767;
    else                    return a;
}

uint16_t get_le16(VFSFile *vfd);
uint32_t get_le32(VFSFile *vfd);
int put_le32(uint32_t val, VFSFile *vfd);
uint64_t get_le64(VFSFile *vfd);

int ape_read_header(APEContext *ape, VFSFile *pb, int probe_only);
int ape_read_packet(APEContext *ape, VFSFile *pb, uint8_t *pkt, int *pkt_size);
int ape_read_close(APEContext *ape);

int ape_decode_init(APEDecoderContext *s, APEContext *ctx);
int ape_decode_frame(APEDecoderContext *s,
                            void *data, int *data_size,
                            uint8_t * buf, int buf_size);

int ape_decode_close(APEDecoderContext *s);

#endif // APE_H
