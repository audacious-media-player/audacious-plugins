include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libstereo$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(EFFECT_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

SOURCES = stereo.c

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../..

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
