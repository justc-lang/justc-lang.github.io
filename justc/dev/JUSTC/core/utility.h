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

#ifndef UTILITY_H
#define UTILITY_H

#include "parser.h"
#include <string>
#include <codecvt>
#include <locale>
#include <string_view>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <type_traits>

class Utility {
private:
    static std::string _stringifyValue(const Value& value, int indentLevel = 0);
    static std::string _stringifyValue(const Value::Property& value, int indentLevel = 0);
public:
    template<typename Op>
    static inline std::string stringMath(
        std::string_view left,
        std::string_view right,
        Op&& op
    ) {
        const size_t size = std::max(left.size(), right.size());

        std::string result;
        result.resize(size);

        const char* l = left.data();
        const char* r = right.data();
        char* out = result.data();

        const size_t lsize = left.size();
        const size_t rsize = right.size();

        for (size_t i = 0; i < size; ++i) {
            const unsigned char a =
                (i < lsize) ? static_cast<unsigned char>(l[i]) : 0;

            const unsigned char b =
                (i < rsize) ? static_cast<unsigned char>(r[i]) : 0;

            out[i] = static_cast<char>(op(a, b));
        }

        return result;
    }

    static inline std::string stringAdd(
        std::string_view left,
        std::string_view right
    ) {
        return stringMath(left, right,
            [](uint8_t a, uint8_t b) {
                return a + b;
            }
        );
    }

    static inline std::string stringSub(
        std::string_view left,
        std::string_view right
    ) {
        return stringMath(left, right,
            [](uint8_t a, uint8_t b) {
                return a - b;
            }
        );
    }

    static inline std::string stringMul(
        std::string_view left,
        std::string_view right
    ) {
        return stringMath(left, right,
            [](uint8_t a, uint8_t b) {
                return a * b;
            }
        );
    }

    static inline std::string stringDiv(
        std::string_view left,
        std::string_view right
    ) {
        return stringMath(left, right,
            [](uint8_t a, uint8_t b) {
                return a / b;
            }
        );
    }

    static inline std::string stringXor(
        std::string_view left,
        std::string_view right
    ) {
        return stringMath(left, right,
            [](uint8_t a, uint8_t b) {
                return a ^ b;
            }
        );
    }

    static inline std::string stringAnd(
        std::string_view left,
        std::string_view right
    ) {
        return stringMath(left, right,
            [](uint8_t a, uint8_t b) {
                return a & b;
            }
        );
    }

    static inline std::string stringOr(
        std::string_view left,
        std::string_view right
    ) {
        return stringMath(left, right,
            [](uint8_t a, uint8_t b) {
                return a | b;
            }
        );
    }

    static inline std::string stringNot(std::string_view str) {
        std::string result(str.size(), '\0');

        const char* in = str.data();
        char* out = result.data();

        for (size_t i = 0; i < str.size(); ++i) {
            out[i] = static_cast<char>(
                ~static_cast<unsigned char>(in[i])
            );
        }

        return result;
    }

    static inline std::string stringLShift(
        std::string_view left,
        std::string_view right
    ) {
        return stringMath(left, right,
            [](uint8_t a, uint8_t b) {
                return a << b;
            }
        );
    }

    static inline std::string stringRShift(
        std::string_view left,
        std::string_view right
    ) {
        return stringMath(left, right,
            [](uint8_t a, uint8_t b) {
                return a >> b;
            }
        );
    }

    static inline std::string stringPow(
        std::string_view left,
        std::string_view right
    ) {
        return stringMath(left, right,
            [](uint8_t a, uint8_t b) {
                return std::pow(a, b);
            }
        );
    }

    static inline std::string stringFMod(
        std::string_view left,
        std::string_view right
    ) {
        return stringMath(left, right,
            [](uint8_t a, uint8_t b) {
                return std::fmod(a, b);
            }
        );
    }

    static std::string numberValue2string(const Value& value);
    static std::string value2string(const Value& value);
    static std::string double2hexString(const double d);
    static std::string double2octString(const double d);
    static std::string double2binString(const double d);
    static bool checkNumbers(const Value& left, const Value& right);
    static std::pair<size_t, size_t> pos(const size_t& pos, const std::string& script);
    static std::string position(const size_t& pos_, const std::string& script);
    static DataType typeDeclaration2dataType(const std::string& typeDeclaration, const std::string& position);
    static Value convert(const Value value, const DataType type);
    static Value ParseResult2Value(const ParseResult parseresult);
    static bool isGitHubActions();
    static std::unordered_map<std::string, std::string> ParseHeaders(const std::string& headers);
    static std::string defaultHTTPAccept;
    static void Warn(const std::string& warning);
    static std::string escapeJUSTCString(const std::string& str);
    static std::string stringifyValue(const Value& value);
    static bool checkNumber(const Value& val);
    static bool checkObject(const Value& val);
    static bool checkObjects(const Value& left, const Value& right);
    static bool checkString(const Value& val);
    static bool checkStrings(const Value& left, const Value& right);
    static std::string doubleToString(double value);
    static bool compareValues(const Value& left, const Value& right);
    static bool compareValues(const Value::Property& left, const Value& right);
    static bool compareValues(const Value& left, const Value::Property& right);
    static bool compareValues(const Value::Property& left, const Value::Property& right);
    static bool checkElement(const Value& val);
    static bool checkArray(const Value& val);
    static bool checkArrays(const Value& left, const Value& right);

    static inline std::string uint64ToHexString(uint64_t num) {
        std::stringstream ss;
        ss << "0x" << std::setfill('0') << std::setw(16) << std::hex << num;
        return ss.str();
    }
    static inline std::string uint32ToHexString(uint32_t num) {
        std::stringstream ss;
        ss << "0x" << std::setfill('0') << std::setw(8) << std::hex << num;
        return ss.str();
    }

    static std::string hashString(const Value& val);
    static std::string hashString(const Value::Property& val);

    static std::pair<bool, std::string> env(const std::string& name);

    template<typename To, typename From>
    static std::vector<To> convertVector(const std::vector<From>& input);

    template<typename To, typename From>
    static std::vector<To> expandVector(const std::vector<From>& input);

    template<typename To, typename From>
    static std::vector<To> shrinkVector(const std::vector<From>& input);

    template<typename To, typename From>
    static std::vector<To> reinterpretSign(const std::vector<From>& input);

    static std::vector<uint8_t>  toUint8(const std::vector<int8_t>& input);
    static std::vector<uint16_t> toUint16(const std::vector<int16_t>& input);
    static std::vector<uint32_t> toUint32(const std::vector<int32_t>& input);
    static std::vector<uint64_t> toUint64(const std::vector<int64_t>& input);
    static std::vector<int8_t>   toInt8(const std::vector<uint8_t>& input);
    static std::vector<int16_t>  toInt16(const std::vector<uint16_t>& input);
    static std::vector<int32_t>  toInt32(const std::vector<uint32_t>& input);
    static std::vector<int64_t>  toInt64(const std::vector<uint64_t>& input);

    template<typename To, typename From>
    static To bitCast(const From& from);

    static uint64_t checkPointer(const Value& val);
};
class UnicodeUtility {
public:
    static bool isValidUTF8(const std::string& str) {
        try {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            std::wstring wstr = converter.from_bytes(str);
            return true;
        } catch (...) {
            return false;
        }
    }

    static std::string toUTF8(const std::wstring& wstr) {
        try {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.to_bytes(wstr);
        } catch (...) {
            return "";
        }
    }

    static std::wstring fromUTF8(const std::string& str) {
        try {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.from_bytes(str);
        } catch (...) {
            return L"";
        }
    }

    static std::string toLowerUTF8(const std::string& str) {
        try {
            std::wstring wstr = fromUTF8(str);
            std::transform(wstr.begin(), wstr.end(), wstr.begin(), ::towlower);
            return toUTF8(wstr);
        } catch (...) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
            return result;
        }
    }

    static bool isUnicodeIdentifier(const std::string& str) {
        if (str.empty()) return false;

        try {
            std::wstring wstr = fromUTF8(str);

            wchar_t first = wstr[0];
            if (first != L'_' && !iswalpha(first)) {
                return false;
            }

            for (wchar_t c : wstr) {
                if (c != L'_' && !iswalnum(c)) {
                    return false;
                }
            }

            return true;
        } catch (...) {
            return false;
        }
    }
};
class StringEscape {
public:
    static std::string unescape(const std::string& str);
    static std::string escape(const std::string& str);
    static bool isValidEscape(const std::string& str);
private:
    static char hexToChar(char c);
    static std::string codepointToUTF8(uint32_t cp);
};

#endif
