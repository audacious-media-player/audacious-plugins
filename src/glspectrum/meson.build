opengl_dep = dependency('GL', required: false)
epoxy_dep = dependency('epoxy', required: false)
glm_dep = dependency('glm', required: false)

if epoxy_dep.found() and glm_dep.found()
  conf.set10('USE_MODERNGL', true)
endif

if conf.has('USE_GTK3') and conf.has('USE_MODERNGL')
  conf.set10('USE_GTKGLAREA', true)
endif

have_glspectrum = opengl_dep.found() and (
    (have_windows or x11_dep.found()) or
    conf.has('USE_GTKGLAREA')
)

if have_glspectrum
  shared_module('gl-spectrum',
    'gl-spectrum.cc', 'modern_renderer.cc',
    dependencies: [audacious_dep, math_dep, gtk_dep, opengl_dep, x11_dep,
                   epoxy_dep, glm_dep],
    name_prefix: '',
    install: true,
    install_dir: visualization_plugin_dir
  )
endif
