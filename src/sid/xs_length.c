/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Get song length from SLDB for PSID/RSID files

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2009 Tecnic Software productions (TNSP)

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
#include "xs_length.h"
#include "xs_support.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


/* Free memory allocated for given SLDB node
 */
static void xs_sldb_node_free(sldb_node_t *node)
{
    if (node) {
        /* Nothing much to do here ... */
        g_free(node->lengths);
        g_free(node);
    }
}


/* Insert given node to db linked list
 */
static void xs_sldb_node_insert(xs_sldb_t *db, sldb_node_t *node)
{
    assert(db);

    if (db->nodes) {
        /* The first node's prev points to last node */
        node->prev = db->nodes->prev;    /* New node's prev = Previous last node */
        db->nodes->prev->next = node;    /* Previous last node's next = New node */
        db->nodes->prev = node;    /* New last node = New node */
        node->next = NULL;    /* But next is NULL! */
    } else {
        db->nodes = node;    /* First node ... */
        node->prev = node;    /* ... it's also last */
        node->next = NULL;    /* But next is NULL! */
    }
}


/* Parse a time-entry in SLDB format
 */
static gint xs_sldb_gettime(gchar *str, size_t *pos)
{
    gint result, tmp;

    /* Check if it starts with a digit */
    if (isdigit(str[*pos])) {
        /* Get minutes-field */
        result = 0;
        while (isdigit(str[*pos]))
            result = (result * 10) + (str[(*pos)++] - '0');

        result *= 60;

        /* Check the field separator char */
        if (str[*pos] == ':') {
            /* Get seconds-field */
            (*pos)++;
            tmp = 0;
            while (isdigit(str[*pos])) {
                tmp = (tmp * 10) + (str[(*pos)++] - '0');
            }

            result += tmp;
        } else
            result = -2;
    } else
        result = -1;

    /* Ignore and skip the possible attributes */
    while (str[*pos] && !isspace(str[*pos]))
        (*pos)++;

    return result;
}


/* Parse one SLDB definition line, return SLDB node
 */
sldb_node_t * xs_sldb_read_entry(gchar *inLine)
{
    size_t linePos;
    gint i;
    gboolean isOK;
    sldb_node_t *tmnode;

    /* Allocate new node */
    tmnode = (sldb_node_t *) g_malloc0(sizeof(sldb_node_t));
    if (!tmnode) {
        xs_error("Error allocating new node. Fatal error.\n");
        return NULL;
    }

    /* Get hash value */
    linePos = 0;
    for (i = 0; i < XS_MD5HASH_LENGTH; i++, linePos += 2) {
        guint tmpu;
        sscanf(&inLine[linePos], "%2x", &tmpu);
        tmnode->md5Hash[i] = tmpu;
    }

    /* Get playtimes */
    if (inLine[linePos] != 0) {
        if (inLine[linePos] != '=') {
            xs_error("'=' expected on column #%d.\n", linePos);
            xs_sldb_node_free(tmnode);
            return NULL;
        } else {
            size_t tmpLen, savePos;

            /* First playtime is after '=' */
            savePos = ++linePos;
            tmpLen = strlen(inLine);

            /* Get number of sub-tune lengths */
            isOK = TRUE;
            while ((linePos < tmpLen) && isOK) {
                xs_findnext(inLine, &linePos);

                if (xs_sldb_gettime(inLine, &linePos) >= 0)
                    tmnode->nlengths++;
                else
                    isOK = FALSE;
            }

            /* Allocate memory for lengths */
            if (tmnode->nlengths > 0) {
                tmnode->lengths = (gint *) g_malloc0(tmnode->nlengths * sizeof(gint));
                if (!tmnode->lengths) {
                    xs_error("Could not allocate memory for node.\n");
                    xs_sldb_node_free(tmnode);
                    return NULL;
                }
            } else {
                xs_sldb_node_free(tmnode);
                return NULL;
            }

            /* Read lengths in */
            i = 0;
            linePos = savePos;
            isOK = TRUE;
            while ((linePos < tmpLen) && (i < tmnode->nlengths) && isOK) {
                gint l;

                xs_findnext(inLine, &linePos);

                l = xs_sldb_gettime(inLine, &linePos);
                if (l >= 0)
                    tmnode->lengths[i] = l;
                else
                    isOK = FALSE;

                i++;
            }

            if (!isOK) {
                xs_sldb_node_free(tmnode);
                return NULL;
            } else
                return tmnode;
        }
    }

    return NULL;
}


/* Read database to memory
 */
gint xs_sldb_read(xs_sldb_t *db, const gchar *dbFilename)
{
    FILE *inFile;
    gchar inLine[XS_BUF_SIZE];
    size_t lineNum;
    sldb_node_t *tmnode;
    assert(db);

    /* Try to open the file */
    if ((inFile = fopen(dbFilename, "r")) == NULL) {
        xs_error("Could not open SongLengthDB '%s'\n", dbFilename);
        return -1;
    }

    /* Read and parse the data */
    lineNum = 0;

    while (fgets(inLine, XS_BUF_SIZE, inFile) != NULL) {
        size_t linePos = 0;
        lineNum++;

        xs_findnext(inLine, &linePos);

        /* Check if it is datafield */
        if (isxdigit(inLine[linePos])) {
            /* Check the length of the hash */
            gint hashLen;
            for (hashLen = 0; inLine[linePos] && isxdigit(inLine[linePos]); hashLen++, linePos++);

            if (hashLen != XS_MD5HASH_LENGTH_CH) {
                xs_error("Invalid MD5-hash in SongLengthDB file '%s' line #%d!\n",
                    dbFilename, lineNum);
            } else {
                /* Parse and add node to db */
                if ((tmnode = xs_sldb_read_entry(inLine)) != NULL) {
                    xs_sldb_node_insert(db, tmnode);
                } else {
                    xs_error("Invalid entry in SongLengthDB file '%s' line #%d!\n",
                        dbFilename, lineNum);
                }
            }
        } else if ((inLine[linePos] != ';') && (inLine[linePos] != '[') && (inLine[linePos] != 0)) {
            xs_error("Invalid line in SongLengthDB file '%s' line #%d\n",
                dbFilename, lineNum);
        }

    }

    /* Close the file */
    fclose(inFile);

    return 0;
}


/* Compare two given MD5-hashes.
 * Return: 0 if equal
 *         negative if testHash1 < testHash2
 *         positive if testHash1 > testHash2
 */
static gint xs_sldb_cmphash(xs_md5hash_t testHash1, xs_md5hash_t testHash2)
{
    gint i, d;

    /* Compute difference of hashes */
    for (i = 0, d = 0; (i < XS_MD5HASH_LENGTH) && !d; i++)
        d = (testHash1[i] - testHash2[i]);

    return d;
}


/* Compare two nodes
 */
static gint xs_sldb_cmp(const void *node1, const void *node2)
{
    /* We assume here that we never ever get NULL-pointers or similar */
    return xs_sldb_cmphash(
        (*(sldb_node_t **) node1)->md5Hash,
        (*(sldb_node_t **) node2)->md5Hash);
}


/* (Re)create index
 */
gint xs_sldb_index(xs_sldb_t * db)
{
    sldb_node_t *pCurr;
    size_t i;
    assert(db);

    /* Free old index */
    if (db->pindex) {
        g_free(db->pindex);
        db->pindex = NULL;
    }

    /* Get size of db */
    pCurr = db->nodes;
    db->n = 0;
    while (pCurr) {
        db->n++;
        pCurr = pCurr->next;
    }

    /* Check number of nodes */
    if (db->n > 0) {
        /* Allocate memory for index-table */
        db->pindex = (sldb_node_t **) g_malloc(sizeof(sldb_node_t *) * db->n);
        if (!db->pindex)
            return -1;

        /* Get node-pointers to table */
        i = 0;
        pCurr = db->nodes;
        while (pCurr && (i < db->n)) {
            db->pindex[i++] = pCurr;
            pCurr = pCurr->next;
        }

        /* Sort the indexes */
        qsort(db->pindex, db->n, sizeof(sldb_node_t *), xs_sldb_cmp);
    }

    return 0;
}


/* Free a given song-length database
 */
void xs_sldb_free(xs_sldb_t * db)
{
    sldb_node_t *pCurr, *next;

    if (!db)
        return;

    /* Free the memory allocated for nodes */
    pCurr = db->nodes;
    while (pCurr) {
        next = pCurr->next;
        xs_sldb_node_free(pCurr);
        pCurr = next;
    }

    db->nodes = NULL;

    /* Free memory allocated for index */
    if (db->pindex) {
        g_free(db->pindex);
        db->pindex = NULL;
    }

    /* Free structure */
    db->n = 0;
    g_free(db);
}


/* Compute md5hash of given SID-file
 */
typedef struct {
    gchar magicID[4];    /* "PSID" / "RSID" magic identifier */
    guint16 version,    /* Version number */
        dataOffset,    /* Start of actual c64 data in file */
        loadAddress,    /* Loading address */
        initAddress,    /* Initialization address */
        playAddress,    /* Play one frame */
        nSongs,        /* Number of subsongs */
        startSong;    /* Default starting song */
    guint32 speed;        /* Speed */
    gchar sidName[32];    /* Descriptive text-fields, ASCIIZ */
    gchar sidAuthor[32];
    gchar sidCopyright[32];
} psidv1_header_t;


typedef struct {
    guint16 flags;        /* Flags */
    guint8 startPage, pageLength;
    guint16 reserved;
} psidv2_header_t;


static gint xs_get_sid_hash(const gchar *filename, xs_md5hash_t hash)
{
    xs_file_t *inFile;
    xs_md5state_t inState;
    psidv1_header_t psidH;
    psidv2_header_t psidH2;
    guint8 *songData;
    guint8 ib8[2], i8;
    gint index, result;

    /* Try to open the file */
    if ((inFile = xs_fopen(filename, "rb")) == NULL)
        return -1;

    /* Read PSID header in */
    if (xs_fread(psidH.magicID, 1, sizeof psidH.magicID, inFile) < sizeof psidH.magicID) {
        xs_fclose(inFile);
        return -1;
    }

    if (strncmp(psidH.magicID, "PSID", 4) && strncmp(psidH.magicID, "RSID", 4)) {
        xs_fclose(inFile);
        xs_error("Not a PSID or RSID file '%s'\n", filename);
        return -2;
    }

    psidH.version = xs_fread_be16(inFile);
    psidH.dataOffset = xs_fread_be16(inFile);
    psidH.loadAddress = xs_fread_be16(inFile);
    psidH.initAddress = xs_fread_be16(inFile);
    psidH.playAddress = xs_fread_be16(inFile);
    psidH.nSongs = xs_fread_be16(inFile);
    psidH.startSong = xs_fread_be16(inFile);
    psidH.speed = xs_fread_be32(inFile);

    if (xs_fread(psidH.sidName, 1, sizeof psidH.sidName, inFile) < sizeof psidH.sidName
     || xs_fread(psidH.sidAuthor, 1, sizeof psidH.sidAuthor, inFile) < sizeof psidH.sidAuthor
     || xs_fread(psidH.sidCopyright, 1, sizeof psidH.sidCopyright, inFile) < sizeof psidH.sidCopyright) {
        xs_fclose(inFile);
        xs_error("Error reading SID file header from '%s'\n", filename);
        return -4;
    }

    /* Check if we need to load PSIDv2NG header ... */
    psidH2.flags = 0;    /* Just silence a stupid gcc warning */

    if (psidH.version == 2) {
        /* Yes, we need to */
        psidH2.flags = xs_fread_be16(inFile);
        psidH2.startPage = xs_fgetc(inFile);
        psidH2.pageLength = xs_fgetc(inFile);
        psidH2.reserved = xs_fread_be16(inFile);
    }

    /* Allocate buffer */
    songData = (guint8 *) g_malloc(XS_SIDBUF_SIZE * sizeof(guint8));
    if (!songData) {
        xs_fclose(inFile);
        xs_error("Error allocating temp data buffer for file '%s'\n", filename);
        return -3;
    }

    /* Read data to buffer */
    result = xs_fread(songData, sizeof(guint8), XS_SIDBUF_SIZE, inFile);
    xs_fclose(inFile);

    /* Initialize and start MD5-hash calculation */
    xs_md5_init(&inState);

    if (psidH.loadAddress == 0) {
        /* Strip load address (2 first bytes) */
        xs_md5_append(&inState, &songData[2], result - 2);
    } else {
        /* Append "as is" */
        xs_md5_append(&inState, songData, result);
    }

    /* Free buffer */
    g_free(songData);

    /* Append header data to hash */
#define XSADDHASH(QDATAB) do {                    \
    ib8[0] = (QDATAB & 0xff);                \
    ib8[1] = (QDATAB >> 8);                    \
    xs_md5_append(&inState, (guint8 *) &ib8, sizeof(ib8));    \
    } while (0)

    XSADDHASH(psidH.initAddress);
    XSADDHASH(psidH.playAddress);
    XSADDHASH(psidH.nSongs);
#undef XSADDHASH

    /* Append song speed data to hash */
    i8 = 0;
    for (index = 0; (index < psidH.nSongs) && (index < 32); index++) {
        i8 = (psidH.speed & (1 << index)) ? 60 : 0;
        xs_md5_append(&inState, &i8, sizeof(i8));
    }

    /* Rest of songs (more than 32) */
    for (index = 32; index < psidH.nSongs; index++) {
        xs_md5_append(&inState, &i8, sizeof(i8));
    }

    /* PSIDv2NG specific */
    if (psidH.version == 2) {
        /* SEE SIDPLAY HEADERS FOR INFO */
        i8 = (psidH2.flags >> 2) & 3;
        if (i8 == 2)
            xs_md5_append(&inState, &i8, sizeof(i8));
    }

    /* Calculate the hash */
    xs_md5_finish(&inState, hash);

    return 0;
}


/* Get node from db index via binary search
 */
sldb_node_t *xs_sldb_get(xs_sldb_t *db, const gchar *filename)
{
    sldb_node_t keyItem, *key, **item;

    /* Check the database pointers */
    if (!db || !db->nodes || !db->pindex)
        return NULL;

    /* Get the hash and then look up from db */
    if (xs_get_sid_hash(filename, keyItem.md5Hash) == 0) {
        key = &keyItem;
        item = bsearch(&key, db->pindex, db->n,
            sizeof(db->pindex[0]), xs_sldb_cmp);

        if (item)
            return *item;
        else
            return NULL;
    } else
        return NULL;
}


