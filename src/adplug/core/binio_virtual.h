/*
 * Copyright (c) 2006 William Pitcock <nenolod -at- atheme.org>
 * This code is in the public domain.
 */

#ifndef __BINIO_VIRTUAL__
#define __BINIO_VIRTUAL__

#include <libbinio/binio.h>
#include <stdio.h>

extern "C" {
#include <libaudcore/vfs.h>
};

class vfsistream : public binistream {
private:
	VFSFile *fd; bool own;

public:
	vfsistream(VFSFile *fd = 0) { this->fd = fd; this->own = false; };

	void open(const char *file) {
		if ((this->fd = vfs_fopen(file, "r")))
			this->own = true;
		else
			err |= NotFound;
	};

	void open(std::string &file) { open(file.c_str()); };

	vfsistream(const char *file) { this->fd = 0; this->own = false; open(file); };
	vfsistream(std::string &file) {	this->fd = 0; this->own = false; open(file); };

	~vfsistream() {
		if (this->own)
			vfs_fclose(this->fd);
		this->fd = 0; this->own = false;
	};

	Byte getByte(void) {
		int c = vfs_getc(this->fd);
		if (c < 0)
			err |= Eof;
		return (Byte) c;
	};

	void seek(long pos, Offset offs = Set) {
		int wh = (offs == Add) ? SEEK_CUR : (offs == End) ? SEEK_END : SEEK_SET;
		if (vfs_fseek(this->fd, pos, wh))
			err |= Eof;
	}

	long pos(void) {
		return vfs_ftell(this->fd);
	}
};

class vfsostream : public binostream {
private:
	VFSFile *fd; bool own;

public:
	vfsostream(VFSFile *fd = 0) { this->fd = fd; this->own = false; };

	void open(const char *file) {
		if ((this->fd = vfs_fopen(file, "w")))
			this->own = true;
		else
			err |= Denied;
	};

	void open(std::string &file) { open(file.c_str()); };

	vfsostream(const char *file) { this->fd = 0; this->own = false; open(file); };
	vfsostream(std::string &file) {	this->fd = 0; this->own = false; open(file); };

	~vfsostream() {
		if (this->own)
			vfs_fclose(this->fd);
		this->fd = 0; this->own = false;
	};

	void putByte(Byte b) {
		if (vfs_fwrite(&b, 1, 1, this->fd) != 1)
			err |= Fatal;
	};

	void seek(long pos, Offset offs = Set) {
		int wh = (offs == Add) ? SEEK_CUR : (offs == End) ? SEEK_END : SEEK_SET;
		if (vfs_fseek(this->fd, pos, wh))
			err |= Fatal;
	}

	long pos(void) {
		return vfs_ftell(this->fd);
	}
};

#endif
