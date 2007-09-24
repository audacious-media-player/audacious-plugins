include ../../extra.mk

PLUGIN = audiocompress$(SHARED_SUFFIX)
plugindir = audacious/$(EFFECT_PLUGIN_DIR)

SRCS = compress.c audacious-glue.c

include ../../buildsys.mk

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../..
LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

