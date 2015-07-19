/*
 * Copyright (c) 2006 William Pitcock <nenolod -at- atheme.org>
 * This code is in the public domain.
 */

#ifndef __BINIO_VIRTUAL__
#define __BINIO_VIRTUAL__

#include <binio.h>
#include <stdio.h>

#include <libaudcore/vfs.h>

class vfsistream : public binistream {
private:
        VFSFile *fd = nullptr;
        VFSFile own;

public:
        vfsistream(VFSFile *fd = nullptr) :
                fd(fd) {}

        void open(const char *file) {
                if ((own = VFSFile(file, "r")))
                        fd = &own;
                else
                        err |= NotFound;
        }

        void open(std::string &file) { open(file.c_str()); }

        vfsistream(const char *file) { open(file); }
        vfsistream(std::string &file) { open(file); }

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
};

class vfsostream : public binostream {
private:
        VFSFile *fd = nullptr;
        VFSFile own;

public:
        vfsostream(VFSFile *fd = nullptr) :
                fd(fd) {}

        void open(const char *file) {
                if ((own = VFSFile(file, "w")))
                        fd = &own;
                else
                        err |= Denied;
        }

        void open(std::string &file) { open(file.c_str()); }

        vfsostream(const char *file) { open(file); }
        vfsostream(std::string &file) { open(file); }

        void putByte(Byte b) {
                if (fd->fwrite (&b, 1, 1) != 1)
                        err |= Fatal;
        }

        void seek(long pos, Offset offs = Set) {
                VFSSeekType wh = (offs == Add) ? VFS_SEEK_CUR : (offs == End) ? VFS_SEEK_END : VFS_SEEK_SET;
                if (fd->fseek (pos, wh))
                        err |= Fatal;
        }

        long pos(void) {
                return fd->ftell ();
        }
};

#endif
