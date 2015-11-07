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
#include "binio.h"
#include "binfile.h"

#include "fprovide.h"
#include "binio_virtual.h"

/***** CFileProvider *****/

bool CFileProvider::extension(const std::string &filename,
                              const std::string &extension)
{
  const char *fname = filename.c_str(), *ext = extension.c_str();

  if(strlen(fname) < strlen(ext) ||
     strcasecmp(fname + strlen(fname) - strlen(ext), ext))
    return false;
  else
    return true;
}

unsigned long CFileProvider::filesize(binistream *f)
{
  return static_cast<vfsistream*>(f)->size();
}

binistream *CFileProvider::open(std::string filename) const
{
  binistream *f;

  if (!strcmp(filename.c_str(), m_file.filename()) && !m_file.fseek(0, VFS_SEEK_SET))
    f = new vfsistream(&m_file);
  else
    f = new vfsistream(filename);

  if(f->error()) { delete f; return 0; }

  // Open all files as little endian with IEEE floats by default
  f->setFlag(binio::BigEndian, false); f->setFlag(binio::FloatIEEE);

  return f;
}
