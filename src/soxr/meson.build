soxr_dep = dependency('soxr', required: false)


if soxr_dep.found()
  shared_module('sox-resampler',
    'sox-resampler.cc',
    include_directories: [src_inc],
    dependencies: [audacious_dep, soxr_dep],
    install: true,
    install_dir: effect_plugin_dir
  )
endif
