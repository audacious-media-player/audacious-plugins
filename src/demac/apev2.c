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
#include <audacious/plugin.h> 

#include "ape.h"
#include "apev2.h"

#define TMP_BUFSIZE 256
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define FLAGS_HEADER_EXISTS (1 << 31)
#define FLAGS_HEADER (1 << 29)
#define APE_SIGNATURE MKTAG64('A', 'P', 'E', 'T', 'A', 'G', 'E', 'X')

/*typedef struct {
  int tag_items;
  int tag_size;
  VFSFile *vfd;
} iterator_pvt_t;*/

mowgli_dictionary_t* parse_apev2_tag(VFSFile *vfd) {
  unsigned char tmp[TMP_BUFSIZE+1];
  unsigned char tmp2[TMP_BUFSIZE+1];
  guint64 signature;
  int tag_version;
  long tag_size, item_size;
  int item_flags;
  int tag_items;
  unsigned int tag_flags;
  mowgli_dictionary_t *dict;

  aud_vfs_fseek(vfd, -32, SEEK_END);
  signature = get_le64(vfd);
  if (signature != APE_SIGNATURE) {
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
      if (item_size > 0 && item_size < tag_size) { /* be bulletproof */
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
  }

  return dict;
}

static void write_header_or_footer(guint32 version, guint32 size, guint32 items, guint32 flags, VFSFile *vfd) {
  guint64 filling = 0;

  aud_vfs_fwrite("APETAGEX", 1, 8, vfd);
  put_le32(version, vfd);
  put_le32(size, vfd);
  put_le32(items, vfd);
  put_le32(flags, vfd);
  aud_vfs_fwrite(&filling, 1, 8, vfd);
}

gboolean write_apev2_tag(VFSFile *vfd, mowgli_dictionary_t *tag) {
  guint64 signature;
  guint32 tag_version;
  guint32 tag_size, tag_items = 0, tag_flags;
  guint32 item_size, item_flags=0;
  long file_size;
  void *current_field;

  if (vfd == NULL || tag == NULL) return FALSE;

  aud_vfs_fseek(vfd, -32, SEEK_END);
  signature = get_le64(vfd);

  /* strip existing tag */
  if (signature == APE_SIGNATURE) {
      tag_version = get_le32(vfd);
      tag_size = get_le32(vfd);
      tag_items = get_le32(vfd);
      tag_flags = get_le32(vfd);
      aud_vfs_fseek(vfd, 0, SEEK_END);
      file_size = aud_vfs_ftell(vfd);
      file_size -= (long)tag_size;

      /* also strip header */
      if((tag_version >= 2000) && (tag_flags | FLAGS_HEADER_EXISTS)) {
          aud_vfs_fseek(vfd, -((long)tag_size)-32, SEEK_END);
          signature = get_le64(vfd); /* be bulletproof: check header also */
          if (signature == APE_SIGNATURE) {
#ifdef DEBUG
              fprintf(stderr, "stripping also header\n");
#endif
              file_size -= 32;
          }
      }
#ifdef DEBUG
      fprintf(stderr, "stripping existing tag\n");
#endif
      if(aud_vfs_truncate(vfd, file_size) < 0) return FALSE;
  }
  aud_vfs_fseek(vfd, 0, SEEK_END);

  mowgli_dictionary_iteration_state_t state;
  
  tag_size = 32; /* footer size */
  /* let's count tag size */
  tag_items = 0;
  MOWGLI_DICTIONARY_FOREACH(current_field, &state, tag) {
          if(strlen((char*)current_field) != 0) {
              tag_items++;
              tag_size += strlen((char*)state.cur->key) + strlen((char*)current_field) + 9; /* length in bytes not symbols */
          }
  }

  if(tag_items == 0) {
#ifdef DEBUG
      fprintf(stderr, "tag stripped, all done\n");
#endif
      return TRUE; /* old tag is stripped, new one is empty */
  }

  write_header_or_footer(2000, tag_size, tag_items, FLAGS_HEADER | FLAGS_HEADER_EXISTS, vfd); /* header */
  MOWGLI_DICTIONARY_FOREACH(current_field, &state, tag) {
          if( (item_size = strlen((char*)current_field)) != 0 ) {
#ifdef DEBUG
              fprintf(stderr, "Writing field %s = %s\n", (char*)state.cur->key, (char*)current_field);
#endif
              put_le32(item_size, vfd);
              put_le32(item_flags, vfd); /* all set to zero */
              aud_vfs_fwrite(state.cur->key, 1, strlen((char*)state.cur->key) + 1, vfd); /* null-terminated */
              aud_vfs_fwrite(current_field, 1, item_size, vfd);
          }
  }
  write_header_or_footer(2000, tag_size, tag_items, FLAGS_HEADER_EXISTS, vfd); /* footer */

  return TRUE;
}

