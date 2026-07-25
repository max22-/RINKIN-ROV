#include <lua.hpp>
#include <raylib.h>
#include <rlImGui.h>

extern float pos_x, pos_y, pos_z;
extern float heading, pitch, roll;
extern Quaternion quaternion;
extern RenderTexture2D model_texture;

static int lua_set_pos(lua_State *L) {
    pos_x = luaL_checknumber(L, 1);
    pos_y = luaL_checknumber(L, 2);
    pos_z = luaL_checknumber(L, 3);
    return 0;
}

static int lua_set_heading(lua_State *L) {
    heading = DEG2RAD * luaL_checknumber(L, 1);
    return 0;
}

static int lua_set_pitch(lua_State *L) {
    pitch = DEG2RAD * luaL_checknumber(L, 1);
    return 0;
}

static int lua_set_roll(lua_State *L) {
    roll = DEG2RAD * luaL_checknumber(L, 1);
    return 0;
}

static int lua_set_quaternion(lua_State *L) {
    const float x = luaL_checknumber(L, 1);
    const float y = luaL_checknumber(L, 2);
    const float z = luaL_checknumber(L, 3);
    const float w = luaL_checknumber(L, 4);
    quaternion.x = x;
    quaternion.y = y;
    quaternion.z = z;
    quaternion.w = w;
    return 0;
}

static int lua_display(lua_State *L) {
    rlImGuiImage((const Texture*)&model_texture.texture);
    return 0;
}

static int lua_draw_vector(lua_State *L) {
    const float pos_x = luaL_checknumber(L, 1);
    const float pos_y = luaL_checknumber(L, 2);
    const float pos_z = luaL_checknumber(L, 3);
    const float vx = luaL_checknumber(L, 4);
    const float vy = luaL_checknumber(L, 5);
    const float vz = luaL_checknumber(L, 6);
    const unsigned char r = luaL_checkinteger(L, 7);
    const unsigned char g = luaL_checkinteger(L, 8);
    const unsigned char b = luaL_checkinteger(L, 9);
    DrawLine3D(
        Vector3{pos_x, pos_y, pos_z}, 
        Vector3{
            pos_x + vx,
            pos_y + vy,
            pos_z + vz,
        },
        Color{r, g, b, 255}
    );
    return 0;
}

static const struct luaL_Reg model_lib[] = {
    {"set_pos", lua_set_pos},
    {"set_heading", lua_set_heading},
    {"set_pitch", lua_set_pitch},
    {"set_roll", lua_set_roll},
    {"set_quaternion", lua_set_quaternion},
    {"display", lua_display},
    {"draw_vector", lua_draw_vector},
    {nullptr, nullptr},
};

int lua_open_model(lua_State *L) {
    luaL_newlib(L, model_lib);
    return 1;
}