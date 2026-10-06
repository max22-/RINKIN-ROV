#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <raylib.h>
#include <rlImGui.h>
#include "gui.h"
#include "udp.h"
#include "video.h"
#include "motors.h"
#include "config.h"
#include "telemetry.h"

Video *video = nullptr;
static std::string ip;
static int gamepad_num = 0;
extern RenderTexture2D model_texture;
static Motors motors;

static int gamepad_count() {
    int i;
    for(i = 0; IsGamepadAvailable(i); i++);
    return i;
}

void gui_init() {
    ip = config["ip"].value_or("192.168.4.1");
    video = new Video(std::string("rtsp://") + ip + ":8554/cam", 640, 480);
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

                }
                motors.update();
                motors.sliders();
                motors.plot(ImVec2(640, 480));

                ImGui::Text("%d FPS", GetFPS());

                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Config")) {
                ImGui::InputText("IP", &ip);
                ImGui::SameLine();
                if(ImGui::Button("Valider")) {
                    delete video;
                    video = new Video(ip, 640, 480);
                }
                int amplitude = config["moteurs"]["amplitude"].value_or(Motors::default_amplitude());
                if(ImGui::InputInt("Amplitude moteurs", &amplitude)) {
                    if(auto *motors = config["moteurs"].as_table()) {
                        TraceLog(LOG_INFO, "before: amplitude = %d", (*motors)["amplitude"]);
                        motors->insert_or_assign("amplitude", amplitude);
                        TraceLog(LOG_INFO, "after: amplitude = %d", (*motors)["amplitude"]);
                    } else {
                        TraceLog(LOG_INFO, "no motors table");
                    }
                }
                if(ImGui::Button("Sauvegarder")) {
                    config.insert_or_assign("ip", ip);
                    config.insert_or_assign("moteurs", toml::table());
                    if(auto *motors = config["moteurs"].as_table()) {
                        motors->insert_or_assign("amplitude", amplitude);
                    }
                    save_config();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
