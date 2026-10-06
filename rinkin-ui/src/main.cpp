#include <stdio.h>
#include <stdlib.h>
#ifdef PLATFORM_ANDROID
#include <sys/stat.h>
#include <android/asset_manager.h>
#include "raymob.h"
#include "android_soft_keyboard.h"
#endif
#include <raylib.h>
#include <raymath.h>
#define RLIGHTS_IMPLEMENTATION
#include <rlights.h>
#include <imgui.h>
#include <rlImGui.h>
#include <implot.h>
#include "config.h"
#include "util.h"
#include "windows_fix.h"
#include "gui.h"
#include "udp.h"
#include "telemetry.h"

#ifndef GLSL_VERSION
#ifdef PLATFORM_ANDROID
#define GLSL_VERSION            300
#else
#define GLSL_VERSION            330
#endif
#endif

#if GLSL_VERSION == 330
#include "shaders/glsl330/lighting.vs.h"
#include "shaders/glsl330/lighting.fs.h"
#elif GLSL_VERSION == 120
#include "shaders/glsl120/lighting.vs.h"
#include "shaders/glsl120/lighting.fs.h"
#elif GLSL_VERSION == 300
#include "shaders/gles300/lighting.vs.h"
#include "shaders/gles300/lighting.fs.h"
#else
#error "Invalid GLSL version"
#endif

float heading = 0.0f, pitch = 0.0f, roll = 0.0f;
RenderTexture2D model_texture;

#ifdef PLATFORM_ANDROID

static void create_storage_dir(const char* folder) {
    char* storagePath = GetAppStoragePath();
    if (storagePath) {
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", storagePath, folder);
        mkdir(fullPath, 0770);
        free(storagePath);
    }
}

static void copy_lua_scripts() {
    create_storage_dir("lua");

    struct android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->assetManager) return;

    AAssetManager* assetManager = app->activity->assetManager;
    AAssetDir* assetDir = AAssetManager_openDir(assetManager, "lua");

    if (assetDir) {
        const char* fileName = NULL;
        while ((fileName = AAssetDir_getNextFileName(assetDir)) != NULL) {
            char assetPath[512];
            snprintf(assetPath, sizeof(assetPath), "lua/%s", fileName);

            int dataSize = 0;
            unsigned char* data = LoadFileData(assetPath, &dataSize);
            if (data) {
                WriteToAppStorage(assetPath, data, (unsigned int)dataSize);
                UnloadFileData(data);
                TraceLog(LOG_INFO, "LUA: Synced %s to internal storage", assetPath);
            }
        }
        AAssetDir_close(assetDir);
    }
}
#endif

int main(int argc, char* argv[]) {
	load_config();
	#ifdef _WIN32
	windows_networking_init();
	#endif
	
	printf("GLSL_VERSION=%d\n", GLSL_VERSION);
	printf("%.*s\n", lighting_fs_len, lighting_fs);
	
	#ifdef __ANDROID__
	SetConfigFlags(FLAG_FULLSCREEN_MODE);
	InitWindow(GetScreenWidth(), GetScreenHeight(), "Rinkin");
	#else	
	int screenWidth = 1280;
	int screenHeight = 800;

	SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(screenWidth, screenHeight, "Rinkin");
	#endif
	SetTargetFPS(60);
	rlImGuiSetup(true);
	ImGui::StyleColorsLight();
	ImPlot::CreateContext();

	#ifdef __ANDROID__
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScrollbarSize = 40.0f;
	#endif
	

	Camera camera = { 0 };
    camera.position = (Vector3){ 0.0f, 100.0f, -1000.0f };// Camera position perspective
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
    camera.up = (Vector3){ 0.0f, -1.0f, 0.0f };          // Camera up vector (rotation towards target)
    camera.fovy = 30.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera type

	TraceLog(LOG_INFO, "shader: %.s\n", lighting_fs_len, lighting_fs);

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

	Telemetry& telemetry = Telemetry::get_instance();
	gui_init();

	while (!WindowShouldClose()) {
		UDP& udp = UDP::get_instance();
		if(udp.data_available()) {
			std::string msg = udp.receive();
			Telemetry::get_instance().handle_message(msg);
		}

		#ifdef __ANDROID__
		android_soft_keyboard();
		#endif
			

		//UpdateCamera(&camera, CAMERA_ORBITAL);
		float cameraPos[3] = { camera.position.x, camera.position.y, camera.position.z };
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

		model.transform = MatrixRotateXYZ((Vector3){telemetry.get_roll(), telemetry.get_heading(), telemetry.get_pitch()});

		BeginTextureMode(model_texture);
		{
			ClearBackground(DARKGRAY);
			BeginMode3D(camera);
			{
				BeginShaderMode(shader);
				{
					DrawModel(model, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
					//DrawCube(Vector3Zero(), 50, 50, 50, WHITE);
				}
				EndShaderMode();
				DrawSphereEx(light.position, 10.0f, 8, 8, light.color);
			}
			EndMode3D();
		}
		EndTextureMode();

		BeginDrawing();
		{
			ClearBackground(DARKGRAY);
			rlImGuiBegin();
			gui();
			rlImGuiEnd();
		}
		EndDrawing();
	}

	gui_deinit();
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
