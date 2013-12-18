PLUGIN = adplug${PLUGIN_SUFFIX}

SRCS = adplug-xmms.cc		\
       core/fmopl.c		\
       core/debug.c		\
       core/adlibemu.c		\
       core/adplug.cxx		\
       core/emuopl.cxx		\
       core/fprovide.cxx	\
       core/player.cxx		\
       core/database.cxx	\
       core/hsc.cxx		\
       core/sng.cxx		\
       core/imf.cxx		\
       core/players.cxx		\
       core/protrack.cxx	\
       core/a2m.cxx		\
       core/adtrack.cxx		\
       core/amd.cxx		\
       core/bam.cxx		\
       core/cmf.cxx		\
       core/d00.cxx		\
       core/dfm.cxx		\
       core/dmo.cxx		\
       core/hsp.cxx		\
       core/ksm.cxx		\
       core/mad.cxx		\
       core/mid.cxx		\
       core/mkj.cxx		\
       core/cff.cxx		\
       core/dtm.cxx		\
       core/fmc.cxx		\
       core/mtk.cxx		\
       core/rad.cxx		\
       core/raw.cxx		\
       core/sa2.cxx		\
       core/s3m.cxx		\
       core/xad.cxx		\
       core/flash.cxx		\
       core/bmf.cxx		\
       core/hybrid.cxx		\
       core/hyp.cxx		\
       core/psi.cxx		\
       core/rat.cxx		\
       core/u6m.cxx		\
       core/rol.cxx		\
       core/xsm.cxx		\
       core/dro.cxx		\
       core/dro2.cxx		\
       core/lds.cxx		\
       core/temuopl.cxx		\
       core/msc.cxx		\
       core/rix.cxx		\
       core/adl.cxx		\
       core/jbm.cxx		\
       plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${BINIO_CFLAGS} -I../.. -I./core
LIBS += ${GLIB_LIBS} ${BINIO_LIBS}
