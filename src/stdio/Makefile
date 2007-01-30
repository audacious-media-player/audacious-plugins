include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libstdio$(SHARED_SUFFIX)

# XXX
LIBDIR = $(plugindir)/$(CONTAINER_PLUGIN_DIR)

SOURCES = stdio.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) -I../../intl -I../..

CXXFLAGS = $(CFLAGS)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

include ../../mk/objective.mk
