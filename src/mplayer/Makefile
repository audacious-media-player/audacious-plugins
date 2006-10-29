include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libmplayer$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LIBADD += $(GTK_LIBS)

SOURCES = xmmsmplayer.c common.c interface.c 

CFLAGS   += $(GTK_CFLAGS) -I../../intl -I../..
CXXFLAGS += $(GTK_CFLAGS) -I../../intl -I../..

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
