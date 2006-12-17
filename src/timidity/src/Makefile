include ../../../mk/rules.mk
include ../../../mk/init.mk

OBJECTIVE_LIBS = libtimidity$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LIBADD = ../libtimidity/libtimidity.a $(AUDACIOUS_LIBS)

SOURCES = \
	callbacks.c \
	interface.c \
	xmms-timidity.c \

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(AUDACIOUS_CFLAGS) -I../../.. -I../libtimidity

include ../../../mk/objective.mk
