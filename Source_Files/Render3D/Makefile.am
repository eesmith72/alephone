## Process this file with automake to produce Makefile.in 

noinst_LIBRARIES = librendermain.a

if MAKE_OPENGL
OPENGL_SOURCES = Rasterizer_Shader.cpp RenderRasterize_Shader.cpp
else
OPENGL_SOURCES = 
endif

librendermain_a_SOURCES =                                                    \
  $(OPENGL_SOURCES)                                                          \
  AnimatedTextures.cpp                                                       \
  AnimatedTextures.h                                                         \
  collection_definition.h                                                    \
  DDS.h                                                                      \
  ImageLoader_SDL.cpp                                                        \
  ImageLoader_Shared.cpp                                                     \
  ImageLoader.h                                                              \
  low_level_textures.h                                                       \
  OGL_Faders.cpp                                                             \
  OGL_Faders.h                                                               \
  OGL_FBO.cpp                                                                \
  OGL_FBO.h                                                                  \
  OGL_Headers.h                                                              \
  OGL_Model_Def.cpp                                                          \
  OGL_Model_Def.h                                                            \
  OGL_Render.cpp                                                             \
  OGL_Render.h                                                               \
  OGL_Setup.cpp                                                              \
  OGL_Setup.h                                                                \
  OGL_Shader.cpp                                                             \
  OGL_Shader.h                                                               \
  OGL_Subst_Texture_Def.cpp                                                  \
  OGL_Subst_Texture_Def.h                                                    \
  OGL_Texture_Def.h                                                          \
  OGL_Textures.cpp                                                           \
  OGL_Textures.h                                                             \
  Rasterizer_Shader.h                                                        \
  Rasterizer_SW.h                                                            \
  Rasterizer.h                                                               \
  render.cpp                                                                 \
  render.h                                                                   \
  RenderPlaceObjs.cpp                                                        \
  RenderPlaceObjs.h                                                          \
  RenderRasterize_Shader.h                                                   \
  RenderRasterize.cpp                                                        \
  RenderRasterize.h                                                          \
  RenderSortPoly.cpp                                                         \
  RenderSortPoly.h                                                           \
  RenderVisTree.cpp                                                          \
  RenderVisTree.h                                                            \
  scottish_textures.cpp                                                      \
  scottish_textures.h                                                        \
  shapes.h                                                        \
  shapes.cpp                                                                 \
  textures.cpp                                                               \
  textures.h                                                                 \
  vec3.h                                                                     \
                                                                             \
  Shaders/bump_bloom.frag                                                    \
  Shaders/bump.frag                                                          \
  Shaders/invincible_bloom.frag                                              \
  Shaders/invincible.frag                                                    \
  Shaders/invisible_bloom.frag                                               \
  Shaders/invisible.frag                                                     \
  Shaders/landscape_bloom.frag                                               \
  Shaders/landscape_infravision.frag                                         \
  Shaders/landscape_sphere_bloom.frag                                        \
  Shaders/landscape_sphere_infravision.frag                                  \
  Shaders/landscape_sphere.frag                                              \
  Shaders/landscape_sphere.vert                                              \
  Shaders/landscape.frag                                                     \
  Shaders/landscape.vert                                                     \
  Shaders/sprite_bloom.frag                                                  \
  Shaders/sprite_infravision.frag                                            \
  Shaders/sprite.frag                                                        \
  Shaders/sprite.vert                                                        \
  Shaders/wall_bloom.frag                                                    \
  Shaders/wall_infravision.frag                                              \
  Shaders/wall.frag                                                          \
  Shaders/wall.vert


EXTRA_librendermain_a_SOURCES =                                              \
  Rasterizer_Shader.cpp                                                      \
  RenderRasterize_Shader.cpp


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
                                                                             \
  -I$(top_srcdir)/Source_Files/Screen                                   \
  -I$(top_srcdir)/Source_Files/Sound                                         \
  -I$(top_srcdir)/Source_Files/TCPMess                                       \
  -I$(top_srcdir)/Source_Files/Terminal                                      \
  -I$(top_srcdir)/Source_Files/Videos                                        \
  -I$(top_srcdir)/Source_Files/XML                                           \
  -I$(top_srcdir)/Source_Files
