libs = ['fmt', 'mysql', 'openal', 'sdl', 'sqlite', 'ssl', 'ui', 'uv', 'video']
if host_machine.system() == 'windows'
	libs += ['directx']
endif
foreach lib: libs
	if get_option('with_' + lib)
		subdir(lib)
	endif
endforeach