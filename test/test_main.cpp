#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include <thread>
#include <chrono>
#include <iostream>

int main(int argc, char** argv) {
    doctest::Context context;
    context.applyCommandLine(argc, argv);

    std::cout << "[Test setup] Waiting 2 seconds for crow_app to start..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    int res = context.run();

    return res;
}
