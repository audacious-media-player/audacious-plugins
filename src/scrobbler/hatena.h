#ifndef HATENA_H
#define HATENA_H 1

#include "audacious/titlestring.h"

int hatena_sc_idle(GMutex *);
void hatena_sc_init(char *, char *);
void hatena_sc_addentry(GMutex *, TitleInput *, int);
void hatena_sc_cleaner(void);
int hatena_sc_catch_error(void);
char *hatena_sc_fetch_error(void);
void hatena_sc_clear_error(void);
#endif
