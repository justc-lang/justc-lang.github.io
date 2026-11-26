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

#ifndef MATH_HPP
#define MATH_HPP

#include <string>
#include <vector>
#include <cmath>
#include <random>
#include <type_traits>
#include <initializer_list>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <limits>
#include "../../parser.h"

class Math {
private:
    static std::random_device rd;
    static std::mt19937 gen;

public:
    template<typename... Args>
    static double Min(Args... args) {
        static_assert(sizeof...(args) > 0, "math.min requires at least one argument");
        return minRecursive(args...);
    }

    static double Min(const std::vector<double>& values);

    template<typename... Args>
    static double Max(Args... args) {
        static_assert(sizeof...(args) > 0, "math.max requires at least one argument");
        return maxRecursive(args...);
    }

    static double Max(const std::vector<double>& values);

    static Value Abs(Value x) {
        if (static_cast<bool>(x.numeric_data)) {
            switch (x.getNumericType()) {
                case NumericType::FLOAT32:
                    return Value::createNumberWithType(static_cast<float>(std::abs(x.getNumericValue<float>())), NumericType::FLOAT32);
                case NumericType::NONE:
                case NumericType::FLOAT64:
                    return Value::createNumberWithType(std::abs(x.getNumericValue<double>()), NumericType::FLOAT64);
                case NumericType::BIGNUM:
                    return Value::createNumberWithType(std::abs(x.getNumericValue<long double>()), NumericType::BIGNUM);
                case NumericType::INT8:
                    return Value::createNumberWithType(static_cast<int8_t>(std::abs(x.getNumericValue<int8_t>())), NumericType::INT8);
                case NumericType::INT16:
                    return Value::createNumberWithType(static_cast<int16_t>(std::abs(x.getNumericValue<int16_t>())), NumericType::INT16);
                case NumericType::INT32:
                    return Value::createNumberWithType(static_cast<int32_t>(std::abs(x.getNumericValue<int32_t>())), NumericType::INT32);
                case NumericType::INT64:
                    return Value::createNumberWithType(static_cast<int64_t>(std::abs(x.getNumericValue<int64_t>())), NumericType::INT64);
                #if JUSTC_HAS_INT128
                    case NumericType::INT128: {
                        const __int128 value = x.getNumericValue<__int128>();

                        if (value == std::numeric_limits<__int128>::min()) {
                            throw std::runtime_error("math.abs: integer overflow");
                        }

                        return Value::createNumberWithType(
                            value < 0 ? -value : value,
                            NumericType::INT128
                        );
                    }
                #endif
                #if JUSTC_HAS_FLOAT128
                    case NumericType::FLOAT128: {
                        const __float128 value = x.getNumericValue<__float128>();

                        if (value == std::numeric_limits<__float128>::min()) {
                            throw std::runtime_error("math.abs: float overflow");
                        }

                        return Value::createNumberWithType(
                            value < 0.0Q ? -value : value, 
                            NumericType::FLOAT128
                        );
                    }
                #endif
                default:
                    return x; // uint, cuint
            }
        } else {
            return Value::createNumber(std::abs(x.number_value));
        }
    }

    #ifndef MATH_UNARY_FUNC
        #define MATH_UNARY_FUNC(NAME, OP) \
        static Value NAME(const Value& x) { \
            if (static_cast<bool>(x.numeric_data)) { \
                NumericType t = x.getNumericType(); \
                switch (t) { \
                    case NumericType::FLOAT32: \
                        return Value::createNumberWithType( \
                            static_cast<float>(OP(x.getNumericValue<float>())), t); \
                    case NumericType::BIGNUM: \
                        return Value::createNumberWithType( \
                            OP(x.getNumericValue<long double>()), t); \
                    case NumericType::INT8: \
                        return Value::createNumberWithType( \
                            static_cast<int8_t>(OP(x.getNumericValue<int8_t>())), t); \
                    case NumericType::INT16: \
                        return Value::createNumberWithType( \
                            static_cast<int16_t>(OP(x.getNumericValue<int16_t>())), t); \
                    case NumericType::INT32: \
                        return Value::createNumberWithType( \
                            static_cast<int32_t>(OP(x.getNumericValue<int32_t>())), t); \
                    case NumericType::INT64: \
                        return Value::createNumberWithType( \
                            static_cast<int64_t>(OP(x.getNumericValue<int64_t>())), t); \
                    case NumericType::UINT8: \
                    case NumericType::CUINT8: \
                        return Value::createNumberWithType( \
                            static_cast<uint8_t>(OP(x.getNumericValue<uint8_t>())), t); \
                    case NumericType::UINT16: \
                    case NumericType::CUINT16: \
                        return Value::createNumberWithType( \
                            static_cast<uint16_t>(OP(x.getNumericValue<uint16_t>())), t); \
                    case NumericType::UINT32: \
                    case NumericType::CUINT32: \
                        return Value::createNumberWithType( \
                            static_cast<uint32_t>(OP(x.getNumericValue<uint32_t>())), t); \
                    case NumericType::UINT64: \
                    case NumericType::CUINT64: \
                        return Value::createNumberWithType( \
                            static_cast<uint64_t>(OP(x.getNumericValue<uint64_t>())), t); \
                    default: \
                        return Value::createNumberWithType( \
                            OP(x.getNumericValue<double>()), NumericType::FLOAT64); \
                } \
            } else { \
                return Value::createNumber(OP(x.number_value)); \
            } \
        }
    #endif

    MATH_UNARY_FUNC(Sin, std::sin)
    MATH_UNARY_FUNC(Cos, std::cos)
    MATH_UNARY_FUNC(Tan, std::tan)
    MATH_UNARY_FUNC(Asin, std::asin)
    MATH_UNARY_FUNC(Acos, std::acos)
    MATH_UNARY_FUNC(Atan, std::atan)
    MATH_UNARY_FUNC(Ceil, std::ceil)
    MATH_UNARY_FUNC(Floor, std::floor)
    MATH_UNARY_FUNC(Round, std::round)

    static double Atan2(double y, double x);

    static double Random();
    static double Random(double min, double max);

    static double Pow(double base, double exponent);
    static double Sqrt(double x);
    static double Exp(double x);
    static double Log(double x);
    static double Log10(double x);

    static double Clamp(double value, double min, double max);
    static double Lerp(double start, double end, double t);
    static double Sign(double x);
    static bool IsPrime(int n);
    static long long Factorial(int n);
    static double Hypot(double x, double y);
    static double ToDegrees(double radians);
    static double ToRadians(double degrees);

    static constexpr double PI = 3.14159265358979323846;
    static constexpr double E = 2.71828182845904523536;
    static constexpr double LN2 = 0.6931471805599453;
    static constexpr double LN10 = 2.302585092994046;
    static constexpr double SQRT2 = 1.4142135623730951;
    static constexpr double SQRT1_2 = 0.7071067811865476;
    static constexpr double LOG2E = 1.4426950408889634;
    static constexpr double LOG10E = 0.4342944819032518;

    static double ParseNum(const std::string& str, int radix) {
        if (radix < 2 || radix > 64) {
            throw std::runtime_error("Radix must be between 2 and 64");
        }

        if (str.empty()) return 0.0;

        size_t start = 0;
        bool negative = false;
        if (str[0] == '-') {
            negative = true;
            start = 1;
        } else if (str[0] == '+') {
            start = 1;
        }

        double result = 0.0;

        for (size_t i = start; i < str.length(); i++) {
            char c = str[i];
            int digit;

            if (c >= '0' && c <= '9') {
                digit = c - '0';
            } else if (c >= 'a' && c <= 'z') {
                digit = 10 + (c - 'a');
            } else if (c >= 'A' && c <= 'Z') {
                digit = 10 + (c - 'A');
            } else if (c == '+') {
                digit = 62;
            } else if (c == '/') {
                digit = 63;
            } else if (c == '.' || c == ',') {
                break;
            } else {
                throw std::runtime_error("Invalid character for radix " + std::to_string(radix));
            }

            if (digit >= radix) {
                throw std::runtime_error("Digit '" + std::string(1, c) + "' out of range for radix " + std::to_string(radix));
            }

            result = result * radix + digit;
        }

        return negative ? -result : result;
    }

private:
    template<typename T>
    static double minRecursive(T value) {
        return static_cast<double>(value);
    }

    template<typename T, typename... Args>
    static double minRecursive(T first, T second, Args... args) {
        return minRecursive(std::min(static_cast<double>(first), static_cast<double>(second)), args...);
    }

    template<typename T>
    static double maxRecursive(T value) {
        return static_cast<double>(value);
    }

    template<typename T, typename... Args>
    static double maxRecursive(T first, T second, Args... args) {
        return maxRecursive(std::max(static_cast<double>(first), static_cast<double>(second)), args...);
    }
};

#endif
