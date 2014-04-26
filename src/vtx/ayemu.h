/*
  ayemu - AY/YM sound chip emulator and music file loader
  Copyright (C) 2003-2004 Sashnov Alexander

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

  Alexander Sashnov
  sashnov@ngs.ru
*/
#ifndef _AYEMU_H
#define _AYEMU_H

#define EXTERN extern

/* typedefs for 32-bit architecture */
typedef unsigned char	Uint8;
typedef signed char	Sint8;
typedef unsigned short	Uint16;
typedef signed short	Sint16;
typedef unsigned int	Uint32;
typedef signed int	Sint32;

/* include other library headers */
#include "ayemu_8912.h"
#include "ayemu_vtxfile.h"

#endif
