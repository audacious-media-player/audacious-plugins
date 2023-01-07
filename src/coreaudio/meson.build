shared_module('coreaudio',
  'audiodevice.mm',
  'coreaudio.cc',
  dependencies: [audacious_dep, math_dep],
  name_prefix: '',
  objcpp_args: '-std=c++11',
  link_args: [
    '-framework', 'AudioUnit',
    '-framework', 'CoreAudio',
    '-framework', 'Foundation'
  ],
  install: true,
  install_dir: output_plugin_dir
)
