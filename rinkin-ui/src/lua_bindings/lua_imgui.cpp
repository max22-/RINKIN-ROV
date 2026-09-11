#include <imgui.h>
#include "lua_imgui.h"

static int lua_imgui_begin(lua_State *L) {
    const char *title = luaL_checkstring(L, 1);
    const int flags = luaL_optinteger(L, 2, 0);
    lua_pushboolean(L, ImGui::Begin(title, nullptr, flags));
    return 1;
}

static int lua_imgui_end(lua_State *L) {
    ImGui::End();
    return 0;
}

static int lua_imgui_begin_child(lua_State *L) {
    const char *str_id = luaL_checkstring(L, 1);
    int vx = luaL_optinteger(L, 2, 0);
    int vy = luaL_optinteger(L, 3, 0);
    int child_flags = luaL_optinteger(L, 4, 0);
    int window_flags = luaL_optinteger(L, 5, 0);
    bool r = ImGui::BeginChild(str_id, ImVec2(vx, vy), child_flags, window_flags);
    lua_pushboolean(L, r);
    return 1;
}

static int lua_imgui_end_child(lua_State *L) {
    ImGui::EndChild();
    return 0;
}

static int lua_imgui_begin_group(lua_State *L) {
    ImGui::BeginGroup();
    return 0;
}

static int lua_imgui_end_group(lua_State *L) {
    ImGui::EndGroup();
    return 0;
}

static int lua_imgui_push_id(lua_State *L) {
    switch(lua_type(L, 1)) {
    case LUA_TNIL:
        luaL_argerror(L, 1, "can't use `nil' with PushID");
        break;
    case LUA_TBOOLEAN: {
        bool b = lua_toboolean(L, 1);
        ImGui::PushID(b);
        break;
    }
    case LUA_TLIGHTUSERDATA: {
        void *p = lua_touserdata(L, 1);
        ImGui::PushID(p);
        break;
    }
    case LUA_TNUMBER: {
        int i = luaL_checkinteger(L, 1);
        ImGui::PushID(i);
        break;
    }
    case LUA_TSTRING: {
        const char *str = lua_tostring(L, 1);
        ImGui::PushID(str);
        break;
    }
    case LUA_TTABLE:
        luaL_argerror(L, 1, "can't use tables with PushID");
        break;
    case LUA_TFUNCTION:
        luaL_argerror(L, 1, "can't use functions with PushID");
        break;
    case LUA_TUSERDATA:
        luaL_argerror(L, 1, "can't use userdata with PushID");
        break;
    case LUA_TTHREAD:
        luaL_argerror(L, 1, "can't use coroutines with PushID");
        break;
    default:
        luaL_argerror(L, 1, "unexpected lua type");
    }
    return 0;
}

static int lua_imgui_pop_id(lua_State *L) {
    ImGui::PopID();
    return 0;
}


static int lua_imgui_set_next_window_pos(lua_State *L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    ImGui::SetNextWindowPos(ImVec2(x, y));
    return 0;
}

static int lua_imgui_set_next_window_size(lua_State *L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    ImGui::SetNextWindowSize(ImVec2(x, y));
    return 0;
}

static int lua_imgui_get_viewport_size(lua_State *L) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    lua_pushinteger(L, viewport->Size.x);
    lua_pushinteger(L, viewport->Size.y);
    return 2;
}

static int lua_imgui_text(lua_State *L) {
    const char *text = luaL_checkstring(L, 1);
    ImGui::Text("%s", text);
    return 0;
}

static int lua_imgui_button(lua_State *L) {
    const char *label = luaL_checkstring(L, 1);
    lua_pushboolean(L, ImGui::Button(label));
    return 1;
}

static int lua_imgui_input_double(lua_State *L) {
    const char *label = luaL_checkstring(L, 1);
    double v = luaL_checknumber(L, 2);
    bool modified = ImGui::InputDouble(label, &v);
    lua_pushnumber(L, v);
    lua_pushboolean(L, modified);
    return 2;
}

static int lua_imgui_slider_int(lua_State *L) {
    const char *label = luaL_checkstring(L, 1);
    int v = luaL_checkinteger(L, 2);
    const int v_min = luaL_checkinteger(L, 3);
    const int v_max = luaL_checkinteger(L, 4);
    bool modified = ImGui::SliderInt(label, &v, v_min, v_max);
    lua_pushinteger(L, v);
    lua_pushboolean(L, modified);
    return 2;
}

static int lua_imgui_slider_float(lua_State *L) {
    const char *label = luaL_checkstring(L, 1);
    float v = luaL_checknumber(L, 2);
    const int v_min = luaL_checkinteger(L, 3);
    const int v_max = luaL_checkinteger(L, 4);
    bool modified = ImGui::SliderFloat(label, &v, v_min, v_max);
    lua_pushnumber(L, v);
    lua_pushboolean(L, modified);
    return 2;
}

static int lua_imgui_checkbox(lua_State *L) {
    const char *label = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    bool checked = lua_toboolean(L, 2);
    bool modified = ImGui::Checkbox(label, &checked);
    lua_pushboolean(L, checked);
    lua_pushboolean(L, modified);
    return 2;
}

static int lua_imgui_sameline(lua_State *L) {
    ImGui::SameLine();
    return 0;
}

static int lua_imgui_separator_text(lua_State *L) {
    const char *label = luaL_checkstring(L, 1);
    ImGui::SeparatorText(label);
    return 0;
}

int lua_open_imgui(lua_State *L) {
    const struct luaL_Reg ImGuiLib[] = {
        {"Begin", lua_imgui_begin},
        {"End", lua_imgui_end},
        {"BeginChild", lua_imgui_begin_child},
        {"EndChild", lua_imgui_end_child},
        {"BeginGroup", lua_imgui_begin_group},
        {"EndGroup", lua_imgui_end_group},
        {"PushID", lua_imgui_push_id},
        {"PopID", lua_imgui_pop_id},
        {"SetNextWindowPos", lua_imgui_set_next_window_pos},
        {"SetNextWindowSize", lua_imgui_set_next_window_size},
        {"GetViewportSize", lua_imgui_get_viewport_size},
        {"Text", lua_imgui_text},
        {"Button", lua_imgui_button},
        {"InputDouble", lua_imgui_input_double},
        {"SliderInt", lua_imgui_slider_int},
        {"SliderFloat", lua_imgui_slider_float},
        {"Checkbox", lua_imgui_checkbox},
        {"SameLine", lua_imgui_sameline},
        {"SeparatorText", lua_imgui_separator_text},
        {nullptr, nullptr},
    };
    luaL_newlib(L, ImGuiLib);
    lua_pushstring(L, "WindowFlags");
    lua_createtable(L, 0, 0);
    lua_pushstring(L, "HorizontalScrollbar");
    lua_pushinteger(L, ImGuiWindowFlags_HorizontalScrollbar);
    lua_settable(L, -3);
    lua_settable(L, -3);
    return 1;
}