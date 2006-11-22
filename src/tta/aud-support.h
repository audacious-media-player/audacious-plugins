#include <audacious/vfs.h>

#define FILE VFSFile
#define fopen 	vfs_fopen
#define fclose 	vfs_fclose
#define fwrite  vfs_fwrite
#define fread	vfs_fread
#define frewind vfs_frewind
#define ftell	vfs_ftell
#define fseek	vfs_fseek

size_t file_size (char *filename)
{
    VFSFile *f;
    size_t size = -1;

    if ((f = vfs_fopen (filename, "r")))
    {
	vfs_fseek (f, 0, SEEK_END);
	size = vfs_ftell (f);
	vfs_fclose (f); 
    }
    return size;
}
