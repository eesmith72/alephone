## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = libtcpmess.a


libtcpmess_a_SOURCES =                                                       \
  CommunicationsChannel.cpp                                                  \
  CommunicationsChannel.h                                                    \
  Message.cpp                                                                \
  Message.h                                                                  \
  MessageDispatcher.h                                                        \
  MessageHandler.h                                                           \
  MessageInflater.cpp                                                        \
  MessageInflater.h


AM_CPPFLAGS =                                                                \
  -I$(top_srcdir)/Source_Files/CSeries                                       \
  -I$(top_srcdir)/Source_Files/Files                                         \
  -I$(top_srcdir)/Source_Files/Gameworld                                     \
  -I$(top_srcdir)/Source_Files/Input                                         \
  -I$(top_srcdir)/Source_Files/Lua                                           \
  -I$(top_srcdir)/Source_Files/Misc                                          \
  -I$(top_srcdir)/Source_Files/ModelView                                     \
  -I$(top_srcdir)/Source_Files/Network                                       \
  -I$(top_srcdir)/Source_Files/Network/Metaserver                            \
  -I$(top_srcdir)/Source_Files/RenderMain                                    \
  -I$(top_srcdir)/Source_Files/Screen                                   \
  -I$(top_srcdir)/Source_Files/Sound                                         \
                                                                             \
  -I$(top_srcdir)/Source_Files/Terminal                                      \
  -I$(top_srcdir)/Source_Files/Videos                                        \
  -I$(top_srcdir)/Source_Files/XML                                           \
  -I$(top_srcdir)/Source_Files
