#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>

#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#include "ayemu.h"

/* defined in lh5dec.c */
extern void lh5_decode(unsigned char *inp,unsigned char *outp,unsigned long original_size, unsigned long packed_size);


/* Read 8-bit integer from file.
 * Return 1 if error occurs
 */
static int read_byte(VFSFile *fp, int *p)
{
  int c;
  if ((c = vfs_getc(fp)) == EOF) {
    perror("libayemu: read_byte()");
    return 1;
  }
  *p = c;
  return 0;
}

/* Read 16-bit integer from file.
 * Return 1 if error occurs
 */
static int read_word16(VFSFile *fp, int *p)
{
  int c;
  if ((c = vfs_getc(fp)) == EOF) {
    perror("libayemu: read_word16()");
    return 1;
  }
  *p = c;

  if ((c = vfs_getc(fp)) == EOF) {
    perror("libayemu: read_word16()");
    return 1;
  }
  *p += c << 8;

  return 0;
}

/* read 32-bit integer from file.
 *  Returns 1 if error occurs
 */
static int read_word32(VFSFile *fp, int32_t *p)
{
  int c;

  if ((c = vfs_getc(fp)) == EOF) {
    perror("libayemu: read_word32()");
    return 1;
  }
  *p = c;

  if ((c = vfs_getc(fp)) == EOF) {
    perror("libayemu: read_word32()");
    return 1;
  }
  *p += c << 8;

  if ((c = vfs_getc(fp)) == EOF) {
    perror("libayemu: read_word32()");
    return 1;
  }
  *p += c << 16;

  if ((c = vfs_getc(fp)) == EOF) {
    perror("libayemu: read_word32()");
    return 1;
  }
  *p += c << 24;

  return 0;
}

/* read_NTstring: reads null-terminated string from file.
 *  Return 1 if error occures.
 */
static int read_NTstring(VFSFile *fp, char s[])
{
  int i, c;
  for (i = 0 ; i < AYEMU_VTX_NTSTRING_MAX && (c = vfs_getc(fp)) != EOF && c ; i++)
    s[i] = c;
  s[i] = '\0';
  if (c == EOF) {
    fprintf(stderr, "libayemu: read_NTstring(): uninspected end of file!\n");
    return 1;
  }
  return 0;
}

/** Open specified .vtx file and read vtx file header
 *
 *  Open specified .vtx file and read vtx file header in struct vtx
 *  Return value: true if success, else false
 */
int ayemu_vtx_open (ayemu_vtx_t *vtx, const char *filename)
{
  char buf[2];
  int error = 0;
  int32_t int_regdata_size;

  vtx->regdata = NULL;

  if ((vtx->fp = vfs_fopen (filename, "rb")) == NULL) {
    fprintf(stderr, "ayemu_vtx_open: Cannot open file %s: %s\n", filename, strerror(errno));
    return 0;
  }

  if (vfs_fread(buf, 2, 1, vtx->fp) != 1) {
    fprintf(stderr,"ayemu_vtx_open: Can't read from %s: %s\n", filename, strerror(errno));
    error = 1;
  }

  buf[0] = tolower(buf[0]);
  buf[1] = tolower(buf[1]);

  if (strncmp(buf, "ay", 2) == 0)
    vtx->hdr.chiptype = AYEMU_AY;
  else if (strncmp (buf, "ym", 2) == 0)
    vtx->hdr.chiptype = AYEMU_YM;
  else {
    fprintf (stderr, "File %s is _not_ VORTEX format!\nIt not begins from AY or YM.\n", filename);
    error = 1;
  }

  /* read VTX header info in order format specified, see http:// ..... */
  if (!error) error = read_byte(vtx->fp, &vtx->hdr.stereo);
  if (!error) error = read_word16(vtx->fp, &vtx->hdr.loop);
  if (!error) error = read_word32(vtx->fp, &vtx->hdr.chipFreq);
  if (!error) error = read_byte(vtx->fp, &vtx->hdr.playerFreq);
  if (!error) error = read_word16(vtx->fp, &vtx->hdr.year);
  if (!error) {
	error = read_word32(vtx->fp, &int_regdata_size);
	vtx->hdr.regdata_size = (size_t) int_regdata_size;
  }

  if (!error) error = read_NTstring(vtx->fp, vtx->hdr.title);
  if (!error) error = read_NTstring(vtx->fp, vtx->hdr.author);
  if (!error) error = read_NTstring(vtx->fp, vtx->hdr.from);
  if (!error) error = read_NTstring(vtx->fp, vtx->hdr.tracker);
  if (!error) error = read_NTstring (vtx->fp, vtx->hdr.comment);

  if (error) {
    vfs_fclose(vtx->fp);
    vtx->fp = NULL;
  }
  return !error;
}

/** Read and encode lha data from .vtx file
 *
 * Return value: pointer to unpacked data or NULL
 * Note: you must call ayemu_vtx_open() first.
 */
char *ayemu_vtx_load_data (ayemu_vtx_t *vtx)
{
  char *packed_data;
  size_t packed_size;
  size_t buf_alloc;
  int c;

  if (vtx->fp == NULL) {
    fprintf(stderr, "ayemu_vtx_load_data: tune file not open yet (do you call ayemu_vtx_open first?)\n");
    return NULL;
  }
  packed_size = 0;
  buf_alloc = 4096;
  packed_data = (char *) malloc (buf_alloc);
  /* read packed AY register data to end of file. */
  while ((c = vfs_getc (vtx->fp)) != EOF) {
    if (buf_alloc < packed_size) {
      buf_alloc *= 2;
      packed_data = (char *) realloc (packed_data, buf_alloc);
      if (packed_data == NULL) {
	fprintf (stderr, "ayemu_vtx_load_data: Packed data out of memory!\n");
	vfs_fclose (vtx->fp);
	return NULL;
      }
    }
    packed_data[packed_size++] = c;
  }
  vfs_fclose (vtx->fp);
  vtx->fp = NULL;
  if ((vtx->regdata = (char *) malloc (vtx->hdr.regdata_size)) == NULL) {
    fprintf (stderr, "ayemu_vtx_load_data: Can allocate %d bytes for unpack register data\n", (int)(vtx->hdr.regdata_size));
    free (packed_data);
    return NULL;
  }
  lh5_decode ((unsigned char *)packed_data, (unsigned char *)(vtx->regdata), vtx->hdr.regdata_size, packed_size);
  free (packed_data);
  vtx->pos = 0;
  return vtx->regdata;
}

/** Get next 14-bytes frame of AY register data.
 *
 * Return value: 1 if sucess, 0 if no enought data.
 */
int ayemu_vtx_get_next_frame (ayemu_vtx_t *vtx, char *regs)
{
  int numframes = vtx->hdr.regdata_size / 14;
  if (vtx->pos++ >= numframes)
    return 0;
  else {
    int n;
    char *p = vtx->regdata + vtx->pos;
    for(n = 0 ; n < 14 ; n++, p+=numframes)
      regs[n] = *p;
    return 1;
  }
}

static void append_string(char *buf, const int sz, const char *str)
{
  if (strlen(buf) + strlen(str) < sz - 1)
    strcat(buf, str);
}

static void append_number(char *buf, const int sz, const int num)
{
  char s[32];
  str_itoa (num, s, sizeof s);
  append_string(buf, sz, s);
}

static void append_char(char *buf, const int sz, const char c)
{
  int pos = strlen(buf);
  if (pos < sz - 1)
    buf[pos++] = c;
  buf[pos] = '\0';
}

/** Print formated file name. If fmt is NULL the default format %a - %t will used
 *
 * %% the % sign
 * %a author of song
 * %t song title
 * %y year
 * %f song from
 * %T Tracker
 * %C Comment
 * %s stereo type (ABC, BCA, ...)
 * %l 'looped' or 'non-looped'
 * %c chip type: 'AY' or 'YM'
 * %F chip Freq
 * %P player freq
 */
void ayemu_vtx_sprintname (const ayemu_vtx_t *vtx, char *const buf, const int sz, const char *fmt)
{
  static char *stereo_types[] = { "MONO", "ABC", "ACB", "BAC", "BCA", "CAB", "CBA" };

  if (fmt == NULL)
    fmt = "%a - %t";

  buf[0] = '\0';

  while (*fmt != '\0') {
    if (*fmt == '%') {
      switch(*++fmt) {
      case 'a':
	append_string(buf, sz, vtx->hdr.author);
	break;
      case 't':
	append_string(buf, sz, vtx->hdr.title);
	break;
      case 'y':
	append_number(buf, sz, vtx->hdr.year);
	break;
      case 'f':
	append_string(buf, sz, vtx->hdr.from);
	break;
      case 'T':
	append_string(buf, sz, vtx->hdr.tracker);
	break;
      case 'C':
	append_string(buf, sz, vtx->hdr.comment);
	break;
      case 's':
	append_string(buf, sz, stereo_types[vtx->hdr.stereo]);
	break;
      case 'l':
	append_string(buf, sz, (vtx->hdr.loop)? "looped" : "non-looped" );
	break;
      case 'c':
	append_string(buf, sz, (vtx->hdr.chiptype == AYEMU_AY)? "AY" : "YM" );
	break;
      case 'F':
	append_number(buf, sz, vtx->hdr.chipFreq);
	break;
      case 'P':
	append_number(buf, sz, vtx->hdr.playerFreq);
	break;
      default:
	append_char(buf, sz, *fmt);
      }
      fmt++;
    } else {
      append_char(buf, sz, *fmt++);
    }
  }
}

/** Free all of allocaded resource for this file.
 *
 * Free the unpacket register data if is and close file.
 */
void ayemu_vtx_free (ayemu_vtx_t *vtx)
{
  if (vtx->fp) {
    vfs_fclose(vtx->fp);
    vtx->fp = NULL;
  }

  if (vtx->regdata) {
    free(vtx->regdata);
    vtx->regdata = NULL;
  }
}
