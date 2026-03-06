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
  DataFile.cpp                                                            \
  DataFile.hpp                                                              \
  find_files.cpp                                                         \
  find_files.hpp                                                               \
  map_wad.cpp                                                               \
  map_wad.h                                                                 \
  Packing.cpp                                                                \
  Packing.h                                                                  \
  physics_wad.h                                                               \
  physics_wad.cpp                                                     \
  resource_manager.cpp                                                       \
  resource_manager.h                                                         \
  SDL_rwops_ostream.cpp                                                      \
  SDL_rwops_ostream.h                                                        \
  SDL_rwops_zzip.h                                                           \
  tags.h                                                                     \
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
