include ../../../mk/rules.mk
include ../../../mk/init.mk

OBJECTIVE_LIBS_NOINST= xmms2-daap.a

SOURCES= cc_handlers.c \
		 daap_conn.c \
		 daap_cmd.c \
		 daap_md5.c \
		 daap_util.c \
		 daap_mdns_dnssd.c \
		 daap_mdns_avahi.c \
		 daap_mdns_dummy.c 

#		 daap_xform.c 
LIBADD = $(GLIB_LIBS) 
 

CFLAGS = $(GLIB_CFLAGS)  -I/usr/include   -std=c99 -Wall # -H -v

#CFLAGS += $(PICFLAGS) $(ARCH_DEFINES) $(CURL_CFLAGS) -c -I../../../intl -I../../.. -I/usr/include -Wall   -std=c99 -H -v
OBJECTS=${SOURCES:.c=.o}

include ../../../mk/objective.mk
