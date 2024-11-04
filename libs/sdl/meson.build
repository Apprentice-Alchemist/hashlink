sdl_dep = dependency('sdl2')
opengl_dep = dependency('opengl')

sdl_deps = [libhl_dep, sdl_dep, opengl_dep]
if host_machine.system() == 'windows'
	sdl_deps += [glext_dep]
endif

shared_library(
	'sdl',
	['sdl.c', 'gl.c'],
	name_prefix: '',
	name_suffix: 'hdll',
	dependencies: sdl_deps,
	install: true
)
