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
	p->file = p->owned_file = vfs_fopen (path, "r");
	if (! p->file)
		return "Couldn't open file";
	return 0;
}

long Vfs_File_Reader::size() const
{
	return vfs_fsize (p->file);
}

long Vfs_File_Reader::read_avail (void * buf, long size)
{
	return (long) vfs_fread (buf, 1, size, p->file);
}

long Vfs_File_Reader::tell() const
{
	return vfs_ftell (p->file);
}

Vfs_File_Reader::error_t Vfs_File_Reader::seek( long n )
{
	if (vfs_fseek (p->file, n, SEEK_SET) < 0)
		return eof_error;
	return 0;
}

void Vfs_File_Reader::close()
{
	p->file = 0;
	if (p->owned_file)
	{
		vfs_fclose (p->owned_file);
		p->owned_file = 0;
	}
}
