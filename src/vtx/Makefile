include ../../extra.mk

PLUGIN = vtx$(SHARED_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = about.c  ay8912.c  config.c  info.c  lh5dec.c  vtx.c  vtxfile.c

include ../../buildsys.mk

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) -I../../intl -I../.. -Wall
CPPFLAGS = $(CFLAGS) -I.
LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
