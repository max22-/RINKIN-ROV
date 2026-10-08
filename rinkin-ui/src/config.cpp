#include <fstream>
#include "config.h"

Config config;

void Config::load() {
    config.motors_amplitude = 20;
    config.ip = "192.168.4.1";
    config.window_width = 1280;
    config.window_height = 800;
    config.fullscreen = true;
    config.high_dpi = true;
}

void Config::save() {
    std::ofstream f("config.txt");
    f << "motors_amplitude=" << motors_amplitude << std::endl;
    f << "ip=" << ip << std::endl;
    f << "fullscreen=" << fullscreen << std::endl;
    f << "high_dpi=" << high_dpi << std::endl;
}


