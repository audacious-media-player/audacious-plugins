include ../../extra.mk

PLUGIN = alac$(SHARED_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = alac.c demux.c plugin.c stream.c

include ../../buildsys.mk

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) -I../../intl -I../.. -Wall
CPPFLAGS = $(CFLAGS)
LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
