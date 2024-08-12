mbedtls_dep = dependency('mbedtls', required: false)
if not mbedtls_dep.found()
	mbedtls_dep = cc.find_library('mbedtls')
endif
mbedcrypto_dep = dependency('mbedcrypto', required: false)
if not mbedcrypto_dep.found()
	mbedtls_dep = cc.find_library('mbedcrypto')
endif
mbedx509_dep = dependency('mbedx509', required: false)
if not mbedx509_dep.found()
	mbedtls_dep = cc.find_library('mbedx509')
endif

shared_library(
	'ssl',
	['ssl.c'],
	name_prefix: '',
	name_suffix: 'hdll',
	dependencies: [libhl_dep, mbedtls_dep, mbedcrypto_dep, mbedx509_dep],
	install: true
)