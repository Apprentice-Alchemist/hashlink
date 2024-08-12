png_dep = dependency('libpng')
jpg_dep = dependency('libturbojpeg', default_options: ['c_std=gnu11'])
zlib_dep = dependency('zlib')
vorbisfile_dep = dependency('vorbisfile', default_options: ['werror=false', 'c_std=gnu11'])
ogg_dep = dependency('ogg')

shared_library(
	'fmt',
	['dxt.c', 'fmt.c', 'mikkt.c', 'sha1.c'],
	name_prefix: '',
	name_suffix: 'hdll',
	dependencies: [libhl_dep, png_dep, jpg_dep, minimp3_dep, mikktspace_dep, zlib_dep, vorbisfile_dep, ogg_dep],
	install: true,
)