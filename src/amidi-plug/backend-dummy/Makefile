PLUGIN = ap-dummy$(PLUGIN_SUFFIX)
plugindir = audacious/$(AMIDIPLUG_BACKEND_DIR)

noinst_HEADERS = b-dummy.h b-dummy-config.h backend-dummy-icon.xpm

SRCS = b-dummy.c

include ../../../buildsys.mk
include ../../../extra.mk

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(GLIB_CFLAGS) -I../../..
LIBS += ../pcfg/libpcfg.a
