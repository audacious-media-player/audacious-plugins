#ifndef PLUGIN_H
#define PLUGIN_H

#include <curl/curl.h>

void start(void);
void stop(void);
void setup_proxy(CURL *curl);

#endif
