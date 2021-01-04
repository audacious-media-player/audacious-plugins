plugin_sources = [
  'corlett.cc',
  'plugin.cc',
  'vio2sf.cc'
]


desmume_sources = [
  'desmume/armcpu.cc',
  'desmume/arm_instructions.cc',
  'desmume/bios.cc',
  'desmume/cp15.cc',
  'desmume/FIFO.cc',
  'desmume/GPU.cc',
  'desmume/matrix.cc',
  'desmume/mc.cc',
  'desmume/MMU.cc',
  'desmume/NDSSystem.cc',
  'desmume/SPU.cc',
  'desmume/thumb_instructions.cc'
]


cpp_args = cxx.get_supported_arguments([
  '-Wno-sign-compare',
  '-Wno-shift-negative-value'
])


shared_module('xsf',
  plugin_sources,
  desmume_sources,
  dependencies: [audacious_dep, zlib_dep],
  cpp_args: cpp_args,
  name_prefix: '',
  install: true,
  install_dir: input_plugin_dir
)
