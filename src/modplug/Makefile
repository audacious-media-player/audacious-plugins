PLUGIN = modplug${PLUGIN_SUFFIX}

SRCS = plugin.cxx		\
       modplugbmp.cxx		\
       tables.cxx		\
       sndmix.cxx		\
       sndfile.cxx		\
       snd_eq.cxx		\
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
       load_mt2.cxx		\
       load_psm.cxx		\
       archive/archive.cxx	\
       archive/open.cxx		\
       archive/arch_raw.cxx	\
       plugin_main.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I.. -I../.. -I../../intl
LIBS += ${MOWGLI_LIBS}  ${GTK_LIBS} ${GLIB_LIBS}  -lstdc++
