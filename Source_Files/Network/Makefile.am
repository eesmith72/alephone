## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = libnetwork.a

SUBDIRS = Metaserver StandaloneHub


libnetwork_a_SOURCES =                                                       \
  ConnectPool.cpp                                                            \
  ConnectPool.h                                                              \
  HTTP.cpp                                                                   \
  HTTP.h                                                                     \
  network_capabilities.cpp                                                   \
  network_capabilities.h                                                     \
  network_dialog_widgets_sdl.cpp                                             \
  network_dialog_widgets_sdl.h                                               \
  network_dialogs.cpp                                                        \
  network_dialogs.h                                                          \
  network_games.cpp                                                          \
  network_games.h                                                            \
  network_messages.cpp                                                       \
  network_messages.h                                                         \
  network_private.h                                                          \
  network_star_hub.cpp                                                       \
  network_star_spoke.cpp                                                     \
  network_star.h                                                             \
  network_udp.cpp                                                            \
  network.cpp                                                                \
  network.h                                                                  \
  NetworkGameProtocol.h                                                      \
  NetworkInterface.cpp                                                       \
  NetworkInterface.h                                                         \
  Pinger.cpp                                                                 \
  Pinger.h                                                                   \
  PortForward.cpp                                                            \
  PortForward.h                                                              \
  SSLP_API.h                                                                 \
  SSLP_limited.cpp                                                           \
  SSLP_Protocol.h                                                            \
  StarGameProtocol.cpp                                                       \
  StarGameProtocol.h                                                         \
  Update.cpp                                                                 \
  Update.h


EXTRA_libnetwork_a_SOURCES = network_dummy.cpp


AM_CPPFLAGS =                                                                \
  -I$(top_srcdir)/Source_Files/CSeries                                       \
  -I$(top_srcdir)/Source_Files/Files                                         \
  -I$(top_srcdir)/Source_Files/Gameworld                                     \
  -I$(top_srcdir)/Source_Files/Input                                         \
  -I$(top_srcdir)/Source_Files/Lua                                           \
  -I$(top_srcdir)/Source_Files/Misc                                          \
  -I$(top_srcdir)/Source_Files/ModelView                                     \
                                                                             \
  -I$(top_srcdir)/Source_Files/Network/Metaserver                            \
  -I$(top_srcdir)/Source_Files/RenderMain                                    \
  -I$(top_srcdir)/Source_Files/Screen                                   \
  -I$(top_srcdir)/Source_Files/Sound                                         \
  -I$(top_srcdir)/Source_Files/TCPMess                                       \
  -I$(top_srcdir)/Source_Files/Terminal                                      \
  -I$(top_srcdir)/Source_Files/Videos                                        \
  -I$(top_srcdir)/Source_Files/XML                                           \
  -I$(top_srcdir)/Source_Files
