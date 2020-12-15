# XXX - Auto-detect this path from /etc/oss.conf
oss_inc = '/usr/lib/oss/include/sys'


oss_sources = [
  'oss.cc',
  'plugin.cc',
  'utils.cc'
]


if cxx.has_header('soundcard.h', args: '-I' + oss_inc)
  shared_module('oss4',
    oss_sources,
    dependencies: [audacious_dep],
    name_prefix: '',
    include_directories: oss_inc,
    install: true,
    install_dir: output_plugin_dir
  )
endif
