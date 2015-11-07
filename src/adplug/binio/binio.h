/* -*-C++-*-
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
 * binio.h - Binary stream I/O classes
 * Copyright (C) 2002, 2003 Simon Peter <dn.tlp@gmx.net>
 */

#ifndef H_BINIO_BINIO
#define H_BINIO_BINIO

/***** Configuration *****/

// BINIO_ENABLE_STRING - Build std::string supporting methods
//
// Set to 1 to build std::string supporting methods. You need the STL to
// do this.
#define BINIO_ENABLE_STRING     1

/***** Implementation *****/

#if BINIO_ENABLE_STRING
#include <string>
#endif

class binio
{
public:
  typedef enum {
    BigEndian   = 1 << 0,
    FloatIEEE   = 1 << 1
  } Flag;

  typedef enum {
    NoError     = 0,
    Fatal       = 1 << 0,
    Unsupported = 1 << 1,
    NotOpen     = 1 << 2,
    Denied      = 1 << 3,
    NotFound    = 1 << 4,
    Eof         = 1 << 5
  } ErrorCode;

  typedef enum { Set, Add, End } Offset;
  typedef enum { Single, Double } FType;
  typedef int Error;

  binio();
  virtual ~binio();

  void setFlag(Flag f, bool set = true);
  bool getFlag(Flag f);

  Error error();
  bool eof();

  virtual void seek(long, Offset = Set) = 0;
  virtual long pos() = 0;

protected:
  typedef long long     Int;
  typedef long double   Float;
  typedef unsigned char Byte;   // has to be unsigned!

  typedef int           Flags;

  Flags                 my_flags;
  static const Flags    system_flags;
  Error                 err;

  union FloatData {
    Byte   b[8];
    double d;
    float  f;
  };
};

class binistream: virtual public binio
{
public:
  binistream();
  virtual ~binistream();

  Int readInt(unsigned int size);
  Float readFloat(FType ft);
  unsigned long readString(char *str, unsigned long amount);
  unsigned long readString(char *str, unsigned long maxlen, const char delim);
#if BINIO_ENABLE_STRING
  std::string readString(const char delim = '\0');
#endif

  Int peekInt(unsigned int size);
  Float peekFloat(FType ft);

  bool ateof();
  void ignore(unsigned long amount = 1);

protected:
  virtual Byte getByte() = 0;
};

class binostream: virtual public binio
{
public:
  binostream();
  virtual ~binostream();

  void writeInt(Int val, unsigned int size);
  void writeFloat(Float f, FType ft);
  unsigned long writeString(const char *str, unsigned long amount = 0);
#if BINIO_ENABLE_STRING
  unsigned long writeString(const std::string &str);
#endif

protected:
  virtual void putByte(Byte) = 0;
};

class binstream: public binistream, public binostream
{
public:
  binstream();
  virtual ~binstream();
};

#endif
