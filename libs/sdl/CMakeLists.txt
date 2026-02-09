add_library(sdl.hdll
    sdl.c
    gl.c
)

if ((NOT WIN32) AND DOWNLOAD_DEPENDENCIES)
    FetchContent_Declare(
        SDL2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.8/SDL2-2.32.8.tar.gz
        URL_HASH SHA256=0ca83e9c9b31e18288c7ec811108e58bac1f1bb5ec6577ad386830eac51c787e
        EXCLUDE_FROM_ALL
    )
    set(SDL_TEST OFF CACHE INTERNAL "")
    if (ANDROID)
        set(SDL_SHARED ON CACHE INTERNAL "")
        set(SDL_STATIC OFF CACHE INTERNAL "")
    else()
        set(SDL_STATIC ON CACHE INTERNAL "")
        set(SDL_SHARED OFF CACHE INTERNAL "")
        if (BUILD_SHARED_LIBS)
            set(SDL_STATIC_PIC ON CACHE INTERNAL "")
        endif()
    endif()
    FetchContent_MakeAvailable(SDL2)
else()
    if(WIN32)
        if (EXISTS ${INCLUDES_BASE_DIR}/sdl/)
            set(sdl2_SOURCE_DIR ${INCLUDES_BASE_DIR}/sdl)
        else()
            if (MINGW)
                FetchContent_Declare(
                    sdl2
                    URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.zip
                    URL_HASH SHA256=f15cff5fca62ec9381a016ef1d42a95c638cd72d2f226ba5781c76fe43dbd1ac
                )
            else()
                FetchContent_Declare(
                    sdl2
                    URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-VC.zip
                    URL_HASH SHA256=af347939395a58b365846aaea27391e69f9ec9d4dd650d6ac40802159b418a6e
                )
            endif()
            FetchContent_MakeAvailable(sdl2)
        endif()
        set(SDL2_DIR ${sdl2_SOURCE_DIR}/cmake)
        if(NOT MINGW)
            target_include_directories(sdl.hdll
                PRIVATE
                ${INCLUDES_BASE_DIR}/gl
            )
        endif()
    endif()

    find_package(SDL2 REQUIRED)
    if(WIN32)
        install(IMPORTED_RUNTIME_ARTIFACTS SDL2::SDL2)
    endif()
endif()

set_as_hdll(sdl)
target_link_libraries(sdl.hdll
    libhl
    SDL2::SDL2
)
if(APPLE)
    target_compile_definitions(sdl.hdll PRIVATE GL_SILENCE_DEPRECATION)
endif()

if(WIN32)
    target_link_libraries(sdl.hdll
        winmm
        opengl32
    )
endif()

if(ANDROID)
    target_link_libraries(sdl.hdll GLESv3)
endif()

if((APPLE OR UNIX) AND NOT ANDROID)
    find_package(OpenGL REQUIRED)
    target_include_directories(sdl.hdll
        PRIVATE
        ${OPENGL_INCLUDE_DIR}
    )
    target_link_libraries(sdl.hdll
        libhl
        ${OPENGL_gl_LIBRARY}
    )
endif()


install(
    TARGETS sdl.hdll
    DESTINATION ${HDLL_DESTINATION}
)
