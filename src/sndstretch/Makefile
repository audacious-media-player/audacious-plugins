PLUGIN = sndstretch$(PLUGIN_SUFFIX)
plugindir = audacious/$(EFFECT_PLUGIN_DIR)

SRCS = sndstretch_xmms.c sndstretch.c

include ../../extra.mk
include ../../buildsys.mk

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(MOWGLI_CFLAGS) $(PANGO_CFLAGS) \
	-I../../intl -I../..

