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

#include <stdlib.h>
#include <string.h>

#include <libmms/mms.h>
#include <libmms/mmsh.h>

#include <libaudcore/runtime.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>

class MMSFile : public VFSFile {
public:
    MMSFile (const char * path);
    ~MMSFile ();

    class OpenError {};

protected:
    int64_t fread_impl (void * ptr, int64_t size, int64_t nmemb);
    int64_t fwrite_impl (const void * ptr, int64_t size, int64_t nmemb);

    int fseek_impl (int64_t offset, VFSSeekType whence);
    int64_t ftell_impl ();
    int64_t fsize_impl ();
    bool feof_impl ();
    int ftruncate_impl (int64_t size);
    int fflush_impl ();

private:
    mms_t * m_mms = nullptr;
    mmsh_t * m_mmsh = nullptr;
};

MMSFile::MMSFile (const char * path) :
    VFSFile (path)
{
    if (! (m_mmsh = mmsh_connect (nullptr, nullptr, path, 128 * 1024)))
    {
        AUDDBG ("Failed to connect with MMSH protocol; trying MMS.\n");

        if (! (m_mms = mms_connect (nullptr, nullptr, path, 128 * 1024)))
        {
            AUDERR ("Failed to open %s.\n", path);
            throw OpenError ();
        }
    }
}

MMSFile::~MMSFile ()
{
    if (m_mms)
        mms_close (m_mms);
    else
        mmsh_close (m_mmsh);
}

static VFSFile * mms_fopen (const char * path, const char * mode)
{
    AUDDBG ("Opening %s.\n", path);

    try {
        return new MMSFile (path);
    } catch (MMSFile::OpenError) {
        return nullptr;
    }
}

int64_t MMSFile::fread_impl (void * buf, int64_t size, int64_t count)
{
    int64_t bytes_total = size * count;
    int64_t bytes_read = 0;

    while (bytes_read < bytes_total)
    {
        int64_t readsize;

        if (m_mms)
            readsize = mms_read (nullptr, m_mms, (char *) buf + bytes_read, bytes_total - bytes_read);
        else
            readsize = mmsh_read (nullptr, m_mmsh, (char *) buf + bytes_read, bytes_total - bytes_read);

        if (readsize < 0)
            AUDERR ("Read failed.\n");

        if (readsize <= 0)
            break;

        bytes_read += readsize;
    }

    return size ? bytes_read / size : 0;
}

int64_t MMSFile::fwrite_impl (const void * data, int64_t size, int64_t count)
{
    AUDERR ("Writing is not supported.\n");
    return 0;
}

int MMSFile::fseek_impl (int64_t offset, VFSSeekType whence)
{
    if (whence == VFS_SEEK_CUR)
    {
        if (m_mms)
            offset += mms_get_current_pos (m_mms);
        else
            offset += mmsh_get_current_pos (m_mmsh);
    }
    else if (whence == VFS_SEEK_END)
    {
        if (m_mms)
            offset += mms_get_length (m_mms);
        else
            offset += mmsh_get_length (m_mmsh);
    }

    int64_t ret;

    if (m_mms)
        ret = mms_seek (nullptr, m_mms, offset, SEEK_SET);
    else
        ret = mmsh_seek (nullptr, m_mmsh, offset, SEEK_SET);

    if (ret < 0 || ret != offset)
    {
        AUDERR ("Seek failed.\n");
        return -1;
    }

    return 0;
}

int64_t MMSFile::ftell_impl ()
{
    if (m_mms)
        return mms_get_current_pos (m_mms);
    else
        return mmsh_get_current_pos (m_mmsh);
}

bool MMSFile::feof_impl ()
{
    if (m_mms)
        return (mms_get_current_pos (m_mms) < (int64_t) mms_get_length (m_mms));
    else
        return (mmsh_get_current_pos (m_mmsh) < (int64_t) mmsh_get_length (m_mmsh));
}

int MMSFile::ftruncate_impl (int64_t size)
{
    AUDERR ("Truncating is not supported.\n");
    return -1;
}

int64_t MMSFile::fsize_impl ()
{
    if (m_mms)
        return mms_get_length (m_mms);
    else
        return mmsh_get_length (m_mmsh);
}

int MMSFile::fflush_impl ()
{
    return 0;
}

static const char * const mms_schemes[] = {"mms", nullptr};

#define AUD_PLUGIN_NAME        N_("MMS Plugin")
#define AUD_TRANSPORT_SCHEMES  mms_schemes
#define AUD_TRANSPORT_FOPEN    mms_fopen

#define AUD_DECLARE_TRANSPORT
#include <libaudcore/plugin-declare.h>
