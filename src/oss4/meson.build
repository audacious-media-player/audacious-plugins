# XXX - Auto-detect this path from /etc/oss.conf
oss_inc = '/usr/lib/oss/include'
oss_inc_dirs = []


oss_sources = [
  'oss.cc',
  'plugin.cc',
  'utils.cc'
]


if cxx.has_header('sys/soundcard.h', args: '-I' + oss_inc)
  have_oss4 = true
  conf.set10('HAVE_SYS_SOUNDCARD_H', true)
elif cxx.has_header('soundcard.h', args: '-I' + oss_inc)
  have_oss4 = true
else
  have_oss4 = false
endif


if have_oss4
  cmd = run_command('test', '-d', oss_inc, check: false)
  if cmd.returncode() == 0
    oss_inc_dirs += oss_inc
  endif

  shared_module('oss4',
    oss_sources,
    dependencies: [audacious_dep],
    name_prefix: '',
    include_directories: oss_inc_dirs,
    install: true,
    install_dir: output_plugin_dir
  )
endif
