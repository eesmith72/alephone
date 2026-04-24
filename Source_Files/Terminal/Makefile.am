## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = libterminal.a


libterminal_a_SOURCES =                                                      \
  computer_interface.h                                                       \
  computer_terminal.hpp                                                      \
  terminal_parser_m1.cpp                                                       \
  terminal_parser_m1.hpp                                                       \
  PlayerTerminalState.cpp                                                    \
  PlayerTerminalState.hpp                                                    \
  terminal_actions.cpp                                                       \
  terminal_actions.hpp                                                       \
  terminal_renderer.cpp                                                      \
  terminal_renderer.hpp                                                      \
  terminal_support.cpp                                                       \
  terminal_support.hpp                                                       \
  ComputerTerminal.cpp                                                       \
  ComputerTerminal.hpp


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
  -I$(top_srcdir)/Source_Files/TCPMess                                       \
                                                                             \
  -I$(top_srcdir)/Source_Files/Videos                                        \
  -I$(top_srcdir)/Source_Files/XML                                           \
  -I$(top_srcdir)/Source_Files
