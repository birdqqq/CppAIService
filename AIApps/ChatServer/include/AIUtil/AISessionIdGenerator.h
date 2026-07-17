#pragma once
#include <chrono>
#include <random>
#include <cstdlib>
#include <ctime>
#include <string>


class AISessionIdGenerator {
public:
    AISessionIdGenerator() {
        // ʼӣֻһ
        std::srand(static_cast<unsigned>(std::time(nullptr)));
    }
    //ʱһid
    std::string generate();
};
