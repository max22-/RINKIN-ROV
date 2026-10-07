#include <fstream>
#include "config.h"

Config config;

void Config::load() {
    config.motors_amplitude = 20;
    config.ip = "192.168.4.1";
}

void Config::save() {
    std::ofstream f("config.txt");
    f << "motors_amplitude=" << motors_amplitude << std::endl;
    f << "ip=" << ip << std::endl;
}


