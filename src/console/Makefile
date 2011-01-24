PLUGIN = console${PLUGIN_SUFFIX}

SRCS = Ay_Apu.cxx              \
       Ay_Cpu.cxx              \
       Ay_Emu.cxx              \
       Blip_Buffer.cxx         \
       Classic_Emu.cxx         \
       Data_Reader.cxx         \
       Dual_Resampler.cxx      \
       Effects_Buffer.cxx      \
       Fir_Resampler.cxx       \
       Gbs_Emu.cxx             \
       Gb_Apu.cxx              \
       Gb_Cpu.cxx              \
       Gb_Oscs.cxx             \
       gme.cxx                 \
       Gme_File.cxx            \
       Gym_Emu.cxx             \
       Gzip_Reader.cxx         \
       Hes_Apu.cxx             \
       Hes_Cpu.cxx             \
       Hes_Emu.cxx             \
       Kss_Cpu.cxx             \
       Kss_Emu.cxx             \
       Kss_Scc_Apu.cxx         \
       M3u_Playlist.cxx        \
       Multi_Buffer.cxx        \
       Music_Emu.cxx           \
       Nes_Apu.cxx             \
       Nes_Cpu.cxx             \
       Nes_Fme7_Apu.cxx        \
       Nes_Namco_Apu.cxx       \
       Nes_Oscs.cxx            \
       Nes_Vrc6_Apu.cxx        \
       Nsfe_Emu.cxx            \
       Nsf_Emu.cxx             \
       Sap_Apu.cxx             \
       Sap_Cpu.cxx             \
       Sap_Emu.cxx             \
       Sms_Apu.cxx             \
       Snes_Spc.cxx            \
       Spc_Cpu.cxx             \
       Spc_Dsp.cxx             \
       Spc_Emu.cxx             \
       Spc_Filter.cxx          \
       Vfs_File.cxx            \
       Vgm_Emu.cxx             \
       Vgm_Emu_Impl.cxx        \
       Ym2413_Emu.cxx          \
       Ym2612_Emu.cxx          \
       Zlib_Inflater.cxx       \
       Audacious_Driver.cxx    \
       configure.c             \
       plugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${ARCH_DEFINES} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  -lz ${MOWGLI_LIBS}
