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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * binio.cpp - Binary stream I/O classes
 * Copyright (C) 2002, 2003 Simon Peter <dn.tlp@gmx.net>
 */

#include <string.h>

#include "binio.h"

/***** Defines *****/

#if BINIO_ENABLE_STRING
// String buffer size for std::string readString() method
#define STRINGBUFSIZE   256
#endif

/***** binio *****/

#ifdef WORDS_BIGENDIAN
const binio::Flags binio::system_flags = BigEndian | FloatIEEE;
#else
const binio::Flags binio::system_flags = FloatIEEE;
#endif

binio::binio()
  : my_flags(system_flags), err(NoError)
{
}

binio::~binio()
{
}

void binio::setFlag(Flag f, bool set)
{
  if(set)
    my_flags |= f;
  else
    my_flags &= !f;
}

bool binio::getFlag(Flag f)
{
  return (my_flags & f ? true : false);
}

binio::Error binio::error()
{
  Error e = err;

  err = NoError;
  return e;
}

bool binio::eof()
{
  return (err & Eof ? true : false);
}

/***** binistream *****/

binistream::binistream()
{
}

binistream::~binistream()
{
}

binistream::Int binistream::readInt(unsigned int size)
{
  unsigned int  i;
  Int           val = 0, in;

  // Check if 'size' doesn't exceed our system's biggest type.
  if(size > sizeof(Int)) {
    err |= Unsupported;
    return 0;
  }

  for(i = 0; i < size; i++) {
    in = getByte();
    if(getFlag(BigEndian))
      val <<= 8;
    else
      in <<= i * 8;
    val |= in;
  }

  return val;
}

binistream::Float binistream::readFloat(FType ft)
{
  if(getFlag(FloatIEEE)) {      // Read IEEE-754 floating-point value
    unsigned int        i, size = 0;
    FloatData           in;
    bool                swap;

    // Determine appropriate size for given type.
    switch(ft) {
    case Single: size = 4; break;       // 32 bits
    case Double: size = 8; break;       // 64 bits
    }

    // Determine byte ordering
    swap = getFlag(BigEndian) ^ (system_flags & BigEndian);

    // Read the float byte by byte, converting endianess
    for(i = 0; i < size; i++)
      if(swap)
        in.b[size - i - 1] = getByte();
      else
        in.b[i] = getByte();

    // Let the hardware do the conversion
    switch(ft) {
    case Single: return in.f;
    case Double: return in.d;
    }
  }

  // User tried to read a (yet) unsupported floating-point type. Bail out.
  err |= Unsupported; return 0.0;
}

unsigned long binistream::readString(char *str, unsigned long maxlen)
{
  unsigned long i;

  for(i = 0; i < maxlen; i++) {
    str[i] = (char)getByte();
    if(err) { str[i] = '\0'; return i; }
  }

  return maxlen;
}

unsigned long binistream::readString(char *str, unsigned long maxlen,
                                     const char delim)
{
  unsigned long i;

  for(i = 0; i < maxlen; i++) {
    str[i] = (char)getByte();
    if(str[i] == delim || err) { str[i] = '\0'; return i; }
  }

  str[maxlen] = '\0';
  return maxlen;
}

#if BINIO_ENABLE_STRING
std::string binistream::readString(const char delim)
{
  char buf[STRINGBUFSIZE + 1];
  std::string tempstr;
  unsigned long read;

  do {
    read = readString(buf, STRINGBUFSIZE, delim);
    tempstr.append(buf, read);
  } while(read == STRINGBUFSIZE);

  return tempstr;
}
#endif

binistream::Int binistream::peekInt(unsigned int size)
{
  Int val = readInt(size);
  if(!err) seek(-(long)size, Add);
  return val;
}

binistream::Float binistream::peekFloat(FType ft)
{
  Float val = readFloat(ft);

  if(!err)
    switch(ft) {
    case Single: seek(-4, Add); break;
    case Double: seek(-8, Add); break;
    }

  return val;
}

bool binistream::ateof()
{
  Error olderr = err;   // Save current error state
  bool  eof_then;

  peekInt(1);
  eof_then = eof();     // Get error state of next byte
  err = olderr;         // Restore original error state
  return eof_then;
}

void binistream::ignore(unsigned long amount)
{
  unsigned long i;

  for(i = 0; i < amount; i++)
    getByte();
}

/***** binostream *****/

binostream::binostream()
{
}

binostream::~binostream()
{
}

void binostream::writeInt(Int val, unsigned int size)
{
  unsigned int  i;

  // Check if 'size' doesn't exceed our system's biggest type.
  if(size > sizeof(Int)) { err |= Unsupported; return; }

  for(i = 0; i < size; i++) {
    if(getFlag(BigEndian))
      putByte((val >> (size - i - 1) * 8) & 0xff);
    else {
      putByte(val & 0xff);
      val >>= 8;
    }
  }
}

void binostream::writeFloat(Float f, FType ft)
{
  if(getFlag(FloatIEEE)) {      // Write IEEE-754 floating-point value
    unsigned int        i, size = 0;
    FloatData           out;
    bool                swap;

    // Hardware could be big or little endian, convert appropriately
    swap = getFlag(BigEndian) ^ (system_flags & BigEndian);

    // Determine appropriate size for given type and convert by hardware
    switch(ft) {
    case Single: size = 4; out.f = f; break;  // 32 bits
    case Double: size = 8; out.d = f; break;  // 64 bits
    }

    // Write the float byte by byte, converting endianess
    for(i = 0; i < size; i++)
      if(swap)
        putByte(out.b[size - i - 1]);
      else
        putByte(out.b[i]);

    return;     // We're done.
  }

  // User tried to write an unsupported floating-point type. Bail out.
  err |= Unsupported;
}

unsigned long binostream::writeString(const char *str, unsigned long amount)
{
  unsigned int i;

  if(!amount) amount = strlen(str);

  for(i = 0; i < amount; i++) {
    putByte(str[i]);
    if(err) return i;
  }

  return amount;
}

#if BINIO_ENABLE_STRING
unsigned long binostream::writeString(const std::string &str)
{
  return writeString(str.c_str());
}
#endif
