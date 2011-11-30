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
#include "xs_support.h"
#include <ctype.h>
#include <string.h>

#define __AUDACIOUS_NEWVFS__

guint16 xs_fread_be16(xs_file_t *f)
{
    return (((guint16) xs_fgetc(f)) << 8) | ((guint16) xs_fgetc(f));
}


guint32 xs_fread_be32(xs_file_t *f)
{
    return (((guint32) xs_fgetc(f)) << 24) |
        (((guint32) xs_fgetc(f)) << 16) |
        (((guint32) xs_fgetc(f)) << 8) |
        ((guint32) xs_fgetc(f));
}


/* Load a file to a buffer, return 0 on success, negative value on error
 */
gint xs_fload_buffer(const gchar *filename, guint8 **buf, size_t *bufSize)
{
    xs_file_t *f;
    glong seekPos;

    /* Open file, get file size */
    if ((f = xs_fopen(filename, "rb")) == NULL)
        return -1;

#ifdef __AUDACIOUS_NEWVFS__
    seekPos = vfs_fsize(f);
#else
    xs_fseek(f, 0L, SEEK_END);
    seekPos = xs_ftell(f);
#endif

    if (seekPos > 0) {
        size_t readSize = seekPos;
        if (readSize >= *bufSize || *buf == NULL) {
            /* Only re-allocate if the required size > current */
            if (*buf != NULL) {
                g_free(*buf);
                *buf = NULL;
            }

            *bufSize = seekPos;

            *buf = (guint8 *) g_malloc(*bufSize * sizeof(guint8));
            if (*buf == NULL) {
                xs_fclose(f);
                return -2;
            }
        }

        /* Read data */
        if (xs_fseek(f, 0, SEEK_SET))
            readSize = 0;
        else
            readSize = xs_fread(*buf, 1, *bufSize, f);

        xs_fclose(f);

        if (readSize != *bufSize)
            return -3;
        else
            return 0;
    } else {
        xs_fclose(f);
        return -4;
    }
}


/* Copy a string
 */
gchar *xs_strncpy(gchar *dest, const gchar *src, size_t n)
{
    const gchar *s;
    gchar *d;
    size_t i;

    /* Check the string pointers */
    if (!src || !dest)
        return dest;

    /* Copy to the destination */
    i = n;
    s = src;
    d = dest;
    while (*s && (i > 0)) {
        *(d++) = *(s++);
        i--;
    }

    /* Fill rest of space with zeros */
    while (i > 0) {
        *(d++) = 0;
        i--;
    }

    /* Ensure that last is always zero */
    dest[n - 1] = 0;

    return dest;
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


/* Concatenate a given string up to given dest size or \n.
 * If size max is reached, change the end to "..."
 */
void xs_pnstrcat(gchar *dest, size_t iSize, const gchar *str)
{
    size_t i, n;
    const gchar *s;
    gchar *d;

    d = dest;
    i = 0;
    while (*d && (i < iSize)) {
        i++;
        d++;
    }

    s = str;
    while (*s && (*s != '\n') && (i < iSize)) {
        *d = *s;
        d++;
        s++;
        i++;
    }

    *d = 0;

    if (i >= iSize) {
        i--;
        d--;
        n = 3;
        while ((i > 0) && (n > 0)) {
            *d = '.';
            d--;
            i--;
            n--;
        }
    }
}


/* Locate character in string
 */
gchar *xs_strrchr(gchar *str, const gchar ch)
{
    gchar *lastPos = NULL;

    while (*str) {
        if (*str == ch)
            lastPos = str;
        str++;
    }

    return lastPos;
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

