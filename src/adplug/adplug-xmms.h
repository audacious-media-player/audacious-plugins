/*
   AdPlug/XMMS - AdPlug XMMS Plugin
   Copyright (C) 2002, 2003 Simon Peter <dn.tlp@gmx.net>

   AdPlug/XMMS is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This plugin is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this plugin; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ADPLUG_XMMS_H
#define ADPLUG_XMMS_H

#include <libbinio/binio.h>
#include <adplug/fprovide.h>

#include <libaudcore/vfs.h>

class vfsistream : public binistream
{
public:
  vfsistream(VFSFile *fd = nullptr) :
    fd(fd) {}

  vfsistream(std::string &file)
  {
    if ((own = VFSFile(file.c_str(), "r")))
      fd = &own;
    else
      err |= NotFound;
  }

  Byte getByte()
  {
    Byte b = (Byte)-1;
    if (fd->fread(&b, 1, 1) != 1)
      err |= Eof;
    return b;
  }

  void seek(long pos, Offset offs = Set)
  {
    VFSSeekType wh = (offs == Add) ? VFS_SEEK_CUR : (offs == End) ? VFS_SEEK_END : VFS_SEEK_SET;
    if (fd->fseek (pos, wh))
      err |= Eof;
  }

  long pos()
  {
    return fd->ftell ();
  }

  unsigned long size()
  {
    return aud::max(fd->fsize(), (int64_t)0);
  }

private:
  VFSFile *fd = nullptr;
  VFSFile own;
};

class CFileVFSProvider : public CFileProvider
{
public:
  CFileVFSProvider(VFSFile &file) :
    m_file(file) {}

  binistream *open(std::string filename) const
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

  void close(binistream *f) const
  {
    delete f;
  }

private:
  VFSFile &m_file;
};

#endif
