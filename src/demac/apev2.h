#ifndef APEV2_H
#define APEV2_H

#include <mowgli.h>

mowgli_dictionary_t* parse_apev2_tag(VFSFile *vfd);
gboolean write_apev2_tag(VFSFile *vfd, mowgli_dictionary_t *tag);

#endif
