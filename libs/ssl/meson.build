# Mbedtls 2.x does not ship pkgconfig files so fall back to find_library.
mbedtls_dep = dependency('mbedtls', required: false)
if not mbedtls_dep.found()
	mbedtls_dep = cc.find_library('mbedtls')
endif
mbedcrypto_dep = dependency('mbedcrypto', required: false)
if not mbedcrypto_dep.found()
	mbedcrypto_dep = cc.find_library('mbedcrypto')
endif
mbedx509_dep = dependency('mbedx509', required: false)
if not mbedx509_dep.found()
	mbedx509_dep = cc.find_library('mbedx509')
endif

ssl_deps = [
	libhl_dep,
	mbedtls_dep,
	mbedcrypto_dep,
	mbedx509_dep
]

if host_machine.system() == 'darwin'
	ssl_deps += [dependency('appleframeworks', modules: ['Security', 'CoreFoundation'])]
endif

shared_library(
	'ssl',
	['ssl.c'],
	name_prefix: '',
	name_suffix: 'hdll',
	dependencies: ssl_deps,
	install: true
)