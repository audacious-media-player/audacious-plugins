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

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

#include <libaudcore/plugin.h>
#include <libaudcore/i18n.h>

struct format_info {
    int format;
    int frequency;
    int channels;
};

extern struct format_info input;

extern VFSFile *output_file;
extern uint64_t offset;
extern Tuple tuple;

typedef int (*write_output_callback)(void *ptr, int length);

typedef struct _FileWriter
{
    void (*init)(write_output_callback write_output_func);
    void (*configure)(void);
    int (*open)(void);
    void (*write)(void *ptr, int length);
    void (*close)(void);
    int (*format_required)(int fmt);
} FileWriter;

#endif
