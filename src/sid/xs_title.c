/*  
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Titlestring handling
   
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
#include "xs_title.h"
#include "xs_support.h"
#include "xs_config.h"
#ifdef AUDACIOUS_PLUGIN
#include <audacious/titlestring.h>
#else
#include <xmms/titlestring.h>
#endif


static void xs_path_split(gchar *path, gchar **tmpFilename, gchar **tmpFilePath, gchar **tmpFileExt)
{
	gchar *tmpStr;
	
	/* Split the filename into path */
	*tmpFilePath = g_strdup(path);
	tmpStr = xs_strrchr(*tmpFilePath, '/');
	if (tmpStr) tmpStr[1] = 0;

	/* Filename */
	*tmpFilename = xs_strrchr(path, '/');
	if (*tmpFilename)
		*tmpFilename = g_strdup(*tmpFilename + 1);
	else
		*tmpFilename = g_strdup(path);

	tmpStr = xs_strrchr(*tmpFilename, '.');
	tmpStr[0] = 0;

	/* Extension */
	*tmpFileExt = xs_strrchr(path, '.');
}


#if defined(HAVE_XMMSEXTRA) || defined(AUDACIOUS_PLUGIN)
/* Tuple support
 */
static TitleInput * xs_get_titletuple(gchar *tmpFilename, gchar *tmpFilePath,
	gchar *tmpFileExt, t_xs_tuneinfo *p, gint subTune)
{
	TitleInput *pResult;
	
#ifdef AUDACIOUS_PLUGIN
	pResult = bmp_title_input_new();
#else
	pResult = (TitleInput *) g_malloc0(sizeof(TitleInput));
	pResult->__size = XMMS_TITLEINPUT_SIZE;
	pResult->__version = XMMS_TITLEINPUT_VERSION;
#endif

	/* Create the input fields */
	pResult->file_name = tmpFilename;
	pResult->file_ext = tmpFileExt;
	pResult->file_path = tmpFilePath;

	pResult->track_name = g_strdup(p->sidName);
	pResult->track_number = subTune;
	pResult->album_name = NULL;
	pResult->performer = g_strdup(p->sidComposer);
	pResult->date = g_strdup((p->sidModel == XS_SIDMODEL_6581) ? "SID6581" : "SID8580");

	pResult->year = 0;
	pResult->genre = g_strdup("SID-tune");
	pResult->comment = g_strdup(p->sidCopyright);
	
	return pResult;
}

#ifdef AUDACIOUS_PLUGIN
TitleInput * xs_make_titletuple(t_xs_tuneinfo *p, gint subTune)
{
	gchar *tmpFilename, *tmpFilePath, *tmpFileExt;

	xs_path_split(p->sidFilename, &tmpFilename, &tmpFilePath, &tmpFileExt);
	
	return xs_get_titletuple(tmpFilename, tmpFilePath, tmpFileExt, p, subTune);
}
#endif
#endif


/*
 * Create a title string based on given information and settings.
 */
#define VPUTCH(MCH)	\
	if (iIndex < XS_BUF_SIZE) tmpBuf[iIndex++] = MCH;

#define VPUTSTR(MSTR) {						\
	if (MSTR) {						\
		if ((iIndex + strlen(MSTR) + 1) < XS_BUF_SIZE) {	\
			strcpy(&tmpBuf[iIndex], MSTR);		\
			iIndex += strlen(MSTR); 		\
		} else						\
			iIndex = XS_BUF_SIZE;			\
	}							\
}


gchar *xs_make_titlestring(t_xs_tuneinfo *p, gint subTune)
{
	gchar *tmpFilename, *tmpFilePath, *tmpFileExt,
		*pcStr, *pcResult, tmpStr[XS_BUF_SIZE], tmpBuf[XS_BUF_SIZE];
	t_xs_subtuneinfo *subInfo;
	gint iIndex;

	/* Get filename parts */
	xs_path_split(p->sidFilename, &tmpFilename,
		&tmpFilePath, &tmpFileExt);
	
	/* Get sub-tune information */
	if ((subTune > 0) && (subTune <= p->nsubTunes)) {
		subInfo = &(p->subTunes[subTune - 1]);
	} else
		subInfo = NULL;


	/* Check if the titles are overridden or not */
#if defined(HAVE_XMMSEXTRA) || defined(AUDACIOUS_PLUGIN)
	if (!xs_cfg.titleOverride) {
		TitleInput *pTuple = xs_get_titletuple(
			tmpFilename, tmpFilePath, tmpFileExt, p, subTune);
		
		pcResult = xmms_get_titlestring(xmms_get_gentitle_format(), pTuple);

		g_free(pTuple->track_name);
		g_free(pTuple->album_name);
		g_free(pTuple->performer);
		g_free(pTuple->date);
		g_free(pTuple->genre);
		g_free(pTuple->comment);
		g_free(pTuple);
	} else 
#endif
	{
		/* Create the string */
		pcStr = xs_cfg.titleFormat;
		iIndex = 0;
		while (*pcStr && (iIndex < XS_BUF_SIZE)) {
			if (*pcStr == '%') {
				pcStr++;
				switch (*pcStr) {
				case '%':
					VPUTCH('%');
					break;
				case 'f':
					VPUTSTR(tmpFilename);
					break;
				case 'F':
					VPUTSTR(tmpFilePath);
					break;
				case 'e':
					VPUTSTR(tmpFileExt);
					break;
				case 'p':
					VPUTSTR(p->sidComposer);
					break;
				case 't':
					VPUTSTR(p->sidName);
					break;
				case 'c':
					VPUTSTR(p->sidCopyright);
					break;
				case 's':
					VPUTSTR(p->sidFormat);
					break;
				case 'm':
					switch (p->sidModel) {
					case XS_SIDMODEL_6581:
						VPUTSTR("6581");
						break;
					case XS_SIDMODEL_8580:
						VPUTSTR("8580");
						break;
					case XS_SIDMODEL_ANY:
						VPUTSTR("ANY");
						break;
					default:
						VPUTSTR("?");
						break;
					}
					break;
				case 'C':
					if (subInfo && (subInfo->tuneSpeed > 0)) {
						switch (subInfo->tuneSpeed) {
						case XS_CLOCK_PAL:
							VPUTSTR("PAL");
							break;
						case XS_CLOCK_NTSC:
							VPUTSTR("NTSC");
							break;
						case XS_CLOCK_ANY:
							VPUTSTR("ANY");
							break;
						case XS_CLOCK_VBI:
							VPUTSTR("VBI");
							break;
						case XS_CLOCK_CIA:
							VPUTSTR("CIA");
							break;
						default:
							g_snprintf(tmpStr, XS_BUF_SIZE,
							"%iHz", subInfo->tuneSpeed);
							VPUTSTR(tmpStr);
						}
					} else
						VPUTSTR("?");
					break;
				case 'n':
					g_snprintf(tmpStr, XS_BUF_SIZE, "%i", subTune);
					VPUTSTR(tmpStr);
					break;
				case 'N':
					g_snprintf(tmpStr, XS_BUF_SIZE, "%i", p->nsubTunes);
					VPUTSTR(tmpStr);
					break;
				}
			} else
				VPUTCH(*pcStr);

			pcStr++;
		}

		tmpBuf[iIndex] = 0;

		/* Make resulting string */
		pcResult = g_strdup(tmpBuf);
	}

	/* Free temporary strings */
	g_free(tmpFilename);
	g_free(tmpFilePath);

	return pcResult;
}
