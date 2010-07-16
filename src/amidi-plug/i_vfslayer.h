/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/


#ifndef _I_VFSLAYER_H
#define _I_VFSLAYER_H 1

#include "i_common.h"
#include <audacious/plugin.h>


#define VFS_FOPEN( x , y )		vfs_fopen( x , y )
#define VFS_FCLOSE( x )			vfs_fclose( x )
#define VFS_FREAD( x , y , z , w )	vfs_fread( x , y , z , w )
#define VFS_FSEEK( x , y , z )		vfs_fseek( x , y , z )
#define VFS_FEOF( x )			vfs_feof( x )
#define VFS_GETC( x )			vfs_getc( x )
#define VFS_UNGETC( x , y )		vfs_ungetc( x , y )


#endif /* !_I_VFSLAYER_H */
