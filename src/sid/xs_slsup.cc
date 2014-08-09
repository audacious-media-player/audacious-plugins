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

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "xs_config.h"


static xs_sldb_t *xs_sldb_db = nullptr;
pthread_mutex_t xs_sldb_db_mutex = PTHREAD_MUTEX_INITIALIZER;

static xs_stildb_t *xs_stildb_db = nullptr;
pthread_mutex_t xs_stildb_db_mutex = PTHREAD_MUTEX_INITIALIZER;


/* STIL-database handling
 */
int xs_stil_init(void)
{
    pthread_mutex_lock(&xs_cfg_mutex);

    if (!xs_cfg.stilDBPath) {
        pthread_mutex_unlock(&xs_cfg_mutex);
        return -1;
    }

    pthread_mutex_lock(&xs_stildb_db_mutex);

    /* Check if already initialized */
    if (xs_stildb_db)
        xs_stildb_free(xs_stildb_db);

    /* Allocate database */
    xs_stildb_db = g_new0 (xs_stildb_t, 1);

    /* Read the database */
    if (xs_stildb_read(xs_stildb_db, xs_cfg.stilDBPath) != 0) {
        xs_stildb_free(xs_stildb_db);
        xs_stildb_db = nullptr;
        pthread_mutex_unlock(&xs_cfg_mutex);
        pthread_mutex_unlock(&xs_stildb_db_mutex);
        return -3;
    }

    /* Create index */
    xs_stildb_index (xs_stildb_db);

    pthread_mutex_unlock(&xs_cfg_mutex);
    pthread_mutex_unlock(&xs_stildb_db_mutex);
    return 0;
}


void xs_stil_close(void)
{
    pthread_mutex_lock(&xs_stildb_db_mutex);
    xs_stildb_free(xs_stildb_db);
    xs_stildb_db = nullptr;
    pthread_mutex_unlock(&xs_stildb_db_mutex);
}


stil_node_t *xs_stil_get(char *filename)
{
    stil_node_t *result;
    char *tmpFilename;

    pthread_mutex_lock(&xs_stildb_db_mutex);
    pthread_mutex_lock(&xs_cfg_mutex);

    if (xs_cfg.stilDBEnable && xs_stildb_db) {
        if (xs_cfg.hvscPath) {
            /* Remove postfixed directory separator from HVSC-path */
            tmpFilename = strrchr(xs_cfg.hvscPath, '/');
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
        result = nullptr;

    pthread_mutex_unlock(&xs_stildb_db_mutex);
    pthread_mutex_unlock(&xs_cfg_mutex);

    return result;
}


/* Song length database handling glue
 */
int xs_songlen_init(void)
{
    pthread_mutex_lock(&xs_cfg_mutex);

    if (!xs_cfg.songlenDBPath) {
        pthread_mutex_unlock(&xs_cfg_mutex);
        return -1;
    }

    pthread_mutex_lock(&xs_sldb_db_mutex);

    /* Check if already initialized */
    if (xs_sldb_db)
        xs_sldb_free(xs_sldb_db);

    /* Allocate database */
    xs_sldb_db = g_new0 (xs_sldb_t, 1);

    /* Read the database */
    if (xs_sldb_read(xs_sldb_db, xs_cfg.songlenDBPath) != 0) {
        xs_sldb_free(xs_sldb_db);
        xs_sldb_db = nullptr;
        pthread_mutex_unlock(&xs_cfg_mutex);
        pthread_mutex_unlock(&xs_sldb_db_mutex);
        return -3;
    }

    /* Create index */
    xs_sldb_index (xs_sldb_db);

    pthread_mutex_unlock(&xs_cfg_mutex);
    pthread_mutex_unlock(&xs_sldb_db_mutex);
    return 0;
}


void xs_songlen_close(void)
{
    pthread_mutex_lock(&xs_sldb_db_mutex);
    xs_sldb_free(xs_sldb_db);
    xs_sldb_db = nullptr;
    pthread_mutex_unlock(&xs_sldb_db_mutex);
}


sldb_node_t *xs_songlen_get(const char * filename)
{
    sldb_node_t *result;

    pthread_mutex_lock(&xs_sldb_db_mutex);

    if (xs_cfg.songlenDBEnable && xs_sldb_db)
        result = xs_sldb_get(xs_sldb_db, filename);
    else
        result = nullptr;

    pthread_mutex_unlock(&xs_sldb_db_mutex);

    return result;
}


/* Allocate a new tune information structure
 */
xs_tuneinfo_t *xs_tuneinfo_new(const char * filename,
        int nsubTunes, int startTune, const char * sidName,
        const char * sidComposer, const char * sidCopyright,
        int loadAddr, int initAddr, int playAddr,
        int dataFileLen, const char *sidFormat, int sidModel)
{
    xs_tuneinfo_t *result;
    sldb_node_t *tmpLength;
    int i;

    /* Allocate structure */
    result = new xs_tuneinfo_t ();

    result->sidFilename = String (filename);

    /* Allocate space for subtune information */
    result->subTunes = g_new0 (xs_subtuneinfo_t, nsubTunes + 1);

    result->sidName = String (sidName);
    result->sidComposer = String (sidComposer);
    result->sidCopyright = String (sidCopyright);

    result->nsubTunes = nsubTunes;
    result->startTune = startTune;

    result->loadAddr = loadAddr;
    result->initAddr = initAddr;
    result->playAddr = playAddr;
    result->dataFileLen = dataFileLen;
    result->sidFormat = String (sidFormat);

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

    delete tune;
}
