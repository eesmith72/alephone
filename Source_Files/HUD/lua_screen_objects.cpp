


#if defined (_MSC_VER) && !defined (M_PI)
#define _USE_MATH_DEFINES
#endif

#include "lua_hud_objects.h"

#include "lua_objects.h"
#include "lua_templates.h"
#include "lua_hud_script.h"

#include "hud_definitions.hpp"

#include "Screen.hpp"

#include "preferences.hpp"


#include "screen_drawing.h" // screen_rectangle

#include "OGL_Render.h" // modern_renderer_is_active



// TODO: this is not HUD specific so could relocate later


// TODO: bodged in from lua_hud_objects.cpp; obviously public headers would be good... WIP
extern char Lua_SizePref_Name[];
typedef L_Enum<Lua_SizePref_Name> Lua_SizePreference;

extern char Lua_SizePrefs_Name[];
typedef L_EnumContainer<Lua_SizePrefs_Name, Lua_SizePreference> Lua_SizePreferences;



//-----------------------------------------------------------------------------
// code generation would be best (readable C code) but C macros will do at a push

#define RECT_FIELD(RECTNAME, FIELDNAME) \
static int Lua_Screen_Get_##RECTNAME##_##FIELDNAME(lua_State *L) { \
    SDL_Rect rect = main_screen.RECTNAME(); \
    lua_pushnumber(L, rect.FIELDNAME); \
    return 1; \
} \
static int Lua_Screen_Set_##RECTNAME##_##FIELDNAME(lua_State *L) \
{ \
    SDL_Rect rect = main_screen.RECTNAME(); \
    rect.FIELDNAME = (int32_t)lua_tointeger(L, 2); \
    main_screen.set_##RECTNAME(rect); \
    return 0; \
}


#define SCREEN_RECT(RECTNAME) \
    RECT_FIELD(RECTNAME, x); \
    RECT_FIELD(RECTNAME, y); \
    RECT_FIELD(RECTNAME, w); \
    RECT_FIELD(RECTNAME, h); \
const luaL_Reg Lua_Screen_Get_##RECTNAME[] = { \
    {"x", Lua_Screen_Get_##RECTNAME##_x}, \
    {"y", Lua_Screen_Get_##RECTNAME##_y}, \
    {"w", Lua_Screen_Get_##RECTNAME##_w}, \
    {"h", Lua_Screen_Get_##RECTNAME##_h}, \
    {"width", Lua_Screen_Get_##RECTNAME##_w}, \
    {"height", Lua_Screen_Get_##RECTNAME##_h}, \
    {0, 0} \
}; \
const luaL_Reg Lua_Screen_Set_##RECTNAME[] = { \
    {"x", Lua_Screen_Set_##RECTNAME##_x}, \
    {"y", Lua_Screen_Set_##RECTNAME##_y}, \
    {"w", Lua_Screen_Set_##RECTNAME##_w}, \
    {"h", Lua_Screen_Set_##RECTNAME##_h}, \
    {"width", Lua_Screen_Set_##RECTNAME##_w}, \
    {"height", Lua_Screen_Set_##RECTNAME##_h}, \
    {0, 0} \
};


//-----------------------------------------------------------------------------
// SDL_Rect-like Lua objects, mutable, wraps main_screen rect getters and setters;


// plugin should call this to set its drawing area before it draws HUD elements

char Lua_Screen_Clip_Rect_Name[] = "clip_rect";
typedef L_Class<Lua_Screen_Clip_Rect_Name> Lua_Screen_Clip_Rect;
SCREEN_RECT(virtual_drawing_rect);


// the 3D worldview

char Lua_Screen_World_Rect_Name[] = "world_rect";
typedef L_Class<Lua_Screen_World_Rect_Name> Lua_Screen_World_Rect;
SCREEN_RECT(virtual_world_rect);


// automap (this is normally same as worldview rect but supported for backwards compatibility)

char Lua_Screen_Map_Rect_Name[] = "map_rect";
typedef L_Class<Lua_Screen_Map_Rect_Name> Lua_Screen_Map_Rect;
SCREEN_RECT(virtual_automap_rect);


// computer terminal

char Lua_Screen_Term_Rect_Name[] = "term_rect";
typedef L_Class<Lua_Screen_Term_Rect_Name> Lua_Screen_Term_Rect;
SCREEN_RECT(virtual_terminal_rect);


//-----------------------------------------------------------------------------
// used by screen_overlay.cpp

char Lua_Screen_Text_Margins_Name[] = "text_margins";
typedef L_Class<Lua_Screen_Text_Margins_Name> Lua_Screen_Text_Margins;


screen_rectangle lua_text_margins;

static int Lua_Screen_Text_Margins_Get_Bottom(lua_State *L)
{
    lua_pushnumber(L, lua_text_margins.bottom);
    return 1;
}

static int Lua_Screen_Text_Margins_Get_Left(lua_State *L)
{
    lua_pushnumber(L, lua_text_margins.left);
    return 1;
}

static int Lua_Screen_Text_Margins_Get_Right(lua_State *L)
{
    lua_pushnumber(L, lua_text_margins.right);
    return 1;
}

static int Lua_Screen_Text_Margins_Get_Top(lua_State *L)
{
    lua_pushnumber(L, lua_text_margins.top);
    return 1;
}

static int Lua_Screen_Text_Margins_Set_Bottom(lua_State *L)
{
    lua_text_margins.bottom = lua_tointeger(L, 2);
    return 0;
}

static int Lua_Screen_Text_Margins_Set_Left(lua_State *L)
{
    lua_text_margins.left = lua_tointeger(L, 2);
    return 0;
}

static int Lua_Screen_Text_Margins_Set_Right(lua_State *L)
{
    lua_text_margins.right = lua_tointeger(L, 2);
    return 0;
}

static int Lua_Screen_Text_Margins_Set_Top(lua_State *L)
{
    lua_text_margins.top = lua_tointeger(L, 2);
    return 0;
}

const luaL_Reg Lua_Screen_Text_Margins_Get[] = {
    {"bottom", Lua_Screen_Text_Margins_Get_Bottom},
    {"left", Lua_Screen_Text_Margins_Get_Left},
    {"right", Lua_Screen_Text_Margins_Get_Right},
    {"top", Lua_Screen_Text_Margins_Get_Top},
    {0, 0}
};

const luaL_Reg Lua_Screen_Text_Margins_Set[] = {
    {"bottom", Lua_Screen_Text_Margins_Set_Bottom},
    {"left", Lua_Screen_Text_Margins_Set_Left},
    {"right", Lua_Screen_Text_Margins_Set_Right},
    {"top", Lua_Screen_Text_Margins_Set_Top},
    {0, 0}
};


//-----------------------------------------------------------------------------
// field of view


char Lua_Screen_FOV_Name[] = "field_of_view";
typedef L_Class<Lua_Screen_FOV_Name> Lua_Screen_FOV;

static int Lua_Screen_FOV_Get_Horizontal(lua_State *L)
{
    float factor = modern_renderer_is_active() ? 1.3f : 1.0f;
    lua_pushnumber(L, main_camera_settings.half_cone * 360.f / NUMBER_OF_ANGLES * 2.0f / factor);
    return 1;
}

static int Lua_Screen_FOV_Get_Vertical(lua_State *L)
{
    float factor = modern_renderer_is_active() ? 1.3f : 1.0f;
    lua_pushnumber(L, main_camera_settings.half_vertical_cone * 360.f / NUMBER_OF_ANGLES * 2.0f / factor);
    return 1;
}

static int Lua_Screen_FOV_Get_Fix(lua_State *L)
{
    lua_pushboolean(L, graphics_preferences.horizontal_fov_is_constant);
    return 1;
}

const luaL_Reg Lua_Screen_FOV_Get[] = {
    {"horizontal", Lua_Screen_FOV_Get_Horizontal},
    {"vertical", Lua_Screen_FOV_Get_Vertical},
    {"horizontal_fov_is_constant", Lua_Screen_FOV_Get_Fix},
    {0, 0}
};

const luaL_Reg Lua_Screen_FOV_Set[] = {
    {0, 0}
};


//-----------------------------------------------------------------------------
// crosshairs; TODO: what else?


char Lua_Screen_Crosshairs_Name[] = "crosshairs";
typedef L_Class<Lua_Screen_Crosshairs_Name> Lua_Screen_Crosshairs;

static int Lua_Screen_Crosshairs_Get_Active(lua_State *L)
{
    lua_pushboolean(L, crosshairs_is_visible());
    return 1;
}

static int Lua_Screen_Crosshairs_Get_LuaHUD(lua_State *L)
{
    lua_pushboolean(L, graphics_preferences.crosshairs_is_visible);
    return 1;
}

static int Lua_Screen_Crosshairs_Set_LuaHUD(lua_State *L)
{
    graphics_preferences.crosshairs_is_visible = lua_toboolean(L, 2);
    return 0;
}

const luaL_Reg Lua_Screen_Crosshairs_Get[] = {
    {"active", Lua_Screen_Crosshairs_Get_Active},
    {"lua_hud", Lua_Screen_Crosshairs_Get_LuaHUD},
    {0, 0}
};

const luaL_Reg Lua_Screen_Crosshairs_Set[] = {
    {"lua_hud", Lua_Screen_Crosshairs_Set_LuaHUD},
    {0, 0}
};


//-----------------------------------------------------------------------------
// Screen information



char Lua_Screen_Name[] = "Screen";
typedef L_Class<Lua_Screen_Name> Lua_Screen;

static int Lua_Screen_Get_Width(lua_State *L)
{
    lua_pushnumber(L, main_screen.virtual_screen_rect().w);
    return 1;
}

static int Lua_Screen_Get_Height(lua_State *L)
{
    lua_pushnumber(L, main_screen.virtual_screen_rect().h);
    return 1;
}

static int Lua_Screen_Get_Renderer(lua_State *L)
{
    Lua_RendererType::Push(L, modern_renderer_is_active());
    return 1;
}


// the rect objects defined above

static int Lua_Screen_Get_Clip_Rect(lua_State *L)
{
    Lua_Screen_Clip_Rect::Push(L, Lua_Screen::Index(L, 1));
    return 1;
}

static int Lua_Screen_Get_World_Rect(lua_State *L)
{
    Lua_Screen_World_Rect::Push(L, Lua_Screen::Index(L, 1));
    return 1;
}

static int Lua_Screen_Get_Map_Rect(lua_State *L)
{
    Lua_Screen_Map_Rect::Push(L, Lua_Screen::Index(L, 1));
    return 1;
}

static int Lua_Screen_Get_Term_Rect(lua_State *L)
{
    Lua_Screen_Term_Rect::Push(L, Lua_Screen::Index(L, 1));
    return 1;
}

static int Lua_Screen_Get_Text_Margins(lua_State* L)
{
    Lua_Screen_Text_Margins::Push(L, Lua_Screen::Index(L, 1));
    return 1;
}



/*
 static int Lua_Screen_Get_Masking_Mode(lua_State *L)
 {
 Lua_MaskingMode::Push(L, (int32_t)Lua_HUDInstance()->canvas->masking_mode());
 return 1;
 }
 
 static int Lua_Screen_Set_Masking_Mode(lua_State *L)
 {
 Lua_HUDInstance()->canvas->set_masking_mode((Canvas::mask_mode)Lua_MaskingMode::ToIndex(L, 2));
 return 0;
 }
 
 int Lua_Screen_Clear_Mask(lua_State *L)
 {
 Lua_HUDInstance()->canvas->clear_clip();
 return 0;
 }
 
 */



//-----------------------------------------------------------------------------
// computer terminal state; TODO: terminal should get its own object, in preparation for making it scriptable

static int Lua_Screen_Get_Term_Active(lua_State *L)
{
    lua_pushboolean(L, computer_terminal_is_visible());
    return 1;
}

static int Lua_Screen_Get_Term_Size(lua_State *L)
{
    Lua_SizePreference::Push(L, graphics_preferences.terminal_size);
    return 1;
}


// HUD size 0-3 (off, small, medium, large) size

static int Lua_Screen_Get_HUD_Size(lua_State *L)
{
    Lua_SizePreference::Push(L, graphics_preferences.hud_size);
    return 1;
}


// is player automap visible, is it translucent (modern)?

static int Lua_Screen_Get_Map_Active(lua_State *L)
{
    lua_pushboolean(L, automap_is_visible());
    return 1;
}

static int Lua_Screen_Get_Map_Overlay

(lua_State *L)
{
    lua_pushboolean(L, automap_is_translucent());
    return 1;
}


//-----------------------------------------------------------------------------


static int Lua_Screen_Get_FOV(lua_State *L) // not HUD-specific
{
    Lua_Screen_FOV::Push(L, Lua_Screen::Index(L, 1));
    return 1;
}


//-----------------------------------------------------------------------------
// TODO: might use Lua hotkey binding

static int Lua_Screen_Get_Crosshairs(lua_State *L)
{
    Lua_Screen_Crosshairs::Push(L, Lua_Screen::Index(L, 1));
    return 1;
}


//-----------------------------------------------------------------------------
// just to be awkward, here's 2 drawing methods that need to be gotten rid of/moved to general object

// TODO: badly named: these draw a filled/outlined rect
int Lua_Screen_Fill_Rect(lua_State *L)
{
    /*
     int32_t x = (lua_tonumber(L, 1));
     int32_t y = (lua_tonumber(L, 2));
     int32_t w = (lua_tonumber(L, 3));
     int32_t h = (lua_tonumber(L, 4));
     float r = Lua_HUDColor_Get_R(L, 5);
     float g = Lua_HUDColor_Get_G(L, 5);
     float b = Lua_HUDColor_Get_B(L, 5);
     float a = Lua_HUDColor_Get_A(L, 5);
     
     SDL_Rect rect = {x, y, w, h};
     SDL_Color color = to_sdl_color(r, g, b, a);
     
     Lua_HUDInstance()->canvas->draw_filled_rect(rect, color);
     */
    return 0;
}

int Lua_Screen_Frame_Rect(lua_State *L)
{
    /*
     int32_t x = lua_tonumber(L, 1);
     int32_t y = lua_tonumber(L, 2);
     int32_t w = lua_tonumber(L, 3);
     int32_t h = lua_tonumber(L, 4);
     SDL_Color color = Lua_Get_HUDColor(L, 5);
     int32_t thickness = lua_tonumber(L, 6); // TODO: was float but assuming it's 1:1
     
     Lua_HUDInstance()->canvas->draw_outlined_rect({x, y, w, h}, color, thickness);
     */
    return 0;
}

const luaL_Reg Lua_Screen_Get[] = {
    {"width", Lua_Screen_Get_Width},
    {"height", Lua_Screen_Get_Height},
    {"renderer", Lua_Screen_Get_Renderer},
    {"clip_rect", Lua_Screen_Get_Clip_Rect},
    {"world_rect", Lua_Screen_Get_World_Rect},
    {"map_rect", Lua_Screen_Get_Map_Rect},
    {"term_rect", Lua_Screen_Get_Term_Rect},
    {"text_margins", Lua_Screen_Get_Text_Margins},
    {"map_active", Lua_Screen_Get_Map_Active},
    {"map_overlay_active", Lua_Screen_Get_Map_Overlay},
    {"term_active", Lua_Screen_Get_Term_Active},
    {"hud_size_preference", Lua_Screen_Get_HUD_Size},
    {"term_size_preference", Lua_Screen_Get_Term_Size},
    {"field_of_view", Lua_Screen_Get_FOV},
    {"crosshairs", Lua_Screen_Get_Crosshairs},
    //{"masking_mode", Lua_Screen_Get_Masking_Mode},
    //{"clear_mask", L_TableFunction<Lua_Screen_Clear_Mask>},
    {"fill_rect", L_TableFunction<Lua_Screen_Fill_Rect>},
    {"frame_rect", L_TableFunction<Lua_Screen_Frame_Rect>},
    {0, 0}
};

const luaL_Reg Lua_Screen_Set[] = {
    //{"masking_mode", Lua_Screen_Set_Masking_Mode},
    {0, 0}
};


//-----------------------------------------------------------------------------



void Lua_Screen_register(lua_State *L)
{
    Lua_Screen_Clip_Rect::Register(L, Lua_Screen_Get_virtual_drawing_rect, Lua_Screen_Set_virtual_drawing_rect);
    Lua_Screen_World_Rect::Register(L, Lua_Screen_Get_virtual_world_rect, Lua_Screen_Set_virtual_world_rect);
    Lua_Screen_Map_Rect::Register(L, Lua_Screen_Get_virtual_automap_rect, Lua_Screen_Set_virtual_automap_rect);
    Lua_Screen_Term_Rect::Register(L, Lua_Screen_Get_virtual_terminal_rect, Lua_Screen_Set_virtual_terminal_rect);
    Lua_Screen_Text_Margins::Register(L, Lua_Screen_Text_Margins_Get, Lua_Screen_Text_Margins_Set);
    Lua_Screen_FOV::Register(L, Lua_Screen_FOV_Get, Lua_Screen_FOV_Set);
    Lua_Screen_Crosshairs::Register(L, Lua_Screen_Crosshairs_Get, Lua_Screen_Crosshairs_Set);
    
    Lua_Screen::Register(L, Lua_Screen_Get, Lua_Screen_Set);
    Lua_Screen::Push(L, 0);
    lua_setglobal(L, Lua_Screen_Name);
}
