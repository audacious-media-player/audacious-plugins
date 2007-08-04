include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libdaap$(SHARED_SUFFIX)

SUBDIRS = xmms2-daap

LIBDIR = $(plugindir)/$(TRANSPORT_PLUGIN_DIR)

LIBADD += ./xmms2-daap/xmms2-daap.a $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) 

SOURCES = daap.c

OBJECTS = ${SOURCES:.c=.o}

LIBDEP = ./xmms2-daap/xmms2-daap.a

INC = -I../.. -I./xmms2-daap

WARN = -Wall  -pedantic -std=c99 

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(INC) $(WARN)  $(GCC42_CFLAGS)

include ../../mk/objective.mk
