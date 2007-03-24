include ../../../mk/rules.mk
include ../../../mk/init.mk

OBJECTIVE_LIBS = libtimidity$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LIBADD = ../libtimidity/libtimidity.a $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

SOURCES = \
	callbacks.c \
	interface.c \
	xmms-timidity.c \

OBJECTS = ${SOURCES:.c=.o}

LIBDEP = ../libtimidity/libtimidity.a

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../.. -I../libtimidity

include ../../../mk/objective.mk
