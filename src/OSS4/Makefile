include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libOSSlib$(SHARED_SUFFIX)

noinst_HEADERS = OSS4.h soundcard.h

LIBDIR = $(plugindir)/$(OUTPUT_PLUGIN_DIR)

LIBADD = $(OSS_LIBS) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
SOURCES = OSS4.c about.c configure.c audio.c init.c convert.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I/usr/lib/oss/include -I../../intl -I../..

include ../../mk/objective.mk
