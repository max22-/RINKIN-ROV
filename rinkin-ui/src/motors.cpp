#include <string>
#include <imgui.h>
#include <implot.h>

#include "motors.h"
#include "config.h"
#include "udp.h"

Motors::Motors() : speeds{0} {
    for(int i = 0; i < MOTORS_COUNT; i++)
        plots[i] = new Plot(std::string("Moteur") + std::to_string(i), 1000);
}

void Motors::set_speed(int n, int speed) {
    if(n >= 0 && n < MOTORS_COUNT)
        speeds[n] = speed;
    send_speed(n);
}

void Motors::update() {
    for(int i = 0; i < MOTORS_COUNT; i++)
        plots[i]->append(speeds[i]);
}

void Motors::sliders() {
    for(int i = 0; i < MOTORS_COUNT; i++)
        slider(i);
}

void Motors::slider(int n) {
    if(n < 0 || n >= MOTORS_COUNT) return;
    std::string label = std::string("Vitesse moteur ") + std::to_string(n);
    ImGui::SliderInt(label.c_str(), &speeds[n], -config.motors_amplitude, config.motors_amplitude);
}

void Motors::plot(const ImVec2& size) {
    if(ImPlot::BeginPlot("Vitesse moteurs", size)) {
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, 1000, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -config.motors_amplitude, config.motors_amplitude, ImGuiCond_Always);
        for(int i = 0; i < MOTORS_COUNT; i++)
            plots[i]->display();
        ImPlot::EndPlot();
    }
}

void Motors::send_speed(int n) {
    if(n < 0 || n >= MOTORS_COUNT) return;
    UDP::get_instance().send(std::string("#") + std::to_string(n) + "m" + std::to_string(speeds[n]) + "!\n");
}
