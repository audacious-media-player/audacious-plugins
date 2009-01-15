#define XSF_FALSE (0)
#define XSF_TRUE (!XSF_FALSE)

int xsf_start(void *pfile, unsigned bytes);
int xsf_gen(void *pbuffer, unsigned samples);
int xsf_get_lib(char *pfilename, void **ppbuffer, unsigned int *plength);
void xsf_term(void);
