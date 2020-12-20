pulse_dep = dependency('libpulse', version: '>= 0.9.5', required: false)
have_pulse = pulse_dep.found()


if have_pulse
  shared_module('pulse_audio',
    'pulse_audio.cc',
    dependencies: [audacious_dep, pulse_dep, glib_dep],
    name_prefix: '',
    install: true,
    install_dir: output_plugin_dir
  )
endif
