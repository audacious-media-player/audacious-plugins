PLUGIN = libdaap${PLUGIN_SUFFIX}

SRCS = daap.c

SUBDIRS = xmms2-daap

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

#For the moment i'm hardcoding it but it will be configurable
#MDNS_LIBS=-ldns_sd
MDNS_LIBS=-lavahi-glib -lavahi-common -lavahi-client
#MDNS_LIBS=

INC = -I../.. -I./xmms2-daap
WARN = -Wall -pedantic -std=c99

CPPFLAGS += ${PICFLAGS} ${GTK_CFLAGS}  ${INC} ${WARN} ${GCC42_CFLAGS}
LIBS += ${GTK_LIBS}  ${MDNS_LIBS} ./xmms2-daap/xmms2-daap.a
