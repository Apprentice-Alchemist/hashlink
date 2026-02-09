add_library(openal.hdll openal.c)

option(WITH_STATIC_OPENAL "Link openal.hdll to static openal-soft" OFF)

if (WITH_STATIC_OPENAL)
    target_compile_definitions(openal.hdll PRIVATE OPENAL_STATIC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
endif()

if(WIN32)
    if(MSVC)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
    endif()

    if (EXISTS ${INCLUDES_BASE_DIR}/openal/)
        set(openal_SOURCE_DIR ${INCLUDES_BASE_DIR}/openal)
    else()
        FetchContent_Declare(openal
            URL https://github.com/kcat/openal-soft/releases/download/1.25.1/openal-soft-1.25.1-bin.zip
            URL_HASH SHA256=0915af01928b17e46f7c59ffc44a7413486bbef8424a3465109f489a80cb68a2
        )
        FetchContent_MakeAvailable(openal)
    endif()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        find_library(OPENAL_LIBRARY OpenAL32 PATHS ${openal_SOURCE_DIR}/libs/Win64)
        install(FILES ${openal_SOURCE_DIR}/bin/Win64/soft_oal.dll TYPE BIN RENAME OpenAL32.dll)
    else()
        find_library(OPENAL_LIBRARY OpenAL32 PATHS ${openal_SOURCE_DIR}/libs/Win32)
        install(FILES ${openal_SOURCE_DIR}/bin/Win32/soft_oal.dll TYPE BIN RENAME OpenAL32.dll)
    endif()
    set(OPENAL_INCLUDE_DIR ${openal_SOURCE_DIR}/include)
    add_library(OpenAL::OpenAL SHARED IMPORTED)
    set_target_properties(OpenAL::OpenAL PROPERTIES
        IMPORTED_IMPLIB ${OPENAL_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${OPENAL_INCLUDE_DIR}
    )
elseif(DOWNLOAD_DEPENDENCIES)
	FetchContent_Declare(openal-soft
        URL https://github.com/kcat/openal-soft/archive/refs/tags/1.24.3.tar.gz
        URL_HASH SHA256=7e1fecdeb45e7f78722b776c5cf30bd33934b961d7fd2a11e0494e064cc631ce
        EXCLUDE_FROM_ALL
    )

    set(ALSOFT_EXAMPLES OFF CACHE INTERNAL "")
    set(ALSOFT_UTILS OFF CACHE INTERNAL "")
    if (WITH_STATIC_OPENAL)
        set(LIBTYPE STATIC)
    endif()

    FetchContent_MakeAvailable(openal-soft)
    install(
    TARGETS
        OpenAL
    LIBRARY
    RUNTIME
)
else()
    find_package(OpenAL REQUIRED)
endif()

set_as_hdll(openal)

target_link_libraries(openal.hdll
    libhl
    OpenAL::OpenAL
)

install(
    TARGETS
        openal.hdll
    DESTINATION ${HDLL_DESTINATION}
)
