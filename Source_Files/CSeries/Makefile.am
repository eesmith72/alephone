## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = libcseries.a


libcseries_a_SOURCES =                                                       \
  \
  BStream.h                                                                  \
  byte_swapping.h                                                            \
  csalerts.hpp                                                                 \
  cscluts.h                                                                  \
  cseries.h                                                                  \
  csfonts.h                                                                  \
  csmacros.h                                                                 \
  csmisc.h                                                                   \
  cspaths.hpp                                                                  \
  cspixels.h                                                                 \
  csstrings.h                                                                \
  cstypes.h                                                                  \
  FilmProfile.h                                                              \
  mytm.h                                                                     \
  string_resources.hpp                                                       \
  string_resources_std.hpp                                               \
  \
  BStream.cpp                                                                \
  byte_swapping.cpp                                                          \
  csalerts_sdl.cpp                                                           \
  cscluts_sdl.cpp                                                            \
  csmisc_sdl.cpp                                                             \
  cspaths_sdl.cpp                                                            \
  csstrings.cpp                                                              \
  FilmProfile.cpp                                                            \
  mytm_sdl.cpp                                                               \
  string_resources.cpp                                                       \
  string_resources_std.cpp


# macOS-only (ObjC++)
EXTRA_libcseries_a_SOURCES =                                                 \
  csalerts.mm                                                                \
  cspaths.mm


# EES: everything links to everything else...
AM_CPPFLAGS =                                                                \
                                                                             \
  -I$(top_srcdir)/Source_Files/Files                                         \
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


confpaths.h: Makefile
	echo "#define PKGDATADIR \"$(pkgdatadir)\"" > $@

DISTCLEANFILES = confpaths.h

cspaths_sdl.o: confpaths.h
