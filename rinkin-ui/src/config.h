#include <string>

class Config {
public:
    void load();
    void save();
    int motors_amplitude;
    std::string ip;
    int window_width, window_height;
    bool fullscreen;
    bool high_dpi;
    
};

extern Config config;