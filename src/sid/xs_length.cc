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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "xmms-sid.h"
#include "xs_support.h"

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
        node->next = nullptr;    /* But next is nullptr! */
    } else {
        db->nodes = node;    /* First node ... */
        node->prev = node;    /* ... it's also last */
        node->next = nullptr;    /* But next is nullptr! */
    }
}


/* Parse a time-entry in SLDB format
 */
static int xs_sldb_gettime(char *str, size_t *pos)
{
    int result, tmp;

    /* Check if it starts with a digit */
    if (g_ascii_isdigit(str[*pos])) {
        /* Get minutes-field */
        result = 0;
        while (g_ascii_isdigit(str[*pos]))
            result = (result * 10) + (str[(*pos)++] - '0');

        result *= 60;

        /* Check the field separator char */
        if (str[*pos] == ':') {
            /* Get seconds-field */
            (*pos)++;
            tmp = 0;
            while (g_ascii_isdigit(str[*pos])) {
                tmp = (tmp * 10) + (str[(*pos)++] - '0');
            }

            result += tmp;
        } else
            result = -2;
    } else
        result = -1;

    /* Ignore and skip the possible attributes */
    while (str[*pos] && !g_ascii_isspace(str[*pos]))
        (*pos)++;

    return result;
}


/* Parse one SLDB definition line, return SLDB node
 */
sldb_node_t * xs_sldb_read_entry(char *inLine)
{
    size_t linePos;
    int i;
    bool isOK;
    sldb_node_t *tmnode;

    /* Allocate new node */
    tmnode = g_new0 (sldb_node_t, 1);

    /* Get hash value */
    linePos = 0;
    for (i = 0; i < XS_MD5HASH_LENGTH; i++, linePos += 2) {
        unsigned tmpu;
        sscanf(&inLine[linePos], "%2x", &tmpu);
        tmnode->md5Hash[i] = tmpu;
    }

    /* Get playtimes */
    if (inLine[linePos] != 0) {
        if (inLine[linePos] != '=') {
            xs_error("'=' expected on column #%d.\n", (int)linePos);
            xs_sldb_node_free(tmnode);
            return nullptr;
        } else {
            size_t tmpLen, savePos;

            /* First playtime is after '=' */
            savePos = ++linePos;
            tmpLen = strlen(inLine);

            /* Get number of sub-tune lengths */
            isOK = true;
            while ((linePos < tmpLen) && isOK) {
                xs_findnext(inLine, &linePos);

                if (xs_sldb_gettime(inLine, &linePos) >= 0)
                    tmnode->nlengths++;
                else
                    isOK = false;
            }

            /* Allocate memory for lengths */
            if (tmnode->nlengths > 0) {
                tmnode->lengths = g_new0 (int, tmnode->nlengths);
            } else {
                xs_sldb_node_free(tmnode);
                return nullptr;
            }

            /* Read lengths in */
            i = 0;
            linePos = savePos;
            isOK = true;
            while ((linePos < tmpLen) && (i < tmnode->nlengths) && isOK) {
                int l;

                xs_findnext(inLine, &linePos);

                l = xs_sldb_gettime(inLine, &linePos);
                if (l >= 0)
                    tmnode->lengths[i] = l;
                else
                    isOK = false;

                i++;
            }

            if (!isOK) {
                xs_sldb_node_free(tmnode);
                return nullptr;
            } else
                return tmnode;
        }
    }

    xs_sldb_node_free(tmnode);
    return nullptr;
}


/* Read database to memory
 */
int xs_sldb_read(xs_sldb_t *db, const char *dbFilename)
{
    FILE *inFile;
    char inLine[XS_BUF_SIZE];
    size_t lineNum;
    sldb_node_t *tmnode;
    assert(db);

    /* Try to open the file */
    if ((inFile = fopen(dbFilename, "r")) == nullptr) {
        xs_error("Could not open SongLengthDB '%s'\n", dbFilename);
        return -1;
    }

    /* Read and parse the data */
    lineNum = 0;

    while (fgets(inLine, XS_BUF_SIZE, inFile) != nullptr) {
        size_t linePos = 0;
        lineNum++;

        xs_findnext(inLine, &linePos);

        /* Check if it is datafield */
        if (g_ascii_isxdigit(inLine[linePos])) {
            /* Check the length of the hash */
            int hashLen;
            for (hashLen = 0; inLine[linePos] && g_ascii_isxdigit(inLine[linePos]); hashLen++, linePos++);

            if (hashLen != XS_MD5HASH_LENGTH_CH) {
                xs_error("Invalid MD5-hash in SongLengthDB file '%s' line #%d!\n",
                    dbFilename, (int)lineNum);
            } else {
                /* Parse and add node to db */
                if ((tmnode = xs_sldb_read_entry(inLine)) != nullptr) {
                    xs_sldb_node_insert(db, tmnode);
                } else {
                    xs_error("Invalid entry in SongLengthDB file '%s' line #%d!\n",
                        dbFilename, (int)lineNum);
                }
            }
        } else if ((inLine[linePos] != ';') && (inLine[linePos] != '[') && (inLine[linePos] != 0)) {
            xs_error("Invalid line in SongLengthDB file '%s' line #%d\n",
                dbFilename, (int)lineNum);
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
static int xs_sldb_cmphash(xs_md5hash_t testHash1, xs_md5hash_t testHash2)
{
    int i, d;

    /* Compute difference of hashes */
    for (i = 0, d = 0; (i < XS_MD5HASH_LENGTH) && !d; i++)
        d = (testHash1[i] - testHash2[i]);

    return d;
}


/* Compare two nodes
 */
static int xs_sldb_cmp(const void *node1, const void *node2)
{
    /* We assume here that we never ever get nullptr-pointers or similar */
    return xs_sldb_cmphash(
        (*(sldb_node_t **) node1)->md5Hash,
        (*(sldb_node_t **) node2)->md5Hash);
}


/* (Re)create index
 */
void xs_sldb_index(xs_sldb_t * db)
{
    sldb_node_t *pCurr;
    size_t i;
    assert(db);

    /* Free old index */
    if (db->pindex) {
        g_free(db->pindex);
        db->pindex = nullptr;
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
        db->pindex = g_new (sldb_node_t *, db->n);

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

    db->nodes = nullptr;

    /* Free memory allocated for index */
    if (db->pindex) {
        g_free(db->pindex);
        db->pindex = nullptr;
    }

    /* Free structure */
    db->n = 0;
    g_free(db);
}


/* Compute md5hash of given SID-file
 */
typedef struct {
    char magicID[4];    /* "PSID" / "RSID" magic identifier */
    uint16_t version,    /* Version number */
        dataOffset,    /* Start of actual c64 data in file */
        loadAddress,    /* Loading address */
        initAddress,    /* Initialization address */
        playAddress,    /* Play one frame */
        nSongs,        /* Number of subsongs */
        startSong;    /* Default starting song */
    uint32_t speed;        /* Speed */
    char sidName[32];    /* Descriptive text-fields, ASCIIZ */
    char sidAuthor[32];
    char sidCopyright[32];
} psidv1_header_t;


typedef struct {
    uint16_t flags;        /* Flags */
    uint8_t startPage, pageLength;
    uint16_t reserved;
} psidv2_header_t;

#define READ_CHARS(c, f) (vfs_fread ((c), 1, sizeof (c), (f)) == sizeof (c))

static int xs_get_sid_hash(const char *filename, xs_md5hash_t hash)
{
    VFSFile *inFile;
    xs_md5state_t inState;
    psidv1_header_t psidH;
    psidv2_header_t psidH2;
    uint8_t *songData;
    uint8_t ib8[2], i8;
    int index, result;

    /* Try to open the file */
    if ((inFile = vfs_fopen(filename, "rb")) == nullptr)
        return -1;

    /* Read PSID header in */
    if (! READ_CHARS (psidH.magicID, inFile))
    {
        vfs_fclose(inFile);
        return -1;
    }

    if (strncmp(psidH.magicID, "PSID", 4) && strncmp(psidH.magicID, "RSID", 4)) {
        vfs_fclose(inFile);
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

    if (! READ_CHARS (psidH.sidName, inFile)
     || ! READ_CHARS (psidH.sidAuthor, inFile)
     || ! READ_CHARS (psidH.sidCopyright, inFile))
    {
        vfs_fclose(inFile);
        xs_error("Error reading SID file header from '%s'\n", filename);
        return -4;
    }

    /* Check if we need to load PSIDv2NG header ... */
    psidH2.flags = 0;    /* Just silence a stupid gcc warning */

    if (psidH.version == 2) {
        /* Yes, we need to */
        psidH2.flags = xs_fread_be16(inFile);
        psidH2.startPage = vfs_getc(inFile);
        psidH2.pageLength = vfs_getc(inFile);
        psidH2.reserved = xs_fread_be16(inFile);
    }

    /* Allocate buffer */
    songData = g_new (uint8_t, XS_SIDBUF_SIZE);

    /* Read data to buffer */
    result = vfs_fread(songData, sizeof(uint8_t), XS_SIDBUF_SIZE, inFile);
    vfs_fclose(inFile);

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
    xs_md5_append(&inState, (uint8_t *) &ib8, sizeof(ib8));    \
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
sldb_node_t *xs_sldb_get(xs_sldb_t *db, const char *filename)
{
    sldb_node_t keyItem, *key, **item;

    /* Check the database pointers */
    if (!db || !db->nodes || !db->pindex)
        return nullptr;

    /* Get the hash and then look up from db */
    if (xs_get_sid_hash(filename, keyItem.md5Hash) == 0) {
        key = &keyItem;
        item = (sldb_node_t * *) bsearch (& key, db->pindex, db->n,
         sizeof db->pindex[0], xs_sldb_cmp);

        if (item)
            return *item;
        else
            return nullptr;
    } else
        return nullptr;
}


