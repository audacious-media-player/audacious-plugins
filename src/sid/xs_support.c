/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Miscellaneous support functions

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2007 Tecnic Software productions (TNSP)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <ctype.h>
#include <fcntl.h>
#include <string.h>

#include "xs_support.h"

#define __AUDACIOUS_NEWVFS__

guint16 xs_fread_be16(VFSFile *f)
{
    return (((guint16) vfs_getc(f)) << 8) | ((guint16) vfs_getc(f));
}


guint32 xs_fread_be32(VFSFile *f)
{
    return (((guint32) vfs_getc(f)) << 24) |
        (((guint32) vfs_getc(f)) << 16) |
        (((guint32) vfs_getc(f)) << 8) |
        ((guint32) vfs_getc(f));
}


/* Copy a given string over in *result.
 */
gint xs_pstrcpy(gchar **result, const gchar *str)
{
    /* Check the string pointers */
    if (!result || !str)
        return -1;

    /* Allocate memory for destination */
    if (*result)
        g_free(*result);
    *result = (gchar *) g_malloc(strlen(str) + 1);
    if (!*result)
        return -2;

    /* Copy to the destination */
    strcpy(*result, str);

    return 0;
}


/* Concatenates a given string into string pointed by *result.
 */
gint xs_pstrcat(gchar **result, const gchar *str)
{
    /* Check the string pointers */
    if (!result || !str)
        return -1;

    if (*result != NULL) {
        *result = (gchar *) g_realloc(*result, strlen(*result) + strlen(str) + 1);
        if (*result == NULL)
            return -1;
        strcat(*result, str);
    } else {
        *result = (gchar *) g_malloc(strlen(str) + 1);
        if (*result == NULL)
            return -1;
        strcpy(*result, str);
    }

    return 0;
}


void xs_findnext(const gchar *str, size_t *pos)
{
    while (str[*pos] && isspace(str[*pos]))
        (*pos)++;
}


void xs_findeol(const gchar *str, size_t *pos)
{
    while (str[*pos] && (str[*pos] != '\n') && (str[*pos] != '\r'))
        (*pos)++;
}


void xs_findnum(const gchar *str, size_t *pos)
{
    while (str[*pos] && isdigit(str[*pos]))
        (*pos)++;
}

