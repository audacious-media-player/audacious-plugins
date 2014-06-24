/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   STIL-database handling functions

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
#include "xs_stil.h"

#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmms-sid.h"
#include "xs_support.h"

/* Database handling functions
 */
static void xs_stildb_node_realloc(stil_node_t *node, int nsubTunes)
{
    /* Re-allocate subTune structure if needed */
    if (nsubTunes > node->nsubTunes) {
        int clearIndex, clearLength;

        node->subTunes = g_renew (stil_subnode_t *, node->subTunes, nsubTunes + 1);

        /* Clear the newly allocated memory */
        if (node->nsubTunes == 0) {
            clearIndex = 0;
            clearLength = nsubTunes + 1;
        } else {
            clearIndex = node->nsubTunes + 1;
            clearLength = (nsubTunes - clearIndex + 1);
        }
        memset(&(node->subTunes[clearIndex]), 0, clearLength * sizeof(stil_subnode_t **));

        node->nsubTunes = nsubTunes;
    }

    /* Allocate memory for subTune */
    if (! node->subTunes[nsubTunes])
        node->subTunes[nsubTunes] = g_new0 (stil_subnode_t, 1);
}


static void xs_stildb_node_free(stil_node_t *node)
{
    int i;
    stil_subnode_t *subnode;

    if (node == nullptr) return;

    /* Free subtune information */
    for (i = 0; i <= node->nsubTunes; i++) {
        subnode = node->subTunes[i];
        if (subnode) {
            g_free(subnode->name);
            g_free(subnode->author);
            g_free(subnode->info);
            g_free(subnode->title);
            g_free(subnode);
        }
    }
    g_free(node->subTunes);
    g_free(node->filename);
    g_free(node);
}


static stil_node_t *xs_stildb_node_new(char *filename)
{
    stil_node_t *result;

//fprintf(stderr, "LL: %s\n", filename);

    /* Allocate memory for new node */
    result = g_new0 (stil_node_t, 1);

    /* Allocate filename and initial space for one subtune */
    result->filename = g_strdup(filename);
    xs_stildb_node_realloc (result, 1);

    return result;
}


/* Insert given node to db linked list
 */
static void xs_stildb_node_insert(xs_stildb_t *db, stil_node_t *node)
{
    assert(db != nullptr);

    if (db->nodes != nullptr) {
        /* The first node's pPrev points to last node */
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


/* Read database (additively) to given db-structure
 */
#define XS_STILDB_MULTI                                         \
    if (multi) {                                                \
        multi = false;                                          \
        xs_pstrcat(&(node->subTunes[subEntry]->info), "\n");    \
    }

static void XS_STILDB_ERR(int linenum, char *line, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    xs_verror(fmt, ap);
    va_end(ap);

    xs_error("#%d: '%s'\n", linenum, line);
}

int xs_stildb_read(xs_stildb_t *db, char *filename)
{
    FILE *f;
    char line[XS_BUF_SIZE + 16];    /* Since we add some chars here and there */
    stil_node_t *node;
    bool multi;
    int lineNum, subEntry;
    assert(db != nullptr);

    /* Try to open the file */
    if ((f = fopen(filename, "r")) == nullptr) {
        xs_error("Could not open STILDB '%s'\n", filename);
        return -1;
    }

    /* Read and parse the data */
    lineNum = 0;
    multi = false;
    node = nullptr;
    subEntry = 0;

    while (fgets(line, XS_BUF_SIZE, f) != nullptr) {
        size_t linePos = 0, eolPos = 0;
        line[XS_BUF_SIZE] = 0;
        xs_findeol(line, &eolPos);
        line[eolPos] = 0;
        lineNum++;

        switch (line[0]) {
        case '/':
            /* Check if we are already parsing entry */
            multi = false;
            if (node != nullptr) {
                XS_STILDB_ERR(lineNum, line,
                    "New entry found before end of current ('%s')!\n",
                    node->filename);
                xs_stildb_node_free(node);
                node = nullptr;
            }

            /* A new node */
            subEntry = 0;
            node = xs_stildb_node_new (line);
            break;

        case '(':
            /* A new sub-entry */
            multi = false;
            linePos++;
            if (line[linePos] == '#') {
                linePos++;
                if (g_ascii_isdigit(line[linePos])) {
                    size_t savePos = linePos;
                    xs_findnum(line, &linePos);
                    line[linePos] = 0;
                    subEntry = atol(&line[savePos]);

                    /* Sanity check */
                    if (subEntry < 1) {
                        char *tmp = node != nullptr ? node->filename : nullptr;
                        XS_STILDB_ERR(lineNum, line,
                            "Number of subEntry (%d) for '%s' is invalid\n",
                            subEntry, tmp);
                        subEntry = 0;
                    }
                } else {
                    XS_STILDB_ERR(lineNum, line,
                        "Syntax error, expected subEntry number.\n");
                    subEntry = 0;
                }
            } else {
                XS_STILDB_ERR(lineNum, line,
                    "Syntax error, expected '#' before subEntry number.\n");
                subEntry = 0;
            }

            break;

        case 0:
        case '#':
        case '\n':
        case '\r':
            /* End of entry/field */
            multi = false;
            if (node != nullptr) {
                xs_stildb_node_insert(db, node);
                node = nullptr;
            }
            break;

        default:
            /* Check if we are parsing an entry */
            xs_findnext(line, &linePos);

            if (node == nullptr) {
                XS_STILDB_ERR(lineNum, line,
                    "Entry data encountered outside of entry or syntax error!\n");
                break;
            }

            xs_stildb_node_realloc (node, subEntry);

            /* Some other type */
            if (strncmp(line, "   NAME:", 8) == 0) {
                XS_STILDB_MULTI;
                g_free(node->subTunes[subEntry]->name);
                node->subTunes[subEntry]->name = g_strdup(&line[9]);
            } else if (strncmp(line, "  TITLE:", 8) == 0) {
                XS_STILDB_MULTI;
                multi = true;
                if (!node->subTunes[subEntry]->title)
                    node->subTunes[subEntry]->title = g_strdup(&line[9]);
                xs_pstrcat(&(node->subTunes[subEntry]->info), &line[2]);
            } else if (strncmp(line, " AUTHOR:", 8) == 0) {
                XS_STILDB_MULTI;
                g_free(node->subTunes[subEntry]->author);
                node->subTunes[subEntry]->author = g_strdup(&line[9]);
            } else if (strncmp(line, " ARTIST:", 8) == 0) {
                XS_STILDB_MULTI;
                multi = true;
                xs_pstrcat(&(node->subTunes[subEntry]->info), &line[1]);
            } else if (strncmp(line, "COMMENT:", 8) == 0) {
                XS_STILDB_MULTI;
                multi = true;
                xs_pstrcat(&(node->subTunes[subEntry]->info), line);
            } else {
                if (multi) {
                    xs_pstrcat(&(node->subTunes[subEntry]->info), " ");
                    xs_pstrcat(&(node->subTunes[subEntry]->info), &line[linePos]);
                } else {
                    XS_STILDB_ERR(lineNum, line,
                    "Entry continuation found when multi == false.\n");
                }
            }
            break;
        }
    } /* while */

    /* Check if there is one remaining node */
    if (node != nullptr)
        xs_stildb_node_insert(db, node);

    /* Close the file */
    fclose(f);

    return 0;
}


/* Compare two nodes
 */
static int xs_stildb_cmp(const void *node1, const void *node2)
{
    /* We assume here that we never ever get nullptr-pointers or similar */
    return strcmp(
        (*(stil_node_t **) node1)->filename,
        (*(stil_node_t **) node2)->filename);
}


/* (Re)create index
 */
void xs_stildb_index(xs_stildb_t *db)
{
    stil_node_t *curr;
    size_t i;

    /* Free old index */
    if (db->pindex) {
        g_free(db->pindex);
        db->pindex = nullptr;
    }

    /* Get size of db */
    curr = db->nodes;
    db->n = 0;
    while (curr) {
        db->n++;
        curr = curr->next;
    }

    /* Check number of nodes */
    if (db->n > 0) {
        /* Allocate memory for index-table */
        db->pindex = g_new (stil_node_t *, db->n);

        /* Get node-pointers to table */
        i = 0;
        curr = db->nodes;
        while (curr && (i < db->n)) {
            db->pindex[i++] = curr;
            curr = curr->next;
        }

        /* Sort the indexes */
        qsort(db->pindex, db->n, sizeof(stil_node_t *), xs_stildb_cmp);
    }
}


/* Free a given STIL database
 */
void xs_stildb_free(xs_stildb_t *db)
{
    stil_node_t *curr, *next;

    if (!db)
        return;

    /* Free the memory allocated for nodes */
    curr = db->nodes;
    while (curr) {
        next = curr->next;
        xs_stildb_node_free(curr);
        curr = next;
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


/* Get STIL information node from database
 */
stil_node_t *xs_stildb_get_node(xs_stildb_t *db, char *filename)
{
    stil_node_t keyItem, *key, **item;

    /* Check the database pointers */
    if (!db || !db->nodes || !db->pindex)
        return nullptr;

    /* Look-up index using binary search */
    keyItem.filename = filename;
    key = &keyItem;
    item = (stil_node_t * *) bsearch (& key, db->pindex, db->n,
     sizeof (stil_node_t *), xs_stildb_cmp);

    if (item)
        return *item;
    else
        return nullptr;
}
