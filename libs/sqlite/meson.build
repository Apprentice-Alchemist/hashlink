sqlite_dep = dependency('sqlite3')

shared_library(
	'sqlite',
	['sqlite.c'],
	name_prefix: '',
	name_suffix: 'hdll',
	dependencies: [libhl_dep, sqlite_dep],
	install: true
)