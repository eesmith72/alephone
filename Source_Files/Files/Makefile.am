## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = libfiles.a


if BUILD_ZZIP_RWOPS
ZZIP_SRCS = SDL_rwops_zzip.c
else
ZZIP_SRCS = 
endif

libfiles_a_SOURCES =                                                         \
  $(ZZIP_SRCS)                                                               \
  AStream.cpp                                                                \
  AStream.h                                                                  \
  crc.cpp                                                                    \
  crc.h                                                                      \
  extensions.h                                                               \
  FileHandler.cpp                                                            \
  FileHandler.h                                                              \
  find_files_sdl.cpp                                                         \
  find_files.h                                                               \
  game_wad.cpp                                                               \
  game_wad.h                                                                 \
  import_definitions.cpp                                                     \
  Packing.cpp                                                                \
  Packing.h                                                                  \
  preprocess_map_sdl.cpp                                                     \
  preprocess_map_shared.cpp                                                  \
  resource_manager.cpp                                                       \
  resource_manager.h                                                         \
  SDL_rwops_ostream.cpp                                                      \
  SDL_rwops_ostream.h                                                        \
  SDL_rwops_zzip.h                                                           \
  tags.h                                                                     \
  wad_prefs.cpp                                                              \
  wad_prefs.h                                                                \
  wad_sdl.cpp                                                                \
  wad.cpp                                                                    \
  wad.h                                                                      \
  WadImageCache.cpp                                                          \
  WadImageCache.h


EXTRA_libfiles_a_SOURCES = SDL_rwops_zzip.c


AM_CPPFLAGS =                                                                \
  -I$(top_srcdir)/Source_Files/CSeries                                       \
                                                                             \
  -I$(top_srcdir)/Source_Files/Gameworld                                     \
  -I$(top_srcdir)/Source_Files/Input                                         \
  -I$(top_srcdir)/Source_Files/Lua                                           \
  -I$(top_srcdir)/Source_Files/Misc                                          \
  -I$(top_srcdir)/Source_Files/ModelView                                     \
  -I$(top_srcdir)/Source_Files/Network                                       \
  -I$(top_srcdir)/Source_Files/Network/Metaserver                            \
  -I$(top_srcdir)/Source_Files/RenderMain                                    \
  -I$(top_srcdir)/Source_Files/RenderOther                                   \
  -I$(top_srcdir)/Source_Files/Sound                                         \
  -I$(top_srcdir)/Source_Files/TCPMess                                       \
  -I$(top_srcdir)/Source_Files/Terminal                                      \
  -I$(top_srcdir)/Source_Files/Videos                                        \
  -I$(top_srcdir)/Source_Files/XML                                           \
  -I$(top_srcdir)/Source_Files
