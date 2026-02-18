## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = libmisc.a


if MAKE_WINDOWS
THREAD_PRIORITY = thread_priority_sdl_win32.cpp
else
THREAD_PRIORITY = thread_priority_sdl_posix.cpp
endif

if STEAM
STEAMSHIM_CHILD = steamshim_child.cpp 
else
STEAMSHIM_CHILD =
endif

libmisc_a_SOURCES =                                                          \
  $(STEAMSHIM_CHILD)                                                         \
  $(THREAD_PRIORITY)                                                         \
  achievements.cpp                                                           \
  achievements.h                                                             \
  ActionQueues.cpp                                                           \
  ActionQueues.h                                                             \
  AlephSansMono-Bold.h                                                       \
  alephversion.h                                                             \
  binders.h                                                                  \
  CircularByteBuffer.cpp                                                     \
  CircularByteBuffer.h                                                       \
  CircularQueue.h                                                            \
  Console.cpp                                                                \
  Console.h                                                                  \
  CourierPrime.h                                                             \
  CourierPrimeBold.h                                                         \
  CourierPrimeBoldItalic.h                                                   \
  CourierPrimeItalic.h                                                       \
  DefaultStringSets.cpp                                                      \
  DefaultStringSets.h                                                        \
  game_errors.cpp                                                            \
  game_errors.h                                                              \
  interface_menus.h                                                          \
  interface.cpp                                                              \
  interface.h                                                                \
  key_definitions.h                                                          \
  Logging.cpp                                                                \
  Logging.h                                                                  \
  PlayerImage_sdl.cpp                                                        \
  PlayerImage_sdl.h                                                          \
  PlayerName.cpp                                                             \
  PlayerName.h                                                               \
  powered_by_alephone_h.h                                                    \
  powered_by_alephone.h                                                      \
  preference_dialogs.cpp                                                     \
  preference_dialogs.h                                                       \
  preferences_widgets_sdl.cpp                                                \
  preferences_widgets_sdl.h                                                  \
  preferences.cpp                                                            \
  preferences.h                                                              \
  ProFontAO.h                                                                \
  progress.h                                                                 \
  Random.h                                                                   \
  Scenario.cpp                                                               \
  Scenario.h                                                                 \
  ScenarioChooser.cpp                                                        \
  ScenarioChooser.h                                                          \
  sdl_dialogs.cpp                                                            \
  sdl_dialogs.h                                                              \
  sdl_widgets.cpp                                                            \
  sdl_widgets.h                                                              \
  shared_widgets.cpp                                                         \
  shared_widgets.h                                                           \
  Statistics.cpp                                                             \
  Statistics.h                                                               \
  thread_priority_sdl.h                                                      \
  vbl_definitions.h                                                          \
  vbl.cpp                                                                    \
  vbl.h                                                                      \
  VecOps.h                                                                   \
  WindowedNthElementFinder.h


EXTRA_libmisc_a_SOURCES =                                                    \
  alephone.xpm                                                               \
  alephone32.xpm                                                             \
  inf_achievements.lua                                                       \
  m1_achievements.lua                                                        \
  m2_achievements.lua                                                        \
  steamshim_child.cpp                                                        \
  steamshim_child.h                                                          \
  thread_priority_sdl_dummy.cpp                                              \
  thread_priority_sdl_macosx.cpp                                             \
  thread_priority_sdl_posix.cpp                                              \
  thread_priority_sdl_win32.cpp


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
