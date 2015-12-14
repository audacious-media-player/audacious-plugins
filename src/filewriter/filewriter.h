/*  FileWriter-Plugin
 *  (C) copyright 2007 merging of Disk Writer and Out-Lame by Michael Färber
 *
 *  Original Out-Lame-Plugin:
 *  (C) copyright 2002 Lars Siebold <khandha5@gmx.net>
 *  (C) copyright 2006-2007 porting to audacious by Yoshiki Yazawa <yaz@cc.rim.or.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef FILEWRITER_H
#define FILEWRITER_H

#define WANT_AUD_BSWAP
#include <libaudcore/audio.h>
#include <libaudcore/tuple.h>
#include <libaudcore/vfs.h>

struct format_info {
    int format;
    int frequency;
    int channels;
};

struct FileWriterImpl
{
    void (* init) ();
    bool (* open) (VFSFile & file, const format_info & info, const Tuple & tuple);
    void (* write) (VFSFile & file, const void * data, int length);
    void (* close) (VFSFile & file);
    int (* format_required) (int fmt);
};

extern FileWriterImpl wav_plugin;

#ifdef FILEWRITER_MP3
extern FileWriterImpl mp3_plugin;
#endif

#ifdef FILEWRITER_VORBIS
extern FileWriterImpl vorbis_plugin;
#endif

#ifdef FILEWRITER_FLAC
extern FileWriterImpl flac_plugin;
#endif

#endif
