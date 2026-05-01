## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = liba1lua.a

liba1lua_a_SOURCES = \
  lua_script.h \
  lua_script.cpp \
  lua_map.h \
  lua_map.cpp \
  lua_mnemonics.h \
  lua_monsters.h \
  lua_monsters.cpp \
  lua_objects.h \
  lua_objects.cpp \
  lua_player.h \
  lua_player.cpp \
  lua_music.h \
  lua_music.cpp \
  lua_projectiles.h \
  lua_projectiles.cpp \
  lua_saved_objects.h \
  lua_saved_objects.cpp \
  lua_templates.h \
  language_definition.h \
  lua_serialize.h \
  lua_serialize.cpp \
  lua_hud_objects.h \
  lua_hud_objects.cpp \
  lua_hud_script.h \
  lua_hud_script.cpp \
  lua_ephemera.h \
  lua_ephemera.cpp

EXTRA_DIST = COPYRIGHT README


AM_CPPFLAGS =                                                                \
  -I$(top_srcdir)/Source_Files/CSeries                                       \
  -I$(top_srcdir)/Source_Files/Files                                         \
  -I$(top_srcdir)/Source_Files/Gameworld                                     \
  -I$(top_srcdir)/Source_Files/Input                                         \
                                                                             \
  -I$(top_srcdir)/Source_Files/Misc                                          \
  -I$(top_srcdir)/Source_Files/ModelView                                     \
  -I$(top_srcdir)/Source_Files/Network                                       \
  -I$(top_srcdir)/Source_Files/Network/Metaserver                            \
  -I$(top_srcdir)/Source_Files/RenderMain                                    \
  -I$(top_srcdir)/Source_Files/Screen                                   \
  -I$(top_srcdir)/Source_Files/Sound                                         \
  -I$(top_srcdir)/Source_Files/TCPMess                                       \
  -I$(top_srcdir)/Source_Files/Terminal                                      \
  -I$(top_srcdir)/Source_Files/Videos                                        \
  -I$(top_srcdir)/Source_Files/XML                                           \
  -I$(top_srcdir)/Source_Files
