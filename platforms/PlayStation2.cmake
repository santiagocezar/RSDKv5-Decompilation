# PKG-CONFIG IS USED AS THE MAIN DRIVER
# bc cmake is inconsistent as fuuuckkk

find_package(PkgConfig REQUIRED)

add_executable(RetroEngine ${RETRO_FILES})

option(USE_SDL_AUDIO "Whether or not to use SDL for audio instead of the default MiniAudio." ON)

pkg_check_modules(OGG ogg)

if(NOT OGG_FOUND)
    set(COMPILE_OGG TRUE)
    message(NOTICE "libogg not found, attempting to build from source")
else()
    message("found libogg")

    target_link_libraries(RetroEngine ${OGG_STATIC_LIBRARIES})
    target_link_options(RetroEngine PRIVATE ${OGG_STATIC_LDLIBS_OTHER})
    target_compile_options(RetroEngine PRIVATE ${OGG_STATIC_CFLAGS})
endif()

set(COMPILE_THEORA TRUE)

pkg_check_modules(SDL2 sdl2 REQUIRED)
target_link_libraries(RetroEngine ${SDL2_STATIC_LIBRARIES} "SDL2main")
target_link_options(RetroEngine PRIVATE ${SDL2_STATIC_LDLIBS_OTHER})
target_compile_options(RetroEngine PRIVATE ${SDL2_STATIC_CFLAGS})

if(USE_SDL_AUDIO)
    target_link_libraries(RetroEngine ${SDL2_STATIC_LIBRARIES})
    target_link_options(RetroEngine PRIVATE ${SDL2_STATIC_LDLIBS_OTHER})
    target_compile_options(RetroEngine PRIVATE ${SDL2_STATIC_CFLAGS})
    target_compile_definitions(RetroEngine PRIVATE RETRO_AUDIODEVICE_SDL2=1)
endif()

target_link_libraries(RetroEngine "ps2_drivers")
