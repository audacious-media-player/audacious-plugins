/* 
 * Audacious Monkey's Audio plugin, an APE tag reading stuff
 *
 * Copyright (C) Eugene Zagidullin 2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 *  
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 *  
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110, USA
 *
 */

#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 

#include <glib.h>
#include <mowgli.h>
#include <audacious/vfs.h>
#include <audacious/plugin.h> 

#include "ape.h"
#include "apev2.h"

#define TMP_BUFSIZE 256
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

mowgli_dictionary_t* parse_apev2_tag(VFSFile *vfd) {
  unsigned char tmp[TMP_BUFSIZE+1];
  unsigned char tmp2[TMP_BUFSIZE+1];
  guint64 signature;
  int tag_version;
  long tag_size, item_size;
  int item_flags;
  int tag_items;
  int tag_flags;
  mowgli_dictionary_t *dict;

  aud_vfs_fseek(vfd, -32, SEEK_END);
  signature = get_le64(vfd);
  if (signature != MKTAG64('A', 'P', 'E', 'T', 'A', 'G', 'E', 'X')) {
#ifdef DEBUG
    fprintf(stderr, "** demac: apev2.c: APE tag not found\n");
#endif
    return NULL;
  }
  
  tag_version = get_le32(vfd);
  tag_size = get_le32(vfd);
  tag_items = get_le32(vfd);
  tag_flags = get_le32(vfd);
#ifdef DEBUG
  fprintf(stderr, "** demac: apev2.c: found APE tag version %d, size %ld, contains %d items, flags %08x\n",
         tag_version, tag_size, tag_items, tag_flags);
#endif
  if(tag_items == 0) {
#ifdef DEBUG
    fprintf(stderr, "** demac: apev2.c: found empty tag\n");
#endif
    return NULL;
  }
  
  dict = mowgli_dictionary_create(g_ascii_strcasecmp);

  aud_vfs_fseek(vfd, -tag_size, SEEK_END);
  int i;
  unsigned char *p;
  for(i=0; i<tag_items; i++) {
      item_size = get_le32(vfd);
      item_flags = get_le32(vfd);
#ifdef DEBUG
      fprintf(stderr, "** demac: apev2.c: item %d, size %ld, flags %d\n", i, item_size, item_flags);
#endif
      
      /* read key */
      for(p = tmp; p <= tmp+TMP_BUFSIZE; p++) {
        aud_vfs_fread(p, 1, 1, vfd);
	if(*p == '\0') break;
      }
      *(p+1) = '\0';

      /* read item */
      aud_vfs_fread(tmp2, 1, MIN(item_size, TMP_BUFSIZE), vfd);
      tmp2[item_size] = '\0';
#ifdef DEBUG
      fprintf(stderr, "%s: \"%s\", f:%08x\n", tmp, tmp2, item_flags);
#endif
      /* APEv2 stores all items in utf-8 */
      gchar *item = ((tag_version == 1000 ) ? aud_str_to_utf8((gchar*)tmp2) : g_strdup((gchar*)tmp2));
      
      mowgli_dictionary_add(dict, (char*)tmp, item);
  }
  
  return dict;
}
