/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   common.c

   */

#include <config.h>
#include <stdlib.h>

/* I guess "rb" should be right for any libc */
#define OPEN_MODE "rb"

#include "timidity.h"
#include "timidity_internal.h"
#include "options.h"
#include "common.h"

/* The paths in this list will be tried whenever we're reading a file */
static PathList *pathlist = NULL;       /* This is a linked list */

/* This is meant to find and open files for reading */
VFSFile *open_file(gchar * name)
{
    PathList *plp = pathlist;
    VFSFile *fp = NULL;
    gchar *uri, *tmp;

    if (name == NULL || !*name)
    {
        DEBUG_MSG("Attempted to open NULL or nameless file.\n");
        return NULL;
    }

    /* First try the given name */
    DEBUG_MSG("Trying to open %s\n", name);
    fp = NULL;
    if (g_path_is_absolute(name))
        tmp = g_strdup(name);
    else
    {
        gchar *cwd = g_get_current_dir();
        tmp = g_build_filename(cwd, name, NULL);
        g_free(cwd);
    }

    uri = g_filename_to_uri(tmp, NULL, NULL);
    g_free(tmp);

    if (aud_vfs_file_test(uri, G_FILE_TEST_EXISTS))
        fp = aud_vfs_fopen(uri, OPEN_MODE);
    g_free(uri);

    if (fp != NULL)
        return fp;

    while (plp != NULL)
    {
        tmp = g_build_filename(plp->path, name, NULL);
        uri = g_filename_to_uri(tmp, NULL, NULL);
        g_free(tmp);

        DEBUG_MSG("Trying to open %s\n", tmp);

        if (uri != NULL && aud_vfs_file_test(uri, G_FILE_TEST_EXISTS))
            fp = aud_vfs_fopen(uri, OPEN_MODE);
        g_free(uri);
        if (fp != NULL)
            return fp;
        plp = plp->next;
    }

    /* Nothing could be opened. */
    DEBUG_MSG("Could not open %s\n", name);
    return NULL;
}

/* This adds a directory to the path list */
void add_to_pathlist(char *s)
{
    PathList *plp = g_malloc(sizeof(PathList));

    if (plp == NULL)
        return;

    plp->path = g_strdup(s);
    plp->next = pathlist;
    pathlist = plp;
}

void free_pathlist(void)
{
    PathList *plp = pathlist;

    while (plp != NULL)
    {
        PathList *next = plp->next;
        g_free(plp->path);
        g_free(plp);
        plp = next;
    }
    pathlist = NULL;
}

void *safe_malloc(gsize count)
{
    return malloc(count);
}
