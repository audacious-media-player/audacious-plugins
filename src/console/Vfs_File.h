// File_Reader wrapper over a VFSFile

#ifndef VFS_FILE_H
#define VFS_FILE_H

#include "Data_Reader.h"

class Vfs_File_Reader : public File_Reader {
public:
	// use already-open file and doesn't close it in close()
	void reset (/* VFSFile * */ void * file);
	error_t open( const char* path );
	void close();

public:
	Vfs_File_Reader();
	~Vfs_File_Reader();
	long size() const;
	long read_avail( void*, long );
	long tell() const;
	error_t seek( long );
private:
    struct reader_private * p;
};

#endif
