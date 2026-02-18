## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = libcseries.a


libcseries_a_SOURCES =                                                       \
  BStream.cpp                                                                \
  BStream.h                                                                  \
  byte_swapping.cpp                                                          \
  byte_swapping.h                                                            \
  csalerts_sdl.cpp                                                           \
  csalerts.h                                                                 \
  cscluts_sdl.cpp                                                            \
  cscluts.h                                                                  \
  csdialogs_sdl.cpp                                                          \
  csdialogs.h                                                                \
  cseries.h                                                                  \
  csfonts.h                                                                  \
  csmacros.h                                                                 \
  csmisc_sdl.cpp                                                             \
  csmisc.h                                                                   \
  cspaths_sdl.cpp                                                            \
  cspaths.h                                                                  \
  cspixels.h                                                                 \
  csstrings.cpp                                                              \
  csstrings.h                                                                \
  cstypes.h                                                                  \
  FilmProfile.cpp                                                            \
  FilmProfile.h                                                              \
  mytm.h                                                                     \
  mytm_sdl.cpp


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
