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

extern "C" {
#include <audacious/plugin.h>
};

class vfsistream : public binistream, virtual public binio {
private:
	VFSFile *fd;

public:
	vfsistream() {};
	~vfsistream() {};

	vfsistream(VFSFile *fd) {
		this->fd = fd;
	};

	vfsistream(const char *file) {
		this->fd = aud_vfs_fopen(file, "rb");
	};

	vfsistream(std::string &file) {
		this->fd = aud_vfs_fopen(file.c_str(), "rb");
	};

	void open(const char *file) {
		this->fd = aud_vfs_fopen(file, "rb");
	};

	void open(std::string &file) {
		this->fd = aud_vfs_fopen(file.c_str(), "rb");
	};

	// XXX: this sucks because binio won't let us do sanity checking
	Byte getByte(void) {
		int c = aud_vfs_getc(this->fd);

		if (c == EOF) err |= Eof;

		return (Byte) c;
	};

	void seek(long pos, Offset offs = Set) {
		switch (offs)
		{
			case Add:
				aud_vfs_fseek(this->fd, pos, SEEK_CUR);
				break;
			case End:
				aud_vfs_fseek(this->fd, pos, SEEK_END);
				break;
			case Set:
			default:
				aud_vfs_fseek(this->fd, pos, SEEK_SET);
				break;
		}

	}

	long pos(void) {
		return aud_vfs_ftell(this->fd);
	}
};

// XXX: binio sucks and doesn't let us just combine the two.
class vfsostream : public binostream, virtual public binio {
private:
	VFSFile *fd;

public:
	vfsostream() {};
	~vfsostream() {};

	vfsostream(VFSFile *fd) {
		this->fd = fd;
	};

	vfsostream(const char *file) {
		this->fd = aud_vfs_fopen(file, "wb");
	};

	vfsostream(std::string &file) {
		this->fd = aud_vfs_fopen(file.c_str(), "wb");
	};

	void open(const char *file) {
		this->fd = aud_vfs_fopen(file, "wb");
	};

	void open(std::string &file) {
		this->fd = aud_vfs_fopen(file.c_str(), "wb");
	};

	// XXX: this sucks because binio won't let us do sanity checking
	void putByte(Byte b) {
		aud_vfs_fwrite(&b, 1, 1, this->fd);
	};

	void seek(long pos, Offset offs = Set) {
		switch (offs)
		{
			case Add:
				aud_vfs_fseek(this->fd, pos, SEEK_CUR);
				break;
			case End:
				aud_vfs_fseek(this->fd, pos, SEEK_END);
				break;
			case Set:
			default:
				aud_vfs_fseek(this->fd, pos, SEEK_SET);
				break;
		}

	}

	long pos(void) {
		return aud_vfs_ftell(this->fd);
	}
};

#endif
