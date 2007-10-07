// File_Reader wrapper over a VFSFile

#ifndef VFS_FILE_H
#define VFS_FILE_H

#include "Data_Reader.h"

#include <audacious/plugin.h>

class Vfs_File_Reader : public File_Reader {
public:
	void reset( VFSFile* ); // use already-open file and doesn't close it in close()
	error_t open( const char* path );
	VFSFile* file() const { return file_; }
	void close();
	
public:
	Vfs_File_Reader();
	~Vfs_File_Reader();
	long size() const;
	long read_avail( void*, long );
	long tell() const;
	error_t seek( long );
private:
	VFSFile* file_;
	VFSFile* owned_file_;
};

#endif
