/*  
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   STIL-database handling functions
   
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
#include "xs_stil.h"
#include "xs_support.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>


/* Database handling functions
 */
static gboolean xs_stildb_node_realloc(t_xs_stil_node *pNode, gint nsubTunes)
{
	if (!pNode) return FALSE;

	/* Re-allocate subTune structure if needed */
	if (nsubTunes > pNode->nsubTunes) {
		gint clearIndex, clearLength;
		
		pNode->subTunes =
			(t_xs_stil_subnode **) g_realloc(pNode->subTunes,
			(nsubTunes + 1) * sizeof(t_xs_stil_subnode **));

		if (!pNode->subTunes) {
			xs_error("SubTune pointer structure realloc failed.\n");
			return FALSE;
		}
		
		/* Clear the newly allocated memory */
		if (pNode->nsubTunes == 0) {
			clearIndex = 0;
			clearLength = nsubTunes + 1;
		} else {
			clearIndex = pNode->nsubTunes + 1;
			clearLength = (nsubTunes - clearIndex + 1);
		}
		memset(&(pNode->subTunes[clearIndex]), 0, clearLength * sizeof(t_xs_stil_subnode **));
		
		pNode->nsubTunes = nsubTunes;
	}

	/* Allocate memory for subTune */
	if (!pNode->subTunes[nsubTunes]) {
		pNode->subTunes[nsubTunes] = (t_xs_stil_subnode *)
			g_malloc0(sizeof(t_xs_stil_subnode));
		
		if (!pNode->subTunes[nsubTunes]) {
			xs_error("SubTune structure malloc failed!\n");
			return FALSE;
		}
	}

	return TRUE;
}


static void xs_stildb_node_free(t_xs_stil_node *pNode)
{
	gint i;
	t_xs_stil_subnode *pSub;

	if (pNode) {
		/* Free subtune information */
		for (i = 0; i <= pNode->nsubTunes; i++) {
			pSub = pNode->subTunes[i];
			if (pSub) {
				g_free(pSub->pName);
				g_free(pSub->pAuthor);
				g_free(pSub->pInfo);
				g_free(pSub->pTitle);

				g_free(pSub);
			}
		}
		
		g_free(pNode->subTunes);
		g_free(pNode->pcFilename);
		g_free(pNode);
	}
}


static t_xs_stil_node *xs_stildb_node_new(gchar *pcFilename)
{
	t_xs_stil_node *pResult;

	/* Allocate memory for new node */
	pResult = (t_xs_stil_node *) g_malloc0(sizeof(t_xs_stil_node));
	if (!pResult)
		return NULL;

	/* Allocate filename and initial space for one subtune */
	pResult->pcFilename = g_strdup(pcFilename);
	if (!pResult->pcFilename || !xs_stildb_node_realloc(pResult, 1)) {
		xs_stildb_node_free(pResult);
		return NULL;
	}
	
	return pResult;
}


/* Insert given node to db linked list
 */
static void xs_stildb_node_insert(t_xs_stildb *db, t_xs_stil_node *pNode)
{
	assert(db);

	if (db->pNodes) {
		/* The first node's pPrev points to last node */
		LPREV = db->pNodes->pPrev;	/* New node's prev = Previous last node */
		db->pNodes->pPrev->pNext = pNode;	/* Previous last node's next = New node */
		db->pNodes->pPrev = pNode;	/* New last node = New node */
		LNEXT = NULL;	/* But next is NULL! */
	} else {
		db->pNodes = pNode;	/* First node ... */
		LPREV = pNode;	/* ... it's also last */
		LNEXT = NULL;	/* But next is NULL! */
	}
}


/* Read database (additively) to given db-structure
 */
#define XS_STILDB_MULTI							\
	if (isMulti) {							\
		isMulti = FALSE;					\
		xs_pstrcat(&(tmpNode->subTunes[subEntry]->pInfo), "\n");\
	}

static void XS_STILDB_ERR(gint lineNum, gchar *inLine, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	xs_error(fmt, ap);
	va_end(ap);
	
	fprintf(stderr, "#%d: '%s'\n", lineNum, inLine);
}

gint xs_stildb_read(t_xs_stildb *db, gchar *dbFilename)
{
	FILE *inFile;
	gchar inLine[XS_BUF_SIZE + 16];	/* Since we add some chars here and there */
	size_t lineNum;
	t_xs_stil_node *tmpNode;
	gboolean isError, isMulti;
	gint subEntry;
	gchar *tmpLine = inLine;
	assert(db);

	/* Try to open the file */
	if ((inFile = fopen(dbFilename, "ra")) == NULL) {
		xs_error("Could not open STILDB '%s'\n", dbFilename);
		return -1;
	}

	/* Read and parse the data */
	lineNum = 0;
	isError = FALSE;
	isMulti = FALSE;
	tmpNode = NULL;
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
			if (tmpNode) {
				XS_STILDB_ERR(lineNum, tmpLine,
					"New entry found before end of current ('%s')!\n",
					tmpNode->pcFilename);
				xs_stildb_node_free(tmpNode);
			}

			/* A new node */
			subEntry = 0;
			tmpNode = xs_stildb_node_new(tmpLine);
			if (!tmpNode) {
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
							subEntry, tmpNode->pcFilename);
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
			if (tmpNode) {
				/* Insert to database */
				xs_stildb_node_insert(db, tmpNode);
				tmpNode = NULL;
			}
			break;

		default:
			/* Check if we are parsing an entry */
			xs_findnext(tmpLine, &linePos);
			
			if (!tmpNode) {
				XS_STILDB_ERR(lineNum, tmpLine,
					"Entry data encountered outside of entry or syntax error!\n");
				break;
			}

			if (!xs_stildb_node_realloc(tmpNode, subEntry)) {
				XS_STILDB_ERR(lineNum, tmpLine,
					"Could not (re)allocate memory for subEntries!\n");
				isError = TRUE;
				break;
			}
			
			/* Some other type */
			if (strncmp(tmpLine, "   NAME:", 8) == 0) {
				XS_STILDB_MULTI;
				g_free(tmpNode->subTunes[subEntry]->pName);
				tmpNode->subTunes[subEntry]->pName = g_strdup(&tmpLine[9]);
			} else if (strncmp(tmpLine, "  TITLE:", 8) == 0) {
				XS_STILDB_MULTI;
				isMulti = TRUE;
				if (!tmpNode->subTunes[subEntry]->pTitle)
					tmpNode->subTunes[subEntry]->pTitle = g_strdup(&tmpLine[9]);
				xs_pstrcat(&(tmpNode->subTunes[subEntry]->pInfo), &tmpLine[2]);
			} else if (strncmp(tmpLine, " AUTHOR:", 8) == 0) {
				XS_STILDB_MULTI;
				g_free(tmpNode->subTunes[subEntry]->pAuthor);
				tmpNode->subTunes[subEntry]->pAuthor = g_strdup(&tmpLine[9]);
			} else if (strncmp(tmpLine, " ARTIST:", 8) == 0) {
				XS_STILDB_MULTI;
				isMulti = TRUE;
				xs_pstrcat(&(tmpNode->subTunes[subEntry]->pInfo), &tmpLine[1]);
			} else if (strncmp(tmpLine, "COMMENT:", 8) == 0) {
				XS_STILDB_MULTI;
				isMulti = TRUE;
				xs_pstrcat(&(tmpNode->subTunes[subEntry]->pInfo), tmpLine);
			} else {
				if (isMulti) {
					xs_pstrcat(&(tmpNode->subTunes[subEntry]->pInfo), " ");
					xs_pstrcat(&(tmpNode->subTunes[subEntry]->pInfo), &tmpLine[linePos]);
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
	if (tmpNode)
		xs_stildb_node_insert(db, tmpNode);

	/* Close the file */
	fclose(inFile);

	return 0;
}


/* Compare two nodes
 */
static gint xs_stildb_cmp(const void *pNode1, const void *pNode2)
{
	/* We assume here that we never ever get NULL-pointers or similar */
	return strcmp(
		(*(t_xs_stil_node **) pNode1)->pcFilename,
		(*(t_xs_stil_node **) pNode2)->pcFilename);
}


/* (Re)create index
 */
gint xs_stildb_index(t_xs_stildb *db)
{
	t_xs_stil_node *pCurr;
	size_t i;

	/* Free old index */
	if (db->ppIndex) {
		g_free(db->ppIndex);
		db->ppIndex = NULL;
	}

	/* Get size of db */
	pCurr = db->pNodes;
	db->n = 0;
	while (pCurr) {
		db->n++;
		pCurr = pCurr->pNext;
	}

	/* Check number of nodes */
	if (db->n > 0) {
		/* Allocate memory for index-table */
		db->ppIndex = (t_xs_stil_node **) g_malloc(sizeof(t_xs_stil_node *) * db->n);
		if (!db->ppIndex)
			return -1;

		/* Get node-pointers to table */
		i = 0;
		pCurr = db->pNodes;
		while (pCurr && (i < db->n)) {
			db->ppIndex[i++] = pCurr;
			pCurr = pCurr->pNext;
		}

		/* Sort the indexes */
		qsort(db->ppIndex, db->n, sizeof(t_xs_stil_node *), xs_stildb_cmp);
	}

	return 0;
}


/* Free a given STIL database
 */
void xs_stildb_free(t_xs_stildb *db)
{
	t_xs_stil_node *pCurr, *pNext;

	if (!db)
		return;

	/* Free the memory allocated for nodes */
	pCurr = db->pNodes;
	while (pCurr) {
		pNext = pCurr->pNext;
		xs_stildb_node_free(pCurr);
		pCurr = pNext;
	}

	db->pNodes = NULL;

	/* Free memory allocated for index */
	if (db->ppIndex) {
		g_free(db->ppIndex);
		db->ppIndex = NULL;
	}

	/* Free structure */
	db->n = 0;
	g_free(db);
}


/* Get STIL information node from database
 */
t_xs_stil_node *xs_stildb_get_node(t_xs_stildb *db, gchar *pcFilename)
{
	t_xs_stil_node keyItem, *key, **item;

	/* Check the database pointers */
	if (!db || !db->pNodes || !db->ppIndex)
		return NULL;

	/* Look-up index using binary search */
	keyItem.pcFilename = pcFilename;
	key = &keyItem;
	item = bsearch(&key, db->ppIndex, db->n, sizeof(t_xs_stil_node *), xs_stildb_cmp);
	if (item)
		return *item;
	else
		return NULL;
}
