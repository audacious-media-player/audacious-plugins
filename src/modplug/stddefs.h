/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

// stddefs.h: Standard defs to streamline coding style.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__MODPLUGXMMS_STDDEFS_H__INCLUDED__)
#define __MODPLUGXMMS_STDDEFS_H__INCLUDED__

#include <glib.h>

#define MODPLUG_CFGID		"modplug"
#define MODPLUG_CONVERT(X)	g_convert(X, -1, "UTF-8", "CP850", NULL, NULL, NULL)

//Invalid pointer
#ifndef NULL
#define NULL 0
#endif

//Standard types. ----------------------------------------
//These data types are provided because the standard types vary across
// platforms.  For example, long is 64-bit on some 64-bit systems.
//u = unsigned
//# = size in bits
typedef guchar                 uchar;

typedef gint8                  int8;
typedef gint16                 int16;
typedef gint32                 int32;
typedef gint64                 int64;

typedef guint8                 uint8;
typedef guint16                uint16;
typedef guint32                uint32;
typedef guint64                uint64;

typedef float                  float32;
typedef double                 float64;
typedef long double            float128;

#endif // included
