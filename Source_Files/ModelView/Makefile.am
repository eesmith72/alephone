## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = libmodelview.a

libmodelview_a_SOURCES =                                                     \
  Dim3_Loader.cpp                                                            \
  Dim3_Loader.h                                                              \
  Model3D.cpp                                                                \
  Model3D.h                                                                  \
  ModelRenderer.cpp                                                          \
  ModelRenderer.h                                                            \
  StudioLoader.cpp                                                           \
  StudioLoader.h                                                             \
  WavefrontLoader.cpp                                                        \
  WavefrontLoader.h


AM_CPPFLAGS =                                                                \
  -I$(top_srcdir)/Source_Files/CSeries                                       \
  -I$(top_srcdir)/Source_Files/Files                                         \
  -I$(top_srcdir)/Source_Files/Gameworld                                     \
  -I$(top_srcdir)/Source_Files/Input                                         \
  -I$(top_srcdir)/Source_Files/Lua                                           \
  -I$(top_srcdir)/Source_Files/Misc                                          \
                                                                             \
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
