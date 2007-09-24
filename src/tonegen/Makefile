include ../../extra.mk

PLUGIN = tonegen$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = tonegen.c

include ../../buildsys.mk

LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

CFLAGS += $(PLUGIN_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../..
CPPFLAGS = $(CFLAGS)
