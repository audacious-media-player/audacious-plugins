plugin_sources = [
  'plugin.cc',
  'sndif2sf.cc',
  'XSFFile.cc'
]


desmume_sources = [
  'desmume/armcpu.cc',
  'desmume/arm_instructions.cc',
  'desmume/bios.cc',
  'desmume/cp15.cc',
  'desmume/emufile.cc',
  'desmume/FIFO.cc',
  'desmume/firmware.cc',
  'desmume/mc.cc',
  'desmume/metaspu.cc',
  'desmume/MMU.cc',
  'desmume/NDSSystem.cc',
  'desmume/readwrite.cc',
  'desmume/slot1.cc',
  'desmume/slot1_retail.cc',
  'desmume/SPU.cc',
  'desmume/thumb_instructions.cc'
]


spu_sources = [
  'spu/adpcmdecoder.cc',
  'spu/interpolator.cc',
  'spu/samplecache.cc',
  'spu/sampledata.cc'
]


cpp_args = cxx.get_supported_arguments([
  '-Wno-sign-compare',
  '-Wno-shift-negative-value'
])


shared_module('xsf',
  plugin_sources,
  desmume_sources,
  spu_sources,
  dependencies: [audacious_dep, zlib_dep],
  cpp_args: cpp_args,
  name_prefix: '',
  install: true,
  install_dir: input_plugin_dir
)
