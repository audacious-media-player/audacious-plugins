PLUGIN = lirc$(PLUGIN_SUFFIX)

plugindir = audacious/$(GENERAL_PLUGIN_DIR)
SRCS = about.c lirc.c

include ../../buildsys.mk
include ../../extra.mk

LIBS += -llirc_client $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(GTK_CFLAGS) $(MOWGLI_CFLAGS) $(PANGO_CFLAGS) $(DBUS_CFLAGS) -I../../intl -I../..

