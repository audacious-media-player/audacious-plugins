PLUGIN = lirc$(PLUGIN_SUFFIX)

SRCS = about.c \
       lirc.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/$(GENERAL_PLUGIN_DIR)

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(GTK_CFLAGS) $(MOWGLI_CFLAGS) $(PANGO_CFLAGS) $(DBUS_CFLAGS) -I../../intl -I../..
LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) -llirc_client
