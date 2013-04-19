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


static xs_sldb_t *xs_sldb_db = NULL;
XS_MUTEX(xs_sldb_db);

static xs_stildb_t *xs_stildb_db = NULL;
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
    xs_stildb_db = (xs_stildb_t *) g_malloc0(sizeof(xs_stildb_t));
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


stil_node_t *xs_stil_get(gchar *filename)
{
    stil_node_t *result;
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
            tmpFilename = strstr(filename, xs_cfg.hvscPath);
            if (tmpFilename)
                tmpFilename += strlen(xs_cfg.hvscPath);
            else
                tmpFilename = filename;
        } else
            tmpFilename = filename;

        result = xs_stildb_get_node(xs_stildb_db, tmpFilename);
    } else
        result = NULL;

    XS_MUTEX_UNLOCK(xs_stildb_db);
    XS_MUTEX_UNLOCK(xs_cfg);

    return result;
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
    xs_sldb_db = (xs_sldb_t *) g_malloc0(sizeof(xs_sldb_t));
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


sldb_node_t *xs_songlen_get(const gchar * filename)
{
    sldb_node_t *result;

    XS_MUTEX_LOCK(xs_sldb_db);

    if (xs_cfg.songlenDBEnable && xs_sldb_db)
        result = xs_sldb_get(xs_sldb_db, filename);
    else
        result = NULL;

    XS_MUTEX_UNLOCK(xs_sldb_db);

    return result;
}


/* Allocate a new tune information structure
 */
xs_tuneinfo_t *xs_tuneinfo_new(const gchar * filename,
        gint nsubTunes, gint startTune, const gchar * sidName,
        const gchar * sidComposer, const gchar * sidCopyright,
        gint loadAddr, gint initAddr, gint playAddr,
        gint dataFileLen, const gchar *sidFormat, gint sidModel)
{
    xs_tuneinfo_t *result;
    sldb_node_t *tmpLength;
    gint i;

    /* Allocate structure */
    result = (xs_tuneinfo_t *) g_malloc0(sizeof(xs_tuneinfo_t));
    if (!result) {
        xs_error("Could not allocate memory for tuneinfo ('%s')\n",
            filename);
        return NULL;
    }

    result->sidFilename = g_strdup(filename);
    if (!result->sidFilename) {
        xs_error("Could not allocate sidFilename ('%s')\n",
            filename);
        g_free(result);
        return NULL;
    }

    /* Allocate space for subtune information */
    result->subTunes = g_malloc0(sizeof(xs_subtuneinfo_t) * (nsubTunes + 1));
    if (!result->subTunes) {
        xs_error("Could not allocate memory for subtuneinfo ('%s', %i)\n",
            filename, nsubTunes);

        g_free(result->sidFilename);
        g_free(result);
        return NULL;
    }

    /* The following allocations don't matter if they fail */
    result->sidName = g_convert(sidName, -1, "UTF-8", XS_SID_CHARSET, NULL, NULL, NULL);
    result->sidComposer = g_convert(sidComposer, -1, "UTF-8", XS_SID_CHARSET, NULL, NULL, NULL);
    result->sidCopyright = g_convert(sidCopyright, -1, "UTF-8", XS_SID_CHARSET, NULL, NULL, NULL);

    result->nsubTunes = nsubTunes;
    result->startTune = startTune;

    result->loadAddr = loadAddr;
    result->initAddr = initAddr;
    result->playAddr = playAddr;
    result->dataFileLen = dataFileLen;
    result->sidFormat = g_convert(sidFormat, -1, "UTF-8", XS_SID_CHARSET, NULL, NULL, NULL);

    result->sidModel = sidModel;

    /* Get length information (NOTE: Do not free this!) */
    tmpLength = xs_songlen_get(filename);

    /* Fill in sub-tune information */
    for (i = 0; i < result->nsubTunes; i++) {
        if (tmpLength && (i < tmpLength->nlengths))
            result->subTunes[i].tuneLength = tmpLength->lengths[i];
        else
            result->subTunes[i].tuneLength = -1;

        result->subTunes[i].tuneSpeed = -1;
    }

    return result;
}


/* Free given tune information structure
 */
void xs_tuneinfo_free(xs_tuneinfo_t * tune)
{
    if (!tune) return;

    g_free(tune->subTunes);
    g_free(tune->sidFilename);
    g_free(tune->sidName);
    g_free(tune->sidComposer);
    g_free(tune->sidCopyright);
    g_free(tune->sidFormat);
    g_free(tune);
}
