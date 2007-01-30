include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libtonegen$(SHARED_SUFFIX)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = tonegen.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../..

include ../../mk/objective.mk
