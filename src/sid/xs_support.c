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
#include <stdlib.h>
#include <string.h>

#include <glib.h>

uint16_t xs_fread_be16(VFSFile *f)
{
    uint16_t val;
    if (vfs_fread (& val, 1, sizeof val, f) != sizeof val)
        return 0;

    return GUINT16_FROM_BE (val);
}


uint32_t xs_fread_be32(VFSFile *f)
{
    uint32_t val;
    if (vfs_fread (& val, 1, sizeof val, f) != sizeof val)
        return 0;

    return GUINT32_FROM_BE (val);
}


/* Copy a given string over in *result.
 */
int xs_pstrcpy(char **result, const char *str)
{
    /* Check the string pointers */
    if (!result || !str)
        return -1;

    /* Allocate memory for destination */
    if (*result)
        free(*result);
    *result = (char *) malloc(strlen(str) + 1);
    if (!*result)
        return -2;

    /* Copy to the destination */
    strcpy(*result, str);

    return 0;
}


/* Concatenates a given string into string pointed by *result.
 */
int xs_pstrcat(char **result, const char *str)
{
    /* Check the string pointers */
    if (!result || !str)
        return -1;

    if (*result != NULL) {
        *result = (char *) realloc(*result, strlen(*result) + strlen(str) + 1);
        if (*result == NULL)
            return -1;
        strcat(*result, str);
    } else {
        *result = (char *) malloc(strlen(str) + 1);
        if (*result == NULL)
            return -1;
        strcpy(*result, str);
    }

    return 0;
}


void xs_findnext(const char *str, size_t *pos)
{
    while (str[*pos] && isspace(str[*pos]))
        (*pos)++;
}


void xs_findeol(const char *str, size_t *pos)
{
    while (str[*pos] && (str[*pos] != '\n') && (str[*pos] != '\r'))
        (*pos)++;
}


void xs_findnum(const char *str, size_t *pos)
{
    while (str[*pos] && isdigit(str[*pos]))
        (*pos)++;
}

