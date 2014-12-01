#include <libaudcore/index.h>

int xsf_start(void *pfile, unsigned bytes);
int xsf_gen(void *pbuffer, unsigned samples);
Index<char> xsf_get_lib(char *pfilename);
void xsf_term(void);
