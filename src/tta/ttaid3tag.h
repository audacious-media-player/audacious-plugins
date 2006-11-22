/*
 * id3tag.h
 *
 * Description:	ID3 functions prototypes
 *
 */

#pragma pack(1)

#define MAX_LINE 4096
//#define ID3_VERSION 3

/* ID3 common headers set */

#define TIT2	1
#define TPE1	2
#define TALB	3
#define TRCK	4
#define TYER	5
#define TCON	6
#define COMM	7

/* ID3 tag checked flags */

#define ID3_UNSYNCHRONISATION_FLAG		0x80
#define ID3_EXTENDEDHEADER_FLAG			0x40
#define ID3_EXPERIMENTALTAG_FLAG		0x20
#define ID3_FOOTERPRESENT_FLAG			0x10

/* ID3 frame checked flags */

#define FRAME_COMPRESSION_FLAG			0x0008
#define FRAME_ENCRYPTION_FLAG			0x0004
#define FRAME_UNSYNCHRONISATION_FLAG	0x0002

/* ID3 field text encoding */

#define FIELD_TEXT_ISO_8859_1	0x00
#define FIELD_TEXT_UTF_16		0x01
#define FIELD_TEXT_UTF_16BE		0x02
#define FIELD_TEXT_UTF_8		0x03

#define GENRES	148

typedef struct {
	unsigned char  id[3];
	unsigned char  title[30];
	unsigned char  artist[30];
	unsigned char  album[30];
	unsigned char  year[4];
	unsigned char  comment[28];
	unsigned char  zero;
	unsigned char  track;
	unsigned char  genre;
} id3v1_tag;

typedef struct {
	unsigned char  id[3];
	unsigned short version;
	unsigned char  flags;
	unsigned char  size[4];
} id3v2_tag;

typedef struct {
	unsigned char  id[4];
	unsigned char  size[4];
	unsigned short flags;
} id3v2_frame;

typedef struct {
	unsigned char  name[31];
	unsigned char  title[31];
	unsigned char  artist[31];
	unsigned char  album[31];
	unsigned char  comment[31];
	unsigned char  year[5];
	unsigned char  track;
	unsigned char  genre;
	unsigned char  id3has;
} id3v1_data;

typedef struct {
	unsigned char  name[MAX_LINE];
	unsigned char  title[MAX_LINE];
	unsigned char  artist[MAX_LINE];
	unsigned char  album[MAX_LINE];
	unsigned char  comment[MAX_LINE];
	unsigned char  year[5];
	unsigned char  track[3];
	unsigned char  genre[256];
	unsigned char  id3has;
	unsigned long  size;
} id3v2_data;


