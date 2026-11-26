/*

MIT License

Copyright (c) 2025-2026 JustStudio. <https://juststudio.is-a.dev/>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

std::random_device Math::rd;
std::mt19937 Math::gen(Math::rd());

double Math::Min(const std::vector<double>& values) {
    if (values.empty()) {
        throw std::invalid_argument("math.min: vector cannot be empty");
    }
    return *std::min_element(values.begin(), values.end());
}

double Math::Max(const std::vector<double>& values) {
    if (values.empty()) {
        throw std::invalid_argument("math.max: vector cannot be empty");
    }
    return *std::max_element(values.begin(), values.end());
}

double Math::Atan2(double y, double x) {
    return std::atan2(y, x);
}
double Math::Random() {
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    return dis(gen);
}

double Math::Random(double min, double max) {
    if (min >= max) {
        throw std::invalid_argument("math.random: max must be greater than min");
    }
    std::uniform_real_distribution<double> dis(min, max);
    return dis(gen);
}

double Math::Pow(double base, double exponent) {
    return std::pow(base, exponent);
}

double Math::Sqrt(double x) {
    if (x < 0) {
        throw std::invalid_argument("math.sqrt: cannot calculate square root of negative number");
    }
    return std::sqrt(x);
}

double Math::Exp(double x) {
    return std::exp(x);
}

double Math::Log(double x) {
    if (x <= 0) {
        throw std::invalid_argument("math.log: argument must be positive");
    }
    return std::log(x);
}

double Math::Log10(double x) {
    if (x <= 0) {
        throw std::invalid_argument("math.log10: argument must be positive");
    }
    return std::log10(x);
}

double Math::Clamp(double value, double min, double max) {
    if (min > max) {
        throw std::invalid_argument("math.clamp: min cannot be greater than max");
    }
    return (value < min) ? min : (value > max) ? max : value;
}

double Math::Lerp(double start, double end, double t) {
    return start + t * (end - start);
}

double Math::Sign(double x) {
    if (x > 0) return 1.0;
    if (x < 0) return -1.0;
    return 0.0;
}

bool Math::IsPrime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;

    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

long long Math::Factorial(int n) {
    if (n < 0) {
        throw std::invalid_argument("math.factorial: argument cannot be negative");
    }
    if (n > 20) {
        throw std::invalid_argument("math.factorial: argument too large for long long");
    }

    long long result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}

double Math::Hypot(double x, double y) {
    return std::hypot(x, y);
}

double Math::ToDegrees(double radians) {
    return radians * (180.0 / PI);
}

double Math::ToRadians(double degrees) {
    return degrees * (PI / 180.0);
}
