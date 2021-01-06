shared_module('coreaudio',
  'audiodevice.mm',
  'coreaudio.cc',
  dependencies: [audacious_dep],
  name_prefix: '',
  objcpp_args: '-std=c++11',
  link_args: ['-framework', 'CoreAudio'],
  install: true,
  install_dir: output_plugin_dir
)
