/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#include <cstdlib>

#include "arch_raw.h"

using namespace std;

arch_Raw::arch_Raw(const string& aFileName)
{
    mFileDesc = VFSFile(aFileName.c_str(), "r");
    if (!mFileDesc)
    {
        mSize = 0;
        return;
    }

    mSize = mFileDesc.fsize ();
    if (mSize <= 0)
    {
        mSize = 0;
        return;
    }

    mMap = malloc(mSize);
    if (mFileDesc.fread (mMap, 1, mSize) < mSize)
    {
        free(mMap);
        mSize = 0;
        return;
    }
}

arch_Raw::~arch_Raw()
{
    if(mSize != 0)
    {
        free(mMap);
    }
}

bool arch_Raw::ContainsMod(const string& aFileName)
{
    return IsOurFile(aFileName);
}
