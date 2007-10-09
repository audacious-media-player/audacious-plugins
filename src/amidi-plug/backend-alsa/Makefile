PLUGIN = ap-alsa$(PLUGIN_SUFFIX)
plugindir = audacious/$(AMIDIPLUG_BACKEND_DIR)

noinst_HEADERS = b-alsa.h b-alsa-config.h backend-alsa-icon.xpm

SRCS = b-alsa.c

include ../../../buildsys.mk
include ../../../extra.mk

CFLAGS += $(PLUGIN_CFLAGS) $(ALSA_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(ALSA_CFLAGS) $(GLIB_CFLAGS) -I../../..
LIBS += $(ALSA_LIBS) ../pcfg/libpcfg.a
