include ../../extra.mk

PLUGIN = musepack$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = libmpc.cxx

include ../../buildsys.mk

LIBS += -lmpcdec $(TAGLIB_LIBS) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

CXXFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(TAGLIB_CFLAGS) -I../../intl -I../..
