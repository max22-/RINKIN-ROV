#include <fstream>
#include <cstdio>
#include <cstring>
#include <raylib.h>
#include "config.h"

Config config;

void Config::load() {
    config.motors_amplitude = 20;
    config.ip = "192.168.4.1";
    config.window_width = 1280;
    config.window_height = 800;
    config.fullscreen = true;
    config.high_dpi = true;
    FILE *f = fopen("config.txt", "r");
    if(!f) return;
    char buf[1024] = {0};
    for(int i = 0; !feof(f); i++) {
        fgets(buf, sizeof(buf), f);
        const char *key = buf;
        char *value = strchr(buf, '=');
        if(value != NULL && value != key) {
            value[-1] = 0;
            if(!strcmp(key, "motors_amplitude")) {
                config.motors_amplitude = atoi(value);
            } else if(!strcmp(key, "ip")) {
                config.ip = value;
            } else if(!strcmp(key, "window_width")) {
                config.window_width = atoi(value);
            } else if(!strcmp(key, "window_height")) {
                config.window_height = atoi(value);
            } else if(!strcmp(key, "fullscreen")) {
                if(!strcmp(value, "true")) {
                    config.fullscreen = true;
                } else if(!strcmp(value, "false")) {
                    config.fullscreen = false;
                } else {
                    TraceLog(LOG_ERROR, "config.txt:%d: syntax error", i);
                }
            } else if(!strcmp(key, "high_dpi")) {
                if(!strcmp(value, "true")) {
                    config.high_dpi = true;
                } else if(!strcmp(value, "false")) {
                    config.high_dpi = false;
                } else {
                    TraceLog(LOG_ERROR, "config.txt:%d: syntax error", i);
                }
            }
        } else {
            TraceLog(LOG_ERROR, "config.txt:%d: syntax error", i);
        }
    }
}

void Config::save() {
    std::ofstream f("config.txt");
    f << "motors_amplitude=" << motors_amplitude << std::endl;
    f << "ip=" << ip << std::endl;
    f << "fullscreen=" << fullscreen << std::endl;
    f << "high_dpi=" << high_dpi << std::endl;
}


