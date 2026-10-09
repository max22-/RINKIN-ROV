#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#ifdef _WIN32
#include <external/fix_win32_compatibility.h>
#endif
#include <raylib.h>
#include <rlImGui.h>
#include "gui.h"
#include "udp.h"
#include "video.h"
#include "motors.h"
#include "config.h"
#include "telemetry.h"

Video *video = nullptr;
static int gamepad_num = 0;
extern RenderTexture2D model_texture;
static Motors motors;

static int gamepad_count() {
    int i;
    for(i = 0; IsGamepadAvailable(i); i++);
    return i;
}

static std::string video_url(std::string ip) {
    return std::string("rtsp://") + ip + ":8554/cam";
}

static float axis(int i) {
    if(!IsGamepadAvailable(gamepad_num)) return 0.0f;
    float val = GetGamepadAxisMovement(gamepad_num, i);
    if(i == 4 || i == 5) val = (val + 1) / 2;
    return val;
}

void gui_init() {
    video = new Video(video_url(config.ip), 640, 480);
}

void gui_deinit() {
    delete video;
}

void gui() {
    UDP& udp = UDP::get_instance();
    if(IsGamepadButtonPressed(gamepad_num, 11)) {
        udp.send("#0l1\n");
    } else if(IsGamepadButtonReleased(gamepad_num, 11)) {
        udp.send("#0l0!\n");
    }

    if(IsGamepadButtonPressed(gamepad_num, 6)) {
        for(int i = 0; i < MOTORS_COUNT; i++)
            motors.set_speed(i, 0);
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
    if(ImGui::Begin("Rinkin", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {
        if(ImGui::BeginTabBar("tab bar")) {
            if(ImGui::BeginTabItem("Accueil")) {
                ImGui::BeginGroup();
                {
                    video->display();
                    if(ImGui::Button("Démarrer"))
                        video->start();
                    ImGui::SameLine();
                    if(ImGui::Button("Arrêter"))
                        video->stop();
                    ImGui::SameLine();
                    if(ImGui::Button("Capturer image")) {
                        // capture image
                    }
                    ImGui::SameLine();
                    if(!video->is_recording()) {
                        if(ImGui::Button("Enregister")) {
                            // start recording
                        }
                    } else {
                        if(ImGui::Button("Arrêter enregistrement")) {
                            // stop recording
                        }
                    }
                    ImGui::SameLine();
                    static bool rotation = false;
                    ImGui::Checkbox("Rotation", &rotation);
                    video->rotation(rotation);
                }
                ImGui::EndGroup();
                ImGui::SameLine();
                rlImGuiImage((const Texture*)&model_texture.texture);
                ImGui::Separator();
                if(ImGui::Button("Allumer LED A")) udp.send("#0l1!\n");
                ImGui::SameLine();
                if(ImGui::Button("Éteindre LED A")) udp.send("#0l0!\n");
                ImGui::SameLine();
                if(ImGui::Button("Allumer LED B")) udp.send("#1l1!\n");
                ImGui::SameLine();
                if(ImGui::Button("Éteindre LED B")) udp.send("#1l0!\n");
                ImGui::SameLine();
                if(ImGui::Button("Allumer TEST")) udp.send("#2l1!\n");
                ImGui::SameLine();
                if(ImGui::Button("Éteindre TEST")) udp.send("#2l0!\n");
                ImGui::SameLine();
                if(ImGui::Button("Batterie")) udp.send("#0b0!\n");
                ImGui::SameLine();
                ImGui::Text("%.2f", Telemetry::get_instance().get_battery());

                ImGui::Combo("Gamepad", &gamepad_num, [](void* data, int n){ return GetGamepadName(n); }, NULL, gamepad_count());

                static bool gamepad_enabled = true;
                ImGui::Checkbox("Gamepad activé", &gamepad_enabled);

                if(gamepad_enabled) {
                    motors.set_speed(0, round((axis(1) + axis(4) - axis(5)) * config.motors_amplitude));
                    motors.set_speed(1, round((-axis(3) + axis(0)) * config.motors_amplitude));
                    motors.set_speed(2, round((-axis(3) - axis(0)) * config.motors_amplitude));
                    motors.set_speed(3, round((-axis(1) + axis(4) - axis(5)) * config.motors_amplitude));
                    motors.set_speed(4, round((-axis(1) + axis(4) - axis(5)) * config.motors_amplitude));
                }
                motors.update();
                motors.sliders();
                motors.plot(ImVec2(640, 480));

                ImGui::SameLine();

                Telemetry::get_instance().plot_imu(ImVec2(640, 480));

                static int up_down = 0;
                if(ImGui::SliderInt("Haut/Bas", &up_down, -config.motors_amplitude, config.motors_amplitude)) {
                    motors.set_speed(0, up_down);
                    motors.set_speed(3, up_down);
                    motors.set_speed(4, up_down);
                }
                if(ImGui::Button("Stop")) {
                    up_down = 0;
                    motors.set_speed(0, 0);
                    motors.set_speed(3, 0);
                    motors.set_speed(4, 0);
                }

                ImGui::Text("%d FPS", GetFPS());

                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Config")) {
                ImGui::InputText("IP", &config.ip);
                ImGui::SameLine();
                if(ImGui::Button("Valider")) {
                    delete video;
                    video = new Video(video_url(config.ip), 640, 480);
                    UDP::get_instance().reset(config.ip.c_str());
                }
                ImGui::InputInt("Amplitude moteurs", &config.motors_amplitude);
                if(ImGui::Button("Sauvegarder"))
                    config.save();
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Manette")) {
                if(IsGamepadAvailable(gamepad_num)) {
                    ImGui::Text(GetGamepadName(gamepad_num));
                    const int n = GetGamepadAxisCount(gamepad_num);
                    for(int i = 0; i < n; i++) {
                        float v = axis(i);
                        ImGui::SliderFloat(TextFormat("axe %d", i), &v, -1.0f, 1.0f);
                    }
                } else {
                    ImGui::Text("Manette non disponible");
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
