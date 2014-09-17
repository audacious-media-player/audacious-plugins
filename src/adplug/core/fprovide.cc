/*
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2002 Simon Peter, <dn.tlp@gmx.net>, et al.
 *
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
 * fprovide.cpp - File provider class framework, by Simon Peter <dn.tlp@gmx.net>
 */

#include <string.h>
#include <binio.h>
#include <binfile.h>

#include "fprovide.h"

#include <libaudcore/audstrings.h>

/***** CFileProvider *****/

bool CFileProvider::extension(const std::string &filename,
			      const std::string &extension)
{
  return str_has_suffix_nocase(filename.c_str(), extension.c_str());
}

unsigned long CFileProvider::filesize(binistream *f)
{
  unsigned long oldpos = f->pos(), size;

  f->seek(0, binio::End);
  size = f->pos();
  f->seek(oldpos, binio::Set);

  return size;
}

/***** CProvider_Filesystem *****/

binistream *CProvider_Filesystem::open(VFSFile &fd) const
{
  if(!fd) return 0;

  vfsistream *f = new vfsistream(&fd);

  if(!f) return 0;
  if(f->error()) { delete f; return 0; }

  // Open all files as little endian with IEEE floats by default
  f->setFlag(binio::BigEndian, false); f->setFlag(binio::FloatIEEE);

  return f;
}

void CProvider_Filesystem::close(binistream *f) const
{
  vfsistream *ff = (vfsistream *)f;

  if(f) {
    delete ff;
  }
}
