#include <string>
#include "plot.h"

class Telemetry {
public:
    static Telemetry& get_instance();
    void handle_message(const std::string msg);
    float get_heading();
    float get_pitch();
    float get_roll();
    float get_battery();
    void plot_imu(const ImVec2& size);
private:
    Telemetry() : 
        heading_plot("heading", 1000), 
        pitch_plot("pitch", 1000),
        roll_plot("roll", 1000)
        {}
    Telemetry(const Telemetry&) = delete;
    Telemetry& operator=(const Telemetry&) = delete;
    float 
        heading = 0.0f, 
        pitch = 0.0f,
        roll = 0.0f,
        battery = 0.0f;
    Plot heading_plot, pitch_plot, roll_plot;
};