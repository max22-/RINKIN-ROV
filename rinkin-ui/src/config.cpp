#include <fstream>
#define TOML_IMPLEMENTATION
#include "toml.hpp"

toml::table config;

void load_config() {
    config = toml::parse_file("config.toml");
}

void save_config() {
    std::ofstream("config.toml") << config;
}

