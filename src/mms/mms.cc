/*
 * MMS/MMSH Transport for Audacious
 * Copyright 2007-2013 William Pitcock and John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <libmms/mms.h>
#include <libmms/mmsh.h>

#include <libaudcore/runtime.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>

typedef struct
{
    mms_t * mms;
    mmsh_t * mmsh;
}
MMSHandle;

static void * mms_vfs_fopen_impl (const char * path, const char * mode)
{
    AUDDBG ("Opening %s.\n", path);

    MMSHandle * h = g_new0 (MMSHandle, 1);

    if (! (h->mmsh = mmsh_connect (NULL, NULL, path, 128 * 1024)))
    {
        AUDDBG ("Failed to connect with MMSH protocol; trying MMS.\n");

        if (! (h->mms = mms_connect (NULL, NULL, path, 128 * 1024)))
        {
            fprintf (stderr, "mms: Failed to open %s.\n", path);
            g_free (h);
            return NULL;
        }
    }

    return h;
}

static int mms_vfs_fclose_impl (VFSFile * file)
{
    MMSHandle * h = (MMSHandle *) vfs_get_handle (file);

    if (h->mms)
        mms_close (h->mms);
    else
        mmsh_close (h->mmsh);

    g_free (h);
    return 0;
}

static int64_t mms_vfs_fread_impl (void * buf, int64_t size, int64_t count, VFSFile * file)
{
    MMSHandle * h = (MMSHandle *) vfs_get_handle (file);
    int64_t bytes_total = size * count;
    int64_t bytes_read = 0;

    while (bytes_read < bytes_total)
    {
        int64_t readsize;

        if (h->mms)
            readsize = mms_read (NULL, h->mms, (char *) buf + bytes_read, bytes_total - bytes_read);
        else
            readsize = mmsh_read (NULL, h->mmsh, (char *) buf + bytes_read, bytes_total - bytes_read);

        if (readsize < 0)
            fprintf (stderr, "mms: Read failed.\n");

        if (readsize <= 0)
            break;

        bytes_read += readsize;
    }

    return size ? bytes_read / size : 0;
}

static int64_t mms_vfs_fwrite_impl (const void * data, int64_t size, int64_t count, VFSFile * file)
{
    fprintf (stderr, "mms: Writing is not supported.\n");
    return 0;
}

static int mms_vfs_fseek_impl (VFSFile * file, int64_t offset, int whence)
{
    MMSHandle * h = (MMSHandle *) vfs_get_handle (file);

    if (whence == SEEK_CUR)
    {
        if (h->mms)
            offset += mms_get_current_pos (h->mms);
        else
            offset += mmsh_get_current_pos (h->mmsh);
    }
    else if (whence == SEEK_END)
    {
        if (h->mms)
            offset += mms_get_length (h->mms);
        else
            offset += mmsh_get_length (h->mmsh);
    }

    int64_t ret;

    if (h->mms)
        ret = mms_seek (NULL, h->mms, offset, SEEK_SET);
    else
        ret = mmsh_seek (NULL, h->mmsh, offset, SEEK_SET);

    if (ret < 0 || ret != offset)
    {
        fprintf (stderr, "mms: Seek failed.\n");
        return -1;
    }

    return 0;
}

static int64_t mms_vfs_ftell_impl (VFSFile * file)
{
    MMSHandle * h = (MMSHandle *) vfs_get_handle (file);

    if (h->mms)
        return mms_get_current_pos (h->mms);
    else
        return mmsh_get_current_pos (h->mmsh);
}

static bool_t mms_vfs_feof_impl (VFSFile * file)
{
    MMSHandle * h = (MMSHandle *) vfs_get_handle (file);

    if (h->mms)
        return (mms_get_current_pos (h->mms) < mms_get_length (h->mms));
    else
        return (mmsh_get_current_pos (h->mmsh) < mmsh_get_length (h->mmsh));
}

static int mms_vfs_truncate_impl (VFSFile * file, int64_t size)
{
    fprintf (stderr, "mms: Truncating is not supported.\n");
    return -1;
}

static int64_t mms_vfs_fsize_impl (VFSFile * file)
{
    MMSHandle * h = (MMSHandle *) vfs_get_handle (file);

    if (h->mms)
        return mms_get_length (h->mms);
    else
        return mmsh_get_length (h->mmsh);
}

static const char * const mms_schemes[] = {"mms", NULL};

static const VFSConstructor constructor = {
    mms_vfs_fopen_impl,
    mms_vfs_fclose_impl,
    mms_vfs_fread_impl,
    mms_vfs_fwrite_impl,
    mms_vfs_fseek_impl,
    mms_vfs_ftell_impl,
    mms_vfs_feof_impl,
    mms_vfs_truncate_impl,
    mms_vfs_fsize_impl
};

#define AUD_PLUGIN_NAME        N_("MMS Plugin")
#define AUD_TRANSPORT_SCHEMES  mms_schemes
#define AUD_TRANSPORT_VTABLE   & constructor

#define AUD_DECLARE_TRANSPORT
#include <libaudcore/plugin-declare.h>
