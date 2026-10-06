#include <implot.h>
#include "plot.h"

Plot::Plot(std::string name, size_t size) : name(name), size(size) {
    x = new float [size];
    y = new float [size];
    for(size_t i = 0; i < size; i++) {
        x[i] = i;
        y[i] = 0.0f;
    }
}

Plot::~Plot() {
    delete[] x;
    delete[] y;
}

void Plot::append(float y) {
    for(size_t i = 0; i < size - 1; i++)
        this->y[i] = this->y[i + 1];
    this->y[size - 1] = y;
}

void Plot::display() {
    ImPlot::PlotLine(name.c_str(), x, y, size, 0, 0);
}

