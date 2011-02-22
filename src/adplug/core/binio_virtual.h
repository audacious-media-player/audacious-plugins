/*
 * Copyright (c) 2006 William Pitcock <nenolod -at- atheme.org>
 * This code is in the public domain.
 */

#ifndef __BINIO_VIRTUAL__
#define __BINIO_VIRTUAL__

#include <binio.h>

extern "C" {
#include <libaudcore/vfs.h>
};

class vfsistream : public binistream {
private:
	VFSFile *fd;

public:
	vfsistream() {
		this->fd = 0;
	};

	~vfsistream() {
		if (this->fd) {
			vfs_fclose(this->fd);
			this->fd = 0;
		}
	};

	vfsistream(VFSFile *fd) {
		this->fd = fd;
	};

	vfsistream(const char *file) {
		this->fd = vfs_fopen(file, "r");
		if (!this->fd)
			err |= NotFound;
	};

	vfsistream(std::string &file) {
		this->fd = vfs_fopen(file.c_str(), "r");
		if (!this->fd)
			err |= NotFound;
	};

	void open(const char *file) {
		g_return_if_fail(!this->fd);
		this->fd = vfs_fopen(file, "r");
		if (!this->fd)
			err |= NotFound;
	};

	void open(std::string &file) {
		g_return_if_fail(!this->fd);
		this->fd = vfs_fopen(file.c_str(), "r");
		if (!this->fd)
			err |= NotFound;
	};

	Byte getByte(void) {
		g_return_val_if_fail(this->fd, EOF);
		int c = vfs_getc(this->fd);
		if (c < 0)
			err |= Eof;
		return (Byte) c;
	};

	void seek(long pos, Offset offs = Set) {
		g_return_if_fail(this->fd);
		int wh = (offs == Add) ? SEEK_CUR : (offs == End) ? SEEK_END : SEEK_SET;
		if (vfs_fseek(this->fd, pos, wh))
			err |= Eof;
	}

	long pos(void) {
		g_return_val_if_fail(this->fd, -1);
		return vfs_ftell(this->fd);
	}
};

class vfsostream : public binostream {
private:
	VFSFile *fd;

public:
	vfsostream() {
		this->fd = 0;
	};

	~vfsostream() {
		if (this->fd) {
			vfs_fclose(this->fd);
			this->fd = 0;
		}
	};

	vfsostream(VFSFile *fd) {
		this->fd = fd;
	};

	vfsostream(const char *file) {
		this->fd = vfs_fopen(file, "w");
		if (!this->fd)
			err |= Denied;
	};

	vfsostream(std::string &file) {
		this->fd = vfs_fopen(file.c_str(), "w");
		if (!this->fd)
			err |= Denied;
	};

	void open(const char *file) {
		g_return_if_fail(!this->fd);
		this->fd = vfs_fopen(file, "w");
		if (!this->fd)
			err |= Denied;
	};

	void open(std::string &file) {
		g_return_if_fail(!this->fd);
		this->fd = vfs_fopen(file.c_str(), "w");
		if (!this->fd)
			err |= Denied;
	};

	void putByte(Byte b) {
		g_return_if_fail(this->fd);
		if (vfs_fwrite(&b, 1, 1, this->fd) != 1)
			err |= Fatal;
	};

	void seek(long pos, Offset offs = Set) {
		g_return_if_fail(this->fd);
		int wh = (offs == Add) ? SEEK_CUR : (offs == End) ? SEEK_END : SEEK_SET;
		if (vfs_fseek(this->fd, pos, wh))
			err |= Fatal;
	}

	long pos(void) {
		g_return_val_if_fail(this->fd, -1);
		return vfs_ftell(this->fd);
	}
};

#endif
