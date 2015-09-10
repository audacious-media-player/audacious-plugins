/* extractiong lh5 module
   (c) Haruhiko Okumura
   (m) Roman Scherbakov
*/
#include <string.h>   /* memmove */
#include <limits.h>

#include <libaudcore/runtime.h>

#include "vtx.h"

static unsigned short bitbuf;

#define BITBUFSIZ (CHAR_BIT * sizeof bitbuf)

#define DICBIT    13    /* 12(-lh4-) or 13(-lh5-) */
#define DICSIZ   (1UL << DICBIT)
#define MATCHBIT   8    /* bits for MAXMATCH - THRESHOLD */
#define MAXMATCH 256    /* formerly F (not more than unsigned char_MAX + 1) */
#define THRESHOLD  3    /* choose optimal value */
#define NC (UCHAR_MAX + MAXMATCH + 2 - THRESHOLD)
#define CBIT 9          /* $\lfloor \log_2 NC \rfloor + 1$ */
#define CODE_BIT  16    /* codeword length */
#define NP (DICBIT + 1)
#define NT (CODE_BIT + 3)
#define PBIT 4          /* smallest integer such that (1U << PBIT) > NP */
#define TBIT 5          /* smallest integer such that (1U << TBIT) > NT */
#if NT > NP
#define NPT NT
#else
#define NPT NP
#endif

static unsigned long origsize, compsize;
static const unsigned char *in_buf;
static unsigned char *out_buf;

static unsigned short  subbitbuf;
static int   bitcount;

static unsigned short left[2 * NC - 1], right[2 * NC - 1];
static unsigned char  c_len[NC], pt_len[NPT];
static unsigned short   blocksize;

static unsigned short c_table[4096], pt_table[256];

static int j;  /* remaining bytes to copy */

class DecodeError {}; // exception

static void error(const char *msg)
{
  AUDERR("%s\n", msg);
  throw DecodeError();
}

static void fillbuf(int n)  /* Shift bitbuf n bits left, read n bits */
{
  bitbuf <<= n;
  while (n > bitcount) {
    bitbuf |= subbitbuf << (n -= bitcount);
    if (compsize != 0) {
      compsize--;  subbitbuf = *in_buf++;
    } else subbitbuf = 0;
    bitcount = CHAR_BIT;
  }
  bitbuf |= subbitbuf >> (bitcount -= n);
}

static unsigned short getbits(int n)
{
  unsigned short x;

  x = bitbuf >> (BITBUFSIZ - n);  fillbuf(n);
  return x;
}

// make table for decoding

static void make_table(int nchar, unsigned char bitlen[], int tablebits, unsigned short table[])
{
  unsigned short count[17], weight[17], start[18], *p;
  unsigned short i, k, len, ch, jutbits, avail, nextcode, mask;

  for (i = 1; i <= 16; i++) count[i] = 0;
  for (i = 0; i < nchar; i++) count[bitlen[i]]++;

  start[1] = 0;
  for (i = 1; i <= 16; i++)
    start[i + 1] = start[i] + (count[i] << (16 - i));
  if (start[17] != (unsigned short)(1U << 16)) error("Bad table");

  jutbits = 16 - tablebits;
  for (i = 1; i <= tablebits; i++) {
    start[i] >>= jutbits;
    weight[i] = 1U << (tablebits - i);
  }
  while (i <= 16) {
      weight[i] = 1U << (16 - i);
      i++;
  }

  i = start[tablebits + 1] >> jutbits;
  if (i != (unsigned short)(1U << 16)) {
    k = 1U << tablebits;
    while (i != k) table[i++] = 0;
  }

  avail = nchar;
  mask = 1U << (15 - tablebits);
  for (ch = 0; ch < nchar; ch++) {
    if ((len = bitlen[ch]) == 0) continue;
    nextcode = start[len] + weight[len];
    if (len <= tablebits) {
      for (i = start[len]; i < nextcode; i++) table[i] = ch;
    } else {
      k = start[len];
      p = &table[k >> jutbits];
      i = len - tablebits;
      while (i != 0) {
	if (*p == 0) {
	  right[avail] = left[avail] = 0;
	  *p = avail++;
	}
	if (k & mask) p = &right[*p];
	else          p = &left[*p];
	k <<= 1;  i--;
      }
      *p = ch;
    }
    start[len] = nextcode;
  }
}

// static Huffman

static void read_pt_len(int nn, int nbit, int i_special)
{
  int i, c, n;
  unsigned short mask;

  n = getbits(nbit);
  if (n == 0) {
    c = getbits(nbit);
    for (i = 0; i < nn; i++) pt_len[i] = 0;
    for (i = 0; i < 256; i++) pt_table[i] = c;
  } else {
    i = 0;
    while (i < n) {
      c = bitbuf >> (BITBUFSIZ - 3);
      if (c == 7) {
	mask = 1U << (BITBUFSIZ - 1 - 3);
	while (mask & bitbuf) {  mask >>= 1;  c++;  }
      }
      fillbuf((c < 7) ? 3 : c - 3);
      pt_len[i++] = c;
      if (i == i_special) {
	c = getbits(2);
	while (--c >= 0) pt_len[i++] = 0;
      }
    }
    while (i < nn) pt_len[i++] = 0;
    make_table(nn, pt_len, 8, pt_table);
  }
}

static void read_c_len()
{
  int i, c, n;
  unsigned short mask;

  n = getbits(CBIT);
  if (n == 0) {
    c = getbits(CBIT);
    for (i = 0; i < NC; i++) c_len[i] = 0;
    for (i = 0; i < 4096; i++) c_table[i] = c;
  } else {
    i = 0;
    while (i < n) {
      c = pt_table[bitbuf >> (BITBUFSIZ - 8)];
      if (c >= NT) {
	mask = 1U << (BITBUFSIZ - 1 - 8);
	do {
	  if (bitbuf & mask) c = right[c];
	  else               c = left [c];
	  mask >>= 1;
	} while (c >= NT);
      }
      fillbuf(pt_len[c]);
      if (c <= 2) {
	if      (c == 0) c = 1;
	else if (c == 1) c = getbits(4) + 3;
	else             c = getbits(CBIT) + 20;
	while (--c >= 0) c_len[i++] = 0;
      } else c_len[i++] = c - 2;
    }
    while (i < NC) c_len[i++] = 0;
    make_table(NC, c_len, 12, c_table);
  }
}


static unsigned short decode_c()
{
  unsigned short j, mask;

  if (blocksize == 0) {
    blocksize = getbits(16);
    read_pt_len(NT, TBIT, 3);
    read_c_len();
    read_pt_len(NP, PBIT, -1);
  }
  blocksize--;
  j = c_table[bitbuf >> (BITBUFSIZ - 12)];
  if (j >= NC) {
    mask = 1U << (BITBUFSIZ - 1 - 12);
    do {
      if (bitbuf & mask) j = right[j];
      else               j = left [j];
      mask >>= 1;
    } while (j >= NC);
  }
  fillbuf(c_len[j]);
  return j;
}


static unsigned short decode_p()
{
  unsigned short j, mask;

  j = pt_table[bitbuf >> (BITBUFSIZ - 8)];
  if (j >= NP) {
    mask = 1U << (BITBUFSIZ - 1 - 8);
    do {
      if (bitbuf & mask) j = right[j];
      else               j = left [j];
      mask >>= 1;
    } while (j >= NP);
  }
  fillbuf(pt_len[j]);
  if (j != 0) j = (1U << (j - 1)) + getbits(j - 1);
  return j;
}


static void decode(unsigned short count, unsigned char buffer[])
{
  static unsigned short i;
  unsigned short r, c;

  r = 0;
  while (--j >= 0) {
    buffer[r] = buffer[i];
    i = (i + 1) & (DICSIZ - 1);
    if (++r == count) return;
  }
  for ( ; ; ) {
    c = decode_c();
    if (c <= UCHAR_MAX) {
      buffer[r] = c & UCHAR_MAX;
      if (++r == count) return;
    } else {
      j = c - (UCHAR_MAX + 1 - THRESHOLD);
      i = (r - decode_p() - 1) & (DICSIZ - 1);
      while (--j >= 0) {
	buffer[r] = buffer[i];
	i = (i + 1) & (DICSIZ - 1);
	if (++r == count) return;
      }
    }
  }
}

bool lh5_decode(const Index<char> &in, Index<unsigned char> &out)
{
  unsigned short n;
  Index<unsigned char> buffer;

  compsize = in.len();
  origsize = out.len();
  in_buf = (unsigned char *)in.begin();
  out_buf = out.begin();

  buffer.resize(DICSIZ);

  bitbuf = 0;  subbitbuf = 0;  bitcount = 0;
  fillbuf(BITBUFSIZ);
  blocksize = 0;
  j = 0;

  try {
    while (origsize != 0) {
      n = aud::min(DICSIZ, origsize);
      decode(n, buffer.begin());
      memmove(out_buf, buffer.begin(), n);
      out_buf += n;
      origsize -= n;
    }
  } catch (DecodeError &) {
    return false;
  }

  return true;
}
