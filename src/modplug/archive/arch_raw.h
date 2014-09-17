/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#ifndef __MODPLUG_ARCH_RAW_H__INCLUDED__
#define __MODPLUG_ARCH_RAW_H__INCLUDED__

#include "archive.h"

#include <libaudcore/vfs.h>

class arch_Raw: public Archive
{
    VFSFile mFileDesc;

public:
    arch_Raw(const std::string& aFileName);
    virtual ~arch_Raw();

    static bool ContainsMod(const std::string& aFileName);
};

#endif
