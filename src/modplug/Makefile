include ../../extra.mk

PLUGIN = modplug$(SHARED_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SOURCES.plugin  = plugin.cxx modplugbmp.cxx
SOURCES.core = tables.cxx         \
               sndmix.cxx         \
               sndfile.cxx        \
               snd_fx.cxx         \
               snd_flt.cxx        \
               snd_dsp.cxx        \
               fastmix.cxx        \
               mmcmp.cxx          \
               load_xm.cxx        \
               load_wav.cxx       \
               load_umx.cxx       \
               load_ult.cxx       \
               load_stm.cxx       \
               load_s3m.cxx       \
               load_ptm.cxx       \
               load_okt.cxx       \
               load_mtm.cxx       \
               load_mod.cxx       \
               load_med.cxx       \
               load_mdl.cxx       \
               load_it.cxx        \
               load_far.cxx       \
               load_dsm.cxx       \
               load_dmf.cxx       \
               load_dbm.cxx       \
               load_ams.cxx       \
               load_amf.cxx       \
               load_669.cxx       \
               load_j2b.cxx       \
               load_mt2.cxx       \
               load_psm.cxx       \
               modplug.cxx

SRCS = ${SOURCES.core} ${SOURCES.plugin}

LIBDEP = gui/libgui.a archive/libarchive.a

include ../../buildsys.mk

CXXFLAGS += $(PLUGIN_CFLAGS)
CFLAGS   += $(PLUGIN_CFLAGS)

CPPFLAGS += $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I.. -I../.. -I../../intl
LIBS     += $(MOWGLI_LIBS) $(DBUS_LIBS) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) -lstdc++ ./archive/libarchive.a ./gui/libgui.a

SUBDIRS = archive gui
