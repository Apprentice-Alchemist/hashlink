libuv_dep = dependency('libuv')

shared_library(
	'uv',
	['uv.c'],
	name_prefix: '',
	name_suffix: 'hdll',
	dependencies: [libhl_dep, libuv_dep],
	install: true
)