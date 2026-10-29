set(_sdl "${PROJECT_SOURCE_DIR}/3rdParty/SDL12")
file(GLOB _sdl_src
  "${_sdl}/src/*.c" "${_sdl}/src/audio/*.c" "${_sdl}/src/audio/dummy/*.c"
  "${_sdl}/src/cdrom/*.c" "${_sdl}/src/cdrom/dummy/*.c"
  "${_sdl}/src/events/*.c" "${_sdl}/src/file/*.c" "${_sdl}/src/cpuinfo/*.c"
  "${_sdl}/src/joystick/*.c" "${_sdl}/src/joystick/dummy/*.c"
  "${_sdl}/src/loadso/dummy/*.c" "${_sdl}/src/stdlib/*.c"
  "${_sdl}/src/thread/*.c" "${_sdl}/src/thread/generic/*.c"
  "${_sdl}/src/timer/*.c" "${_sdl}/src/video/*.c")
list(FILTER _sdl_src EXCLUDE REGEX "SDL_sysmutex.c$")
add_library(SDL_dsi STATIC ${_sdl_src} sdl_dsi.c dsi_video.c dsi_render.c dsi_trace.c dsi_audio.c)
# Only the small hot kernels use ARM code and speed optimization; the engine
# remains size optimized apart from the selected renderer files below.
set_source_files_properties(dsi_video.c dsi_render.c PROPERTIES COMPILE_OPTIONS "-O2;-marm")
target_include_directories(SDL_dsi PUBLIC "${_sdl}/include" PRIVATE "${_sdl}/src" "${_sdl}/src/video" "${_sdl}/src/events")
target_compile_options(SDL_dsi PRIVATE -Wno-implicit-function-declaration -Wno-incompatible-pointer-types)
target_link_libraries(SDL_dsi PUBLIC fat nds9 calico_ds9 m)
