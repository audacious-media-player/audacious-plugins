#include <stdio.h>

#include "Vfs_File.h"

#include "libaudcore/vfs.h"

void Vfs_File_Reader::reset(VFSFile &file_)
{
	close();
	file = &file_;
}

Vfs_File_Reader::error_t Vfs_File_Reader::open( const char* path )
{
	close();
	file = owned_file = new VFSFile (path, "r");

	if (! * file)
	{
		close();
		return "Couldn't open file";
	}

	return 0;
}

long Vfs_File_Reader::size() const
{
	return file->fsize ();
}

long Vfs_File_Reader::read_avail (void * buf, long size)
{
	return (long) file->fread (buf, 1, size);
}

long Vfs_File_Reader::tell() const
{
	return file->ftell ();
}

Vfs_File_Reader::error_t Vfs_File_Reader::seek( long n )
{
	if (file->fseek (n, VFS_SEEK_SET) < 0)
		return eof_error;
	return 0;
}

void Vfs_File_Reader::close()
{
	file = 0;
	if (owned_file)
	{
		delete owned_file;
		owned_file = 0;
	}
}
