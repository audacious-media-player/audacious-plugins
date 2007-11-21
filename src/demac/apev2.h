#ifndef APEV2_H
#define APEV2_H

#include <glib.h>
#include <audacious/vfs.h>

GHashTable* parse_apev2_tag(VFSFile *vfd);

#endif
