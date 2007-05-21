include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libcdaudio-ng$(SHARED_SUFFIX)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(CDIO_LIBS)
LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = cdaudio-ng.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(CDIO_LIBS) -I../../intl -I../..

include ../../mk/objective.mk
