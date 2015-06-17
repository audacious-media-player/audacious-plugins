#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/vfs.h>
#include <libaudcore/runtime.h>

#include "ayemu.h"
#include "vtx.h"

/* Read 8-bit integer from file.
 * Return 1 if error occurs
 */
static int read_byte(VFSFile &fp, int *p)
{
  unsigned char c;
  if (fp.fread(&c, 1, 1) != 1) {
    AUDERR("read_byte() error\n");
    return 1;
  }
  *p = c;
  return 0;
}

/* Read 16-bit integer from file.
 * Return 1 if error occurs
 */
static int read_word16(VFSFile &fp, int *p)
{
  unsigned char c[2];
  if (fp.fread(&c, 1, 2) != 2) {
    AUDERR("read_word16() error\n");
    return 1;
  }
  *p = c[0] | (c[1] << 8);
  return 0;
}

/* read 32-bit integer from file.
 *  Returns 1 if error occurs
 */
static int read_word32(VFSFile &fp, int32_t *p)
{
  unsigned char c[4];
  if (fp.fread(&c, 1, 4) != 4) {
    AUDERR("read_word32() error\n");
    return 1;
  }
  *p = c[0] | (c[1] << 8) | (c[2] << 16) | (c[3] << 24);
  return 0;
}

/* read_NTstring: reads null-terminated string from file.
 *  Return 1 if error occures.
 */
static int read_NTstring(VFSFile &fp, char s[])
{
  int i, n = 0;
  unsigned char c;
  for (i = 0 ; i < AYEMU_VTX_NTSTRING_MAX && (n = fp.fread(&c, 1, 1)) == 1 && c ; i++)
    s[i] = c;
  s[i] = '\0';
  if (n != 1) {
    AUDERR("unexpected end of file!\n");
    return 1;
  }
  return 0;
}

/** Open specified .vtx file and read vtx file header
 *
 *  Open specified .vtx file and read vtx file header in struct vtx
 *  Return value: true if success, else false
 */
bool ayemu_vtx_t::read_header(VFSFile &file)
{
  char buf[2];
  int error = 0;
  int32_t int_regdata_size;

  if (file.fread(buf, 2, 1) != 1) {
    AUDERR("Can't read from %s\n", file.filename());
    error = 1;
  }

  if (!strcmp_nocase(buf, "ay", 2))
    hdr.chiptype = AYEMU_AY;
  else if (!strcmp_nocase(buf, "ym", 2))
    hdr.chiptype = AYEMU_YM;
  else {
    AUDERR("File %s is _not_ VORTEX format!\nIt not begins from AY or YM.\n", file.filename());
    error = 1;
  }

  /* read VTX header info in order format specified, see http:// ..... */
  if (!error) error = read_byte(file, &hdr.stereo);
  if (!error) error = read_word16(file, &hdr.loop);
  if (!error) error = read_word32(file, &hdr.chipFreq);
  if (!error) error = read_byte(file, &hdr.playerFreq);
  if (!error) error = read_word16(file, &hdr.year);
  if (!error) {
    error = read_word32(file, &int_regdata_size);
    hdr.regdata_size = int_regdata_size;
  }

  if (!error) error = read_NTstring(file, hdr.title);
  if (!error) error = read_NTstring(file, hdr.author);
  if (!error) error = read_NTstring(file, hdr.from);
  if (!error) error = read_NTstring(file, hdr.tracker);
  if (!error) error = read_NTstring(file, hdr.comment);

  return !error;
}

/** Read and encode lha data from .vtx file
 *
 * Return value: pointer to unpacked data or nullptr
 * Note: you must call ayemu_vtx_open() first.
 */
bool ayemu_vtx_t::load_data(VFSFile &file)
{
  /* read packed AY register data to end of file. */
  Index<char> packed_data = file.read_all();

  regdata.resize(hdr.regdata_size);
  if (!lh5_decode(packed_data, regdata))
    return false;

  pos = 0;
  return true;
}

/** Get next 14-bytes frame of AY register data.
 *
 * Return value: true if success, false if not enough data.
 */
bool ayemu_vtx_t::get_next_frame(unsigned char *regs)
{
  int numframes = hdr.regdata_size / 14;
  if (pos++ >= numframes)
    return 0;
  else {
    int n;
    unsigned char *p = &regdata[pos];
    for(n = 0 ; n < 14 ; n++, p+=numframes)
      regs[n] = *p;
    return 1;
  }
}

/** Print formatted file name. If fmt is nullptr the default format %a - %t will be used
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
StringBuf ayemu_vtx_t::sprintname (const char *fmt)
{
  static const char *stereo_types[] = { "MONO", "ABC", "ACB", "BAC", "BCA", "CAB", "CBA" };

  if (!fmt)
    fmt = "%a - %t";

  StringBuf buf(0);

  while (*fmt) {
    if (*fmt == '%') {
      switch(*++fmt) {
      case 'a':
        buf.insert(-1, hdr.author);
        break;
      case 't':
        buf.insert(-1, hdr.title);
        break;
      case 'y':
        buf.combine(int_to_str(hdr.year));
        break;
      case 'f':
        buf.insert(-1, hdr.from);
        break;
      case 'T':
        buf.insert(-1, hdr.tracker);
        break;
      case 'C':
        buf.insert(-1, hdr.comment);
        break;
      case 's':
        buf.insert(-1, stereo_types[hdr.stereo]);
        break;
      case 'l':
        buf.insert(-1, (hdr.loop)? "looped" : "non-looped" );
        break;
      case 'c':
        buf.insert(-1, (hdr.chiptype == AYEMU_AY)? "AY" : "YM" );
        break;
      case 'F':
        buf.combine(int_to_str(hdr.chipFreq));
        break;
      case 'P':
        buf.combine(int_to_str(hdr.playerFreq));
        break;
      default:
        buf.insert(-1, fmt, 1);
      }
      fmt++;
    } else {
      const char *p = strchr(fmt, '%');
      if (!p)
        p = fmt + strlen(fmt);

      buf.insert(-1, fmt, p - fmt);
      fmt = p;
    }
  }

  return buf;
}
