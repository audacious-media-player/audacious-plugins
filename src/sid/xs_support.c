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


#ifndef __AUDACIOUS_NEWVFS__
/* File handling
 */
t_xs_file *xs_fopen(const gchar *path, const gchar *mode)
{
	return fopen(path, mode);
}


gint xs_fclose(t_xs_file *f)
{
	return fclose(f);
}


gint xs_fgetc(t_xs_file *f)
{
	return fgetc(f);
}


size_t xs_fread(void *p, size_t s, size_t n, t_xs_file *f)
{
	return fread(p, s, n, f);
}


gint xs_feof(t_xs_file *f)
{
	return feof(f);
}


gint xs_ferror(t_xs_file *f)
{
	return ferror(f);
}


glong xs_ftell(t_xs_file *f)
{
	return ftell(f);
}


gint xs_fseek(t_xs_file *f, glong o, gint w)
{
	return fseek(f, o, w);
}
#endif


guint16 xs_fread_be16(t_xs_file *f)
{
	return (((guint16) xs_fgetc(f)) << 8) | ((guint16) xs_fgetc(f));
}


guint32 xs_fread_be32(t_xs_file *f)
{
	return (((guint32) xs_fgetc(f)) << 24) |
		(((guint32) xs_fgetc(f)) << 16) |
		(((guint32) xs_fgetc(f)) << 8) |
		((guint32) xs_fgetc(f));
}


/* Load a file to a buffer, return 0 on success, negative value on error
 */
gint xs_fload_buffer(const gchar *pcFilename, guint8 **buf, size_t *bufSize)
{
	t_xs_file *f;
	glong seekPos;
	
	/* Open file, get file size */
	if ((f = xs_fopen(pcFilename, "rb")) == NULL)
		return -1;

	xs_fseek(f, 0, SEEK_END);
	seekPos = xs_ftell(f);
	
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
		xs_fseek(f, 0, SEEK_SET);
		readSize = xs_fread(*buf, sizeof(guint8), *bufSize, f);
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
gchar *xs_strncpy(gchar *pDest, gchar *pSource, size_t n)
{
	gchar *s, *d;
	size_t i;

	/* Check the string pointers */
	if (!pSource || !pDest)
		return pDest;

	/* Copy to the destination */
	i = n;
	s = pSource;
	d = pDest;
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
	pDest[n - 1] = 0;

	return pDest;
}


/* Copy a given string over in *ppResult.
 */
gint xs_pstrcpy(gchar **ppResult, const gchar *pStr)
{
	/* Check the string pointers */
	if (!ppResult || !pStr)
		return -1;

	/* Allocate memory for destination */
	if (*ppResult)
		g_free(*ppResult);
	*ppResult = (gchar *) g_malloc(strlen(pStr) + 1);
	if (!*ppResult)
		return -2;

	/* Copy to the destination */
	strcpy(*ppResult, pStr);

	return 0;
}


/* Concatenates a given string into string pointed by *ppResult.
 */
gint xs_pstrcat(gchar **ppResult, const gchar *pStr)
{
	/* Check the string pointers */
	if (!ppResult || !pStr)
		return -1;

	if (*ppResult != NULL) {
		*ppResult = (gchar *) g_realloc(*ppResult, strlen(*ppResult) + strlen(pStr) + 1);
		if (*ppResult == NULL)
			return -1;
		strcat(*ppResult, pStr);
	} else {
		*ppResult = (gchar *) g_malloc(strlen(pStr) + 1);
		if (*ppResult == NULL)
			return -1;
		strcpy(*ppResult, pStr);
	}

	return 0;
}


/* Concatenate a given string up to given dest size or \n.
 * If size max is reached, change the end to "..."
 */
void xs_pnstrcat(gchar *pDest, size_t iSize, gchar *pStr)
{
	size_t i, n;
	gchar *s, *d;

	d = pDest;
	i = 0;
	while (*d && (i < iSize)) {
		i++;
		d++;
	}

	s = pStr;
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
gchar *xs_strrchr(gchar *pcStr, gchar ch)
{
	gchar *lastPos = NULL;

	while (*pcStr) {
		if (*pcStr == ch)
			lastPos = pcStr;
		pcStr++;
	}

	return lastPos;
}


void xs_findnext(gchar *pcStr, size_t *piPos)
{
	while (pcStr[*piPos] && isspace(pcStr[*piPos]))
		(*piPos)++;
}


void xs_findeol(gchar *pcStr, size_t *piPos)
{
	while (pcStr[*piPos] && (pcStr[*piPos] != '\n') && (pcStr[*piPos] != '\r'))
		(*piPos)++;
}


void xs_findnum(gchar *pcStr, size_t *piPos)
{
	while (pcStr[*piPos] && isdigit(pcStr[*piPos]))
		(*piPos)++;
}


#ifndef HAVE_MEMSET
void *xs_memset(void *p, int c, size_t n)
{
	guint8 *dp;

	dp = (guint8 *) p;
	while (n--)
		*(dp++) = c;

	return p;
}
#endif
