if host_machine.system() != 'windows'
	subdir_done()
endif

add_languages('cpp')

shared_library(
	'directx',
	['directx.cpp', 'dx12.cpp', 'gamecontroller.c', 'window.c'],
	name_prefix: '',
	name_suffix: 'hdll',
	dependencies: [
		libhl_dep,
		cc.find_library('d3d11.lib'),
		cc.find_library('dxgi.lib'),
		cc.find_library('d3dcompiler.lib'),
		cc.find_library('xinput9_1_0.lib'),
		cc.find_library('dinput8.lib'),
	],
	install: true,
)