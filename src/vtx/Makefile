include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libvtx$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = lh5dec.c ay8912.c about.c config.c info.c vtx.c vtxfile.c 

OBJECTS = ${SOURCES:.c=.o}

LIBADD = $(VTX_LIBS)

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) -I. -I../../intl -I../.. -I../..

include ../../mk/objective.mk
