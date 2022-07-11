PLUGIN = xsf${PLUGIN_SUFFIX}

SRCS = plugin.cc \
       sndif2sf.cc \
       XSFFile.cc \
       spu/adpcmdecoder.cc           spu/interpolator.cc  spu/samplecache.cc spu/sampledata.cc \
       desmume/armcpu.cc             desmume/bios.cc      desmume/FIFO.cc    desmume/metaspu.cc    desmume/MMU.cc \
       desmume/arm_instructions.cc   desmume/cp15.cc      desmume/mc.cc      desmume/NDSSystem.cc  desmume/SPU.cc \
       desmume/thumb_instructions.cc desmume/readwrite.cc desmume/emufile.cc desmume/firmware.cc   \
       desmume/slot1.cc              desmume/slot1_retail.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CXXFLAGS += ${PLUGIN_CFLAGS} -Wno-sign-compare -Wno-shift-negative-value
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. -Ispu/
LIBS += -lm -lz
