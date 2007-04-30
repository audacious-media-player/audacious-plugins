include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libfilewriter$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(OUTPUT_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) -lmp3lame
SOURCES = filewriter.c wav.c mp3.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) -I../../intl -I../.. -Wall

include ../../mk/objective.mk
