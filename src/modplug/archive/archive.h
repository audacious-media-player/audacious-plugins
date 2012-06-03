/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#ifndef __MODPLUG_ARCHIVE_H__INCLUDED__
#define __MODPLUG_ARCHIVE_H__INCLUDED__

#include <stdint.h>
#include <string>

class Archive
{
protected:
    uint32_t mSize;
    void* mMap;

    //This version of IsOurFile is slightly different...
    static bool IsOurFile(const std::string& aFileName);

public:
    virtual ~Archive();

    inline uint32_t Size() {return mSize;}
    inline void* Map() {return mMap;}
};

#endif
