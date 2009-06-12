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
#include "xs_support.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>


/* Database handling functions
 */
static gboolean xs_stildb_node_realloc(stil_node_t *node, gint nsubTunes)
{
    if (!node) return FALSE;

    /* Re-allocate subTune structure if needed */
    if (nsubTunes > node->nsubTunes) {
        gint clearIndex, clearLength;
        
        node->subTunes =
            (stil_subnode_t **) g_realloc(node->subTunes,
            (nsubTunes + 1) * sizeof(stil_subnode_t **));

        if (!node->subTunes) {
            xs_error("SubTune pointer structure realloc failed.\n");
            return FALSE;
        }
        
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
    if (!node->subTunes[nsubTunes]) {
        node->subTunes[nsubTunes] = (stil_subnode_t *)
            g_malloc0(sizeof(stil_subnode_t));
        
        if (!node->subTunes[nsubTunes]) {
            xs_error("SubTune structure malloc failed!\n");
            return FALSE;
        }
    }

    return TRUE;
}


static void xs_stildb_node_free(stil_node_t *node)
{
    gint i;
    stil_subnode_t *subnode;

    if (!node) return;

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


static stil_node_t *xs_stildb_node_new(gchar *filename)
{
    stil_node_t *result;

    /* Allocate memory for new node */
    result = (stil_node_t *) g_malloc0(sizeof(stil_node_t));
    if (!result)
        return NULL;

    /* Allocate filename and initial space for one subtune */
    result->filename = g_strdup(filename);
    if (!result->filename || !xs_stildb_node_realloc(result, 1)) {
        xs_stildb_node_free(result);
        return NULL;
    }
    
    return result;
}


/* Insert given node to db linked list
 */
static void xs_stildb_node_insert(xs_stildb_t *db, stil_node_t *node)
{
    assert(db != NULL);

    if (db->nodes) {
        /* The first node's pPrev points to last node */
        LPREV = db->nodes->prev;    /* New node's prev = Previous last node */
        db->nodes->prev->next = node;    /* Previous last node's next = New node */
        db->nodes->prev = node;    /* New last node = New node */
        LNEXT = NULL;    /* But next is NULL! */
    } else {
        db->nodes = node;    /* First node ... */
        LPREV = node;    /* ... it's also last */
        LNEXT = NULL;    /* But next is NULL! */
    }
}


/* Read database (additively) to given db-structure
 */
#define XS_STILDB_MULTI                            \
    if (isMulti) {                            \
        isMulti = FALSE;                    \
        xs_pstrcat(&(tmnode->subTunes[subEntry]->info), "\n");\
    }

static void XS_STILDB_ERR(gint lineNum, gchar *inLine, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    xs_error(fmt, ap);
    va_end(ap);
    
    fprintf(stderr, "#%d: '%s'\n", lineNum, inLine);
}

gint xs_stildb_read(xs_stildb_t *db, gchar *dbFilename)
{
    FILE *inFile;
    gchar inLine[XS_BUF_SIZE + 16];    /* Since we add some chars here and there */
    size_t lineNum;
    stil_node_t *tmnode;
    gboolean isError, isMulti;
    gint subEntry;
    gchar *tmpLine = inLine;
    assert(db != NULL);

    /* Try to open the file */
    if ((inFile = fopen(dbFilename, "ra")) == NULL) {
        xs_error("Could not open STILDB '%s'\n", dbFilename);
        return -1;
    }

    /* Read and parse the data */
    lineNum = 0;
    isError = FALSE;
    isMulti = FALSE;
    tmnode = NULL;
    subEntry = 0;

    while (!isError && fgets(inLine, XS_BUF_SIZE, inFile) != NULL) {
        size_t linePos = 0, eolPos = 0;
        xs_findeol(inLine, &eolPos);
        inLine[eolPos] = 0;
        lineNum++;
        
        tmpLine = XS_CS_STIL(inLine);

        switch (tmpLine[0]) {
        case '/':
            /* Check if we are already parsing entry */
            isMulti = FALSE;
            if (tmnode) {
                XS_STILDB_ERR(lineNum, tmpLine,
                    "New entry found before end of current ('%s')!\n",
                    tmnode->filename);
                xs_stildb_node_free(tmnode);
            }

            /* A new node */
            subEntry = 0;
            tmnode = xs_stildb_node_new(tmpLine);
            if (!tmnode) {
                /* Allocation failed */
                XS_STILDB_ERR(lineNum, tmpLine,
                    "Could not allocate new STILdb-node!\n");
                isError = TRUE;
            }
            break;

        case '(':
            /* A new sub-entry */
            isMulti = FALSE;
            linePos++;
            if (tmpLine[linePos] == '#') {
                linePos++;
                if (isdigit(tmpLine[linePos])) {
                    size_t savePos = linePos;
                    xs_findnum(tmpLine, &linePos);
                    tmpLine[linePos] = 0;
                    subEntry = atol(&tmpLine[savePos]);

                    /* Sanity check */
                    if (subEntry < 1) {
                        XS_STILDB_ERR(lineNum, tmpLine,
                            "Number of subEntry (%i) for '%s' is invalid\n",
                            subEntry, tmnode ? tmnode->filename : "");
                        subEntry = 0;
                    }
                } else {
                    XS_STILDB_ERR(lineNum, tmpLine,
                        "Syntax error, expected subEntry number.\n");
                    subEntry = 0;
                }
            } else {
                XS_STILDB_ERR(lineNum, tmpLine,
                    "Syntax error, expected '#' before subEntry number.\n");
                subEntry = 0;
            }

            break;

        case 0:
        case '#':
        case '\n':
        case '\r':
            /* End of entry/field */
            isMulti = FALSE;
            if (tmnode) {
                /* Insert to database */
                xs_stildb_node_insert(db, tmnode);
                tmnode = NULL;
            }
            break;

        default:
            /* Check if we are parsing an entry */
            xs_findnext(tmpLine, &linePos);
            
            if (!tmnode) {
                XS_STILDB_ERR(lineNum, tmpLine,
                    "Entry data encountered outside of entry or syntax error!\n");
                break;
            }

            if (!xs_stildb_node_realloc(tmnode, subEntry)) {
                XS_STILDB_ERR(lineNum, tmpLine,
                    "Could not (re)allocate memory for subEntries!\n");
                isError = TRUE;
                break;
            }
            
            /* Some other type */
            if (strncmp(tmpLine, "   NAME:", 8) == 0) {
                XS_STILDB_MULTI;
                g_free(tmnode->subTunes[subEntry]->name);
                tmnode->subTunes[subEntry]->name = g_strdup(&tmpLine[9]);
            } else if (strncmp(tmpLine, "  TITLE:", 8) == 0) {
                XS_STILDB_MULTI;
                isMulti = TRUE;
                if (!tmnode->subTunes[subEntry]->title)
                    tmnode->subTunes[subEntry]->title = g_strdup(&tmpLine[9]);
                xs_pstrcat(&(tmnode->subTunes[subEntry]->info), &tmpLine[2]);
            } else if (strncmp(tmpLine, " AUTHOR:", 8) == 0) {
                XS_STILDB_MULTI;
                g_free(tmnode->subTunes[subEntry]->author);
                tmnode->subTunes[subEntry]->author = g_strdup(&tmpLine[9]);
            } else if (strncmp(tmpLine, " ARTIST:", 8) == 0) {
                XS_STILDB_MULTI;
                isMulti = TRUE;
                xs_pstrcat(&(tmnode->subTunes[subEntry]->info), &tmpLine[1]);
            } else if (strncmp(tmpLine, "COMMENT:", 8) == 0) {
                XS_STILDB_MULTI;
                isMulti = TRUE;
                xs_pstrcat(&(tmnode->subTunes[subEntry]->info), tmpLine);
            } else {
                if (isMulti) {
                    xs_pstrcat(&(tmnode->subTunes[subEntry]->info), " ");
                    xs_pstrcat(&(tmnode->subTunes[subEntry]->info), &tmpLine[linePos]);
                } else {
                    XS_STILDB_ERR(lineNum, tmpLine,
                    "Entry continuation found when isMulti == FALSE.\n");
                }
            }
            break;
        }
        
        XS_CS_FREE(tmpLine);

    } /* while */

    /* Check if there is one remaining node */
    if (tmnode)
        xs_stildb_node_insert(db, tmnode);

    /* Close the file */
    fclose(inFile);

    return 0;
}


/* Compare two nodes
 */
static gint xs_stildb_cmp(const void *node1, const void *node2)
{
    /* We assume here that we never ever get NULL-pointers or similar */
    return strcmp(
        (*(stil_node_t **) node1)->filename,
        (*(stil_node_t **) node2)->filename);
}


/* (Re)create index
 */
gint xs_stildb_index(xs_stildb_t *db)
{
    stil_node_t *curr;
    size_t i;

    /* Free old index */
    if (db->pindex) {
        g_free(db->pindex);
        db->pindex = NULL;
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
        db->pindex = (stil_node_t **) g_malloc(sizeof(stil_node_t *) * db->n);
        if (!db->pindex)
            return -1;

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

    return 0;
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


/* Get STIL information node from database
 */
stil_node_t *xs_stildb_get_node(xs_stildb_t *db, gchar *filename)
{
    stil_node_t keyItem, *key, **item;

    /* Check the database pointers */
    if (!db || !db->nodes || !db->pindex)
        return NULL;

    /* Look-up index using binary search */
    keyItem.filename = filename;
    key = &keyItem;
    item = bsearch(&key, db->pindex, db->n, sizeof(stil_node_t *), xs_stildb_cmp);
    if (item)
        return *item;
    else
        return NULL;
}
