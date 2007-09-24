include ../../extra.mk

PLUGIN = wma$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)
SUBDIRS = libffwma

LIBDEP = ./libffwma/libffwma.a

SRCS = wma.c

include ../../buildsys.mk

LIBS += ./libffwma/libffwma.a $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../.. -I./libffwma -std=c99 $(GCC42_CFLAGS)

