#ifndef APEV2_H
#define APEV2_H

#include <mowgli.h>
#include <audacious/vfs.h>

mowgli_dictionary_t* parse_apev2_tag(VFSFile *vfd);

#endif
