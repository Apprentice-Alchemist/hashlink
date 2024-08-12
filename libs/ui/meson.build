ui_src = []
if host_machine.system() == 'windows'
	ui_src += ['ui_win.c']
else
	ui_src += ['ui_stub.c']
endif

shared_library(
	'ui',
	ui_src,
	name_prefix: '',
	name_suffix: 'hdll',
	dependencies: [libhl_dep],
	install: true
)