include ../../extra.mk

PLUGIN = tta$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = libtta.c ttadec.c

include ../../buildsys.mk

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../..
LIBS += -L$(libdir) -laudid3tag $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
