#pragma once
#include <chrono>
#include <iostream>

class PerfTimer {
public:
    PerfTimer(const char* name = "")
        : name(name), start(std::chrono::high_resolution_clock::now()) {}

    ~PerfTimer() {
        // auto end = std::chrono::high_resolution_clock::now();
        // auto duration = end - start;

        // print(duration);
    }
    std::string getDurationString(){
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = end - start;


        using namespace std::chrono;

        auto ns = duration_cast<nanoseconds>(duration).count();


        if (ns < 1'000) {
            return std::to_string(ns) + " ns";
        }
        else if (ns < 1'000'000) {
            return std::to_string(ns / 1'000.0) + " us";
        }
        else if (ns < 1'000'000'000) {
            return std::to_string(ns / 1'000'000.0) + " ms";
        }
        else {
            return std::to_string(ns / 1'000'000'000.0) + " s";
        }
        return "";
    }

private:
    const char* name;
    std::chrono::high_resolution_clock::time_point start;

    template <typename Duration>
    void print(Duration d) {
        using namespace std::chrono;

        auto ns = duration_cast<nanoseconds>(d).count();

        std::cout << name << " took: ";

        if (ns < 1'000) {
            std::cout << ns << " ns\n";
        }
        else if (ns < 1'000'000) {
            std::cout << ns / 1'000.0 << " us\n";
        }
        else if (ns < 1'000'000'000) {
            std::cout << ns / 1'000'000.0 << " ms\n";
        }
        else {
            std::cout << ns / 1'000'000'000.0 << " s\n";
        }
    }

};

#define MEASURE_PERF(name) PerfTimer timer##__LINE__(name);

/*
USAGE:

wrap the function in a block
{
    MEASURE_PERF("func")
        func();
}

*/