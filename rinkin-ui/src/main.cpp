#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include <raymath.h>
#define RLIGHTS_IMPLEMENTATION
#include <rlights.h>
#include <imgui.h>
#include <rlImGui.h>
#include <implot.h>
#include <lua.hpp>
#include "lua_bindings/lua_bindings.h"
#include "lua_bindings/lua_udp.h"
#include "config.h"
#include "util.h"
#include "windows_fix.h"

#ifndef GLSL_VERSION
#define GLSL_VERSION            330
#endif

#if GLSL_VERSION == 330
#include "shaders/glsl330/lighting.vs.h"
#include "shaders/glsl330/lighting.fs.h"
#elif GLSL_VERSION == 120
#include "shaders/glsl120/lighting.vs.h"
#include "shaders/glsl120/lighting.fs.h"
#else
#error "Invalid GLSL version"
#endif


float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
float heading = 0.0f, pitch = 0.0f, roll = 0.0f;
Quaternion quaternion = QuaternionIdentity();
RenderTexture2D model_texture;

void lua_simple_fcall(lua_State *L, const char *fname) {
	lua_getglobal(L, fname);
	if(lua_isfunction(L, -1)) {
		int res = lua_pcall(L, 0, 0, 0)	;
		if(res != LUA_OK)
			fprintf(stderr, "%s\n", lua_tostring(L, -1));
	}
	lua_settop(L, 0);
}

lua_State *lua_start(lua_State *L) {
	if(L) lua_close(L);
	L = luaL_newstate();
	if(!L) FATAL("failed to initialize Lua");
	luaL_openlibs(L);
	lua_register_bindings(L);

	if(luaL_dostring(L, "package.path = './lua/?.lua;' .. package.path") != LUA_OK) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_close(L);
		return NULL;
	}

	int res = luaL_dofile(L, "lua/main.lua");
	if(res != LUA_OK) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_settop(L, 0);
	}
	lua_simple_fcall(L, "setup");

	return L;
}

int main(int argc, char* argv[]) {
	#ifdef _WIN32
	windows_networking_init();
	#endif
	
	printf("GLSL_VERSION=%d\n", GLSL_VERSION);
	printf("%.*s\n", lighting_fs_len, lighting_fs);
	
	
	int screenWidth = 1280;
	int screenHeight = 800;

	SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(screenWidth, screenHeight, "Rinkin");
	SetTargetFPS(60);
	rlImGuiSetup(true);
	ImGui::StyleColorsLight();
	ImPlot::CreateContext();
	

	Camera camera = { 0 };
    camera.position = (Vector3){ 0.0f, 100.0f, -1000.0f };// Camera position perspective
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
    camera.up = (Vector3){ 0.0f, -1.0f, 0.0f };          // Camera up vector (rotation towards target)
    camera.fovy = 30.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera type

	Shader shader = LoadShaderFromMemory((const char*)lighting_vs, (const char*)lighting_fs);
	shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    int ambientLoc = GetShaderLocation(shader, "ambient");
    SetShaderValue(shader, ambientLoc, (float[4]){ 0.1f, 0.1f, 0.1f, 1.0f }, SHADER_UNIFORM_VEC4);

	Light light;
    light = CreateLight(LIGHT_POINT, (Vector3){ 0, 500, 0 }, Vector3Zero(), WHITE, shader);

	Model model = LoadModel("model.obj");

	model_texture = LoadRenderTexture(640, 480);

	for (int i = 0; i < model.materialCount; i++) {
    	model.materials[i].shader = shader;
	}

	lua_State *L = lua_start(NULL);
	if(L == NULL) goto cleanup;

	while (!WindowShouldClose()) {
		if(IsKeyPressed(KEY_F5)) {
			L = lua_start(L);
		}
		lua_udp_callback(L);

		//UpdateCamera(&camera, CAMERA_ORBITAL);
		camera.position = Vector3{ pos_x + 0.0f, pos_y + 100.0f, pos_z - 1000.0f };
		camera.target = Vector3{pos_x, pos_y, pos_z};

		float cameraPos[3] = { camera.position.x, camera.position.y, camera.position.z };
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

		// https://www.raylib.com/examples/models/loader.html?name=models_yaw_pitch_roll
		//model.transform = MatrixRotateXYZ((Vector3){pitch, heading, roll});
		model.transform = QuaternionToMatrix(quaternion);

		BeginTextureMode(model_texture);
		{
			ClearBackground(DARKGRAY);
			BeginMode3D(camera);
			{
				BeginShaderMode(shader);
				{
					DrawModel(model, (Vector3){pos_x, pos_y, pos_z}, 1.0f, WHITE);
					//DrawCube(Vector3Zero(), 50, 50, 50, WHITE);
				}
				EndShaderMode();
				DrawSphereEx(light.position, 10.0f, 8, 8, light.color);
				DrawGrid(20, 40.0f);
				lua_simple_fcall(L, "draw_3d");
			}
			EndMode3D();
		}
		EndTextureMode();

		BeginDrawing();
		{
			ClearBackground(DARKGRAY);

			DrawFPS(10, 10);

			rlImGuiBegin();

			lua_simple_fcall(L, "loop");

			rlImGuiEnd();
		}
		EndDrawing();
	}

cleanup:

	if(L) lua_close(L);
	UnloadShader(shader);
	UnloadModel(model);
	ImPlot::DestroyContext();
    rlImGuiShutdown();
	CloseWindow();
	#ifdef _WIN32
	windows_networking_cleanup();
	#endif
	return 0;
}
