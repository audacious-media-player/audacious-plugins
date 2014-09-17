#include <stdio.h>

#include "Vfs_File.h"

#include "libaudcore/vfs.h"

struct reader_private {
	VFSFile * file, * owned_file;
};

Vfs_File_Reader::Vfs_File_Reader ()
{
	p = new struct reader_private;
	p->file = 0;
	p->owned_file = 0;
}

Vfs_File_Reader::~Vfs_File_Reader ()
{
	close ();
	delete p;
}

void Vfs_File_Reader::reset (/* VFSFile * */ void * file)
{
	close ();
	p->file = (VFSFile *) file;
}

Vfs_File_Reader::error_t Vfs_File_Reader::open( const char* path )
{
	close();
	auto file = new VFSFile (path, "r");

	if (! * file)
	{
		delete file;
		return "Couldn't open file";
	}

	p->file = p->owned_file = file;
	return 0;
}

long Vfs_File_Reader::size() const
{
	return p->file->fsize ();
}

long Vfs_File_Reader::read_avail (void * buf, long size)
{
	return (long) p->file->fread (buf, 1, size);
}

long Vfs_File_Reader::tell() const
{
	return p->file->ftell ();
}

Vfs_File_Reader::error_t Vfs_File_Reader::seek( long n )
{
	if (p->file->fseek (n, VFS_SEEK_SET) < 0)
		return eof_error;
	return 0;
}

void Vfs_File_Reader::close()
{
	p->file = 0;
	if (p->owned_file)
	{
		delete p->owned_file;
		p->owned_file = 0;
	}
}
