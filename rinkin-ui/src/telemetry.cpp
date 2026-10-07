#include <regex>
#include <raylib.h>
#include <imgui.h>
#include <implot.h>

#include "telemetry.h"

Telemetry& Telemetry::get_instance() {
    static Telemetry instance;
    return instance;
}





void Telemetry::handle_message(const std::string msg) {
    std::regex cmd_regex("^#([a-zA-Z]+),([^!]+)!\s*$");
    std::smatch m;
    if(std::regex_search(msg, m, cmd_regex)) {
        std::string command = m[1].str();
        std::string param_s = m[2].str();
        try { 
            float param = std::stof(param_s);
            printf("param_s = %s\n", param_s.c_str());
            printf("param = %f\n", param);
            TraceLog(LOG_INFO, "command: %s, param: %f", command.c_str(), param);
            if(command == "heading") {
                heading = DEG2RAD * param;
                heading_plot.append(heading);
                return;
            } else if(command == "pitch") {
                pitch = DEG2RAD * param;
                pitch_plot.append(pitch);
                return;
            } else if(command == "roll") {
                roll = DEG2RAD * param;
                roll_plot.append(roll);
                return;
            } else if(command == "battery") {
                battery = (param * 3.3 /65535) / 0.234;
                return;
            }
        } catch(std::invalid_argument &e) { 
        } catch(std::out_of_range &e) {}

    } else {
        TraceLog(LOG_ERROR, "invalid telemetry message: \"%s\"", msg.c_str());
    }
}

float Telemetry::get_heading() {
    return heading;
}

float Telemetry::get_pitch() {
    return pitch;
}

float Telemetry::get_roll() {
    return roll;
}

float Telemetry::get_battery() {
    return battery;
}

void Telemetry::plot_imu(const ImVec2& size) {
    if(ImPlot::BeginPlot("IMU", size)) {
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, 1000, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 360, ImGuiCond_Always);
        heading_plot.display();
        pitch_plot.display();
        roll_plot.display();
        ImPlot::EndPlot();
    }
}