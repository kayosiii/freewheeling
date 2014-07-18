
include_directories ( /usr/include/libxml2 )
link_directories ()

set( FWEELIN_SOURCES stacktrace.c elastin/elastin.cc fweelin.cc fweelin_datatypes.cc fweelin_rcu.cc fweelin_osc.cc fweelin_event.cc fweelin_config.cc fweelin_paramset.cc fweelin_browser.cc fweelin_audioio.cc fweelin_sdlio.cc fweelin_midiio.cc fweelin_amixer.cc fweelin_videoio.cc fweelin_videoio_displays.cc fweelin_core.cc fweelin_mem.cc fweelin_block.cc fweelin_core_dsp.cc fweelin_fluidsynth.cc)

add_definitions (-D_REENTRANT -DPTHREADS -DNDEBUG -DVERSION="0.6.1" -DFWEELIN_DATADIR="${CMAKE_INSTALL_PREFIX}/share/fweelin")
set ( CMAKE_CXX_FLAGS_RELEASE "-g -Wall -Wno-write-strings -Wno-non-virtual-dtor -funroll-loops -finline-functions -fomit-frame-pointer -ffast-math -fexpensive-optimizations -fstrict-aliasing -falign-loops=2 -falign-jumps=2 -falign-functions=2 -O9")

add_executable ( freewheeling ${FWEELIN_SOURCES})
target_link_libraries ( freewheeling  gnutls-openssl lo pthread jack xml2 sndfile vorbisenc vorbisfile ogg vorbis SDL asound X11 SDL_gfx SDL_ttf)

install (TARGETS freewheeling DESTINATION bin)