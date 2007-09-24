PLUGIN = neon$(PLUGIN_SUFFIX)
plugindir = audacious/$(TRANSPORT_PLUGIN_DIR)

SRCS = neon.c rb.c

include ../../buildsys.mk
include ../../extra.mk

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) $(XML_CPPFLAGS) \
	$(NEON_CFLAGS) -I../../intl -I../.. -Wall
LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(XML_LIBS) $(NEON_LIBS)
