include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libwma$(SHARED_SUFFIX)

SUBDIRS = libffwma

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LIBADD = ./libffwma/libffwma.a $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

SOURCES = wma.c

OBJECTS = ${SOURCES:.c=.o}

LIBDEP = ./libffwma/libffwma.a

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../.. -I./libffwma -std=c99 -fgnu89-inline

include ../../mk/objective.mk
