#include <string>

class Config {
public:
    void load();
    void save();
    int motors_amplitude;
    std::string ip;
    std::string video_url_format;
};

extern Config config;