PLUGIN = usf${PLUGIN_SUFFIX}

SRCS = audio.c \
       audio_hle_main.c \
       audio_ucode1.c \
       audio_ucode2.c \
       audio_ucode3.c \
       audio_ucode3mp3.c \
       cpu.c \
       dma.c \
       exception.c \
       interpreter_cpu.c \
       interpreter_ops.c \
       main.c \
       memory.c \
       pif.c \
       psftag.c \
       recompiler_cpu.c \
       recompiler_fpu_ops.c \
       recompiler_ops.c \
       registers.c \
       rsp.c \
       rsp_interpreter_cpu.c \
       rsp_mmx.c \
       rsp_recompiler_analysis.c \
       rsp_recompiler_cpu.c \
       rsp_recompiler_ops.c \
       rsp_sse.c \
       rsp_x86.c \
       tlb.c \
       usf.c \
       x86.c \
       x86_fpu.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} -O0
CXXFLAGS += ${PLUGIN_CFLAGS} -O0
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../.. -Ispu/ -I.
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${MOWGLI_LIBS} -lz -lm

# USF produces tons of warnings; disable the less important ones.
CFLAGS += -Wno-pointer-sign -Wno-parentheses -Wno-unused-variable \
 -Wno-unused-but-set-variable -Wno-unused-result -Wno-unused-value
