sdl_dep = dependency('sdl2', version: '>= 2.0', required: false)

if not sdl_dep.found()
  sdl_dep = dependency('sdl', version: '>= 1.2.11', required: false)
endif

have_sdlout = sdl_dep.found()


if have_sdlout
  shared_module('sdlout',
    'sdlout.cc',
    dependencies: [audacious_dep, math_dep, sdl_dep],
    name_prefix: '',
    install: true,
    install_dir: output_plugin_dir
  )
endif
