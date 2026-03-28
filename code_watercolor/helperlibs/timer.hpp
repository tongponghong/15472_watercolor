#pragma once

#include <chrono> 
#include <functional> 

struct Timer {
    using clock = std::chrono::high_resolution_clock;

    Timer(std::function< void(double) > const &cb_) :
        cb(cb_),
        before(clock::now())
        { }
    ~Timer() {
        clock::time_point after = clock::now();
        cb(std::chrono::duration< double >(after - before).count());
    }

    std::function< void(double) > cb;
    clock::time_point before;
};
