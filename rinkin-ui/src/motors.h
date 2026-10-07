#include "plot.h"

#define MOTORS_COUNT 5

class Motors {
public:
    Motors();
    void set_speed(int n, int speed);
    void update();
    void sliders();
    void slider(int n);
    void plot(const ImVec2& size);

private:
    void send_speed(int n);
    int speeds[MOTORS_COUNT];
    Plot *plots[MOTORS_COUNT];
};