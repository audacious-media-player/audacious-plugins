/*
 * Copyright (c) 2006 William Pitcock <nenolod -at- atheme.org>
 * This code is in the public domain.
 */

#ifndef __BINIO_VIRTUAL__
#define __BINIO_VIRTUAL__

#include "binio.h"

#include <libaudcore/vfs.h>

class vfsistream : public binistream {
private:
        VFSFile *fd = nullptr;
        VFSFile own;

public:
        vfsistream(VFSFile *fd = nullptr) :
                fd(fd) {}

        vfsistream(std::string &file) {
                if ((own = VFSFile(file.c_str(), "r")))
                        fd = &own;
                else
                        err |= NotFound;
        }

        Byte getByte() {
                Byte b = (Byte)-1;
                if (fd->fread(&b, 1, 1) != 1)
                        err |= Eof;
                return b;
        }

        void seek(long pos, Offset offs = Set) {
                VFSSeekType wh = (offs == Add) ? VFS_SEEK_CUR : (offs == End) ? VFS_SEEK_END : VFS_SEEK_SET;
                if (fd->fseek (pos, wh))
                        err |= Eof;
        }

        long pos() {
                return fd->ftell ();
        }

        unsigned long size() {
                return aud::max(fd->fsize(), (int64_t)0);
        }
};

#endif
