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

#For the moment i'm hardcoding it but it will be configurable
#MDNS_IMPL=DAAP_MDNS_DNSSD
MDNS_IMPL=DAAP_MDNS_AVAHI
#MDNS_IMPL=DAAP_MDNS_DUMMY 

CFLAGS = $(GLIB_CFLAGS) $(PICFLAGS) -std=c99 -Wall -D_POSIX_SOURCE -D$(MDNS_IMPL) 

OBJECTS=${SOURCES:.c=.o}

include ../../../mk/objective.mk
