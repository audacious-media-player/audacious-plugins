include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libtta.so

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = aud-tta.c id3.c ttadec.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += -fPIC -DPIC $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../..

LDFLAGS += -Wl,-rpath=$(libdir)

LIBADD = -L$(libdir) -lid3tag $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

include ../../mk/objective.mk
