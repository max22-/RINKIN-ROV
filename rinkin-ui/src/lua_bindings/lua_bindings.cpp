#include <lua.hpp>
#include "lua_imgui.h"
#include "lua_udp.h"
#include "lua_gamepad.h"
#include "lua_model.h"
#include "lua_plot.h"

void lua_register_bindings(struct lua_State *L) {
    luaL_requiref(L, "ImGui", lua_open_imgui, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "udp", lua_open_udp, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "gamepad", lua_open_gamepad, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "model", lua_open_model, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "plot", lua_open_plot, 1);
    lua_pop(L, 1);
}