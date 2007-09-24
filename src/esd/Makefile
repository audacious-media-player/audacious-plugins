include ../../extra.mk

PLUGIN = ESD$(PLUGIN_SUFFIX)
plugindir = audacious/$(OUTPUT_PLUGIN_DIR)

SRCS = esd.c mixer.c about.c configure.c audio.c init.c

include ../../buildsys.mk

LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(ESD_LIBS) $(OSS_LIBS)

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ESD_CFLAGS) \
	-I../../intl -I../.. -I..

