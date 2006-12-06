/*
 * Copyright (c) 2006 William Pitcock <nenolod -at- atheme.org>
 * This code is in the public domain.
 */

#ifndef __BINIO_VIRTUAL__
#define __BINIO_VIRTUAL__

#include <binio.h>
#include <binstr.h>

#include <string>

#include <glib.h>
#include <audacious/vfs.h>

class vfsistream : public binistream, virtual public binio {
private:
	VFSFile *fd;

public:
	vfsistream() {};

	vfsistream(const char *file) {
		fd = vfs_fopen(file, "rb");
	};

	vfsistream(std::string &file) {
		fd = vfs_fopen(file.c_str(), "rb");
	};

	~vfsistream() {
		if (fd != NULL)
			vfs_fclose(fd);
	};

	void open(const char *file) {
		fd = vfs_fopen(file, "rb");
	};

	void open(std::string &file) {
		fd = vfs_fopen(file.c_str(), "rb");
	};

	// XXX: this sucks because binio won't let us do sanity checking
	Byte getByte(void) {
		return vfs_getc(fd);
	};

	void seek(long pos, Offset offs = Set) {
		vfs_fseek(fd, pos, offs == Set ? SEEK_SET : SEEK_CUR);
	}

	long pos(void) {
		return vfs_ftell(fd);
	}
};

// XXX: binio sucks and doesn't let us just combine the two.
class vfsostream : public binostream, virtual public binio {
private:
	VFSFile *fd;

public:
	vfsostream() {};

	vfsostream(const char *file) {
		fd = vfs_fopen(file, "wb");
	};

	vfsostream(std::string &file) {
		fd = vfs_fopen(file.c_str(), "wb");
	};

	~vfsostream() {
		if (fd != NULL)
			vfs_fclose(fd);
	};

	void open(const char *file) {
		fd = vfs_fopen(file, "wb");
	};

	void open(std::string &file) {
		fd = vfs_fopen(file.c_str(), "wb");
	};

	// XXX: this sucks because binio won't let us do sanity checking
	void putByte(Byte b) {
		vfs_fwrite(&b, 1, 1, fd);
	};

	void seek(long pos, Offset offs = Set) {
		vfs_fseek(fd, pos, offs == Set ? SEEK_SET : SEEK_CUR);
	}

	long pos(void) {
		return vfs_ftell(fd);
	}
};

#endif
