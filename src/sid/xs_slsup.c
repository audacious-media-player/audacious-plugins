/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   File information window

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
#include "xs_slsup.h"
#include "xs_config.h"


static t_xs_sldb *xs_sldb_db = NULL;
XS_MUTEX(xs_sldb_db);

static t_xs_stildb *xs_stildb_db = NULL;
XS_MUTEX(xs_stildb_db);


/* STIL-database handling
 */
gint xs_stil_init(void)
{
	XS_MUTEX_LOCK(xs_cfg);

	if (!xs_cfg.stilDBPath) {
		XS_MUTEX_UNLOCK(xs_cfg);
		return -1;
	}

	XS_MUTEX_LOCK(xs_stildb_db);

	/* Check if already initialized */
	if (xs_stildb_db)
		xs_stildb_free(xs_stildb_db);

	/* Allocate database */
	xs_stildb_db = (t_xs_stildb *) g_malloc0(sizeof(t_xs_stildb));
	if (!xs_stildb_db) {
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_stildb_db);
		return -2;
	}

	/* Read the database */
	if (xs_stildb_read(xs_stildb_db, xs_cfg.stilDBPath) != 0) {
		xs_stildb_free(xs_stildb_db);
		xs_stildb_db = NULL;
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_stildb_db);
		return -3;
	}

	/* Create index */
	if (xs_stildb_index(xs_stildb_db) != 0) {
		xs_stildb_free(xs_stildb_db);
		xs_stildb_db = NULL;
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_stildb_db);
		return -4;
	}

	XS_MUTEX_UNLOCK(xs_cfg);
	XS_MUTEX_UNLOCK(xs_stildb_db);
	return 0;
}


void xs_stil_close(void)
{
	XS_MUTEX_LOCK(xs_stildb_db);
	xs_stildb_free(xs_stildb_db);
	xs_stildb_db = NULL;
	XS_MUTEX_UNLOCK(xs_stildb_db);
}


t_xs_stil_node *xs_stil_get(gchar *pcFilename)
{
	t_xs_stil_node *pResult;
	gchar *tmpFilename;

	XS_MUTEX_LOCK(xs_stildb_db);
	XS_MUTEX_LOCK(xs_cfg);

	if (xs_cfg.stilDBEnable && xs_stildb_db) {
		if (xs_cfg.hvscPath) {
			/* Remove postfixed directory separator from HVSC-path */
			tmpFilename = xs_strrchr(xs_cfg.hvscPath, '/');
			if (tmpFilename && (tmpFilename[1] == 0))
				tmpFilename[0] = 0;

			/* Remove HVSC location-prefix from filename */
			tmpFilename = strstr(pcFilename, xs_cfg.hvscPath);
			if (tmpFilename)
				tmpFilename += strlen(xs_cfg.hvscPath);
			else
				tmpFilename = pcFilename;
		} else
			tmpFilename = pcFilename;

		pResult = xs_stildb_get_node(xs_stildb_db, tmpFilename);
	} else
		pResult = NULL;

	XS_MUTEX_UNLOCK(xs_stildb_db);
	XS_MUTEX_UNLOCK(xs_cfg);

	return pResult;
}


/* Song length database handling glue
 */
gint xs_songlen_init(void)
{
	XS_MUTEX_LOCK(xs_cfg);

	if (!xs_cfg.songlenDBPath) {
		XS_MUTEX_UNLOCK(xs_cfg);
		return -1;
	}

	XS_MUTEX_LOCK(xs_sldb_db);

	/* Check if already initialized */
	if (xs_sldb_db)
		xs_sldb_free(xs_sldb_db);

	/* Allocate database */
	xs_sldb_db = (t_xs_sldb *) g_malloc0(sizeof(t_xs_sldb));
	if (!xs_sldb_db) {
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_sldb_db);
		return -2;
	}

	/* Read the database */
	if (xs_sldb_read(xs_sldb_db, xs_cfg.songlenDBPath) != 0) {
		xs_sldb_free(xs_sldb_db);
		xs_sldb_db = NULL;
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_sldb_db);
		return -3;
	}

	/* Create index */
	if (xs_sldb_index(xs_sldb_db) != 0) {
		xs_sldb_free(xs_sldb_db);
		xs_sldb_db = NULL;
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_sldb_db);
		return -4;
	}

	XS_MUTEX_UNLOCK(xs_cfg);
	XS_MUTEX_UNLOCK(xs_sldb_db);
	return 0;
}


void xs_songlen_close(void)
{
	XS_MUTEX_LOCK(xs_sldb_db);
	xs_sldb_free(xs_sldb_db);
	xs_sldb_db = NULL;
	XS_MUTEX_UNLOCK(xs_sldb_db);
}


t_xs_sldb_node *xs_songlen_get(const gchar * pcFilename)
{
	t_xs_sldb_node *pResult;

	XS_MUTEX_LOCK(xs_sldb_db);

	if (xs_cfg.songlenDBEnable && xs_sldb_db)
		pResult = xs_sldb_get(xs_sldb_db, pcFilename);
	else
		pResult = NULL;

	XS_MUTEX_UNLOCK(xs_sldb_db);

	return pResult;
}


/* Allocate a new tune information structure
 */
t_xs_tuneinfo *xs_tuneinfo_new(const gchar * pcFilename,
		gint nsubTunes, gint startTune, const gchar * sidName,
		const gchar * sidComposer, const gchar * sidCopyright,
		gint loadAddr, gint initAddr, gint playAddr,
		gint dataFileLen, const gchar *sidFormat, gint sidModel)
{
	t_xs_tuneinfo *pResult;
	t_xs_sldb_node *tmpLength;
	gint i;

	/* Allocate structure */
	pResult = (t_xs_tuneinfo *) g_malloc0(sizeof(t_xs_tuneinfo));
	if (!pResult) {
		xs_error(_("Could not allocate memory for t_xs_tuneinfo ('%s')\n"),
			pcFilename);
		return NULL;
	}

	pResult->sidFilename = XS_CS_FILENAME(pcFilename);
	if (!pResult->sidFilename) {
		xs_error(_("Could not allocate sidFilename ('%s')\n"),
			pcFilename);
		g_free(pResult);
		return NULL;
	}

	/* Allocate space for subtune information */
	pResult->subTunes = g_malloc0(sizeof(t_xs_subtuneinfo) * (nsubTunes + 1));
	if (!pResult->subTunes) {
		xs_error(_("Could not allocate memory for t_xs_subtuneinfo ('%s', %i)\n"),
			pcFilename, nsubTunes);

		g_free(pResult->sidFilename);
		g_free(pResult);
		return NULL;
	}

	/* The following allocations don't matter if they fail */
	pResult->sidName = XS_CS_SID(sidName);
	pResult->sidComposer = XS_CS_SID(sidComposer);
	pResult->sidCopyright = XS_CS_SID(sidCopyright);

	pResult->nsubTunes = nsubTunes;
	pResult->startTune = startTune;

	pResult->loadAddr = loadAddr;
	pResult->initAddr = initAddr;
	pResult->playAddr = playAddr;
	pResult->dataFileLen = dataFileLen;
	pResult->sidFormat = XS_CS_SID(sidFormat);
	
	pResult->sidModel = sidModel;

	/* Get length information (NOTE: Do not free this!) */
	tmpLength = xs_songlen_get(pcFilename);
	
	/* Fill in sub-tune information */
	for (i = 0; i < pResult->nsubTunes; i++) {
		if (tmpLength && (i < tmpLength->nLengths))
			pResult->subTunes[i].tuneLength = tmpLength->sLengths[i];
		else
			pResult->subTunes[i].tuneLength = -1;
		
		pResult->subTunes[i].tuneSpeed = -1;
	}
	
	return pResult;
}


/* Free given tune information structure
 */
void xs_tuneinfo_free(t_xs_tuneinfo * pTune)
{
	if (!pTune) return;

	g_free(pTune->subTunes);
	g_free(pTune->sidFilename);
	g_free(pTune->sidName);
	g_free(pTune->sidComposer);
	g_free(pTune->sidCopyright);
	g_free(pTune->sidFormat);
	g_free(pTune);
}
