// File_Reader wrapper over a VFSFile

#ifndef VFS_FILE_H
#define VFS_FILE_H

#include "Data_Reader.h"

class VFSFile;

class Vfs_File_Reader : public File_Reader {
public:
	// use already-open file and doesn't close it in close()
	void reset(VFSFile &file_);
	error_t open( const char* path );
	void close();

	~Vfs_File_Reader()
	    { close(); }

	long size() const;
	long read_avail( void*, long );
	long tell() const;
	error_t seek( long );
private:
	VFSFile *file = nullptr, *owned_file = nullptr;
};

#endif
