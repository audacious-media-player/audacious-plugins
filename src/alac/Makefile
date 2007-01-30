include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libalac$(SHARED_SUFFIX)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = alac.c demux.c plugin.c stream.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. -I../..

include ../../mk/objective.mk
