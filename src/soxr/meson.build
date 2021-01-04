soxr_dep = dependency('soxr', required: false)
have_soxr = soxr_dep.found()


if have_soxr
  shared_module('sox-resampler',
    'sox-resampler.cc',
    include_directories: [src_inc],
    dependencies: [audacious_dep, soxr_dep],
    name_prefix: '',
    install: true,
    install_dir: effect_plugin_dir
  )
endif
