#ifndef PLOT_H
#define PLOT_H

#include <cstddef>
#include <string>

class Plot {
public:
    Plot(std::string name, size_t size);
    ~Plot();
    void append(float y);
    void display();
private:
    std::string name;
    float *x, *y;
    size_t size;
};

#endif