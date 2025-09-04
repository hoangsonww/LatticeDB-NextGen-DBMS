#pragma once
#include <random>
#include <cmath>

namespace dp {

class Laplace {
    std::mt19937_64 rng;
public:
    Laplace(uint64_t seed=0xBADC0FFEEULL) : rng(seed) {}
    double sample(double b) { // b = sensitivity/epsilon
        std::uniform_real_distribution<double> U(0.0, 1.0);
        double u = U(rng) - 0.5;
        return -b * std::copysign(1.0, u) * std::log(1 - 2*std::fabs(u));
    }
};
} // namespace dp
