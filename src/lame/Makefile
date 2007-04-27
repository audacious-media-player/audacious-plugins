include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = liblame$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(OUTPUT_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) -lmp3lame
SOURCES = out_lame.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) -I../../intl -I../.. -Wall

include ../../mk/objective.mk
