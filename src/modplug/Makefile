PLUGIN = modplug${PLUGIN_SUFFIX}
SRCS = plugin.cxx		\
       modplugbmp.cxx		\
       tables.cxx		\
       sndmix.cxx		\
       sndfile.cxx		\
       snd_fx.cxx		\
       snd_flt.cxx		\
       snd_dsp.cxx		\
       fastmix.cxx		\
       mmcmp.cxx		\
       load_xm.cxx		\
       load_wav.cxx		\
       load_umx.cxx		\
       load_ult.cxx		\
       load_stm.cxx		\
       load_s3m.cxx		\
       load_ptm.cxx		\
       load_okt.cxx		\
       load_mtm.cxx		\
       load_mod.cxx		\
       load_med.cxx		\
       load_mdl.cxx		\
       load_it.cxx		\
       load_far.cxx		\
       load_dsm.cxx		\
       load_dmf.cxx		\
       load_dbm.cxx		\
       load_ams.cxx		\
       load_amf.cxx		\
       load_669.cxx		\
       load_j2b.cxx		\
       load_mt2.cxx		\
       load_psm.cxx		\
       modplug.cxx		\
       archive/archive.cxx	\
       archive/open.cxx		\
       archive/arch_raw.cxx	\
       archive/arch_gzip.cxx	\
       archive/arch_zip.cxx	\
       archive/arch_rar.cxx	\
       archive/arch_bz2.cxx	\
       gui/main.cxx		\
       gui/support.cxx		\
       gui/interface.cxx	\
       gui/callbacks.cxx

plugindir = audacious/${INPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I.. -I../.. -I../../intl
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${MOWGLI_LIBS} ${DBUS_LIBS} ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
