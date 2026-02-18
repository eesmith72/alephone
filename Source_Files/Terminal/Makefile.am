## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = libterminal.a

libterminal_a_SOURCES =                                               \
  computer_interface.h                                                \
  computer_terminal.hpp                                               \
  terminal_actions.hpp                                                \
  terminal_renderer.hpp                                               \
  PlayerTerminalState.hpp                                             \
  TerminalText.hpp                                                    \
  M1TerminalParser.hpp                                                \
  terminal_support.hpp                                                \
                                                                      \
  terminal_actions.cpp                                                \
  terminal_renderer.cpp                                               \
  PlayerTerminalState.cpp                                             \
  TerminalText.cpp                                                    \
  M1TerminalParser.cpp                                                \
  terminal_support.cpp                                                \


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
  -I$(top_srcdir)/Source_Files/RenderOther                                   \
  -I$(top_srcdir)/Source_Files/Sound                                         \
  -I$(top_srcdir)/Source_Files/TCPMess                                       \
  -I$(top_srcdir)/Source_Files/Terminal                                      \
  -I$(top_srcdir)/Source_Files/Videos                                        \
  -I$(top_srcdir)/Source_Files/XML                                           \
  -I$(top_srcdir)/Source_Files
