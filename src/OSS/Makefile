include ../../extra.mk

PLUGIN = OSS$(SHARED_SUFFIX)
plugindir = audacious/$(OUTPUT_PLUGIN_DIR)

SRCS = OSS.c mixer.c about.c configure.c audio.c init.c convert.c

include ../../buildsys.mk

LIBS += $(OSS_LIBS) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(MOWGLI_CFLAGS) $(OSS_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. -I.

