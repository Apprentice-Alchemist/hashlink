openal_dep = dependency('openal')

shared_library(
	'openal',
	['openal.c'],
	name_prefix: '',
	name_suffix: 'hdll',
	dependencies: [libhl_dep, openal_dep],
	install: true
)