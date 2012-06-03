/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#include "open.h"
#include "arch_raw.h"

using namespace std;

Archive* OpenArchive(const string& aFileName) //aFilename is url --yaz
{
    return new arch_Raw(aFileName);
}
