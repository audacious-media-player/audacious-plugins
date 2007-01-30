include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libcdaudio$(SHARED_SUFFIX)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = cdaudio.c cddb.c cdinfo.c configure.c http.c 

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. -I../..

include ../../mk/objective.mk
