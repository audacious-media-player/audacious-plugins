oss_sources = [
  'oss.cc',
  'plugin.cc',
  'utils.cc'
]


cmd = run_command('awk', '-F=', '/^OSSLIBDIR/{print $2}', '/etc/oss.conf', check: false)
oss_lib_dir = cmd.returncode() == 0 ? cmd.stdout().strip() : '/usr/lib/oss'
oss_inc_dir = oss_lib_dir / 'include'
oss_inc_dirs = []

cmd = run_command('test', '-d', oss_inc_dir, check: false)
if cmd.returncode() == 0
  oss_inc_dirs += oss_inc_dir
endif


have_oss4 = cxx.has_header('sys/soundcard.h', args: '-I' + oss_inc_dir)


if have_oss4
  shared_module('oss4',
    oss_sources,
    dependencies: [audacious_dep],
    name_prefix: '',
    include_directories: oss_inc_dirs,
    install: true,
    install_dir: output_plugin_dir
  )
endif
