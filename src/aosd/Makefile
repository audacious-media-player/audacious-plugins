include ../../extra.mk

PLUGIN = aosd$(PLUGIN_SUFFIX)
plugindir = audacious/$(GENERAL_PLUGIN_DIR)

noinst_HEADERS = aosd.h aosd_osd.h aosd_style.h aosd_style_private.h aosd_trigger.h aosd_trigger_private.h aosd_ui.h aosd_cfg.h aosd_common.h ghosd.h ghosd-internal.h

SRCS = aosd.c aosd_osd.c aosd_style.c aosd_trigger.c aosd_ui.c aosd_cfg.c ghosd.c ghosd-main.c

include ../../buildsys.mk

LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(CAIRO_LIBS) $(PANGOCAIRO_LIBS) $(XRENDER_LIBS) $(XCOMPOSITE_LIBS)
CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(CAIRO_CFLAGS) $(PANGOCAIRO_CFLAGS) \
	$(XRENDER_CFLAGS) $(XCOMPOSITE_CFLAGS) -I../../intl -I../.. -I..
