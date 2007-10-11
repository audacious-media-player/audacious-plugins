STATIC_LIB_NOINST = xmms2-daap.a

SRCS = cc_handlers.c	\
       daap_conn.c	\
       daap_cmd.c	\
       daap_md5.c	\
       daap_util.c	\
       daap_mdns_dnssd.c \
       daap_mdns_avahi.c \
       daap_mdns_dummy.c
#       daap_xform.c 

include ../../../buildsys.mk
include ../../../extra.mk

#For the moment i'm hardcoding it but it will be configurable
#MDNS_IMPL=DAAP_MDNS_DNSSD
MDNS_IMPL=DAAP_MDNS_AVAHI
#MDNS_IMPL=DAAP_MDNS_DUMMY 

WARN = -Wall -std=c99
DEFINES = -D${MDNS_IMPL} -D_POSIX_SOURCE

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${PICFLAGS} ${WARN} ${DEFINES}
LIBS += ${GLIB_LIBS} 
