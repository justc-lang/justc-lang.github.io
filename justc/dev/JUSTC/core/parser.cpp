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

#ifdef _WIN32
    #define NOMINMAX
    #undef INFINITE
    #undef NAN
    #undef ERROR
    #undef DELETE
#endif

#include "parser.h"
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <iostream>
#include <chrono>
#include <ctime>
#include <cstring>
#include "fetch.h"
#include "version.h"
#include "utility.h"
#include <vector>
#include "import.hpp"
#include "lang/luau.hpp"
#include <string>
#include <unordered_map>
#include "built-in/s.hpp"
#include <variant>
#include "unicode.hpp"
#include "builtins.h"
#include "global.h"
#include "justo.hpp"
#include <cstdint>
#include <limits>
#include "cpptypes.h"
#include <iomanip>
#include <functional>
#include <unordered_set>

#ifdef __SIZEOF_FLOAT128__
    #if JUSTC_HAS_QUADMATH
        #include <quadmath.h>
        #define JUSTC_FLOAT128_SUPPORT 1
    #else
        #define JUSTC_FLOAT128_SUPPORT 0
    #endif
#else
    #define JUSTC_FLOAT128_SUPPORT 0
#endif

#ifdef _MSC_VER
    #define JUSTC_INT128_SUPPORT 0
    #define JUSTC_UINT128_SUPPORT 0
#elif defined(__SIZEOF_INT128__)
    #define JUSTC_INT128_SUPPORT 1
    #define JUSTC_UINT128_SUPPORT 1
#else
    #define JUSTC_INT128_SUPPORT 0
    #define JUSTC_UINT128_SUPPORT 0
#endif

#ifdef __EMSCRIPTEN__
    #include "parser.emscripten.h"
    #include <emscripten.h>

    #include <emscripten/val.h>
    #include <emscripten/bind.h>
    Value runJavaScript(const std::string& script, const std::string position, const bool warning) {
        Value output;
        output.name = "{{" + script + "}}";
        try {
            emscripten::val window = emscripten::val::global("window");
            emscripten::val result = window.call<emscripten::val>("eval", script);

            std::string result_type = result.typeOf().as<std::string>();
            if (result.isNull() || result.isUndefined()) {
                output.type = DataType::NULL_TYPE;
                output.string_value = "null";
            } else if (result_type == "string") {
                output.type = DataType::STRING;
                output.string_value = result.as<std::string>();
            } else if (result_type == "number") {
                output.type = DataType::NUMBER;
                output.number_value = result.as<double>();
            } else if (result_type == "boolean") {
                output.type = DataType::BOOLEAN;
                output.boolean_value = result.as<bool>();
            } else if (result_type == "object") {
                emscripten::val JSON = emscripten::val::global("JSON");
                emscripten::val json_string_val = JSON.call<emscripten::val>("stringify", result);
                if (result.isArray()) {
                    output.type = DataType::JSON_ARRAY;
                } else {
                    output.type = DataType::JSON_OBJECT;
                }
                output.string_value = json_string_val.as<std::string>();
            } else {
                emscripten::val String_global = emscripten::val::global("String");
                emscripten::val coerced_string_val = String_global.call<emscripten::val>("call", emscripten::val::undefined(), result);
                output.type = DataType::STRING;
                output.string_value = coerced_string_val.as<std::string>();
                if (warning) {
                    warn_unsupported_js_type(Parser::getCurrentTimestamp().c_str(), output.string_value.c_str(), position.c_str());
                }
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("JavaScript error at " + position + ":\n" + e.what());
        }
        return output;
    }
#else
    #include "lang/js.hpp"
#endif

#include <thread>
#include <chrono>
#include "promise-cpp/promise.hpp"
using namespace promise;

#ifndef DEFAULT_CPP_TYPE
#define DEFAULT_CPP_TYPE "_"
#endif
#ifndef FUNCTION_PREFIX
#define FUNCTION_PREFIX "__function_"
#endif
#ifndef FUNCTION_ID_NAME
#define FUNCTION_ID_NAME "__function_id"
#endif

std::string Value::toString() const {
    std::string valname = isVariable ? variable : name;
    switch (type) {
        case DataType::STRING:
        case DataType::LINK:
        case DataType::PATH:
        case DataType::VARIABLE:
        case DataType::ERROR:
            return string_value;
        case DataType::NUMBER:
            return toNumericString();
        case DataType::HEXADECIMAL:
            return "x" + std::to_string(static_cast<int>(number_value));
        case DataType::BINARY: {
            int num = static_cast<int>(number_value);
            if (num == 0) return "b0";
            std::string binary;
            while (num > 0) {
                binary = (num % 2 == 0 ? "0" : "1") + binary;
                num /= 2;
            }
            return "b" + binary;
        }
        case DataType::OCTAL: {
            std::stringstream ss;
            ss << "o" << std::oct << static_cast<int>(number_value);
            return ss.str();
        }
        case DataType::BOOLEAN:
            return boolean_value ? "true" : "false";
        case DataType::NULL_TYPE:
            return "null";
        case DataType::NOT_A_NUMBER:
            return "NaN";
        case DataType::INFINITE:
            return "Infinity";
        case DataType::NINFINITE:
            return "-Infinity";
        case DataType::JUSTC_OBJECT:
        case DataType::JSON_OBJECT:
            return "[object " + valname + "]";
        case DataType::JSON_ARRAY:
        case DataType::INT8_ARRAY:
        case DataType::INT16_ARRAY:
        case DataType::INT32_ARRAY:
        case DataType::INT64_ARRAY:
        case DataType::UINT8_ARRAY:
        case DataType::UINT16_ARRAY:
        case DataType::UINT32_ARRAY:
        case DataType::UINT64_ARRAY:
        case DataType::CUINT8_ARRAY:
        case DataType::CUINT16_ARRAY:
        case DataType::CUINT32_ARRAY:
        case DataType::CUINT64_ARRAY:
        case DataType::FLOAT32_ARRAY:
        case DataType::FLOAT64_ARRAY:
            return "[array " + valname + "]";
        case DataType::CLASS:
            return "[class " + valname + "]";
        case DataType::SPACE:
            return "[space " + valname + "]";
        case DataType::FUNCTION: {
            std::stringstream ae;
            bool first = true;
            for (Value val : array_elements) {
                if (!first) ae << ", ";
                std::string td = dataTypeToTypeDecl(val.type);
                ae << val.name;
                if (td != "auto") ae << " : " << td;
                first = false;
            }
            
            std::stringstream args;
            first = true;
            for (size_t i = 0; i < function_info.paramNames.size(); i++) {
                std::string arg = function_info.paramNames[i];
                if (native && arg == FUNCTION_ID_NAME) continue;
                if (!first) args << ", ";
                std::string td = dataTypeToTypeDecl(function_info.paramTypes[i]);
                Value dv = function_info.defaultValues[i];

                args << arg;
                if (td != "auto") args << " : " << td;
                if (dv.type != DataType::UNKNOWN && dv.type != DataType::NULL_TYPE) args << " = " << dv.toString();
                
                first = false;
            }

            return std::string(
                function_info.isIsolated ? "isolated " : ""
            ) + "function " + valname + (
                array_elements.size() > 0 ? " [" + ae.str() + "] " : ""
            ) + "(" + args.str() + ") {" + string_value + "}";
        }
        case DataType::STRUCT: 
            return "[struct " + valname + "]";
        case DataType::JSX_ELEMENT:
            return "[element " + valname + "]";
        case DataType::MAP:
            return "[map " + valname + "]";
        case DataType::SET:
            return "[set " + valname + "]";
        case DataType::PROMISE:
            return "[promise " + valname + "]";
        case DataType::ENUM:
            return "[enum " + valname + "]";
        case DataType::POINTER:
            return "[pointer " + Utility::uint64ToHexString(getNumericValue<uint64_t>()) + "]";
        default:
            return "unknown";
    }
}
std::string Value::toIdentifier() const {
    switch (type) {
        case DataType::STRING:
        case DataType::LINK:
        case DataType::PATH:
        case DataType::VARIABLE:
            return string_value;
        case DataType::UNKNOWN:
        case DataType::FUNCTION:
        case DataType::JUSTC_OBJECT:
        case DataType::JSON_OBJECT:
        case DataType::JSON_ARRAY:
        case DataType::CLASS:
        case DataType::SPACE:
        case DataType::STRUCT:
        case DataType::MAP:
        case DataType::SET:
        case DataType::INT8_ARRAY:
        case DataType::INT16_ARRAY:
        case DataType::INT32_ARRAY:
        case DataType::INT64_ARRAY:
        case DataType::UINT8_ARRAY:
        case DataType::UINT16_ARRAY:
        case DataType::UINT32_ARRAY:
        case DataType::UINT64_ARRAY:
        case DataType::CUINT8_ARRAY:
        case DataType::CUINT16_ARRAY:
        case DataType::CUINT32_ARRAY:
        case DataType::CUINT64_ARRAY:
        case DataType::FLOAT32_ARRAY:
        case DataType::FLOAT64_ARRAY:
        case DataType::PROMISE:
        case DataType::ENUM:
            return isVariable ? variable : name;
        default:
            return toString();
    }
}

double Value::toNumber() const {
    switch (type) {
        case DataType::NUMBER:
        case DataType::HEXADECIMAL:
        case DataType::BINARY:
        case DataType::OCTAL:
            return number_value;
        case DataType::STRING:
            try {
                return std::stod(string_value);
            } catch (...) {
                return 0.0;
            }
        case DataType::BOOLEAN:
            return boolean_value ? 1.0 : 0.0;
        case DataType::NULL_TYPE:
            return 0.0;
        case DataType::NOT_A_NUMBER:
            return std::numeric_limits<double>::quiet_NaN();
        case DataType::INFINITE:
            return std::numeric_limits<double>::infinity();
        case DataType::NINFINITE:
            return -std::numeric_limits<double>::infinity();
        default:
            return 0.0;
    }
}

bool Value::toBoolean() const {
    switch (type) {
        case DataType::BOOLEAN:
            return boolean_value;
        case DataType::NUMBER:
        case DataType::HEXADECIMAL:
        case DataType::BINARY:
        case DataType::OCTAL:
            return number_value != 0.0;
        case DataType::STRING: {
            if (string_value.empty()) return false;
            return true;
        }
        case DataType::LINK:
        case DataType::PATH:
        case DataType::VARIABLE:
        case DataType::INFINITE:
        case DataType::JSON_ARRAY:
        case DataType::JSON_OBJECT:
        case DataType::JUSTC_OBJECT:
        case DataType::CLASS:
        case DataType::SPACE:
        case DataType::JSX_ELEMENT:
        case DataType::STRUCT:
        case DataType::MAP:
        case DataType::SET:
        case DataType::INT8_ARRAY:
        case DataType::INT16_ARRAY:
        case DataType::INT32_ARRAY:
        case DataType::INT64_ARRAY:
        case DataType::UINT8_ARRAY:
        case DataType::UINT16_ARRAY:
        case DataType::UINT32_ARRAY:
        case DataType::UINT64_ARRAY:
        case DataType::CUINT8_ARRAY:
        case DataType::CUINT16_ARRAY:
        case DataType::CUINT32_ARRAY:
        case DataType::CUINT64_ARRAY:
        case DataType::FLOAT32_ARRAY:
        case DataType::FLOAT64_ARRAY:
        case DataType::PROMISE:
        case DataType::ENUM:
            return true;
        default:
            return false;
    }
}

Value Value::createNumber(double num) {
    Value result;
    result.type = DataType::NUMBER;
    result.number_value = num;
    result.name = Utility::doubleToString(num);
    return result;
}

Value Value::createString(const std::string& str) {
    Value result;
    result.type = DataType::STRING;
    result.string_value = str;
    result.name = "\"" + str + "\"";
    return result;
}

Value Value::createBoolean(bool b) {
    Value result;
    result.type = DataType::BOOLEAN;
    result.boolean_value = b;
    result.name = b;
    return result;
}

Value Value::createNull() {
    Value result;
    result.type = DataType::NULL_TYPE;
    result.name = "nil";
    return result;
}

Value Value::createLink(const std::string& link) {
    Value result;
    result.type = DataType::LINK;
    result.string_value = link;
    result.name = "l<" + link + ">";
    return result;
}

Value Value::createPath(const std::string& path) {
    Value result;
    result.type = DataType::PATH;
    result.string_value = path;
    result.name = path;
    return result;
}

Value Value::createVariable(const std::string& varName) {
    Value result;
    result.type = DataType::VARIABLE;
    result.string_value = varName;
    result.name = varName;
    return result;
}

Value Value::createHexadecimal(double num) {
    Value result;
    result.type = DataType::HEXADECIMAL;
    result.number_value = num;
    result.name = "x" + Utility::double2hexString(num);
    return result;
}

Value Value::createBinary(double num) {
    Value result;
    result.type = DataType::BINARY;
    result.number_value = num;
    result.name = "b" + Utility::double2binString(num);
    return result;
}

Value Value::createOctal(double num) {
    Value result;
    result.type = DataType::OCTAL;
    result.number_value = num;
    result.name = "o" + Utility::double2octString(num);
    return result;
}

Value Value::createBinaryData(const std::vector<unsigned char>& data) {
    Value result;
    result.type = DataType::BINARY_DATA;
    result.binary_data = data;
    result.name = "BinaryData size=" + std::to_string(data.size()) + "";
    return result;
}

Value Value::createJustcObject(const std::shared_ptr<ObjectContext>& context) {
    Value result;
    result.type = DataType::JUSTC_OBJECT;
    result.object_context = context;
    result.object_type = DataType::JUSTC_OBJECT;
    result.name = "Object";
    return result;
}

Value Value::createJsonObject(const std::unordered_map<std::string, Value>& obj) {
    Value result;
    result.type = DataType::JSON_OBJECT;
    result.object_type = DataType::JSON_OBJECT;
    for (const auto& [key, value] : obj) {
        result.properties[key] = Value::Property(value, Access::READ_WRITE);
    }
    result.name = "Object";
    return result;
}

Value Value::createJsonArray(const std::vector<Value>& arr) {
    Value result;
    result.type = DataType::JSON_ARRAY;
    result.object_type = DataType::JSON_ARRAY;
    result.array_elements = arr;
    result.name = "Array";
    return result;
}

template<typename T>
Value Value::createNumberWithType(T num, NumericType numType) {
    Value result;
    result.type = DataType::NUMBER;
    result.number_value = static_cast<double>(num);
    
    result.numeric_data = std::make_shared<NumericValue>();
    result.numeric_data->value = static_cast<double>(num);
    result.numeric_data->type = numType;
    
    size_t size = NumericValue::getTypeSize(numType);
    result.numeric_data->data = malloc(size);
    if (static_cast<bool>(result.numeric_data->data)) {
        switch (numType) {
            case NumericType::FLOAT32:
                *(float*)result.numeric_data->data = static_cast<float>(num);
                break;
            case NumericType::FLOAT64:
                *(double*)result.numeric_data->data = static_cast<double>(num);
                break;
            case NumericType::BIGNUM:
                *(long double*)result.numeric_data->data = static_cast<long double>(num);
                break;
            #if JUSTC_FLOAT128_SUPPORT
            case NumericType::FLOAT128:
                *(__float128*)result.numeric_data->data = static_cast<__float128>(num);
                break;
            #endif
            case NumericType::INT8:
                *(int8_t*)result.numeric_data->data = static_cast<int8_t>(num);
                break;
            case NumericType::INT16:
                *(int16_t*)result.numeric_data->data = static_cast<int16_t>(num);
                break;
            case NumericType::INT32:
                *(int32_t*)result.numeric_data->data = static_cast<int32_t>(num);
                break;
            case NumericType::INT64:
                *(int64_t*)result.numeric_data->data = static_cast<int64_t>(num);
                break;
            #if JUSTC_INT128_SUPPORT
            case NumericType::INT128:
                *(__int128*)result.numeric_data->data = static_cast<__int128>(num);
                break;
            #endif
            #if JUSTC_UINT128_SUPPORT
            case NumericType::UINT128:
                *(unsigned __int128*)result.numeric_data->data = static_cast<unsigned __int128>(num);
                break;
            #endif
            case NumericType::UINT8: case NumericType::CUINT8:
                *(uint8_t*)result.numeric_data->data = static_cast<uint8_t>(num);
                break;
            case NumericType::UINT16: case NumericType::CUINT16:
                *(uint16_t*)result.numeric_data->data = static_cast<uint16_t>(num);
                break;
            case NumericType::UINT32: case NumericType::CUINT32:
                *(uint32_t*)result.numeric_data->data = static_cast<uint32_t>(num);
                break;
            case NumericType::UINT64: case NumericType::CUINT64:
                *(uint64_t*)result.numeric_data->data = static_cast<uint64_t>(num);
                break;
            default:
                *(double*)result.numeric_data->data = static_cast<double>(num);
                break;
        }
    }

    result.name = result.toNumericString();
    
    return result;
}

template Value Value::createNumberWithType<int8_t>(int8_t, NumericType);
template Value Value::createNumberWithType<int16_t>(int16_t, NumericType);
template Value Value::createNumberWithType<int32_t>(int32_t, NumericType);
template Value Value::createNumberWithType<int64_t>(int64_t, NumericType);
#if JUSTC_INT128_SUPPORT
template Value Value::createNumberWithType<__int128>(__int128, NumericType);
#endif
#if JUSTC_UINT128_SUPPORT
template Value Value::createNumberWithType<unsigned __int128>(unsigned __int128, NumericType);
#endif
template Value Value::createNumberWithType<uint8_t>(uint8_t, NumericType);
template Value Value::createNumberWithType<uint16_t>(uint16_t, NumericType);
template Value Value::createNumberWithType<uint32_t>(uint32_t, NumericType);
template Value Value::createNumberWithType<uint64_t>(uint64_t, NumericType);
template Value Value::createNumberWithType<float>(float, NumericType);
template Value Value::createNumberWithType<double>(double, NumericType);
template Value Value::createNumberWithType<long double>(long double, NumericType);
#if JUSTC_FLOAT128_SUPPORT
template Value Value::createNumberWithType<__float128>(__float128, NumericType);
#endif

Value Parser::createClass(const Class& value, bool hasName, std::string className) {
    return addClass(registerClass(value), hasName, className);
}
Value Parser::addClass(const uint64_t& classID, bool hasName, std::string className) {
    Value result = Value::createNumberWithType(classID, NumericType::UINT64);
    result.type = DataType::CLASS;
    result.name = hasName ? className : Utility::uint64ToHexString(classID);
    if (hasName) classes[className] = result;
    return result;
}

std::string Value::toNumericString() const {
    if (!static_cast<bool>(numeric_data)) {
        return Utility::doubleToString(number_value);
    }
    
    std::stringstream ss;
    switch (numeric_data->type) {
        case NumericType::FLOAT32:
            ss << std::setprecision(7) << *(float*)numeric_data->data;
            break;
        case NumericType::FLOAT64:
            ss << std::setprecision(15) << *(double*)numeric_data->data;
            break;
        case NumericType::BIGNUM:
            ss << std::setprecision(18) << *(long double*)numeric_data->data;
            break;
        case NumericType::INT8:
            ss << (int)*(int8_t*)numeric_data->data;
            break;
        case NumericType::INT16:
            ss << *(int16_t*)numeric_data->data;
            break;
        case NumericType::INT32:
            ss << *(int32_t*)numeric_data->data;
            break;
        case NumericType::INT64:
            ss << *(int64_t*)numeric_data->data;
            break;
        #if JUSTC_INT128_SUPPORT
        case NumericType::INT128: {
            __int128 val = *(const __int128*)numeric_data->data;
            if (val < 0) {
                ss << "-";
                val = -val;
            }
            if (val == 0) {
                ss << "0";
            } else {
                std::string str;
                while (val > 0) {
                    int digit = val % 10;
                    str = char('0' + digit) + str;
                    val /= 10;
                }
                ss << str;
            }
            break;
        }
        #endif
        #if JUSTC_UINT128_SUPPORT
        case NumericType::UINT128: {
            unsigned __int128 val = *(const unsigned __int128*)numeric_data->data;
            if (val == 0) {
                ss << "0";
            } else {
                std::string str;
                while (val > 0) {
                    int digit = val % 10;
                    str = char('0' + digit) + str;
                    val /= 10;
                }
                ss << str;
            }
            break;
        }
        #endif
        case NumericType::UINT8: case NumericType::CUINT8:
            ss << (unsigned int)*(uint8_t*)numeric_data->data;
            break;
        case NumericType::UINT16: case NumericType::CUINT16:
            ss << *(uint16_t*)numeric_data->data;
            break;
        case NumericType::UINT32: case NumericType::CUINT32:
            ss << *(uint32_t*)numeric_data->data;
            break;
        case NumericType::UINT64: case NumericType::CUINT64:
            ss << *(uint64_t*)numeric_data->data;
            break;
        case NumericType::FLOAT128: {
            #if JUSTC_FLOAT128_SUPPORT
                char buffer[64];
                quadmath_snprintf(buffer, sizeof(buffer), "%.20Qe", 
                                *(const __float128*)numeric_data->data);
                ss << buffer;
            #else
                ss << std::setprecision(18) << *(long double*)numeric_data->data;
            #endif
            break;
        }
        default:
            ss << numeric_data->value;
            break;
    }
    return ss.str();
}

Value Value::toPrimitive() const {
    Value result = *this;
    result.name = "";
    result.isVariable = false;
    result.variable = "";
    result.varType = VariableType::VARIABLE;
    result.isConst = false;
    return result;
}

bool Value::operator==(const Value& other) const {
    return Utility::compareValues(*this, other);
}
bool Value::operator!=(const Value& other) const {
    return !(*this == other);
}
namespace std {
    size_t hash<Value>::operator()(const Value& value) const {
        std::string hashStr = Utility::hashString(value);
        return std::hash<std::string>{}(hashStr);
    }
}

namespace {

Value stringArray(const std::vector<std::string_view>& strings) {
    std::vector<Value> values;
    values.reserve(strings.size());

    for (std::string_view sv : strings) {
        values.emplace_back(Value::createString(std::string(sv)));
    }

    return Value::createJsonArray(std::move(values));
}

Value stringArray(const std::vector<std::string>& strings) {
    std::vector<Value> values;
    values.reserve(strings.size());

    for (const std::string& s : strings) {
        values.emplace_back(Value::createString(s));
    }

    return Value::createJsonArray(std::move(values));
}

std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool isHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool isBinaryDigit(char c) {
    return c == '0' || c == '1';
}

bool isOctalDigit(char c) {
    return c >= '0' && c <= '7';
}

double parseNumber(const std::string& str) {
    try {
        return std::stod(str);
    } catch (...) {
        return 0.0;
    }
}

bool isValidLink(const std::string& str) {
    return str.find("://") != std::string::npos ||
           str.find("www.") != std::string::npos ||
           (str.find('.') != std::string::npos && str.find('/') != std::string::npos);
}

long getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

}

void Parser::builtinObject(const std::string& name, std::unordered_map<std::string, Value> props) {
    auto objCtx = std::make_shared<ObjectContext>();
    std::vector<std::string> outputVars;
    for (const auto& [key, value] : props) {
        objCtx->variables[key] = value;
        outputVars.push_back(key);
    }
    objCtx->outputMode = "specified";
    objCtx->outputVariables = outputVars;
    Value objVal = Value::createJustcObject(objCtx);
    objVal.name = name;
    objVal.properties = pmap(props);
    objVal.type = DataType::JSON_OBJECT;
    variables[name] = objVal;
    constVars[name] = true;
}
Value Parser::builtinObjectFunction(const std::string& name) {
    Value funcVal;
    funcVal.type = DataType::FUNCTION;
    funcVal.name = name;
    funcVal.string_value = "[native code]";
    funcVal.object_type = DataType::FUNCTION;
    funcVal.native = true;
    return funcVal;
}

Parser::Parser(
    const std::vector<ParserToken>& tokens, bool doExecute, bool runAsync, const std::string& input, const bool allowJavaScript,
    const bool canAllowJS, const std::string scriptName, const std::string scriptType, const bool allowLuau, const bool canAllowLuau,
    const bool isFunction, const std::unordered_map<std::string, Value>* initialContext, const CharType chartype, const ParserType parsertype
) :
    tokens(tokens), input(input), position(0), outputMode("everything"), allowJavaScript(allowJavaScript), globalScope(false),
    strictMode(false), hasLogFile(false), allowLuau(allowLuau), canAllowLuau(canAllowLuau), doExecute(doExecute), runAsync(runAsync),
    canAllowJS(allowJavaScript ? true : canAllowJS), scriptName(scriptName), scriptType(scriptType), asJSON(false), isJSONArray(false),
    endOfScript("."), returnValue(DataType::UNKNOWN), isFunction(isFunction), chartype(chartype), currentScope(0), rootIndex(0),
    parsertype(parsertype), nextStructConstructor(0), nextIndex(0), throwError(false), hasWindow(false)
{
    initializeCPPTypes();
    initializeBuiltIns();

    rootIndex = incrementRootCounter();
    currentScope = rootIndex;
    
    localScopes[rootIndex] = std::unordered_map<std::string, Value>();
    localConstVars[rootIndex] = std::unordered_map<std::string, bool>();

    if (initialContext) {
        for (const auto& [key, value] : *initialContext) {
            variables[key] = value;
            constVars[key] = false;
            setLocal(rootIndex, key, value, false);
            if (parsertype == ParserType::STRUCT || parsertype == ParserType::CLASS || parsertype == ParserType::ENUM) outputExcludeVariables.push_back(key);
            if (value.type == DataType::STRUCT) structures[key] = value;
            if (value.type == DataType::CLASS) classes[key] = value;
        }
    }

    // built-in spaces / type methods

    typeMethods[DataType::JUSTC_OBJECT] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"size", "Object::size"},
    };
    typeMethods[DataType::NUMBER] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
    };
    typeMethods[DataType::STRING] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"reverse", "String::reverse"},
        {"graphemeReverse", "String::graphemeReverse"},
        {"codePointReverse", "String::codePointReverse"},
        {"byteReverse", "String::byteReverse"},
        {"trim", "String::trim"},
        {"repeat", "String::repeat"},
        {"slice", "String::slice"},
        {"graphemeSlice", "String::graphemeSlice"},
        {"codePointSlice", "String::codePointSlice"},
        {"byteSlice", "String::byteSlice"},
        {"lower", "String::lower"},
        {"upper", "String::upper"},
        {"normalize", "String::normalize"},
        {"length", "String::length"},
        {"graphemeLength", "String::graphemeLength"},
        {"codePointLength", "String::codePointLength"},
        {"byteLength", "String::byteLength"},
        {"size", "String::size"},
        {"equalsIgnoreCase", "String::equalsIgnoreCase"},
        {"isWhitespace", "String::isWhitespace"},
        {"startsWith", "String::startsWith"},
        {"endsWith", "String::endsWith"},
        {"split", "String::split"}
    };
    typeMethods[DataType::LINK] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
    };
    typeMethods[DataType::BOOLEAN] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
    };
    typeMethods[DataType::JSON_OBJECT] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"size", "Object::size"},
    };
    typeMethods[DataType::JSON_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"join", "Array::join"},
        {"includes", "Array::includes"},
        {"indexOf", "Array::indexOf"},
        {"lastIndexOf", "Array::lastIndexOf"},
        {"reverse", "Array::reverse"},
        {"forEach", "Array::forEach"},
        {"push", "Array::push"},
        {"unshift", "Array::unshift"},
        {"size", "Array::size"},
    };
    typeMethods[DataType::NULL_TYPE] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
    };
    typeMethods[DataType::FUNCTION] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
    };
    typeMethods[DataType::NOT_A_NUMBER] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
    };
    typeMethods[DataType::INFINITE] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
    };
    typeMethods[DataType::NINFINITE] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
    };
    typeMethods[DataType::STRUCT] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
    };
    typeMethods[DataType::JSX_ELEMENT] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"render", "Element::render"},
    };
    typeMethods[DataType::MAP] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"set", "Map::set"},
        {"get", "Map::get"},
        {"has", "Map::has"},
        {"delete", "Map::delete"},
        {"clear", "Map::clear"},
        {"size", "Map::size"},
    };
    typeMethods[DataType::SET] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"add", "Set::add"},
        {"has", "Set::has"},
        {"delete", "Set::delete"},
        {"clear", "Set::clear"},
        {"size", "Set::size"},
    };
    typeMethods[DataType::INT8_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "Int8Array::compress"},
        {"decompress", "Int8Array::decompress"},
        {"size", "Int8Array::size"},
    };
    typeMethods[DataType::INT16_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "Int16Array::compress"},
        {"decompress", "Int16Array::decompress"},
        {"size", "Int16Array::size"},
    };
    typeMethods[DataType::INT32_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "Int32Array::compress"},
        {"decompress", "Int32Array::decompress"},
        {"size", "Int32Array::size"},
    };
    typeMethods[DataType::INT64_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "Int64Array::compress"},
        {"decompress", "Int64Array::decompress"},
        {"size", "Int64Array::size"},
    };
    typeMethods[DataType::UINT8_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "UInt8Array::compress"},
        {"decompress", "UInt8Array::decompress"},
        {"size", "UInt8Array::size"},
    };
    typeMethods[DataType::UINT16_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "UInt16Array::compress"},
        {"decompress", "UInt16Array::decompress"},
        {"size", "UInt16Array::size"},
    };
    typeMethods[DataType::UINT32_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "UInt32Array::compress"},
        {"decompress", "UInt32Array::decompress"},
        {"size", "UInt32Array::size"},
    };
    typeMethods[DataType::UINT64_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "UInt64Array::compress"},
        {"decompress", "UInt64Array::decompress"},
        {"size", "UInt64Array::size"},
    };
    typeMethods[DataType::CUINT8_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "CUInt8Array::compress"},
        {"decompress", "CUInt8Array::decompress"},
        {"size", "CUInt8Array::size"},
    };
    typeMethods[DataType::CUINT16_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "CUInt16Array::compress"},
        {"decompress", "CUInt16Array::decompress"},
        {"size", "CUInt16Array::size"},
    };
    typeMethods[DataType::CUINT32_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "CUInt32Array::compress"},
        {"decompress", "CUInt32Array::decompress"},
        {"size", "CUInt32Array::size"},
    };
    typeMethods[DataType::CUINT64_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},

        {"compress", "CUInt64Array::compress"},
        {"decompress", "CUInt64Array::decompress"},
        {"size", "CUInt64Array::size"},
    };
    typeMethods[DataType::FLOAT32_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
        
        {"size", "Float32Array::size"},
    };
    typeMethods[DataType::FLOAT64_ARRAY] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
        
        {"size", "Float64Array::size"},
    };
    typeMethods[DataType::PROMISE] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
    };
    typeMethods[DataType::ERROR] = {
        {"toString", "String"},
        {"toNumber", "Number"},
        {"toInt", "ParseInt"},
        {"toLink", "Link"},
    };

    // built-in variables

    std::unordered_map<std::string, Value> justcProperties;
    justcProperties["version"] = Value::createString(JUSTC_VERSION);
    justcProperties["parse"] = builtinObjectFunction("JUSTC.parse");
    justcProperties["execute"] = builtinObjectFunction("JUSTC.execute");
    justcProperties["stringify"] = builtinObjectFunction("JUSTC.stringify");
    justcProperties["parser"] = builtinObjectFunction("JUSTC.parser");
    justcProperties["lexer"] = builtinObjectFunction("JUSTC.lexer");
    builtinObject("JUSTC", justcProperties);

    std::unordered_map<std::string, Value> jsonProperties;
    jsonProperties["parse"] = builtinObjectFunction("JSON.parse");
    jsonProperties["stringify"] = builtinObjectFunction("JSON.stringify");
    builtinObject("JSON", jsonProperties);

    std::unordered_map<std::string, Value> jsProperties;
    jsProperties["execute"] = builtinObjectFunction("JavaScript.execute");
    jsProperties["available"] = booleanToValue(
        #ifdef _MSC_VER
            false
        #else
            doExecute
        #endif
    );
    jsProperties["isAllowed"] = builtinObjectFunction("JavaScript.isAllowed");
    jsProperties["canAllow"] = booleanToValue(canAllowJS);
    builtinObject("JavaScript", jsProperties);

    std::unordered_map<std::string, Value> luauProperties;
    luauProperties["execute"] = builtinObjectFunction("Luau.execute");
    luauProperties["compile"] = builtinObjectFunction("Luau.compile");
    luauProperties["available"] = booleanToValue(
        #ifdef JUSTC_NOLUAU
            false
        #else
            doExecute
        #endif
    );
    luauProperties["isAllowed"] = builtinObjectFunction("Luau.isAllowed");
    luauProperties["canAllow"] = booleanToValue(canAllowLuau);
    builtinObject("Luau", luauProperties);

    std::unordered_map<std::string, Value> justoProperties;
    justoProperties["version"] = Value::createString(JUSTC_VERSION);
    justoProperties["parse"] = builtinObjectFunction("JUSTO.parse");
    justoProperties["stringify"] = builtinObjectFunction("JUSTO.stringify");
    builtinObject("JUSTO", justoProperties);

    std::unordered_map<std::string, Value> mathProperties;
    mathProperties["abs"]       = builtinObjectFunction("math.abs");
    mathProperties["acos"]      = builtinObjectFunction("math.acos");
    mathProperties["asin"]      = builtinObjectFunction("math.asin");
    mathProperties["atan"]      = builtinObjectFunction("math.atan");
    mathProperties["atan2"]     = builtinObjectFunction("math.atan2");
    mathProperties["ceil"]      = builtinObjectFunction("math.ceil");
    mathProperties["cos"]       = builtinObjectFunction("math.cos");
    mathProperties["clamp"]     = builtinObjectFunction("math.clamp");
    mathProperties["cube"]      = builtinObjectFunction("math.cube");
    mathProperties["double"]    = builtinObjectFunction("math.double");
    mathProperties["e"]         = numberToValue(Math::E);
    mathProperties["exp"]       = builtinObjectFunction("math.exp");
    mathProperties["factorial"] = builtinObjectFunction("math.factorial");
    mathProperties["floor"]     = builtinObjectFunction("math.floor");
    mathProperties["hypot"]     = builtinObjectFunction("math.hypot");
    mathProperties["isPrime"]   = builtinObjectFunction("math.isPrime");
    mathProperties["lerp"]      = builtinObjectFunction("math.lerp");
    mathProperties["ln2"]       = numberToValue(Math::LN2);
    mathProperties["ln10"]      = numberToValue(Math::LN10);
    mathProperties["log"]       = builtinObjectFunction("math.log");
    mathProperties["log10"]     = builtinObjectFunction("math.log10");
    mathProperties["log2e"]     = numberToValue(Math::LOG2E);
    mathProperties["log10e"]    = numberToValue(Math::LOG10E);
    mathProperties["max"]       = builtinObjectFunction("math.max");
    mathProperties["min"]       = builtinObjectFunction("math.min");
    mathProperties["pi"]        = numberToValue(Math::PI);
    mathProperties["pow"]       = builtinObjectFunction("math.pow");
    mathProperties["random"]    = builtinObjectFunction("math.random");
    mathProperties["round"]     = builtinObjectFunction("math.round");
    mathProperties["sign"]      = builtinObjectFunction("math.sign");
    mathProperties["sin"]       = builtinObjectFunction("math.sin");
    mathProperties["sqrt"]      = builtinObjectFunction("math.sqrt");
    mathProperties["sqrt1_2"]   = numberToValue(Math::SQRT1_2);
    mathProperties["sqrt2"]     = numberToValue(Math::SQRT2);
    mathProperties["square"]    = builtinObjectFunction("math.square");
    mathProperties["tan"]       = builtinObjectFunction("math.tan");
    mathProperties["toDegrees"] = builtinObjectFunction("math.toDegrees");
    mathProperties["toRadians"] = builtinObjectFunction("math.toRadians");
    builtinObject("math", mathProperties);

    std::unordered_map<std::string, Value> httpProperties;
    httpProperties["GET"]     = builtinObjectFunction("HTTP.GET");
    httpProperties["POST"]    = builtinObjectFunction("HTTP.POST");
    httpProperties["PUT"]     = builtinObjectFunction("HTTP.PUT");
    httpProperties["PATCH"]   = builtinObjectFunction("HTTP.PATCH");
    httpProperties["DELETE"]  = builtinObjectFunction("HTTP.DELETE");
    httpProperties["HEAD"]    = builtinObjectFunction("HTTP.HEAD");
    httpProperties["OPTIONS"] = builtinObjectFunction("HTTP.OPTIONS");
    builtinObject("HTTP", httpProperties);

    std::unordered_map<std::string, Value> scriptProperties;
    scriptProperties["name"] = stringToValue(scriptName);
    scriptProperties["type"] = stringToValue(scriptType);
    std::string runner =
        #ifdef __EMSCRIPTEN__
            "WebAssembly"
        #elif defined(_WIN64)
            "Windows x64"
        #elif defined(_WIN32)
            "Windows x32"
        #elif defined(__APPLE__) && defined(__MACH__)
            #ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
                "iOS"
            #elif defined(__TV_OS_VERSION_MIN_REQUIRED)
                "tvOS"
            #else
                "macOS"
            #endif
        #elif defined(__ANDROID__)
            "Android"
        #elif defined(__FreeBSD__)
            "FreeBSD"
        #elif defined(__linux__)
            "Linux"
        #elif defined(__unix__) || defined(__unix)
            "Unix/POSIX"
        #else
            "unknown"
        #endif
        ;
        #if defined(__x86_64__) || defined(_M_X64)
            runner += " x86_64";
        #elif defined(__i386__) || defined(_M_IX86)
            runner += " x86";
        #elif defined(__aarch64__) || defined(_M_ARM64)
            runner += " ARM64";
        #elif defined(__arm__) || defined(_M_ARM)
            runner += " ARM";
        #endif
    scriptProperties["runner"] = stringToValue(runner);
    scriptProperties["int128"] = booleanToValue(JUSTC_INT128_SUPPORT);
    scriptProperties["uint128"] = booleanToValue(JUSTC_UINT128_SUPPORT);
    scriptProperties["float128"] = booleanToValue(JUSTC_FLOAT128_SUPPORT);
    builtinObject("script", scriptProperties);

    Value chartypeValue;
    chartypeValue.type = DataType::STRING;
    switch (chartype) {
        case CharType::GRAPHEME:
            chartypeValue.string_value = "grapheme";
            break;
        case CharType::CODEPOINT:
            chartypeValue.string_value = "codepoint";
            break;
        case CharType::BYTE:
            chartypeValue.string_value = "byte";
            break;
    }
    chartypeValue.name = "charType";
    variables["charType"] = chartypeValue;
    constVars["charType"] = false;

    std::unordered_map<std::string, Value> taskProperties;
    taskProperties["sleep"] = builtinObjectFunction("task.sleep");
    taskProperties["wait"] = builtinObjectFunction("task.wait");
    taskProperties["race"] = builtinObjectFunction("task.race");
    builtinObject("task", taskProperties);

    std::unordered_map<std::string, Value> systemProperties;
    systemProperties["env"] = builtinObjectFunction("system.env");
    builtinObject("system", systemProperties);

    std::unordered_map<std::string, Value> memoryProperties;
    memoryProperties["global"] = builtinObjectFunction("memory.global");
    memoryProperties["classes"] = builtinObjectFunction("memory.classes");
    memoryProperties["functions"] = builtinObjectFunction("memory.functions");
    memoryProperties["pointers"] = builtinObjectFunction("memory.pointers");
    builtinObject("memory", memoryProperties);

    builtinClasses();
}

std::string Parser::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::tm timeinfo;

    #ifdef _WIN32
        localtime_s(&timeinfo, &time_t);
    #else
        localtime_r(&time_t, &timeinfo);  // POSIX (Linux/macOS/Emscripten)
    #endif

    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return std::string(buffer);
}

// logs
void Parser::addLog(const std::string& type, const std::string& message, size_t position) {
    std::string time = getCurrentTimestamp();
    logs.push_back({type, message, position, time});
    if (hasLogFile && type == "LOG") {
        appendToLogFile("[" + time + "] " + message);
    }
}
void Parser::setLogFile(const std::string& path) {
    logFilePath = path;
    hasLogFile = true;
}
void Parser::appendToLogFile(const std::string& content) {
    logFileContent += content + "\n";
}
void Parser::addImportLog(const std::string& path, const std::string& script, const std::string& type) {
    std::vector<std::string> log;
    log.push_back(path);
    log.push_back(script);
    log.push_back(type);
    importLogs.push_back(log);
}

ParserToken Parser::currentToken() const {
    if (position >= tokens.size()) {
        return {"EOF", "", 0};
    }
    return tokens[position];
}

ParserToken Parser::peekToken(size_t offset) const {
    if (position + offset >= tokens.size()) {
        return {"EOF", "", 0};
    }
    return tokens[position + offset];
}

void Parser::advance() {
    if (position < tokens.size()) {
        position++;
    }
}

bool Parser::match(const std::string& type) const {
    return currentToken().type == type;
}

bool Parser::match(const std::string& type, const std::string& value) const {
    return currentToken().type == type && currentToken().value == value;
}

bool Parser::isEnd() const {
    return position >= tokens.size();
}

void Parser::skipCommas() {
    while (match(",") || match(";")) advance();
}

bool Parser::isInBracketedExpression() {
    if (position >= tokens.size()) return false;
    
    size_t currentPos = position;
    int parenCount = 0;
    int bracketCount = 0;
    int braceCount = 0;
    
    while (currentPos < tokens.size()) {
        const auto& token = tokens[currentPos];
        
        if ((token.type == ";" || token.type == "," || token.type == ".") && 
            parenCount == 0 && bracketCount == 0 && braceCount == 0) {
            if (currentPos > 0) {
                const auto& prevToken = tokens[currentPos - 1];
                if (prevToken.type == ")" || prevToken.type == "]" || prevToken.type == "}") {
                    return true;
                }
            }
            return false;
        }
        
        if (token.type == "(") parenCount++;
        else if (token.type == ")") parenCount--;
        else if (token.type == "[") bracketCount++;
        else if (token.type == "]") bracketCount--;
        else if (token.type == "{") braceCount++;
        else if (token.type == "}") braceCount--;
        
        if (token.type == "EOF") break;
        
        currentPos++;
    }
    
    return false;
}

ParseResult Parser::parse(bool doExecute) {
    ParseResult result;

    result.variables = std::make_shared<std::unordered_map<std::string, Value>>(variables);
    result.constants = std::make_shared<std::unordered_map<std::string, bool>>(constVars);
    result.dependencies = std::make_shared<std::unordered_map<std::string, std::vector<std::string>>>(dependencies);

    try {
        while (!isEnd()) {
            skipCommas();
            if (isEnd()) break;

            if ((match("{") || match("[")) && position == 0) {
                if (match("[")) {
                    isJSONArray = true;
                    result.array = true;
                    endOfScript = "]";
                } else {
                    endOfScript = "}";
                }
                advance();
                asJSON = true;
            } else if (match("keyword")) {
                std::string keyword = currentToken().value;

                if (keyword == "scope") {
                    ast.push_back(parseScopeCommand());
                } else if (keyword == "output") {
                    ast.push_back(parseOutputCommand());
                } else if (keyword == "return") {
                    ast.push_back(parseReturnCommand());
                } else if (keyword == "allow" || keyword == "disallow") {
                    ast.push_back(parseAllowCommand());
                } else if (keyword == "import") {
                    ast.push_back(parseImportCommand());
                } else if (keyword == "if" || keyword == "while" || keyword == "for" || keyword == "try" || (
                    keyword == "isolated" && peekToken().type == "keyword" && (
                        peekToken().value == "if" || peekToken().value == "while" || peekToken().value == "for" || peekToken().value == "try"
                    )
                )) {
                    Value result = parseCondition(doExecute);
                    ASTNode output("CONDITION", "", currentToken().start);
                    output.value = result;
                    ast.push_back(output);
                } else if (keyword == "throw") {
                    ASTNode output("EXCEPTION", "", currentToken().start);
                    advance();
                    Value result = parseExpression(doExecute);
                    output.value = result;
                    ast.push_back(output);
                    if (throwError) throw std::runtime_error(result.toString());
                    else break;
                } else {
                    ast.push_back(parseStatement(doExecute));
                }
            } else if ((match("identifier") || ((match("string") || match("number") || match("string_i") || match("string_ri")) && !isJSONArray)) && !isInBracketedExpression()) {
                std::string identifier = currentToken().value;
                bool isIdentifier = true;
                size_t originalPos = position;

                if (match("string") || match("number") || match("string_i") || match("string_ri")) {
                    isIdentifier = false;
                    Value exprValue = parseExpression(doExecute, true);
                    identifier = exprValue.toString();

                    ParserToken parsedToken = {"string", identifier, currentToken().start};

                    std::vector<ParserToken> newTokens;
                    for (size_t i = 0; i < originalPos; i++) {
                        newTokens.push_back(tokens[i]);
                    }
                    newTokens.push_back(parsedToken);
                    for (size_t i = position; i < tokens.size(); i++) {
                        newTokens.push_back(tokens[i]);
                    }

                    tokens = newTokens;

                    position = originalPos;
                } else if (doExecute && match(":")) {
                    advance();
                    Value var = resolveVariableValue(identifier, false);
                    auto it = typeMethods.find(var.type);

                    if (var.type != DataType::UNKNOWN && it != typeMethods.end()) {
                        std::string funcName = (match("identifier") ? getIdentifier() : parseExpression(doExecute, true, false)).toIdentifier();
                        auto itFunc = typeMethods[var.type].find(funcName);

                        if (itFunc != typeMethods[var.type].end() && match("(")) {
                            checkVariableNameAvailable(identifier);

                            std::vector<Value> args = {var};
                            std::vector<Value> additionalArgs = parseArguments(doExecute);
                            args.reserve(args.size() + additionalArgs.size());
                            args.insert(args.end(), additionalArgs.begin(), additionalArgs.end());
                            Value result = executeFunction(typeMethods[var.type][funcName], args, currentToken().start);

                            ASTNode node("VARIABLE_DECLARATION", var.isVariable ? var.variable : result.name, currentToken().start);
                            node.value = result;
                            if (var.isVariable) assign(var, result, " at " + Utility::position(currentToken().start, input) + ".");
                            else variables[result.name] = result;

                            ast.push_back(node);
                            skipCommas();
                            continue;
                        }
                    }
                    
                    position = originalPos;
                }

                if (isIdentifier && (identifier == "echo" || identifier == "log" || identifier == "logfile")) {
                    ast.push_back(parseCommand(doExecute));
                } else if (!isJSONArray) {
                    ast.push_back(parseStatement(doExecute));
                } else {
                    ASTNode item("ARRAY_ITEM", "", position);
                    item.value = Value::createString(identifier);
                    ast.push_back(item);
                    arrayItems.push_back(item.value);
                }
            } else if (match(endOfScript)) {
                advance();
                if (!isEnd()) {
                    throw std::runtime_error("After end of script - Unexpected token \"" + currentToken().value + "\" at " + Utility::position(currentToken().start, input) + ".");
                }
                break;
            } else if (match("JavaScript") || match("JavaScript_i")) {
                bool interpolation = match("JavaScript_i");
                if (doExecute && allowJavaScript) {
                    std::string code = interpolation ? parseStringInterpolation(doExecute) : currentToken().value;
                    #ifdef __EMSCRIPTEN__

                    Value result = runJavaScript(code, Utility::position(currentToken().start, input), false);
                    addLog("JAVASCRIPT", Utility::value2string(result), position);
                    if (result.type != DataType::NULL_TYPE) {
                        std::cout << Utility::value2string(result) << std::endl;
                    }

                    #elif !defined(_MSC_VER)

                    std::pair<std::string, bool> jsresult = JavaScript::Eval(code);
                    if (jsresult.second) {
                        throw std::runtime_error("JavaScript error at " + Utility::position(currentToken().start, input) + ":\n" + jsresult.first);
                    } else {
                        addLog("JAVASCRIPT", jsresult.first, position);
                        std::cout << jsresult.first << std::endl;
                    }

                    #endif
                } else if (!allowJavaScript) {
                    #ifdef __EMSCRIPTEN__
                    warn_js_disabled_by_justc(Utility::position(currentToken().start, input).c_str(), currentToken().value.c_str(), getCurrentTimestamp().c_str());
                    #endif
                }
                ast.push_back(ASTNode("JAVASCRIPT"));
                if (!interpolation) advance();
            } else if (match("Luau") || match("Luau_i")) {
                #ifndef JUSTC_NOLUAU
                    bool interpolation = match("Luau_i");
                    if (doExecute && allowLuau) {
                        RunLuau::runScript(interpolation ? parseStringInterpolation(doExecute) : currentToken().value);
                    } else if (!allowLuau) {
                        #ifdef __EMSCRIPTEN__
                        warn_luau_disabled_by_justc(Utility::position(currentToken().start, input).c_str(), currentToken().value.c_str(), getCurrentTimestamp().c_str());
                        #endif
                    }
                    ast.push_back(ASTNode("LUAU"));
                    if (!interpolation) advance();
                #else
                    throw std::runtime_error("To run Luau, use the standard JUSTC build. The current build excludes Luau.");
                #endif
            } else if (match("\\")) {
                advance();
            } else if (isJSONArray) {
                try {
                    Value itemVal = parseBitwiseOR(doExecute);
                    ASTNode item("ARRAY_ITEM", "", position);
                    item.value = itemVal;
                    ast.push_back(item);
                    arrayItems.push_back(itemVal);
                } catch (...) {
                    throw std::runtime_error("Unexpected token \"" + currentToken().value + "\" at " + Utility::position(currentToken().start, input) + ".");
                }
            } else if (position == 0 || (
                tokens[position - 1].type == "," || tokens[position - 1].type == ";"
            )) {
                try {
                    parseExpression(doExecute);
                } catch (...) {
                    throw std::runtime_error("Unexpected token \"" + currentToken().value + "\" at " + Utility::position(currentToken().start, input) + ".");
                }
            } else if (match("(")) {
                parseExpression(doExecute);
            } else throw std::runtime_error("Unexpected token \"" + currentToken().value + "\" at " + Utility::position(currentToken().start, input) + ".");

            skipCommas();
        }
        position -= 1;

        buildDependencyGraph();

        if (detectCycles()) {
            throw std::runtime_error("Circular dependency detected");
        }

        evaluateAllVariables();
        removeBuiltinVariablesFromOutput();
        removeStructsFromOutput();
        removeClassesFromOutput();
        finalizeOutput();

        if (isJSONArray) {
            for (size_t i = 0; i < arrayItems.size(); i++) {
                Value itemVal = arrayItems[i];
                if (itemVal.type == DataType::VARIABLE) {
                    itemVal = resolveVariableValue(itemVal.string_value, true);
                }
                result.returnValues[std::to_string(i)] = convertToDecimal(itemVal);
            }
        } else {
            bool done = false;
            if (outputMode == "specified") {
                if (returnValue.type == DataType::UNKNOWN && !outputVariables.empty()) {
                    for (const auto& varName : outputVariables) {
                        auto it = variables.find(varName);
                        if (it != variables.end()) {
                            size_t index = &varName - &outputVariables[0];
                            std::string outputName = (index < outputNames.size()) ? outputNames[index] : varName;
                            if (outputName != "_") {
                                result.returnValues[outputName] = convertToDecimal(it->second);
                            } else {
                                result.returnValues[varName] = convertToDecimal(it->second);
                            }
                        }
                    }
                } else if (returnValue.type != DataType::UNKNOWN) {
                    Value finalValue = returnValue;

                    if (finalValue.type == DataType::VARIABLE) {
                        finalValue = resolveVariableValue(finalValue.string_value, true);
                    }

                    if (finalValue.type == DataType::JUSTC_OBJECT ||
                        finalValue.type == DataType::JSON_OBJECT) {
                        for (const auto& [key, val] : finalValue.properties) {
                            result.returnValues[key] = convertToDecimal(v(val));
                        }
                    } else if (finalValue.type == DataType::JSON_ARRAY) {
                        for (size_t i = 0; i < finalValue.array_elements.size(); i++) {
                            result.returnValues[std::to_string(i)] = convertToDecimal(finalValue.array_elements[i]);
                        }
                    } else {
                        result.returnValues["return"] = convertToDecimal(finalValue);
                        done = true;
                    }
                }
            } else if (outputMode == "everything") {
                if (returnValue.type != DataType::UNKNOWN || !outputVariables.empty()) {
                    throw std::runtime_error("Got \"return\" command with output mode \"everything\". Output mode \"everything\" returns every variable without \"return\" command.");
                }
                for (const auto& pair : variables) {
                    result.returnValues[pair.first] = convertToDecimal(pair.second);
                }
            } else if (outputMode == "disabled") {
                if (returnValue.type != DataType::UNKNOWN || !outputVariables.empty()) {
                    throw std::runtime_error("Cannot return anything with output mode \"disabled\".");
                }
                if (isFunction) {
                    result.returnValues["return"] = Value::createNull();
                    done = true;
                }
            }
            if (isFunction && !done) {
                Value returnObject = Value::createJsonObject(result.returnValues);
                result.returnValues.clear();
                result.returnValues["return"] = returnObject;
            }
        }

        result.logs = logs;
        result.logFilePath = hasLogFile ? logFilePath : "";
        result.logFileContent = hasLogFile ? logFileContent : "";
        result.importLogs = importLogs;

        if (parsertype == ParserType::CLASS) {
            std::unordered_map<std::string, Value> oldReturnValues = result.returnValues;
            std::unordered_map<std::string, Value> newReturnValues;
            newReturnValues["constructor"] = resolveVariableValue("constructor", false);
            newReturnValues["destructor"] = resolveVariableValue("destructor", true);
            newReturnValues["static"] = Value::createJsonObject(staticValues);
            newReturnValues["instance"] = Value::createJsonObject(oldReturnValues);
            result.returnValues = newReturnValues;
        }
    } catch (const std::exception& e) {
        std::pair<size_t, size_t> pos = Utility::pos(currentToken().start, input);
        std::string err = std::string(e.what()) + "\n    at " + scriptName + ":" + std::to_string(pos.first) + ":" + std::to_string(pos.second);

        result.error = err;
        addLog("ERROR", err, currentToken().start);
    }

    if (throwError && !result.error.empty()) {
        throw std::runtime_error(result.error);
    }

    #ifndef __EMSCRIPTEN__
        if (hasWindow) {
            try {
                JUSTCWindow::RunMessageLoop({});
            } catch (...) {}
            hasWindow = false;
        }
    #endif

    return result;
}

Value Parser::convertToDecimal(const Value& value) {
    if (value.type == DataType::HEXADECIMAL ||
        value.type == DataType::BINARY ||
        value.type == DataType::OCTAL) {
        Value result;
        result.type = DataType::NUMBER;
        result.number_value = value.number_value;
        result.name = value.name;
        return result;
    }
    return value;
}

void Parser::parseScopeCommandError(const std::string scope) {
    throw std::runtime_error("Expected scope mode keyword, got \"" + scope + "\" at " + Utility::position(currentToken().start, input) + ". Scope mode keywords are: \"global\", \"local\", \"strict\".");
}
ASTNode Parser::parseScopeCommand() {
    ASTNode node("SCOPE_COMMAND", "", currentToken().start);
    advance();

    if (match("keyword")) {
        std::string type = currentToken().value;
        if (type == "global") {
            globalScope = true;
        } else if (type == "local") {
            globalScope = false;
        } else if (type == "strict") {
            strictMode = true;
        }
        node.value = stringToValue(type);
        advance();
    }

    return node;
}

void Parser::parseOutputCommandError(const std::string mode) {
    throw std::runtime_error("Expected output mode keyword, got \"" + mode + "\" at " + Utility::position(currentToken().start, input) + ". Output mode keywords are: \"specified\", \"everything\", \"disabled\".");
}
ASTNode Parser::parseOutputCommand() {
    ASTNode node("OUTPUT_COMMAND", "", currentToken().start);
    advance();

    if (match("keyword")) {
        std::string mode = currentToken().value;
        if (mode == "specified" || mode == "everything" || mode == "disabled") {
            outputMode = mode;
            node.value = stringToValue(outputMode);
            advance();
        } else {
            parseOutputCommandError(mode);
        }
    } else {
        parseOutputCommandError(currentToken().value);
    }

    return node;
}

ASTNode Parser::parseReturnCommand() {
    ASTNode node("RETURN_COMMAND", "", currentToken().start);
    advance();

    size_t exprStartPos = position;

    Value exprValue = parseExpression(doExecute);

    returnValue = exprValue;
    node.value = exprValue;

    if (outputMode == "everything") {
        outputMode = "specified";
    }

    return node;
}

void Parser::parseAllowCommandError() {
    throw std::runtime_error("Expected language name, got \"" + currentToken().value + "\" at " + Utility::position(currentToken().start, input) + ". Supported languages are: \"JavaScript\", \"Luau\".");
}
ASTNode Parser::parseAllowCommand() {
    ASTNode node("ALLOW_COMMAND", "", currentToken().start);
    std::string command = currentToken().value;
    advance();

    if (match("keyword", "JavaScript")) {
        if (!canAllowJS && command == "allow") {
            #ifdef __EMSCRIPTEN__
            warn_cant_enable_js(Utility::position(currentToken().start, input).c_str(), getCurrentTimestamp().c_str(), scriptName.c_str(), scriptType.c_str());
            #endif
            addLog("WARN", "Attempt to allow JavaScript at <import " + scriptType + " \"" + scriptName + "\"> at " + Utility::position(currentToken().start, input) + ".", currentToken().start);
        } else allowJavaScript = (command == "allow");
        node.value = booleanToValue(allowJavaScript);
    } else if (match("keyword", "Luau")) {
        if (!canAllowLuau && command == "allow") {
            #ifdef __EMSCRIPTEN__
            warn_cant_enable_luau(Utility::position(currentToken().start, input).c_str(), getCurrentTimestamp().c_str(), scriptName.c_str(), scriptType.c_str());
            #endif
            addLog("WARN", "Attempt to allow Luau at <import " + scriptType + " \"" + scriptName + "\"> at " + Utility::position(currentToken().start, input) + ".", currentToken().start);
        } else allowLuau = (command == "allow");
        node.value = booleanToValue(allowLuau);
    } else parseAllowCommandError();
    advance();

    return node;
}

std::string Parser::readVariableName() {
    std::stringstream name;
    while (!isEnd() && (match("identifier") || match("string") || match("minus") || match("-") || match("string_i") || match("string_ri"))) {
        name << ((match("string_i") || match("string_ri")) ? parseStringInterpolation(doExecute) : currentToken().value);
        advance();
    }
    return name.str();
}
void Parser::checkVariableNameAvailable(std::string name) {
    auto constIt = constVars.find(name);
    if (constIt != constVars.end() && constIt->second) {
        throw new std::runtime_error("Assignment to constant variable \"" + name + "\" at " + Utility::position(currentToken().start, input) + ".");
    }
}

ASTNode Parser::parseImportCommand() {
    ASTNode node("IMPORT_COMMAND", "", currentToken().start);
    advance();

    std::vector<std::string> imports;
    std::vector<std::string> renames;
    bool importAll = false;
    bool rename = false;
    bool single = false;

    if (match("identifier")) {
        imports.push_back(readVariableName());
        single = true;
    } else if (!match("keyword", "as") && !match("keyword", "from")) {
        Value exprValue = parseExpression(doExecute, true);
        switch (exprValue.type) {
            case DataType::JSON_OBJECT:
            case DataType::JUSTC_OBJECT: {
                rename = true;
                for (const auto& [key, value] : exprValue.properties) {
                    imports.push_back(v(value).toString());
                    renames.push_back(key);
                }
                break;
            }
            case DataType::JSON_ARRAY: {
                for (size_t i = 0; i < exprValue.array_elements.size(); i++) {
                    imports.push_back(exprValue.array_elements[i].toString());
                }
                break;
            }
            default:
                imports.push_back(exprValue.toString());
                break;
        }
    } else {
        importAll = true;
    }

    if (match("keyword", "as")) {
        single = false;
        if (rename) renames.clear();
        rename = true;
        if (match("identifier")) {
            renames.push_back(readVariableName());
        } else {
            Value exprValue = parseExpression(doExecute, true);
            switch (exprValue.type) {
                case DataType::JSON_ARRAY: {
                    for (size_t i = 0; i < exprValue.array_elements.size(); i++) {
                        imports.push_back(exprValue.array_elements[i].toString());
                    }
                    break;
                }
                default:
                    renames.push_back(exprValue.toString());
            }
        }
    }
    if (!match("keyword", "from")) throw new std::runtime_error("Expected keyword \"from\" at " + Utility::position(currentToken().start, input) + ".");
    advance();

    std::string importType = parseExpression(doExecute).toString();
    if (importType == "JUSTC") {
        int importStringType = 0; // 0 = link module; 1 = path module; 2 = string module; 3 = link script; 4 = path script; 5 = string script
        bool typeDeclared = false;
        if (match(":")) {
            typeDeclared = true;
            std::string typeDeclaration = parseExpression(doExecute).toString();
            if (typeDeclaration == "webmodule") {
                importStringType = 0;
            } else if (typeDeclaration == "module") {
                importStringType = 1;
            } else if (typeDeclaration == "strmodule") {
                importStringType = 2;
            } else if (typeDeclaration == "webscript") {
                importStringType = 3;
            } else if (typeDeclaration == "script") {
                importStringType = 4;
            } else if (typeDeclaration == "strscript") {
                importStringType = 5;
            } else {
                throw std::runtime_error("Invalid JUSTC import type \"" + typeDeclaration + "\" at " + Utility::position(currentToken().start, input) + ".");
            }
        }

        std::string location;
        Value locationVal = parseExpression(doExecute);
        location = locationVal.toString();
        if (!typeDeclared) switch (locationVal.type) {
            case DataType::LINK:
                importStringType = 0;
                break;
            default:
                importStringType = 1;
                break;
        };

        bool importExecute = doExecute;
        bool importJavaScript = doExecute && allowJavaScript;
        bool importLuau = doExecute && allowLuau;
        if (match("keyword", "options")) {
            advance();
            Value optionsVal = parseExpression(doExecute);
            if (optionsVal.type != DataType::JSON_OBJECT && optionsVal.type != DataType::JUSTC_OBJECT) {
                throw std::runtime_error("Expected object for import options at " + Utility::position(currentToken().start, input) + ".");
            }

            bool optionsExecute     = this->v(optionsVal.getProperty("Execute",   booleanToValue(importExecute))).toBoolean();
            bool optionsJavaScript  = this->v(optionsVal.getProperty("JavaScript",booleanToValue(importJavaScript))).toBoolean();
            bool optionsLuau        = this->v(optionsVal.getProperty("Luau",      booleanToValue(importLuau))).toBoolean();

            if (!importExecute && optionsExecute) {
                throw std::runtime_error("Attempt to execute JUSTC at " + Utility::position(currentToken().start, input) + ".");
            }
            if (!importJavaScript && optionsJavaScript) {
                throw std::runtime_error("Attempt to allow JavaScript at " + Utility::position(currentToken().start, input) + ".");
            }
            if (!importLuau && optionsLuau) {
                throw std::runtime_error("Attempt to allow Luau at " + Utility::position(currentToken().start, input) + ".");
            }

            importExecute = optionsExecute;
            importJavaScript = optionsJavaScript;
            importLuau = optionsLuau;
        }

        std::pair<ParseResult, std::string> imported;
        std::string importedType;
        bool mode; // true = "export", false = "return"
        bool isLink = false;
        bool isString = true;
        switch (importStringType) {
            case 0:
                isLink = true;
            case 1:
                isString = false;
            case 2:
                importedType = "module";
                mode = true;
                break;

            case 3:
                isLink = true;
            case 4:
                isString = false;
            case 5:
                importedType = "script";
                mode = false;
                break;

            default:
                throw std::runtime_error("Unknown JUSTC import type.");
        }
        try {
            imported = Import::JUSTC(location, Utility::position(currentToken().start, input), importExecute, runAsync, importJavaScript, mode, importLuau, isLink, isString);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string(e.what()) + "\n    at <import JUSTC " + importedType + " \"" + location + "\"> at " + Utility::position(currentToken().start, input) + ".");
        } catch (...) {
            throw std::runtime_error("Invalid import JUSTC \"" + location + "\" at " + Utility::position(currentToken().start, input) + ".");
        }

        addImportLog(location, imported.second, "JUSTC " + importedType);
        for (size_t i = 0; i < imported.first.importLogs.size(); i++) {
            std::vector<std::string> importLog = imported.first.importLogs[i];
            std::string _path = importLog[0];
            std::string _script = importLog[1];
            std::string _type = importLog[2];
            addImportLog(_path, _script, _type);
        }

        if (single) {
            std::string name = imports[0];
            checkVariableNameAvailable(name);

            auto objCtx = std::make_shared<ObjectContext>();
            std::vector<std::string> outputVars;
            for (const auto& [key, value] : imported.first.returnValues) {
                objCtx->variables[key] = value;
                outputVars.push_back(key);
            }
            objCtx->outputMode = "specified";
            objCtx->outputVariables = outputVars;
            Value objVal = Value::createJustcObject(objCtx);
            objVal.name = name;
            objVal.properties = pmap(imported.first.returnValues);
            objVal.type = DataType::JSON_OBJECT;

            ASTNode node("VARIABLE_DECLARATION", name, position);
            variables[name] = objVal;
            constVars[name] = true;
            node.value = objVal;
            ast.push_back(name);
        } else {
            size_t i = 0;
            for (const auto& pair : imported.first.returnValues) {
                std::string key = pair.first;

                auto constIt = constVars.find(key);
                if (constIt != constVars.end() && constIt->second) {
                    continue;
                }
                if (isBuiltinVariable(key)) {
                    continue;
                }
                if (!importAll) {
                    auto importIt = std::find(imports.begin(), imports.end(), key);
                    if (importIt == imports.end()) {
                        continue;
                    }
                }

                if (rename) {
                    ++i;
                    key = (i < renames.size()) ? renames[i] : key;
                }

                ASTNode node("VARIABLE_DECLARATION", key, position);
                variables[key] = pair.second;
                constVars[key] = true;
                node.value = pair.second;
                ast.push_back(node);
            }
        }
    } else if (importType == "JUSTO") {
        int importStringType = 0; // 0 = link; 1 = path; 2 = string
        bool typeDeclared = false;
        if (match(":")) {
            typeDeclared = true;
            std::string typeDeclaration = parseExpression(doExecute).toString();
            if (typeDeclaration == "web") {
                importStringType = 0;
            } else if (typeDeclaration == "file") {
                importStringType = 1;
            } else if (typeDeclaration == "string") {
                importStringType = 2;
            } else {
                throw std::runtime_error("Invalid JUSTO import type \"" + typeDeclaration + "\" at " + Utility::position(currentToken().start, input) + ".");
            }
        }

        std::string location;
        Value locationVal = parseExpression(doExecute);
        location = locationVal.toString();
        if (!typeDeclared) switch (locationVal.type) {
            case DataType::LINK:
                importStringType = 0;
                break;
            default:
                importStringType = 1;
                break;
        };

        std::unordered_map<std::string, Value> justoPointers;
        if (match("keyword", "options")) {
            advance();
            Value optionsVal = parseExpression(doExecute);
            if (optionsVal.type != DataType::JSON_OBJECT && optionsVal.type != DataType::JUSTC_OBJECT) {
                throw std::runtime_error("Expected object for import options at " + Utility::position(currentToken().start, input) + ".");
            }

            auto nanIt = optionsVal.properties.find("nan");
            auto infIt = optionsVal.properties.find("inf");
            if (nanIt != optionsVal.properties.end() || infIt != optionsVal.properties.end()) {
                throw std::runtime_error("Attempt to redefine built-in JUSTO pointer at " + Utility::position(currentToken().start, input) + ".");
            }

            justoPointers = vmap(optionsVal.properties);
        }

        std::pair<Value, std::string> imported;
        try {
            imported = Import::JUSTO(location, Utility::position(currentToken().start, input), importStringType == 0, importStringType == 2, justoPointers);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string(e.what()) + "\n    at <import JUSTO object \"" + location + "\"> at " + Utility::position(currentToken().start, input) + ".");
        } catch (...) {
            throw std::runtime_error("Invalid import JUSTO \"" + location + "\" at " + Utility::position(currentToken().start, input) + ".");
        }
        addImportLog(location, imported.second, "JUSTO object");
        if (single) {
            std::string name = imports[0];
            checkVariableNameAvailable(name);

            imported.first.name = name;
            ASTNode node("VARIABLE_DECLARATION", name, position);
            variables[name] = imported.first;
            constVars[name] = true;
            node.value = imported.first;
            ast.push_back(node);
        } else {
            size_t i = 0;
            for (const auto& [keyRaw, valueRaw] : imported.first.properties) {
                std::string key = keyRaw;
                const Value& value = v(valueRaw);

                auto constIt = constVars.find(key);
                if (constIt != constVars.end() && constIt->second) {
                    continue;
                }
                if (isBuiltinVariable(key)) {
                    continue;
                }
                if (!importAll) {
                    auto importIt = std::find(imports.begin(), imports.end(), key);
                    if (importIt == imports.end()) {
                        continue;
                    }
                }

                if (rename) {
                    ++i;
                    key = (i < renames.size()) ? renames[i] : key;
                }

                ASTNode node("VARIABLE_DECLARATION", key, position);
                variables[key] = value;
                constVars[key] = true;
                node.value = value;
                ast.push_back(node);
            }
        }
    } else {
        throw std::runtime_error("Cannot import from \"" + importType + "\" at " + Utility::position(currentToken().start, input) + ".");
    }

    return node;
}

ASTNode Parser::parseStatement(bool doExecute) {
    std::string keyword = currentToken().value;

    if (keyword == "function" || keyword == "isolated") {
        Value funcValue = parseFunctionDeclaration(doExecute);

        ASTNode node("VARIABLE_DECLARATION", funcValue.name, currentToken().start);
        node.value = funcValue;
        node.constant = true;

        variables[funcValue.name] = funcValue;
        constVars[funcValue.name] = true;

        return node;
    } else if (keyword == "struct" || keyword == "class" || keyword == "enum") {
        Value structVal = parseStructDeclaration(doExecute);

        ASTNode node("VARIABLE_DECLARATION", structVal.name, currentToken().start);
        node.value = structVal;
        node.constant = true;

        variables[structVal.name] = structVal;
        constVars[structVal.name] = true;

        return node;
    } else if (keyword == "echo" || keyword == "log" || keyword == "logfile") {
        return parseCommand(doExecute);
    } else if ((((match("identifier") || match("string") || match("string_i") || match("string_ri")) && (parsertype != ParserType::STRUCT && parsertype != ParserType::ENUM)) || isCPPType() || isStruct(currentToken().value).first) && !isJSONArray) {
        return parseVariableDeclaration(doExecute);
    } else if ((match("identifier") || match("string") || match("string_i") || match("string_ri")) && (parsertype == ParserType::STRUCT || parsertype == ParserType::ENUM)) {
        return parseVariableDeclaration(doExecute, true, true);
    } else if (match("keyword", "const") && !isJSONArray) {
        advance();
        bool isLocal = false;
        if (match("keyword", "global")) {
            advance();
            return parseGlobal(doExecute, true);
        } else if (match("keyword", "local")) {
            advance();
            isLocal = true;
        }
        return parseVariableDeclaration(doExecute, true, isLocal);
    } else if (match("keyword", "var") && !isJSONArray) {
        advance();
        bool isLocal = false;
        if (match("keyword", "global")) {
            advance();
            return parseGlobal(doExecute);
        } else if (match("keyword", "local")) {
            advance();
            isLocal = true;
        }
        return parseVariableDeclaration(doExecute, false, isLocal);
    } else if (match("keyword", "global") && !isJSONArray) {
        advance();
        bool isConst = false;
        if (match("keyword", "var")) advance();
        else if (match("keyword", "const")) {
            advance();
            isConst = true;
        }
        return parseGlobal(doExecute, isConst);
    } else if (match("keyword", "local") && !isJSONArray) {
        advance();
        bool isConst = false;
        if (match("keyword", "var")) advance();
        else if (match("keyword", "const")) {
            advance();
            isConst = true;
        }
        return parseVariableDeclaration(doExecute, isConst, true);
    } else if (match("keyword", "set") && !isJSONArray) {
        advance();
        ASTNode set("SET", currentToken().value, currentToken().start);
        Value newValue = parseObjectPropertyAccess(doExecute, true);
        set.value = newValue;
        set.identifier = newValue.name;
        return set;
    } else {
        return parseCommand(doExecute);
    }
}
ASTNode Parser::parseGlobal(bool doExecute, bool constant) {
    ASTNode global("GLOBAL", currentToken().value, currentToken().start);
    if (match("keyword", "function") || match("keyword", "isolated")) {
        Value funcValue = parseFunctionDeclaration(doExecute);
        global.value = funcValue;
        global.identifier = funcValue.name;
        global.constant = constant;
    } else if (match("keyword", "struct") || match("keyword", "class") || match("keyword", "enum")) {
        Value structVal = parseStructDeclaration(doExecute);
        global.value = structVal;
        global.identifier = structVal.name;
        global.constant = constant;
    } else {
        global = parseVariableDeclaration(doExecute, constant, false, true);
    }
    global.type = "GLOBAL";
    registerGlobal(global.identifier, global.value, constant, true);
    return global;
}

bool Parser::CanIgnoreNoAssignmentOperator() {
    return (match("string") || match("number") || match("null") || match("path") || match("link") ||
            match("hex") || match("binary") || match("boolean") || match("identifier") || match("|") ||
            match("JavaScript") || match("Luau") || match(endOfScript) || match(".") || match(",") ||
            match("{") || match("[") || match(";") || match("string_i") || match("string_ri") ||
            match("Luau_i") || match("JavaScript_i") || match("JUSTC") || match("JUSTO") || 
            match("JUSTC_i") || match("JUSTO_i"));
}
ASTNode Parser::parseVariableDeclaration(bool doExecute, bool constant, bool local, bool global) {
    std::string cpptype = DEFAULT_CPP_TYPE;
    if (isCPPType() || isStruct(currentToken().value).first || isClass(currentToken().value).first) {
        cpptype = currentToken().value;
        advance();
    }

    std::string identifier = currentToken().value;
    ASTNode node("VARIABLE_DECLARATION", identifier, currentToken().start);
    node.constant = constant;
    node.local = local;
    advance();

    // handle dashes in variable names
    if (match("-") || match("minus")) {
        size_t originalPos = position;
        size_t lookaheadPos = position;
        std::string potentialIdentifier = identifier;
        int runs = 0;
        bool isVarWithDashes = false;
        size_t tokensConsumed = 0;

        while (lookaheadPos < tokens.size() &&
            (tokens[lookaheadPos].type == "minus" || tokens[lookaheadPos].value == "-") &&
            runs < 128) {
            runs++;

            if (lookaheadPos + 1 < tokens.size() && tokens[lookaheadPos + 1].type == "identifier") {
                std::string nextType = tokens[lookaheadPos].type;
                std::string nextValue = tokens[lookaheadPos].value;

                if (nextType == "=" || nextType == ":" ||
                    (nextType == "keyword" && (nextValue == "is" || nextValue == "isn't" || nextValue == "isif")) ||
                    nextValue == "?" || nextValue == "!=") {
                    isVarWithDashes = false;
                    break;
                }
                else if (nextType == "minus" || nextValue == "-") {
                    potentialIdentifier += "-" + tokens[lookaheadPos + 1].value;
                    lookaheadPos += 2;
                    tokensConsumed += 2;
                    continue;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        if (tokensConsumed > 0) {
            isVarWithDashes = true;
            identifier = potentialIdentifier;
            node.identifier = identifier;

            for (size_t i = 0; i < tokensConsumed; i++) {
                advance();
            }
        } else {
            position = originalPos;
        }
    }

    checkVariableNameAvailable(identifier);

    std::string assignOp;
    std::string typeDecl;
    if (match(":")) {
        advance();
        typeDecl = currentToken().value;
        bool interpolation = match("string_i") || match("string_ri");
        if (!match("identifier") && !match("string") && !match("link") && !match("string_i") && !match("string_ri")) {
            // then `:` and `=` are the same
            Value exprValue = applyCPPTypeDeclaration(parseExpression(doExecute), cpptype, DataType::UNKNOWN, doExecute);
            node.value = exprValue;
            extractReferences(exprValue, node.references);

            if (variables.find(identifier) != variables.end()) {
                variables[identifier] = node.value;
            } else {
                variables[identifier] = node.value;
                constVars[identifier] = constant;
            }

            if (doExecute && isBuiltinVariable(identifier)) {
                handleBuiltinVariableAssignment(identifier, exprValue, currentToken().start);
            }

            return node;
        }
        try {
            node.typeDeclaration = Utility::typeDeclaration2dataType(interpolation ? parseStringInterpolation(doExecute) : typeDecl, Utility::position(currentToken().start, input) + ".");
        } catch (...) {
            // then `:` and `=` are the same
            Value exprValue = applyCPPTypeDeclaration(parseExpression(doExecute), cpptype, DataType::UNKNOWN, doExecute);
            node.value = exprValue;
            extractReferences(exprValue, node.references);

            if (variables.find(identifier) != variables.end()) {
                variables[identifier] = node.value;
            } else {
                variables[identifier] = node.value;
                constVars[identifier] = constant;
            }

            if (doExecute && isBuiltinVariable(identifier)) {
                handleBuiltinVariableAssignment(identifier, exprValue, currentToken().start);
            }

            return node;
        }
        if (!interpolation) advance();
    }

    if (match("keyword", "is") || match("=") || match("-") || match("minus")) {
        assignOp = currentToken().value;
        advance();

        Value exprValue = applyCPPTypeDeclaration(parseExpression(doExecute), cpptype, node.typeDeclaration, doExecute);
        node.value = exprValue;
        extractReferences(exprValue, node.references);
    }
    else if (match("keyword", "isn't") || match("!=")) {
        assignOp = currentToken().value;
        advance();

        Value exprValue = applyCPPTypeDeclaration(parseExpression(doExecute), cpptype, node.typeDeclaration, doExecute);
        exprValue = handleInequality(exprValue);
        node.value = exprValue;
        extractReferences(exprValue, node.references);
    }
    else if (match("keyword", "isif") || match("?")) {
        advance();
        Value conditionalValue = parseConditional(doExecute);
        node.value = conditionalValue;
        extractReferences(conditionalValue, node.references);
    }
    else if (position >= 2 && (
        tokens[position - 2].value == "echo" ||
        tokens[position - 2].value == "log"  ||
        tokens[position - 2].value == "logfile"
    )) {
        position -= 2;
        parseCommand(doExecute);
    }
    else if (match("--") || match("++") || match("#") || match("!") || match("~") || match("$")) { // unary assignment
        Value var = resolveVariableValue(identifier, false);
        if (var.type == DataType::UNKNOWN) throw std::runtime_error("Assignment to undefined variable at " + Utility::position(currentToken().start, input) + ".");
        
        Value val = var;
        if (match("--") || match("++")) {
            val = Value::createNumber(var.toNumber() + (
                match("++") ? 1 : -1
            ));
        } else if (match("#")) {
            val = evaluateLengthOperator(var);
        } else if (match("!")) {
            val = booleanToValue(!var.toBoolean());
        } else if (match("~")) {
            if (Utility::checkNumber(var)) {
                int num = static_cast<int>(var.toNumber());
                val = numberToValue(~num);
            } else if (var.type == DataType::STRING) {
                val = stringToValue(Utility::stringNot(var.toString()));
            } else {
                throw std::runtime_error("Expected number or string for bitwise NOT operation at " + Utility::position(currentToken().start, input) + ".");
            }
        } else if (match("$")) {
            val = evaluateNameOperator(var);
        }
        
        advance();
        node.value = val;
        if (var.isVariable) assign(var, val, " at " + Utility::position(currentToken().start, input) + ".");
    }
    else {
        if (isEnd()) {
            throw std::runtime_error("Expected assignment operator at " + Utility::position(currentToken().start, input) + ", got EOF.");
        } else if (CanIgnoreNoAssignmentOperator()) {
            Value exprValue = parsertype == ParserType::ENUM ? Value::createNumber(static_cast<double>(nextIndex++)) : applyCPPTypeDeclaration(parseExpression(doExecute), cpptype, node.typeDeclaration, doExecute, true);
            node.value = exprValue;
            extractReferences(exprValue, node.references);
        } else throw std::runtime_error("Expected assignment operator at " + Utility::position(currentToken().start, input) + ", got \"" + currentToken().value +"\".");
    }

    if (node.value.type == DataType::FUNCTION) {
        if (userFunctions.find(identifier) != userFunctions.end() && userFunctionsConst.find(identifier)->second) {
            throw std::runtime_error("Assignment to constant function \"" + identifier + "\" at " + Utility::position(currentToken().start, input) + ".");
        }
        try {
            userFunctions.erase(identifier);
        } catch (...) {}
    }

    if (node.local) {
        if (node.value.type != DataType::UNKNOWN) {
            setLocal(currentScope, identifier, node.value, constant);
        }
        
        if (variables.find(identifier) != variables.end()) {
            variables[identifier] = node.value;
        } else {
            variables[identifier] = node.value;
            constVars[identifier] = constant;
        }
    } else {
        if (variables.find(identifier) != variables.end()) {
            variables[identifier] = node.value;
        } else {
            variables[identifier] = node.value;
            constVars[identifier] = constant;
        }
        
        if (node.value.type != DataType::UNKNOWN) {
            setLocal(rootIndex, identifier, node.value, constant);
        }
    }

    if (doExecute && isBuiltinVariable(identifier)) {
        handleBuiltinVariableAssignment(identifier, node.value, currentToken().start);
    }

    return node;
}

Value Parser::parseExpression(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    if (match("keyword", "function") || match("keyword", "isolated") || match("keyword", "struct") || match("keyword", "class")) {
        std::string funcName = std::to_string(position);
        bool gotName = false;
        size_t offset = 1;
        while (!gotName && (
            position - offset >= 0
        )) {
            ParserToken currToken = tokens[position - offset];
            if (currToken.type == "identifier") {
                gotName = true;
                funcName = currToken.value;
            }
            ++offset;
        }
        if (match("keyword", "struct") || match("keyword", "class") || match("keyword", "enum")) return parseStructDeclaration(doExecute, funcName, false);
        return parseFunctionDeclaration(doExecute, funcName, false);
    }
    Value result = parseConditional(doExecute, identifierMode, doFunctionCall, ignoreColon);

    if (match("--") || match("++") || match("#") || match("!") || match("~") || match("$")) { // unary assignment
        Value val = result;
        if (match("--") || match("++")) {
            val = Value::createNumber(result.toNumber() + (
                match("++") ? 1 : -1
            ));
        } else if (match("#")) {
            val = evaluateLengthOperator(result);
        } else if (match("!")) {
            val = booleanToValue(!result.toBoolean());
        } else if (match("~")) {
            if (Utility::checkNumber(result)) {
                int num = static_cast<int>(result.toNumber());
                val = numberToValue(~num);
            } else if (result.type == DataType::STRING) {
                val = stringToValue(Utility::stringNot(result.toString()));
            } else {
                throw std::runtime_error("Expected number or string for bitwise NOT operation at " + Utility::position(currentToken().start, input) + ".");
            }
        } else if (match("$")) {
            val = evaluateNameOperator(result);
        }
        if (result.isVariable) assign(result, val, " at " + Utility::position(currentToken().start, input) + ".");
    }

    return result;
}

Value Parser::parseConditional(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value condition = parseBitwiseOR(doExecute, identifierMode, doFunctionCall, ignoreColon);

    if (!identifierMode) {
        if (match("keyword", "then") || match("?")) {
            std::string thenOp = currentToken().value;
            advance();

            Value thenValue = parseExpression(doExecute, identifierMode, doFunctionCall, true);

            if (match("keyword", "else") || match(":")) {
                std::string elseOp = currentToken().value;
                advance();

                Value elseValue = parseExpression(doExecute, identifierMode, doFunctionCall, ignoreColon);

                return handleConditional(condition, thenValue, elseValue, thenOp, elseOp);
            } else {
                throw std::runtime_error("Expected 'else' after 'then'");
            }
        }

        if (match("keyword", "elseif")) {
            std::string elseifOp = currentToken().value;
            advance();

            Value elseifCondition = parseExpression(doExecute, identifierMode, doFunctionCall, ignoreColon);

            if (match("keyword", "then") || match("?")) {
                std::string thenOp = currentToken().value;
                advance();

                Value thenValue = parseExpression(doExecute, identifierMode, doFunctionCall, true);

                if (match("keyword", "else") || match(":")) {
                    std::string elseOp = currentToken().value;
                    advance();

                    Value elseValue = parseExpression(doExecute, identifierMode, doFunctionCall, ignoreColon);

                    Value nestedConditional = handleConditional(elseifCondition, thenValue, elseValue, thenOp, elseOp);
                    return handleConditional(condition, thenValue, nestedConditional, thenOp, elseOp);
                } else {
                    throw std::runtime_error("Expected 'else' after 'then' in elseif");
                }
            } else {
                throw std::runtime_error("Expected 'then' after 'elseif'");
            }
        }
    }

    return condition;
}

Value Parser::getIdentifier() {
    Value result = stringToValue(currentToken().value);
    advance();
    return result;
}

Value Parser::parseBitwiseOR(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseBitwiseXOR(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("keyword", "OR") || match("|")) {
        std::string op = currentToken().value;
        advance();

        Value right = parseBitwiseXOR(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}
Value Parser::parseBitwiseXOR(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseBitwiseAND(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("keyword", "XOR") || match("^")) {
        std::string op = currentToken().value;
        advance();

        Value right = parseBitwiseAND(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}
Value Parser::parseBitwiseAND(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseBitwiseNOT(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("keyword", "AND") || match("&")) {
        std::string op = currentToken().value;
        advance();

        Value right = parseBitwiseNOT(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}
Value Parser::parseBitwiseSHIFT(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parsePipelineOrMethodCall(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("<<") || match(">>")) {
        std::string op = currentToken().value;
        advance();

        Value right = parsePipelineOrMethodCall(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}

Value Parser::parseBitwiseNOT(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    if (match("keyword", "NOT") || match("~")) {
        Value left;

        while (match("keyword", "NOT") || match("~")) {
            std::string op = currentToken().value;
            advance();

            Value right = parseBitwiseSHIFT(doExecute, identifierMode, doFunctionCall, ignoreColon);
            left = evaluateExpression(left, op, right, doExecute);
        }

        return left;
    }
    else return parseBitwiseSHIFT(doExecute, identifierMode, doFunctionCall, ignoreColon);
}

Value Parser::parsePipelineOrMethodCall(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseElvisOrNullCoalescing(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("|>") || (
        left.type != DataType::VARIABLE && left.type != DataType::UNKNOWN && (match("[") ||
            (match(".") && position + 1 < tokens.size() && ((position - 1 >= 0 && tokens[position - 1].type != "keyword") || !(position - 1 >= 0)))
        )
    )) {
        std::string op = currentToken().value;
        advance();

        Value right = op == "." && match("identifier") ? getIdentifier() : parseElvisOrNullCoalescing(doExecute, true, false, true);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}

Value Parser::parseElvisOrNullCoalescing(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseLogicalOR(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("?:") || match("??")) {
        std::string op = currentToken().value;
        advance();

        Value right = parseLogicalOR(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}

Value Parser::parseLogicalOR(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseLogicalXOR(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("keyword", "or") || match("||") ||
           match("keyword", "orn't") || match("!|") ||
           match("keyword", "nor")
        ) {
        std::string op = currentToken().value;
        advance();

        Value right = parseLogicalXOR(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}

Value Parser::parseLogicalXOR(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseLogicalAND(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("keyword", "xor") || match("keyword", "xnor")) {
        std::string op = currentToken().value;
        advance();

        Value right = parseLogicalAND(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}

Value Parser::parseLogicalAND(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseLogicalIMPLY(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("keyword", "and") || match("&&") ||
           match("keyword", "andn't") || match("!&") ||
           match("keyword", "nand")
        ) {
        std::string op = currentToken().value;
        advance();

        Value right = parseLogicalIMPLY(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}

Value Parser::parseLogicalIMPLY(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseEquality(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("keyword", "imply") || match("keyword", "nimply")) {
        std::string op = currentToken().value;
        advance();

        Value right = parseEquality(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}

Value Parser::parseEquality(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseComparison(doExecute, identifierMode, doFunctionCall, ignoreColon);

    if (!identifierMode) {
        while (match("keyword", "is") || match("==") ||
            match("keyword", "isn't") || match("!=") ||
            match("~=") || match("=")
        ) {
            std::string op = currentToken().value;
            advance();

            Value right = parseComparison(doExecute, identifierMode, doFunctionCall, ignoreColon);
            left = evaluateExpression(left, op, right, doExecute);
        }
    }

    return left;
}

Value Parser::parseComparison(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseTerm(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("<") || match(">") || match("<=") || match(">=")) {
        std::string op = currentToken().value;
        advance();

        Value right = parseTerm(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}

Value Parser::parseTerm(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseFactor(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("+") || match("minus") || match("..")) {
        std::string op = currentToken().value;
        advance();

        Value right = parseFactor(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}

Value Parser::parseFactor(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parsePower(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("*") || match("/") || match("%") || (match(":") && !(identifierMode || ignoreColon))) {
        std::string op = currentToken().value;
        advance();

        Value right = op == ":" && !Utility::checkNumber(right) ? (
            match("identifier") ? getIdentifier() : parsePower(doExecute, true, false, true) // method call
        ) : parsePower(doExecute, identifierMode, doFunctionCall, ignoreColon);
        
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}

Value Parser::parsePower(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    Value left = parseUnary(doExecute, identifierMode, doFunctionCall, ignoreColon);

    while (match("**")) {
        std::string op = currentToken().value;
        advance();

        Value right = parseUnary(doExecute, identifierMode, doFunctionCall, ignoreColon);
        left = evaluateExpression(left, op, right, doExecute);
    }

    return left;
}

Value Parser::parseUnary(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon) {
    if ((match("minus") && !identifierMode) || match("+") || match("!") ||
        (match("-") && !identifierMode) || match("#") || match("$") || match("&") || match("*")) {
        std::string op = currentToken().value;
        advance();

        Value right = parseUnary(doExecute, identifierMode, doFunctionCall, ignoreColon);

        if (op == "#") {
            return evaluateLengthOperator(right);
        } else if (op == "$") {
            return evaluateNameOperator(right);
        } else if (op == "&") {
            return evaluateAddressOfOperator(right);
        } else if (op == "*") {
            return evaluateDereferenceOperator(right);
        }

        return evaluateExpression(Value(), op, right, doExecute);
    }

    if (
        match("**") || match("*") || match("/") || match("%") || match("..") || (!identifierMode && (
            match(":") || match("=") || match("!=") || match("keyword", "is") || match("keyword", "isn't")
        )) || match("keyword", "imply") || match("keyword", "nimply") || match("&&") || match("!&") ||
        match("keyword", "and") || match("keyword", "nand") || match("keyword", "andn't") ||
        match("keyword", "xor") || match("keyword", "xnor") || match("||") || match("!|") ||
        match("keyword", "or") || match("keyword", "nor") || match("keyword", "orn't") || match("~") ||
        match("keyword", "NOT") || match("<<") || match(">>") || match("keyword", "AND") || match("&") ||
        match("keyword", "XOR") || match("^") || match("keyword", "OR") || match("|")
    ) {
        return parseBitwiseOR(doExecute, identifierMode, doFunctionCall, ignoreColon);
    }

    return parsePrimary(doExecute, doFunctionCall);
}

Value Parser::evaluateLengthOperator(const Value& value) {
    Value result;

    switch (value.type) {
        case DataType::STRING:
            result.type = DataType::NUMBER;
            size_t length;
            switch (chartype) {
                case CharType::GRAPHEME:
                    length = Unicode::GraphemeLength(value.string_value);
                    break;
                case CharType::CODEPOINT:
                    length = Unicode::CodePointLength(value.string_value);
                    break;
                case CharType::BYTE:
                    length = Unicode::ByteLength(value.string_value);
                    break;
                default:
                    throw std::runtime_error("Invalid charType.");
            }
            result.number_value = static_cast<double>(length);
            result.name = std::to_string(length);
            break;

        case DataType::JSON_ARRAY:
        case DataType::SET:
            result.type = DataType::NUMBER;
            result.number_value = static_cast<double>(value.array_elements.size());
            result.name = std::to_string(value.array_elements.size());
            break;

        case DataType::JSON_OBJECT:
        case DataType::JUSTC_OBJECT:
            result.type = DataType::NUMBER;
            result.number_value = static_cast<double>(value.properties.size());
            result.name = std::to_string(value.properties.size());
            break;

        case DataType::BINARY_DATA:
            result.type = DataType::NUMBER;
            result.number_value = static_cast<double>(value.binary_data.size());
            result.name = std::to_string(value.binary_data.size());
            break;

        case DataType::NUMBER: {
            // For numbers, get digit count
            std::string str = std::to_string(static_cast<int>(value.number_value));
            str.erase(str.find_last_not_of('0') + 1, std::string::npos);
            if (str.back() == '.') str.pop_back();
            result.type = DataType::NUMBER;
            result.number_value = static_cast<double>(str.length());
            result.name = std::to_string(str.length());
            break;
        }

        default:
            throw std::runtime_error("Cannot apply length operator to type " + dataTypeToString(value.type) + " at " + Utility::position(currentToken().start, input) + ".");
    }

    return result;
}
Value Parser::evaluateNameOperator(const Value& value) {
    return Value::createString(value.isVariable ? value.variable : value.name);
}
Value Parser::evaluateAddressOfOperator(const Value& value) {
    return makePointer(value);
}
Value Parser::evaluateDereferenceOperator(const Value& value) {
    return getPointer(value);
}

Value Parser::astNodeToValue(const ASTNode& node) {
    if (node.type == "VARIABLE_DECLARATION") {
        return evaluateASTNode(node);
    }
    else if (node.type == "COMMAND") {
        return stringToValue(node.identifier);
    }
    else {
        return node.value;
    }
}

Value Parser::parsePrimary(bool doExecute, bool doFunctionCall) {
    if (match("number")) {
        std::string numStr = currentToken().value;
        double num = parseNumber(numStr);
        advance();
        Value result = numberToValue(num);

        if (!numStr.empty() && std::tolower(numStr.back()) == 'b') {
            result.name = std::to_string(num) + "B";
        } else {
            result.name = numStr;
        }

        return result;
    }
    else if (match("hex")) {
        std::string hexStr = currentToken().value;
        advance();
        return hexToValue(hexStr);
    }
    else if (match("binary")) {
        std::string binStr = currentToken().value;
        advance();
        return binaryToValue(binStr);
    }
    else if (match("string")) {
        std::string str = currentToken().value;
        advance();
        return stringToValue(str);
    }
    else if (match("string_i") || match("string_ri")) {
        return stringToValue(parseStringInterpolation(doExecute));
    }
    else if (match("link")) {
        std::string link = currentToken().value;
        advance();
        return linkToValue(link);
    }
    else if (match("boolean")) {
        auto toLower = [](const std::string& str) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            return result;
        };

        std::string tokenValue = currentToken().value;
        bool b = (toLower(tokenValue) == "true" ||
                  toLower(tokenValue) == "yes" ||
                  toLower(tokenValue) == "y");
        advance();
        return booleanToValue(b);
    }
    else if (match("null")) {
        Value result;
        result.type = DataType::NULL_TYPE;
        result.name = "null";
        advance();
        return result;
    }
    else if (match("identifier")) {
        std::string varName = currentToken().value;
        if ((peekToken().type == "." && position + 2 < tokens.size()) || peekToken().type == "[") {
            return parseObjectPropertyAccess(doExecute);
        }

        if (varName == "$TIME" || varName == "$VERSION" || varName == "$LATEST" ||
            varName == "$DBID" || varName == "$SHA" || varName == "$NAV" ||
            varName == "$PAGES" || varName == "$CSS" ||
            varName == "$BACKSLASH" || varName == "$JUST_VERSION"
        ) {
            advance();
            return executeFunction(varName.substr(1), {}, currentToken().start);
        }

        if (peekToken().type == "(") {
            return parseFunctionCall(doExecute, doFunctionCall);
        } else if (peekToken().type == "::") {
            return parseSpaceCall(doExecute, doFunctionCall);
        }

        Value result;
        result.type = DataType::VARIABLE;
        result.string_value = varName;
        advance();
        try {
            return resolveVariableValue(varName, true);
        } catch (...) {
            return result;
        }
    }
    else if (match("keyword", "await")) {
        advance();
        Value val = parseExpression(doExecute, false, doFunctionCall);
        if (val.type != DataType::PROMISE) return val;
        Value result = Value::createNull();
        Promise prom = val.getComplexData<Promise>();
        prom.then([&result](Value output) {
            result = output;
        });
        return result;
    }
    else if (match("keyword", "new")) {
        advance();
        Value val = parseExpression(doExecute, false, doFunctionCall);
        switch (val.type) {
            case DataType::STRUCT: {
                std::string structID = Utility::uint64ToHexString(nextStructConstructor++);
                structConstructors[structID] = val;
                Value result = applyCPPTypeDeclaration(Value::createNull(), structID, DataType::UNKNOWN, doExecute, true, 1);
                structConstructors.erase(structID);
                return result;
            }
            case DataType::CLASS: {
                std::vector<Value> args = parseArguments(doExecute);
                uint64_t classID = val.getNumericValue<uint64_t>();
                Class cls = getClass(classID);
                Value result = callFunction(cls.constructor, args, currentToken().start, doExecute);
                if (result.type != DataType::JSON_OBJECT && result.type != DataType::JUSTC_OBJECT)
                    throw std::runtime_error("Constructor for class " + Utility::uint64ToHexString(classID) + " returned a non-object value.");
                Value instanceObject = Value::createJsonObject({});
                instanceObject.properties = cls.instanceObject;
                result = doubleDot(instanceObject, result);
                return result;
            }
            default: throw std::runtime_error("Expected struct or class after \"new\" keyword at " + Utility::position(currentToken().start, input) + ".");
        }
    }
    else if (match("keyword", "try")) {
        advance();
        Value result = Value::createNull();
        try {
            result = parseExpression(doExecute, false, doFunctionCall);
        } catch (...) {
            if (match("keyword", "catch")) {
                advance();
                result = parseExpression(doExecute, false, doFunctionCall);
            }
            if (match("keyword", "finally")) {
                advance();
                result = parseExpression(doExecute, false, doFunctionCall);
            }
        }
        return result;
    }
    else if (match("keyword", "this")) {
        if ((peekToken().type == "." && position + 2 < tokens.size()) || peekToken().type == "[") {
            return parseObjectPropertyAccess(doExecute);
        }
        return This();
    }
    else if (match("keyword") && peekToken().type == "(") {
        return parseFunctionCall(doExecute, doFunctionCall);
    }
    else if (match("(")) {
        advance();
        Value result = parseExpression(doExecute, false, doFunctionCall);
        if (!match(")")) {
            throw std::runtime_error("Expected \")\" at " + Utility::position(currentToken().start, input) + ".");
        }
        advance();
        return result;
    }
    else if ((
        (endOfScript == "." && match(".") && tokens[position + 1].type != "number") ||
        (endOfScript != "." && match(endOfScript))
    ) || (match(",") && tokens[position + 1].type != "number") || match(";")) {
        Value result;
        result.type = DataType::NULL_TYPE;
        result.string_value = "null";
        result.name = "null";
        return result;
    }
    else if (match("keyword") || match("?") || match("!=") || match("=")) {
        return astNodeToValue(parseStatement(doExecute));
    }
    else if ((match(".") || match(",")) && position + 1 < tokens.size() && tokens[position + 1].type == "number") {
        advance();
        double num = parseNumber("0." + currentToken().value);
        advance();
        return numberToValue(num);
    }
    else if (match("|")) {
        return parseJustcObject(doExecute);
    }
    else if (match("{")) {
        size_t savedPos = position;
        return parseJsonObject(doExecute);
    }
    else if (match("[")) {
        return parseJsonArray(doExecute);
    }/*
    else if (match("|")) {
        advance();
        std::stringstream object;
        while (!match(".")) {
            object << currentToken().value;
            advance();
            if (isEnd()) {
                throw std::runtime_error("Expected \".\" to close object, got EOF at " + Utility::position(currentToken().start, input) + ".");
            }
        }
        object << ".";
        std::string objectstr = object.str();
        advance();
        Value result = stringToValue(objectstr);
        result.type = DataType::JUSTC_OBJECT;
        result.name = objectstr;
        return result;
    }*/
    else if ((match("JavaScript") || match("JavaScript_i")) && doExecute && allowJavaScript) {
        bool interpolation = match("JavaScript_i");
        std::string code = interpolation ? parseStringInterpolation(doExecute) : currentToken().value;

        #ifdef __EMSCRIPTEN__

        Value result = runJavaScript(code, Utility::position(currentToken().start, input), true);
        addLog("JAVASCRIPT", Utility::value2string(result), currentToken().start);
        if (!interpolation) advance();
        result.name = "j'" + code + "'";
        return result;

        #elif !defined(_MSC_VER)

        std::pair<std::string, bool> jsresult = JavaScript::Eval(code);
        if (jsresult.second) {
            throw std::runtime_error("JavaScript error at " + Utility::position(currentToken().start, input) + ":\n" + jsresult.first);
        } else {
            addLog("JAVASCRIPT", jsresult.first, currentToken().start);
        }
        if (!interpolation) advance();
        Value result = stringToValue(jsresult.first);
        result.name = "j'" + code + "'";
        return result;

        #endif
    }
    else if (match("JavaScript") || match("JavaScript_i")) {
        #ifdef __EMSCRIPTEN__
        if (!allowJavaScript) warn_js_disabled_by_justc(Utility::position(currentToken().start, input).c_str(), currentToken().value.c_str(), getCurrentTimestamp().c_str());
        else warn_js_disabled(Utility::position(currentToken().start, input).c_str(), currentToken().value.c_str(), getCurrentTimestamp().c_str());
        #endif
        advance();
        return Value::createNull();
    }
    else if ((match("Luau") || match("Luau_i")) && doExecute && allowLuau) {
        #ifndef JUSTC_NOLUAU
            bool interpolation = match("Luau_i");
            std::pair<std::string, int> luauresult = RunLuau::runScriptWithResult(interpolation ? parseStringInterpolation(doExecute) : currentToken().value);
            Value result;

            switch (luauresult.second) {
                case 1: // number
                    result = Value::createNumber(parseNumber(luauresult.first));
                    result.type = DataType::NUMBER;
                    break;
                case 2: // boolean
                    result = Value::createBoolean(luauresult.first == "true");
                    result.type = DataType::BOOLEAN;
                    break;
                case 3: // null
                    result = Value::createNull();
                    result.type = DataType::NULL_TYPE;
                    break;
                case 4: case 5: // object/array
                    result = isolated(luauresult.first, false, currentToken().start, nullptr, "Luau Table output to JUSTC converter");
                    result.type = luauresult.second == 4 ? DataType::JSON_OBJECT : DataType::JSON_ARRAY;
                    break;
                default: // string/function/thread/userdata
                    result = stringToValue(luauresult.first);
                    result.type = DataType::STRING;
                    break;
            }

            addLog("LUAU", Utility::value2string(result), currentToken().start);
            result.name = "l'" + currentToken().value + "'";
            if (!interpolation) advance();
            return result;
        #else
            throw std::runtime_error("To run Luau, use the standard JUSTC build. The current build excludes Luau.");
        #endif
    }
    else if (match("Luau") || match("Luau_i")) {
        #ifdef __EMSCRIPTEN__
        if (!allowLuau) warn_luau_disabled_by_justc(Utility::position(currentToken().start, input).c_str(), currentToken().value.c_str(), getCurrentTimestamp().c_str());
        else warn_luau_disabled(Utility::position(currentToken().start, input).c_str(), currentToken().value.c_str(), getCurrentTimestamp().c_str());
        #endif
        advance();
        return Value::createNull();
    }
    else if (match("jsx_element")) {
        std::string jsxStr = currentToken().value;
        advance();
        return parseJSXElement(jsxStr);
    }
    else if (match("JUSTO") || match("JUSTO_i")) {
        bool interpolation = match("JUSTO_i");
        std::string code = interpolation ? parseStringInterpolation(doExecute) : currentToken().value;
        advance();
        return ParseJUSTO(code);
    }
    else if (match("JUSTC") || match("JUSTC_i")) {
        bool interpolation = match("JUSTC_i");
        std::string code = interpolation ? parseStringInterpolation(doExecute) : currentToken().value;
        advance();
        return functionJUSTC2(code, doExecute, currentToken().start);
    }

    throw std::runtime_error("Invalid or unexpected token \"" + currentToken().value + "\" at " + Utility::position(currentToken().start, input) + ".");
}

Value Parser::parseFunctionCall(bool doExecute, bool doFunctionCall) {
    std::string funcName = currentToken().value;
    size_t startPos = currentToken().start;

    while (peekToken().type == "." && position + 2 < tokens.size()) {
        advance();
        if (match("identifier") || match("keyword")) {
            funcName += "." + currentToken().value;
            advance();
        } else {
            throw std::runtime_error("Unexpected end of script at " + Utility::position(currentToken().start, input) + ".");
        }
    }

    Value funcValue = resolveVariableValue(funcName, false);
    advance();
    if (!doFunctionCall) return funcValue;
    
    if (funcValue.type == DataType::FUNCTION) {
        if (!match("(")) {
            throw std::runtime_error("Expected '(' after function name at " + Utility::position(currentToken().start, input) + ".");
        }
        advance();

        std::vector<Value> args;
        while (!match(")") && !isEnd()) {
            args.push_back(parseExpression(doExecute));
            if (match(",") || match(";")) advance();
        }

        if (!match(")")) {
            throw std::runtime_error("Expected ')' after function arguments at " + Utility::position(currentToken().start, input) + ".");
        }
        advance();

        return callFunction(funcValue, args, currentToken().start, doExecute);
    } else {
        if (!match("(")) {
            throw std::runtime_error("Expected '(' after function name at " + Utility::position(currentToken().start, input) + ".");
        }
        advance();

        std::vector<Value> args;
        while (!match(")") && !isEnd()) {
            args.push_back(parseExpression(doExecute));
            if (match(",") || match(";")) advance();
        }

        if (!match(")")) {
            throw std::runtime_error("Expected ')' after function arguments at " + Utility::position(currentToken().start, input) + ".");
        }
        advance();

        return executeFunction(funcName, args, currentToken().start);
    }
}
Value Parser::parseSpaceCall(bool doExecute, bool doFunctionCall) {
    std::string spaceName = currentToken().value;
    advance();

    if (!match("::")) {
        throw std::runtime_error("Expected \"::\" after space name at " + Utility::position(currentToken().start, input) + ".");
    }
    advance();

    tokens[position].value = spaceName + "::" + currentToken().value;
    return parseFunctionCall(doExecute, doFunctionCall);
}

ASTNode Parser::parseCommand(bool doExecute) {
    ASTNode node("COMMAND", currentToken().value, currentToken().start);
    std::string command = currentToken().value;
    advance();
    std::vector<Value> args;

    if (doExecute && (command == "echo" || command == "logfile" || command == "log") && !match("(")) {
        while (!match(",") && !match(";") && !match(endOfScript) && !isEnd()) {
            args.push_back(parseExpression(doExecute));
        }
        if (match(",") || match(";")) advance();

        if (command == "echo") {
            for (const auto& arg : args) {
                std::string message = arg.toString();
                auto varval = resolveVariableValue(message, false);
                if (varval.type == DataType::UNKNOWN) {
                    addLog("ECHO", message, node.startPos);
                    std::cout << message << std::endl;
                } else {
                    addLog("ECHO", Utility::value2string(varval), node.startPos);
                    std::cout << Utility::value2string(varval) << std::endl;
                }
            }
        } else if (command == "logfile") {
            if (!args.empty()) {
                std::string path = args[0].toString();
                setLogFile(path);
            }
        } else if (command == "log") {
            for (const auto& arg : args) {
                std::string message = arg.toString();
                auto varval = resolveVariableValue(message, false);
                if (varval.type == DataType::UNKNOWN) {
                    addLog("LOG", message, node.startPos);
                } else {
                    addLog("LOG", Utility::value2string(varval), node.startPos);
                }
            }
        }
        return node;
    }

    if (match("(")) {
        advance();
        while (!match(")") && !isEnd()) {
            args.push_back(parseExpression(doExecute));
            if (match(",") || match(";")) advance();
        }
        if (match(")")) advance();
    }

    if (doExecute) {
        if (command == "echo") {
            for (const auto& arg : args) {
                std::string message = arg.toString();
                auto varval = resolveVariableValue(message, false);
                if (varval.type == DataType::UNKNOWN) {
                    addLog("ECHO", message, node.startPos);
                    std::cout << message << std::endl;
                } else {
                    addLog("ECHO", Utility::value2string(varval), node.startPos);
                    std::cout << Utility::value2string(varval) << std::endl;
                }
            }
        } else if (command == "logfile") {
            if (!args.empty()) {
                std::string path = args[0].toString();
                setLogFile(path);
            }
        } else if (command == "log") {
            for (const auto& arg : args) {
                std::string message = arg.toString();
                auto varval = resolveVariableValue(message, false);
                if (varval.type == DataType::UNKNOWN) {
                    addLog("LOG", message, node.startPos);
                } else {
                    addLog("LOG", Utility::value2string(varval), node.startPos);
                }
            }
        }
    }

    return node;
}

Value Parser::onExecDisabled(size_t startPos, std::string name) {
    #ifdef __EMSCRIPTEN__
    warn_exec_disabled(Utility::position(startPos, input).c_str(), name.c_str(), getCurrentTimestamp().c_str());
    #endif

    Value result;
    result.type = DataType::ERROR;
    result.string_value = "HTTP requests are disabled";
    result.name = "<" + name + ">";
    return result;
}

Value Parser::executeFunction(const std::string& funcName, const std::vector<Value>& args, size_t startPos) {
    if (!doExecute) {
        return onExecDisabled(startPos, funcName);
    }
    
    if (isFunctionId(funcName)) {
        uint64_t funcId = extractFunctionId(funcName);
        try {
            auto func = ::getFunction(funcId, Utility::uint64ToHexString(funcId));
            return func(args);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string(e.what()) + " at " + Utility::position(startPos, input));
        }
    }

    auto customIt = userFunctions.find(funcName);
    if (customIt != userFunctions.end()) {
        try {
            return customIt->second(args);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string(e.what()) + " at " + Utility::position(startPos, input));
        }
    }

    if (funcName == "TIME") {
        long timestamp = getCurrentTime();
        return numberToValue(timestamp);
    }
    else if (funcName == "math.pi") {
        return numberToValue(Math::PI);
    }
    else if (funcName == "backslash") {
        return stringToValue("\\");
    }
    else if (funcName == "version" || funcName == "JUSTC.version" || funcName == "JUSTO.version") {
        return stringToValue(JUSTC_VERSION);
    }
    else if (funcName == "math.e") {
        return numberToValue(Math::E);
    }
    else if (funcName == "math.ln2") {
        return numberToValue(Math::LN2);
    }
    else if (funcName == "math.ln10") {
        return numberToValue(Math::LN10);
    }
    else if (funcName == "math.sqrt2") {
        return numberToValue(Math::SQRT2);
    }
    else if (funcName == "math.sqrt1_2") {
        return numberToValue(Math::SQRT1_2);
    }
    else if (funcName == "math.log2e") {
        return numberToValue(Math::LOG2E);
    }
    else if (funcName == "math.log10e") {
        return numberToValue(Math::LOG10E);
    }

    if (funcName == "valueof") return functionVALUE(args);
    if (funcName == "String") return functionSTRING(args);
    if (funcName == "Link") return functionLINK(args);
    if (funcName == "Binary") return functionBINARY(args);
    if (funcName == "Octal") return functionOCTAL(args);
    if (funcName == "Hexadecimal") return functionHEXADECIMAL(args);
    if (funcName == "typeid") return functionTYPEID(args);
    if (funcName == "typeof") return functionTYPEOF(args);
    if (funcName == "echo") return functionECHO(args);
    if (funcName == "Number") {
        if (args.empty()) return numberToValue(0.0);
        return numberToValue(args[0].toNumber());
    }
    if (funcName == "JSON") return functionJSON(args);
    if (funcName == "HTTP.GET") {
        if (runAsync) {
            auto future = functionHTTPAsync(startPos, "GET", args);
            return future.get();
        }
        return functionHTTP(startPos, "GET", args);
    }
    if (funcName == "HTTP.POST") {
        if (runAsync) {
            auto future = functionHTTPAsync(startPos, "POST", args);
            return future.get();
        }
        return functionHTTP(startPos, "POST", args);
    }
    if (funcName == "HTTP.PUT") {
        if (runAsync) {
            auto future = functionHTTPAsync(startPos, "PUT", args);
            return future.get();
        }
        return functionHTTP(startPos, "PUT", args);
    }
    if (funcName == "HTTP.PATCH") {
        if (runAsync) {
            auto future = functionHTTPAsync(startPos, "PATCH", args);
            return future.get();
        }
        return functionHTTP(startPos, "PATCH", args);
    }
    if (funcName == "HTTP.DELETE") {
        if (runAsync) {
            auto future = functionHTTPAsync(startPos, "DELETE", args);
            return future.get();
        }
        return functionHTTP(startPos, "DELETE", args);
    }
    if (funcName == "HTTP.HEAD") {
        if (runAsync) {
            auto future = functionHTTPAsync(startPos, "HEAD", args);
            return future.get();
        }
        return functionHTTP(startPos, "HEAD", args);
    }
    if (funcName == "HTTP.OPTIONS") {
        if (runAsync) {
            auto future = functionHTTPAsync(startPos, "OPTIONS", args);
            return future.get();
        }
        return functionHTTP(startPos, "OPTIONS", args);
    }
    if (funcName == "JUSTC") return functionJUSTC(args, startPos);
    if (funcName == "file") {
        if (runAsync) {
            auto future = functionFILEAsync(args);
            return future.get();
        }
        return functionFILE(args);
    }
    if (funcName == "size") return functionSTAT(args);
    if (funcName == "config") return functionCONFIG(args);

    if (funcName == "JavaScript.isAllowed") {
        return booleanToValue(allowJavaScript);
    }
    if (funcName == "Luau.isAllowed") {
        return booleanToValue(allowLuau);
    }

    double inpnum = 0.0;
    if (
        args.empty() && funcName != "math.random" && funcName != "Window" && funcName != "task.wait" && funcName != "memory.global" &&
        funcName != "memory.classes" && funcName != "memory.funcitons" && funcName != "memory.pointers"
    ) {
        if (funcName == "JUSTC.parse" || funcName == "JUSTC.execute") {
            return emptyJUSTC();
        }
        if (funcName == "JUSTO.stringify") {
            return stringToValue("");
        }
        if (funcName == "JUSTC.stringify") {
            return stringToValue("return.");
        }
        throw std::runtime_error("Expected at least one argument, got 0 at " + Utility::position(startPos, input) + ".");
    }
    if (!args.empty()) inpnum = args[0].number_value;
    try {
        if (funcName == "Binary::toText") {
            return Binary::ToText(args);
        }
        if (funcName == "Binary::dromText") {
            return Binary::FromText(args);
        }
        if (funcName == "Binary::toDataURL") {
            return Binary::ToDataURL(args);
        }
        if (funcName == "Binary::fromDataURL") {
            return Binary::FromDataURL(args);
        }
        if (funcName == "Binary::data") {
            return Binary::Data(args);
        }
        if (funcName == "math.abs") {
            return Value::createNumber(Math::Abs(inpnum));
        }
        if (funcName == "math.acos") {
            return Value::createNumber(Math::Acos(inpnum));
        }
        if (funcName == "math.asin") {
            return Value::createNumber(Math::Asin(inpnum));
        }
        if (funcName == "math.atan") {
            return Value::createNumber(Math::Atan(inpnum));
        }
        if (funcName == "math.atan2") {
            return Value::createNumber(Math::Atan2(inpnum, args[1].number_value));
        }
        if (funcName == "math.ceil") {
            return Value::createNumber(Math::Ceil(inpnum));
        }
        if (funcName == "math.cos") {
            return Value::createNumber(Math::Cos(inpnum));
        }
        if (funcName == "math.clamp") {
            return Value::createNumber(Math::Clamp(inpnum, args[1].number_value, args[2].number_value));
        }
        if (funcName == "math.cube") {
            return Value::createNumber(inpnum * inpnum * inpnum);
        }
        if (funcName == "math.double") {
            return Value::createNumber(inpnum * 2);
        }
        if (funcName == "math.exp") {
            return Value::createNumber(Math::Exp(inpnum));
        }
        if (funcName == "math.factorial") {
            int intValue = static_cast<int>(std::round(inpnum));
            long long res = Math::Factorial(intValue);
            double outVal = static_cast<double>(res);
            return Value::createNumber(outVal);
        }
        if (funcName == "math.floor") {
            return Value::createNumber(Math::Floor(inpnum));
        }
        if (funcName == "math.hypot") {
            return Value::createNumber(Math::Hypot(inpnum, args[1].number_value));
        }
        if (funcName == "math.isPrime") {
            int intValue = static_cast<int>(std::round(inpnum));
            return Value::createBoolean(Math::IsPrime(intValue));
        }
        if (funcName == "math.lerp") {
            return Value::createNumber(Math::Lerp(inpnum, args[1].number_value, args[2].number_value));
        }
        if (funcName == "math.log") {
            return Value::createNumber(Math::Log(inpnum));
        }
        if (funcName == "math.log10") {
            return Value::createNumber(Math::Log10(inpnum));
        }
        if (funcName == "math.max") {
            return Value::createNumber(Math::Max(values2numbers(args)));
        }
        if (funcName == "math.min") {
            return Value::createNumber(Math::Min(values2numbers(args)));
        }
        if (funcName == "math.pow") {
            return Value::createNumber(Math::Pow(inpnum, args[1].number_value));
        }
        if (funcName == "math.random") {
            if (args.empty()) return Value::createNumber(Math::Random());
            if (args.size() == 1) return Value::createNumber(Math::Random(0, inpnum));
            return Value::createNumber(Math::Random(inpnum, args[1].number_value));
        }
        if (funcName == "math.round") {
            return Value::createNumber(Math::Round(inpnum));
        }
        if (funcName == "math.sign") {
            return Value::createNumber(Math::Sign(inpnum));
        }
        if (funcName == "math.sin") {
            return Value::createNumber(Math::Sin(inpnum));
        }
        if (funcName == "math.sqrt") {
            return Value::createNumber(Math::Sqrt(inpnum));
        }
        if (funcName == "math.square") {
            return Value::createNumber(inpnum * inpnum);
        }
        if (funcName == "math.tan") {
            return Value::createNumber(Math::Tan(inpnum));
        }
        if (funcName == "math.toDegrees") {
            return Value::createNumber(Math::ToDegrees(inpnum));
        }
        if (funcName == "math.toRadians") {
            return Value::createNumber(Math::ToRadians(inpnum));
        }
        if (funcName == "parseNum" || funcName == "parseInt") {
            std::string str = args[0].toString();
            int radix = 10;

            if (args.size() > 1) {
                radix = static_cast<int>(args[1].toNumber());
                if (radix < 2 || radix > 64) {
                    throw std::runtime_error(funcName + ": Radix must be between 2 and 64");
                }
            }

            if (radix == 10) return numberToValue(
                funcName == "parseNum" ? args[0].toNumber() : static_cast<double>(static_cast<int>(args[0].toNumber()))
            );

            try {
                double result = Math::ParseNum(str, radix);
                return Value::createNumber(
                    funcName == "parseNum" ? result : static_cast<double>(static_cast<int>(result))
                );
            } catch (const std::exception& e) {
                throw std::runtime_error(funcName + ": " + std::string(e.what()));
            }
        }
        if (funcName == "String::graphemeReverse" || (
            chartype == CharType::GRAPHEME && funcName == "String::reverse"
        )) {
            return stringToValue(Unicode::GraphemeReverse(args[0].toString()));
        }
        if (funcName == "String::trim") {
            return stringToValue(String::Trim(args[0].toString()));
        }
        if (funcName == "String::repeat") {
            size_t count = 1;
            if (args.size() > 1) {
                count = static_cast<size_t>(args[1].toNumber());
            }

            return stringToValue(String::Repeat(args[0].toString(), count));
        }
        if (funcName == "String::graphemeSlice" || (
            chartype == CharType::GRAPHEME && funcName == "String::slice"
        )) {
            std::string str = args[0].toString();
            int64_t start = 0;
            int64_t end = static_cast<int64_t>(str.length());

            if (args.size() > 1) {
                start = static_cast<int64_t>(args[1].toNumber());
                if (args.size() > 2) {
                    end = static_cast<int64_t>(args[2].toNumber());
                }
            }

            return stringToValue(Unicode::GraphemeSlice(str, start, end));
        }
        if (funcName == "String::startsWith") {
            if (args.size() < 2) {
                return booleanToValue(false);
            }
            return booleanToValue(String::StartsWith(args[0].toString(), args[1].toString()));
        }
        if (funcName == "String::endsWith") {
            if (args.size() < 2) {
                return booleanToValue(false);
            }
            return booleanToValue(String::EndsWith(args[0].toString(), args[1].toString()));
        }
        if (funcName == "String::split") {
            if (args.size() < 2) {
                std::vector<std::string> result( {args[0].toString()} );
                return stringArray(result);
            }
            if (args[1].toString() == "") {
                size_t len = static_cast<size_t>(executeFunction("String::length", {args[0]}, startPos).toNumber());
                std::vector<std::string> result;
                for (size_t i = 0; i < len; i++) {
                    result.push_back(executeFunction("String::slice", {
                        args[0],
                        numberToValue(static_cast<double>(i)),
                        numberToValue(static_cast<double>(i + 1))
                    }, startPos).toString());
                }
                return stringArray(result);
            }
            return stringArray(String::Split(args[0].toString(), args[1].toString()));
        }
        if (funcName == "String::codePointReverse" || (
            chartype == CharType::CODEPOINT && funcName == "String::reverse"
        )) {
            return stringToValue(Unicode::CodePointReverse(args[0].toString()));
        }
        if (funcName == "String::byteReverse" || (
            chartype == CharType::BYTE && funcName == "String::reverse"
        )) {
            return stringToValue(Unicode::ByteReverse(args[0].toString()));
        }
        if (funcName == "String::codePointSlice" || (
            chartype == CharType::CODEPOINT && funcName == "String::slice"
        )) {
            std::string str = args[0].toString();
            int64_t start = 0;
            int64_t end = static_cast<int64_t>(str.length());

            if (args.size() > 1) {
                start = static_cast<int64_t>(args[1].toNumber());
                if (args.size() > 2) {
                    end = static_cast<int64_t>(args[2].toNumber());
                }
            }

            return stringToValue(Unicode::CodePointSlice(str, start, end));
        }
        if (funcName == "String::byteSlice" || (
            chartype == CharType::BYTE && funcName == "String::slice"
        )) {
            std::string str = args[0].toString();
            int64_t start = 0;
            int64_t end = static_cast<int64_t>(str.length());

            if (args.size() > 1) {
                start = static_cast<int64_t>(args[1].toNumber());
                if (args.size() > 2) {
                    end = static_cast<int64_t>(args[2].toNumber());
                }
            }

            return stringToValue(Unicode::ByteSlice(str, start, end));
        }
        if (funcName == "String::lower") {
            return stringToValue(Unicode::Lower(args[0].toString()));
        }
        if (funcName == "String::upper") {
            return stringToValue(Unicode::Upper(args[0].toString()));
        }
        if (funcName == "String::normalize") {
            if (args.size() < 2) throw std::runtime_error("Expected form");
            std::string form = Unicode::Lower(args[1].toString());
            std::string original = args[0].toString();
            if (form == "nfc") {
                return stringToValue(Unicode::NormalizeNFC(original));
            } else if (form == "nfd") {
                return stringToValue(Unicode::NormalizeNFD(original));
            } else if (form == "nfkc") {
                return stringToValue(Unicode::NormalizeNFKC(original));
            } else if (form == "nfkd") {
                return stringToValue(Unicode::NormalizeNFKD(original));
            } else throw std::runtime_error("Unknown form \"" + args[1].toString() + "\"");
        }
        if (funcName == "String::graphemeLength" || (
            chartype == CharType::GRAPHEME && funcName == "String::length"
        )) {
            return Value::createNumber(static_cast<double>(Unicode::GraphemeLength(args[0].toString())));
        }
        if (funcName == "String::codePointLength" || (
            chartype == CharType::CODEPOINT && funcName == "String::length"
        )) {
            return Value::createNumber(static_cast<double>(Unicode::CodePointLength(args[0].toString())));
        }
        if (funcName == "String::size" || funcName == "String::byteLength" || (
            chartype == CharType::BYTE && funcName == "String::length"
        )) {
            return Value::createNumber(static_cast<double>(Unicode::ByteLength(args[0].toString())));
        }
        if (funcName == "String::equalsIgnoreCase") {
            std::string right;
            if (args.size() < 2) {
                right = "";
            } else {
                right = args[1].toString();
            }
            return booleanToValue(Unicode::EqualsIgnoreCase(args[0].toString(), right));
        }
        if (funcName == "String::isWhitespace") {
            return booleanToValue(Unicode::IsWhitespace(args[0].toString()));
        }
        if (funcName == "JUSTC.parse") {
            return functionJUSTC2(args[0].toString(), false, startPos);
        }
        if (funcName == "JUSTC.execute") {
            return functionJUSTC2(args[0].toString(), true, startPos);
        }
        if (funcName == "JSON.parse") {
            return functionJSON(args);
        }
        if (funcName == "JavaScript" || funcName == "JavaScript.execute") {
            if (allowJavaScript) {
                #ifdef __EMSCRIPTEN__

                    Value result = runJavaScript(args[0].toString(), Utility::position(startPos, input), true);
                    addLog("JAVASCRIPT", Utility::value2string(result), startPos);
                    result.name = funcName + "(...)";
                    return result;

                #elif !defined(_MSC_VER)

                    std::pair<std::string, bool> jsresult = JavaScript::Eval(args[0].toString());
                    if (jsresult.second) {
                        throw std::runtime_error("JavaScript error at " + Utility::position(startPos, input) + ":\n" + jsresult.first);
                    } else {
                        addLog("JAVASCRIPT", jsresult.first, startPos);
                    }
                    Value result = stringToValue(jsresult.first);
                    result.name = funcName + "(...)";
                    return result;

                #else

                    throw std::runtime_error("Cannot run JavaScript due to OS limitations. Attempt to execute JavaScript code");

                #endif
            } else {
                throw std::runtime_error("JavaScript disallowed - Cannot run JavaScript \"" + args[0].toString() + "\"");
            }
        }
        if (funcName == "Luau" || funcName == "Luau.execute") {
            #ifndef JUSTC_NOLUAU
                if (allowLuau) {
                    std::pair<std::string, int> luauresult = RunLuau::runScriptWithResult(args[0].toString());
                    Value result;

                    switch (luauresult.second) {
                        case 1: // number
                            result = Value::createNumber(parseNumber(luauresult.first));
                            result.type = DataType::NUMBER;
                            break;
                        case 2: // boolean
                            result = Value::createBoolean(luauresult.first == "true");
                            result.type = DataType::BOOLEAN;
                            break;
                        case 3: // null
                            result = Value::createNull();
                            result.type = DataType::NULL_TYPE;
                            break;
                        case 4: case 5: // object/array
                            result = isolated(luauresult.first, false, startPos, nullptr, "Luau Table output to JUSTC converter");
                            result.type = luauresult.second == 4 ? DataType::JSON_OBJECT : DataType::JSON_ARRAY;
                            break;
                        default: // string/function/thread/userdata
                            result = stringToValue(luauresult.first);
                            result.type = DataType::STRING;
                            break;
                    }

                    addLog("LUAU", Utility::value2string(result), startPos);
                    result.name = funcName + "(...)";
                    return result;
                } else {
                    throw std::runtime_error("Luau disallowed - Cannot run Luau \"" + args[0].toString() + "\"");
                }
            #else
                throw std::runtime_error("To run Luau, use the standard JUSTC build. The current build excludes Luau.");
            #endif
        }
        if (funcName == "JUSTO" || funcName == "JUSTO.parse") {
            return functionJUSTO(args);
        }
        if (funcName == "JUSTO.stringify") {
            return toJUSTO(args);
        }
        if (funcName == "JUSTC.stringify") {
            Value result = stringToValue(Utility::stringifyValue(args[0]));
            result.type = DataType::STRING;
            return result;
        }
        if (funcName == "Luau.compile") {
            #ifndef JUSTC_NOLUAU
                if (allowLuau) {
                    std::string error;
                    return booleanToValue(RunLuau::compileScript(args[0].toString(), error));
                } else {
                    return booleanToValue(false);
                }
            #else
                throw std::runtime_error("To run Luau, use the standard JUSTC build. The current build excludes Luau.");
            #endif
        }
        if (funcName == "Array::join") {
            std::stringstream ss;
            std::string sep = ",";
            if (args.size() > 1) sep = args[1].toString();
            bool first = true;
            for (Value val : args[0].array_elements) {
                if (!first) ss << sep;
                ss << val.toString();
                first = false;
            }
            return stringToValue(ss.str());
        }
        if (funcName == "Array::includes") {
            if (args.size() < 2) return booleanToValue(false);
            bool includes = false;
            for (Value val : args[0].array_elements) {
                includes = Utility::compareValues(val, args[1]);
                if (includes) break;
            }
            return booleanToValue(includes);
        }
        if (funcName == "Array::indexOf") {
            if (args.size() < 2) throw std::runtime_error("Expected value");
            for (size_t i = 0; i < args[0].array_elements.size(); i++) {
                if (Utility::compareValues(args[0].array_elements[i], args[1])) return Value::createNumber(static_cast<double>(i));
            }
            throw std::runtime_error("Value not found in array");
        }
        if (funcName == "Array::lastIndexOf") {
            if (args.size() < 2) throw std::runtime_error("Expected value");
            for (size_t i = args[0].array_elements.size() - 1; i >= 0; i--) {
                if (Utility::compareValues(args[0].array_elements[i], args[1])) return Value::createNumber(static_cast<double>(i));
            }
            throw std::runtime_error("Value not found in array");
        }
        if (funcName == "Array::reverse") {
            std::vector<Value> arr = args[0].array_elements;
            std::reverse(arr.begin(), arr.end());

            Value result = Value::createJsonArray(arr);
            result.name = "Array";
            return result;
        }
        if (funcName == "Array::forEach") {
            if (args.size() < 2 || args[1].type != DataType::FUNCTION) throw std::runtime_error("Expected function");
            Value lastResult = Value::createNull();
            lastResult.type = DataType::NULL_TYPE;

            for (size_t i = 0; i < args[0].array_elements.size(); i++) {
                lastResult = callFunction(args[1], {
                    args[0].array_elements[i], 
                    Value::createNumber(static_cast<double>(i)),
                    args[0]
                }, startPos, doExecute);
            }

            return lastResult;
        }
        if (funcName == "Array::filter") {
            if (args.size() < 2 || args[1].type != DataType::FUNCTION) throw std::runtime_error("Expected function");
            std::vector<Value> arr;
            
            for (size_t i = 0; i < args[0].array_elements.size(); i++) {
                if (callFunction(args[1], {
                    args[0].array_elements[i], 
                    Value::createNumber(static_cast<double>(i)),
                    args[0]
                }, startPos, doExecute).toBoolean()) arr.push_back(args[0].array_elements[i]);
            }

            Value result = Value::createJsonArray(arr);
            result.name = "Array";
            return result;
        }
        if (funcName == "Array::push") {
            if (args.size() < 2) throw std::runtime_error("Expected value");
            if (args[0].type != DataType::JSON_ARRAY) throw std::runtime_error("Expected array");

            if (args[0].isVariable) {
                auto& arr = variables[args[0].variable].array_elements;
                arr.reserve(arr.size() + args.size() - 1);
                for (size_t i = 1; i < args.size(); i++) {
                    arr.push_back(args[i]);
                }
                return variables[args[0].variable];
            } else {
                std::vector<Value> arr;
                arr.reserve(args[0].array_elements.size() + args.size() - 1);
                arr.insert(
                    arr.end(),
                    std::make_move_iterator(const_cast<std::vector<Value>&>(args[0].array_elements).begin()),
                    std::make_move_iterator(const_cast<std::vector<Value>&>(args[0].array_elements).end())
                );
                for (size_t i = 1; i < args.size(); i++) {
                    arr.push_back(args[i]);
                }
                Value result = Value::createJsonArray(std::move(arr));
                result.name = "Array";
                return result;
            }
        }
        if (funcName == "Array::unshift") {
            if (args.size() < 2) throw std::runtime_error("Expected value");
            if (args[0].type != DataType::JSON_ARRAY) throw std::runtime_error("Expected array");

            if (args[0].isVariable) {
                auto& arr = variables[args[0].variable].array_elements;
                arr.reserve(arr.size() + args.size() - 1);
                arr.insert(arr.begin(), args.begin() + 1, args.end());
                return variables[args[0].variable];
            } else {
                std::vector<Value> arr;
                arr.reserve(args[0].array_elements.size() + args.size() - 1);
                arr.insert(arr.end(), args.begin() + 1, args.end());
                arr.insert(
                    arr.end(),
                    std::make_move_iterator(const_cast<std::vector<Value>&>(args[0].array_elements).begin()),
                    std::make_move_iterator(const_cast<std::vector<Value>&>(args[0].array_elements).end())
                );
                Value result = Value::createJsonArray(std::move(arr));
                result.name = "Array";
                return result;
            }
        }
        if (funcName == "Element::render") {
            return Value::createString(renderJSX(args[0]));
        }
        if (funcName == "Window") {
            #ifndef __EMSCRIPTEN__
                hasWindow = true;
                std::vector<Value> defArgs( {Value::createJsonObject({})} );
                Value windowHandle = JUSTCWindow::Create(args.empty() ? defArgs : args, this);
                std::unordered_map<std::string, Value> obj;
                obj["_handle"] = windowHandle;
                
                obj["show"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    bool success = JUSTCWindow::showWindow(windowHandle.getNumericValue<uint64_t>());
                    return Value::createBoolean(success);
                }, "Window.show");
                
                obj["hide"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    bool success = JUSTCWindow::hideWindow(windowHandle.getNumericValue<uint64_t>());
                    return Value::createBoolean(success);
                }, "Window.hide");
                
                obj["close"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    bool success = JUSTCWindow::closeWindow(windowHandle.getNumericValue<uint64_t>());
                    return Value::createBoolean(success);
                }, "Window.close");
                
                obj["getTitle"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        return Value::createString(info.title);
                    } catch (...) {
                        return Value::createString("");
                    }
                }, "Window.getTitle");
                
                obj["setTitle"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    if (args.empty()) {
                        return Value::createBoolean(false);
                    }
                    std::string title = args[0].toString();
                    bool success = JUSTCWindow::setWindowTitle(windowHandle.getNumericValue<uint64_t>(), title);
                    return Value::createBoolean(success);
                }, "Window.setTitle");
                
                obj["getWidth"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        return Value::createNumber(info.width);
                    } catch (...) {
                        return Value::createNumber(0);
                    }
                }, "Window.getWidth");
                
                obj["setWidth"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    if (args.empty()) {
                        return Value::createBoolean(false);
                    }
                    int width = static_cast<int>(args[0].toNumber());
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        bool success = JUSTCWindow::setWindowSize(windowHandle.getNumericValue<uint64_t>(), width, info.height);
                        return Value::createBoolean(success);
                    } catch (...) {
                        return Value::createBoolean(false);
                    }
                }, "Window.setWidth");
                
                obj["getHeight"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        return Value::createNumber(info.height);
                    } catch (...) {
                        return Value::createNumber(0);
                    }
                }, "Window.getHeight");
                
                obj["setHeight"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    if (args.empty()) {
                        return Value::createBoolean(false);
                    }
                    int height = static_cast<int>(args[0].toNumber());
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        bool success = JUSTCWindow::setWindowSize(windowHandle.getNumericValue<uint64_t>(), info.width, height);
                        return Value::createBoolean(success);
                    } catch (...) {
                        return Value::createBoolean(false);
                    }
                }, "Window.setHeight");
                
                obj["getSize"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        std::unordered_map<std::string, Value> result;
                        result["width"] = Value::createNumber(info.width);
                        result["height"] = Value::createNumber(info.height);
                        return Value::createJsonObject(result);
                    } catch (...) {
                        return Value::createJsonObject({});
                    }
                }, "Window.getSize");
                
                obj["setSize"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    if (args.size() < 2) {
                        return Value::createBoolean(false);
                    }
                    int width = static_cast<int>(args[0].toNumber());
                    int height = static_cast<int>(args[1].toNumber());
                    bool success = JUSTCWindow::setWindowSize(windowHandle.getNumericValue<uint64_t>(), width, height);
                    return Value::createBoolean(success);
                }, "Window.setSize");
                
                obj["getX"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        return Value::createNumber(info.x);
                    } catch (...) {
                        return Value::createNumber(0);
                    }
                }, "Window.getX");
                
                obj["setX"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    if (args.empty()) {
                        return Value::createBoolean(false);
                    }
                    int x = static_cast<int>(args[0].toNumber());
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        bool success = JUSTCWindow::setWindowPosition(windowHandle.getNumericValue<uint64_t>(), x, info.y);
                        return Value::createBoolean(success);
                    } catch (...) {
                        return Value::createBoolean(false);
                    }
                }, "Window.setX");
                
                obj["getY"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        return Value::createNumber(info.y);
                    } catch (...) {
                        return Value::createNumber(0);
                    }
                }, "Window.getY");
                
                obj["setY"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    if (args.empty()) {
                        return Value::createBoolean(false);
                    }
                    int y = static_cast<int>(args[0].toNumber());
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        bool success = JUSTCWindow::setWindowPosition(windowHandle.getNumericValue<uint64_t>(), info.x, y);
                        return Value::createBoolean(success);
                    } catch (...) {
                        return Value::createBoolean(false);
                    }
                }, "Window.setY");
                
                obj["getPosition"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        std::unordered_map<std::string, Value> result;
                        result["x"] = Value::createNumber(info.x);
                        result["y"] = Value::createNumber(info.y);
                        return Value::createJsonObject(result);
                    } catch (...) {
                        return Value::createJsonObject({});
                    }
                }, "Window.getPosition");
                
                obj["setPosition"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    if (args.size() < 2) {
                        return Value::createBoolean(false);
                    }
                    int x = static_cast<int>(args[0].toNumber());
                    int y = static_cast<int>(args[1].toNumber());
                    bool success = JUSTCWindow::setWindowPosition(windowHandle.getNumericValue<uint64_t>(), x, y);
                    return Value::createBoolean(success);
                }, "Window.setPosition");
                
                obj["isVisible"] = createFunction([windowHandle](const std::vector<Value>& args) -> Value {
                    try {
                        JUSTCWindow::WindowInfo info = JUSTCWindow::getWindowInfo(windowHandle.getNumericValue<uint64_t>());
                        return Value::createBoolean(info.isVisible);
                    } catch (...) {
                        return Value::createBoolean(false);
                    }
                }, "Window.isVisible");

                obj["runMessageLoop"] = createFunction([](const std::vector<Value>& args) -> Value {
                    return JUSTCWindow::RunMessageLoop(args);
                }, "Window.runMessageLoop");

                Value result = Value::createJsonObject(obj);
                result.name = "Window";
                return result;
            #else
                throw std::runtime_error("JUSTC Window is not supported in WebAssembly builds.");
            #endif
        }
        if (funcName == "Set") {
            Value result;
            result.type = DataType::SET;
            std::unordered_set<Value> arr;
            arr.reserve(args.size());
            for (Value val : args) {
                arr.insert(val.toPrimitive());
            }
            result.setComplexData(arr);
            result.name = "Set";
            return result;
        }
        if (funcName == "Map") {
            Value result;
            result.type = DataType::MAP;
            std::unordered_map<Value, Value> map;
            result.setComplexData(map);
            result.name = "Map";
            return result;
        }
        if (funcName == "Int8Array") {
            Value result;
            result.type = DataType::INT8_ARRAY;
            std::vector<int8_t> arr;
            arr.reserve(args.size());
            for (Value val : args) {
                arr.push_back(val.getNumericValue<int8_t>());
            }
            result.setComplexData(arr);
            result.name = "Int8 Array";
            return result;
        }
        if (funcName == "Int16Array") {
            Value result;
            result.type = DataType::INT16_ARRAY;
            std::vector<int16_t> arr;
            arr.reserve(args.size());
            for (Value val : args) {
                arr.push_back(val.getNumericValue<int16_t>());
            }
            result.setComplexData(arr);
            result.name = "Int16 Array";
            return result;
        }
        if (funcName == "Int32Array") {
            Value result;
            result.type = DataType::INT32_ARRAY;
            std::vector<int32_t> arr;
            arr.reserve(args.size());
            for (Value val : args) {
                arr.push_back(val.getNumericValue<int32_t>());
            }
            result.setComplexData(arr);
            result.name = "Int32 Array";
            return result;
        }
        if (funcName == "Int64Array") {
            Value result;
            result.type = DataType::INT64_ARRAY;
            std::vector<int64_t> arr;
            arr.reserve(args.size());
            for (Value val : args) {
                arr.push_back(val.getNumericValue<int64_t>());
            }
            result.setComplexData(arr);
            result.name = "Int32 Array";
            return result;
        }
        if (funcName == "UInt8Array" || funcName == "CUInt8Array") {
            Value result;
            result.type = funcName == "UInt8Array" ? DataType::UINT8_ARRAY : DataType::CUINT8_ARRAY;
            std::vector<uint8_t> arr;
            arr.reserve(args.size());
            for (Value val : args) {
                arr.push_back(val.getNumericValue<uint8_t>());
            }
            result.setComplexData(arr);
            result.name = funcName == "UInt8Array" ? "UInt8 Array" : "UInt8 clamped Array";
            return result;
        }
        if (funcName == "UInt16Array" || funcName == "CUInt16Array") {
            Value result;
            result.type = funcName == "UInt16Array" ? DataType::UINT16_ARRAY : DataType::CUINT16_ARRAY;
            std::vector<uint16_t> arr;
            arr.reserve(args.size());
            for (Value val : args) {
                arr.push_back(val.getNumericValue<uint16_t>());
            }
            result.setComplexData(arr);
            result.name = funcName == "UInt16Array" ? "UInt16 Array" : "UInt16 clamped Array";
            return result;
        }
        if (funcName == "UInt32Array" || funcName == "CUInt32Array") {
            Value result;
            result.type = funcName == "UInt32Array" ? DataType::UINT32_ARRAY : DataType::CUINT32_ARRAY;
            std::vector<uint32_t> arr;
            arr.reserve(args.size());
            for (Value val : args) {
                arr.push_back(val.getNumericValue<uint32_t>());
            }
            result.setComplexData(arr);
            result.name = funcName == "UInt32Array" ? "UInt32 Array" : "UInt32 clamped Array";
            return result;
        }
        if (funcName == "UInt64Array" || funcName == "CUInt64Array") {
            Value result;
            result.type = funcName == "UInt64Array" ? DataType::UINT64_ARRAY : DataType::CUINT64_ARRAY;
            std::vector<uint64_t> arr;
            arr.reserve(args.size());
            for (Value val : args) {
                arr.push_back(val.getNumericValue<uint64_t>());
            }
            result.setComplexData(arr);
            result.name = funcName == "UInt64Array" ? "UInt64 Array" : "UInt64 clamped Array";
            return result;
        }
        if (funcName == "Float32Array") {
            Value result;
            result.type = DataType::FLOAT32_ARRAY;
            std::vector<float> arr;
            arr.reserve(args.size());
            for (Value val : args) {
                arr.push_back(val.getNumericValue<float>());
            }
            result.setComplexData(arr);
            result.name = "Float32 Array";
            return result;
        }
        if (funcName == "Float64Array") {
            Value result;
            result.type = DataType::FLOAT64_ARRAY;
            std::vector<double> arr;
            arr.reserve(args.size());
            for (Value val : args) {
                arr.push_back(val.getNumericValue<double>());
            }
            result.setComplexData(arr);
            result.name = "Float64 Array";
            return result;
        }
        if (funcName == "hash") {
            std::hash<Value> hasher;
            uint64_t hash = static_cast<uint64_t>(hasher(args[0]));
            return Value::createNumberWithType(hash, NumericType::UINT64);
        }
        if (funcName == "system.env") {
            const std::pair<bool, std::string> getEnv = Utility::env(args[0].toString());
            if (!getEnv.first) return Value::createNull();
            return Value::createString(getEnv.second);
        }
        if (funcName == "Int8Array::compress") {
            if (args[0].type != DataType::INT8_ARRAY) throw std::runtime_error("Expected Int8 Array");
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = DataType::INT8_ARRAY;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<int8_t> arr = args[0].getComplexData<std::vector<int8_t>>();
            std::vector<int8_t> compressed;

            int lvl = 6;
            if (args.size() > 2) lvl = static_cast<int>(args[2].toNumber());

            if (alg == "zlib") compressed = ZLIB::CompressI8(arr, lvl);
            else if (alg == "gzip") compressed = GZIP::CompressI8(arr, lvl);
            else if (alg == "bzip2") compressed = BZIP2::CompressI8(arr, lvl);
            else if (alg == "lzma") compressed = LZMA::CompressI8(arr, lvl);
            else if (alg == "zstd") compressed = ZSTD::CompressI8(arr, lvl);
            else if (alg == "lz4") compressed = LZ4::CompressI8(arr, lvl);
            else if (alg == "snappy") compressed = SNAPPY::CompressI8(arr, lvl);
            else if (alg == "deflate") compressed = DEFLATE::CompressI8(arr, lvl);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(compressed);
            result.name = "Int8 Array";
            return result;
        }
        if (funcName == "Int16Array::compress") {
            if (args[0].type != DataType::INT16_ARRAY) throw std::runtime_error("Expected Int16 Array");
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = DataType::INT16_ARRAY;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<int16_t> arr = args[0].getComplexData<std::vector<int16_t>>();
            std::vector<int16_t> compressed;

            int lvl = 6;
            if (args.size() > 2) lvl = static_cast<int>(args[2].toNumber());

            if (alg == "zlib") compressed = ZLIB::CompressI16(arr, lvl);
            else if (alg == "gzip") compressed = GZIP::CompressI16(arr, lvl);
            else if (alg == "bzip2") compressed = BZIP2::CompressI16(arr, lvl);
            else if (alg == "lzma") compressed = LZMA::CompressI16(arr, lvl);
            else if (alg == "zstd") compressed = ZSTD::CompressI16(arr, lvl);
            else if (alg == "lz4") compressed = LZ4::CompressI16(arr, lvl);
            else if (alg == "snappy") compressed = SNAPPY::CompressI16(arr, lvl);
            else if (alg == "deflate") compressed = DEFLATE::CompressI16(arr, lvl);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(compressed);
            result.name = "Int16 Array";
            return result;
        }
        if (funcName == "Int32Array::compress") {
            if (args[0].type != DataType::INT32_ARRAY) throw std::runtime_error("Expected Int32 Array");
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = DataType::INT32_ARRAY;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<int32_t> arr = args[0].getComplexData<std::vector<int32_t>>();
            std::vector<int32_t> compressed;

            int lvl = 6;
            if (args.size() > 2) lvl = static_cast<int>(args[2].toNumber());

            if (alg == "zlib") compressed = ZLIB::CompressI32(arr, lvl);
            else if (alg == "gzip") compressed = GZIP::CompressI32(arr, lvl);
            else if (alg == "bzip2") compressed = BZIP2::CompressI32(arr, lvl);
            else if (alg == "lzma") compressed = LZMA::CompressI32(arr, lvl);
            else if (alg == "zstd") compressed = ZSTD::CompressI32(arr, lvl);
            else if (alg == "lz4") compressed = LZ4::CompressI32(arr, lvl);
            else if (alg == "snappy") compressed = SNAPPY::CompressI32(arr, lvl);
            else if (alg == "deflate") compressed = DEFLATE::CompressI32(arr, lvl);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(compressed);
            result.name = "Int32 Array";
            return result;
        }
        if (funcName == "Int64Array::compress") {
            if (args[0].type != DataType::INT64_ARRAY) throw std::runtime_error("Expected Int64 Array");
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = DataType::INT64_ARRAY;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<int64_t> arr = args[0].getComplexData<std::vector<int64_t>>();
            std::vector<int64_t> compressed;

            int lvl = 6;
            if (args.size() > 2) lvl = static_cast<int>(args[2].toNumber());

            if (alg == "zlib") compressed = ZLIB::CompressI64(arr, lvl);
            else if (alg == "gzip") compressed = GZIP::CompressI64(arr, lvl);
            else if (alg == "bzip2") compressed = BZIP2::CompressI64(arr, lvl);
            else if (alg == "lzma") compressed = LZMA::CompressI64(arr, lvl);
            else if (alg == "zstd") compressed = ZSTD::CompressI64(arr, lvl);
            else if (alg == "lz4") compressed = LZ4::CompressI64(arr, lvl);
            else if (alg == "snappy") compressed = SNAPPY::CompressI64(arr, lvl);
            else if (alg == "deflate") compressed = DEFLATE::CompressI64(arr, lvl);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(compressed);
            result.name = "Int64 Array";
            return result;
        }
        if (funcName == "UInt8Array::compress" || funcName == "CUInt8Array::compress") {
            DataType t = funcName == "UInt8Array::compress" ? DataType::UINT8_ARRAY : DataType::CUINT8_ARRAY;
            std::string n = "UInt8 " + std::string(t == DataType::CUINT8_ARRAY ? "clamped " : "") + "Array";

            if (args[0].type != t) throw std::runtime_error("Expected " + n);
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = t;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<uint8_t> arr = args[0].getComplexData<std::vector<uint8_t>>();
            std::vector<uint8_t> compressed;

            int lvl = 6;
            if (args.size() > 2) lvl = static_cast<int>(args[2].toNumber());

            if (alg == "zlib") compressed = ZLIB::CompressU8(arr, lvl);
            else if (alg == "gzip") compressed = GZIP::CompressU8(arr, lvl);
            else if (alg == "bzip2") compressed = BZIP2::CompressU8(arr, lvl);
            else if (alg == "lzma") compressed = LZMA::CompressU8(arr, lvl);
            else if (alg == "zstd") compressed = ZSTD::CompressU8(arr, lvl);
            else if (alg == "lz4") compressed = LZ4::CompressU8(arr, lvl);
            else if (alg == "snappy") compressed = SNAPPY::CompressU8(arr, lvl);
            else if (alg == "deflate") compressed = DEFLATE::CompressU8(arr, lvl);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(compressed);
            result.name = n;
            return result;
        }
        if (funcName == "UInt16Array::compress" || funcName == "CUInt16Array::compress") {
            DataType t = funcName == "UInt16Array::compress" ? DataType::UINT16_ARRAY : DataType::CUINT16_ARRAY;
            std::string n = "UInt16 " + std::string(t == DataType::CUINT16_ARRAY ? "clamped " : "") + "Array";

            if (args[0].type != t) throw std::runtime_error("Expected " + n);
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = t;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<uint16_t> arr = args[0].getComplexData<std::vector<uint16_t>>();
            std::vector<uint16_t> compressed;

            int lvl = 6;
            if (args.size() > 2) lvl = static_cast<int>(args[2].toNumber());

            if (alg == "zlib") compressed = ZLIB::CompressU16(arr, lvl);
            else if (alg == "gzip") compressed = GZIP::CompressU16(arr, lvl);
            else if (alg == "bzip2") compressed = BZIP2::CompressU16(arr, lvl);
            else if (alg == "lzma") compressed = LZMA::CompressU16(arr, lvl);
            else if (alg == "zstd") compressed = ZSTD::CompressU16(arr, lvl);
            else if (alg == "lz4") compressed = LZ4::CompressU16(arr, lvl);
            else if (alg == "snappy") compressed = SNAPPY::CompressU16(arr, lvl);
            else if (alg == "deflate") compressed = DEFLATE::CompressU16(arr, lvl);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(compressed);
            result.name = n;
            return result;
        }
        if (funcName == "UInt32Array::compress" || funcName == "CUInt32Array::compress") {
            DataType t = funcName == "UInt32Array::compress" ? DataType::UINT32_ARRAY : DataType::CUINT32_ARRAY;
            std::string n = "UInt32 " + std::string(t == DataType::CUINT32_ARRAY ? "clamped " : "") + "Array";

            if (args[0].type != t) throw std::runtime_error("Expected " + n);
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = t;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<uint32_t> arr = args[0].getComplexData<std::vector<uint32_t>>();
            std::vector<uint32_t> compressed;

            int lvl = 6;
            if (args.size() > 2) lvl = static_cast<int>(args[2].toNumber());

            if (alg == "zlib") compressed = ZLIB::CompressU32(arr, lvl);
            else if (alg == "gzip") compressed = GZIP::CompressU32(arr, lvl);
            else if (alg == "bzip2") compressed = BZIP2::CompressU32(arr, lvl);
            else if (alg == "lzma") compressed = LZMA::CompressU32(arr, lvl);
            else if (alg == "zstd") compressed = ZSTD::CompressU32(arr, lvl);
            else if (alg == "lz4") compressed = LZ4::CompressU32(arr, lvl);
            else if (alg == "snappy") compressed = SNAPPY::CompressU32(arr, lvl);
            else if (alg == "deflate") compressed = DEFLATE::CompressU32(arr, lvl);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(compressed);
            result.name = n;
            return result;
        }
        if (funcName == "UInt64Array::compress" || funcName == "CUInt64Array::compress") {
            DataType t = funcName == "UInt64Array::compress" ? DataType::UINT64_ARRAY : DataType::CUINT64_ARRAY;
            std::string n = "UInt64 " + std::string(t == DataType::CUINT64_ARRAY ? "clamped " : "") + "Array";

            if (args[0].type != t) throw std::runtime_error("Expected " + n);
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = t;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<uint64_t> arr = args[0].getComplexData<std::vector<uint64_t>>();
            std::vector<uint64_t> compressed;

            int lvl = 6;
            if (args.size() > 2) lvl = static_cast<int>(args[2].toNumber());

            if (alg == "zlib") compressed = ZLIB::CompressU64(arr, lvl);
            else if (alg == "gzip") compressed = GZIP::CompressU64(arr, lvl);
            else if (alg == "bzip2") compressed = BZIP2::CompressU64(arr, lvl);
            else if (alg == "lzma") compressed = LZMA::CompressU64(arr, lvl);
            else if (alg == "zstd") compressed = ZSTD::CompressU64(arr, lvl);
            else if (alg == "lz4") compressed = LZ4::CompressU64(arr, lvl);
            else if (alg == "snappy") compressed = SNAPPY::CompressU64(arr, lvl);
            else if (alg == "deflate") compressed = DEFLATE::CompressU64(arr, lvl);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(compressed);
            result.name = n;
            return result;
        }
        if (funcName == "Int8Array::decompress") {
            if (args[0].type != DataType::INT8_ARRAY) throw std::runtime_error("Expected Int8 Array");
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = DataType::INT8_ARRAY;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<int8_t> arr = args[0].getComplexData<std::vector<int8_t>>();
            std::vector<int8_t> decompressed;

            if (alg == "zlib") decompressed = ZLIB::DecompressI8(arr);
            else if (alg == "gzip") decompressed = GZIP::DecompressI8(arr);
            else if (alg == "bzip2") decompressed = BZIP2::DecompressI8(arr);
            else if (alg == "lzma") decompressed = LZMA::DecompressI8(arr);
            else if (alg == "zstd") decompressed = ZSTD::DecompressI8(arr);
            else if (alg == "lz4") decompressed = LZ4::DecompressI8(arr);
            else if (alg == "snappy") decompressed = SNAPPY::DecompressI8(arr);
            else if (alg == "deflate") decompressed = DEFLATE::DecompressI8(arr);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(decompressed);
            result.name = "Int8 Array";
            return result;
        }
        if (funcName == "Int16Array::decompress") {
            if (args[0].type != DataType::INT16_ARRAY) throw std::runtime_error("Expected Int16 Array");
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = DataType::INT16_ARRAY;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<int16_t> arr = args[0].getComplexData<std::vector<int16_t>>();
            std::vector<int16_t> decompressed;

            if (alg == "zlib") decompressed = ZLIB::DecompressI16(arr);
            else if (alg == "gzip") decompressed = GZIP::DecompressI16(arr);
            else if (alg == "bzip2") decompressed = BZIP2::DecompressI16(arr);
            else if (alg == "lzma") decompressed = LZMA::DecompressI16(arr);
            else if (alg == "zstd") decompressed = ZSTD::DecompressI16(arr);
            else if (alg == "lz4") decompressed = LZ4::DecompressI16(arr);
            else if (alg == "snappy") decompressed = SNAPPY::DecompressI16(arr);
            else if (alg == "deflate") decompressed = DEFLATE::DecompressI16(arr);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(decompressed);
            result.name = "Int16 Array";
            return result;
        }
        if (funcName == "Int32Array::decompress") {
            if (args[0].type != DataType::INT32_ARRAY) throw std::runtime_error("Expected Int32 Array");
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = DataType::INT32_ARRAY;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<int32_t> arr = args[0].getComplexData<std::vector<int32_t>>();
            std::vector<int32_t> decompressed;

            if (alg == "zlib") decompressed = ZLIB::DecompressI32(arr);
            else if (alg == "gzip") decompressed = GZIP::DecompressI32(arr);
            else if (alg == "bzip2") decompressed = BZIP2::DecompressI32(arr);
            else if (alg == "lzma") decompressed = LZMA::DecompressI32(arr);
            else if (alg == "zstd") decompressed = ZSTD::DecompressI32(arr);
            else if (alg == "lz4") decompressed = LZ4::DecompressI32(arr);
            else if (alg == "snappy") decompressed = SNAPPY::DecompressI32(arr);
            else if (alg == "deflate") decompressed = DEFLATE::DecompressI32(arr);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(decompressed);
            result.name = "Int32 Array";
            return result;
        }
        if (funcName == "Int64Array::decompress") {
            if (args[0].type != DataType::INT64_ARRAY) throw std::runtime_error("Expected Int64 Array");
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = DataType::INT64_ARRAY;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<int64_t> arr = args[0].getComplexData<std::vector<int64_t>>();
            std::vector<int64_t> decompressed;

            if (alg == "zlib") decompressed = ZLIB::DecompressI64(arr);
            else if (alg == "gzip") decompressed = GZIP::DecompressI64(arr);
            else if (alg == "bzip2") decompressed = BZIP2::DecompressI64(arr);
            else if (alg == "lzma") decompressed = LZMA::DecompressI64(arr);
            else if (alg == "zstd") decompressed = ZSTD::DecompressI64(arr);
            else if (alg == "lz4") decompressed = LZ4::DecompressI64(arr);
            else if (alg == "snappy") decompressed = SNAPPY::DecompressI64(arr);
            else if (alg == "deflate") decompressed = DEFLATE::DecompressI64(arr);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(decompressed);
            result.name = "Int64 Array";
            return result;
        }
        if (funcName == "UInt8Array::decompress" || funcName == "CUInt8Array::decompress") {
            DataType t = funcName == "UInt8Array::compress" ? DataType::UINT8_ARRAY : DataType::CUINT8_ARRAY;
            std::string n = "UInt8 " + std::string(t == DataType::CUINT8_ARRAY ? "clamped " : "") + "Array";

            if (args[0].type != t) throw std::runtime_error("Expected " + n);
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = t;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<uint8_t> arr = args[0].getComplexData<std::vector<uint8_t>>();
            std::vector<uint8_t> decompressed;

            if (alg == "zlib") decompressed = ZLIB::DecompressU8(arr);
            else if (alg == "gzip") decompressed = GZIP::DecompressU8(arr);
            else if (alg == "bzip2") decompressed = BZIP2::DecompressU8(arr);
            else if (alg == "lzma") decompressed = LZMA::DecompressU8(arr);
            else if (alg == "zstd") decompressed = ZSTD::DecompressU8(arr);
            else if (alg == "lz4") decompressed = LZ4::DecompressU8(arr);
            else if (alg == "snappy") decompressed = SNAPPY::DecompressU8(arr);
            else if (alg == "deflate") decompressed = DEFLATE::DecompressU8(arr);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(decompressed);
            result.name = n;
            return result;
        }
        if (funcName == "UInt16Array::decompress" || funcName == "CUInt16Array::decompress") {
            DataType t = funcName == "UInt16Array::compress" ? DataType::UINT16_ARRAY : DataType::CUINT16_ARRAY;
            std::string n = "UInt16 " + std::string(t == DataType::CUINT16_ARRAY ? "clamped " : "") + "Array";

            if (args[0].type != t) throw std::runtime_error("Expected " + n);
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = t;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<uint16_t> arr = args[0].getComplexData<std::vector<uint16_t>>();
            std::vector<uint16_t> decompressed;

            if (alg == "zlib") decompressed = ZLIB::DecompressU16(arr);
            else if (alg == "gzip") decompressed = GZIP::DecompressU16(arr);
            else if (alg == "bzip2") decompressed = BZIP2::DecompressU16(arr);
            else if (alg == "lzma") decompressed = LZMA::DecompressU16(arr);
            else if (alg == "zstd") decompressed = ZSTD::DecompressU16(arr);
            else if (alg == "lz4") decompressed = LZ4::DecompressU16(arr);
            else if (alg == "snappy") decompressed = SNAPPY::DecompressU16(arr);
            else if (alg == "deflate") decompressed = DEFLATE::DecompressU16(arr);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(decompressed);
            result.name = n;
            return result;
        }
        if (funcName == "UInt32Array::decompress" || funcName == "CUInt32Array::decompress") {
            DataType t = funcName == "UInt32Array::compress" ? DataType::UINT32_ARRAY : DataType::CUINT32_ARRAY;
            std::string n = "UInt32 " + std::string(t == DataType::CUINT32_ARRAY ? "clamped " : "") + "Array";

            if (args[0].type != t) throw std::runtime_error("Expected " + n);
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = t;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<uint32_t> arr = args[0].getComplexData<std::vector<uint32_t>>();
            std::vector<uint32_t> decompressed;

            if (alg == "zlib") decompressed = ZLIB::DecompressU32(arr);
            else if (alg == "gzip") decompressed = GZIP::DecompressU32(arr);
            else if (alg == "bzip2") decompressed = BZIP2::DecompressU32(arr);
            else if (alg == "lzma") decompressed = LZMA::DecompressU32(arr);
            else if (alg == "zstd") decompressed = ZSTD::DecompressU32(arr);
            else if (alg == "lz4") decompressed = LZ4::DecompressU32(arr);
            else if (alg == "snappy") decompressed = SNAPPY::DecompressU32(arr);
            else if (alg == "deflate") decompressed = DEFLATE::DecompressU32(arr);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(decompressed);
            result.name = n;
            return result;
        }
        if (funcName == "UInt64Array::decompress" || funcName == "CUInt64Array::decompress") {
            DataType t = funcName == "UInt64Array::compress" ? DataType::UINT64_ARRAY : DataType::CUINT64_ARRAY;
            std::string n = "UInt64 " + std::string(t == DataType::CUINT64_ARRAY ? "clamped " : "") + "Array";

            if (args[0].type != t) throw std::runtime_error("Expected " + n);
            if (args.size() < 2) throw std::runtime_error("Expected algorithm");
            Value result;
            result.type = t;
            std::string alg = Unicode::Lower(args[1].toString());
            const std::vector<uint64_t> arr = args[0].getComplexData<std::vector<uint64_t>>();
            std::vector<uint64_t> decompressed;

            if (alg == "zlib") decompressed = ZLIB::DecompressU64(arr);
            else if (alg == "gzip") decompressed = GZIP::DecompressU64(arr);
            else if (alg == "bzip2") decompressed = BZIP2::DecompressU64(arr);
            else if (alg == "lzma") decompressed = LZMA::DecompressU64(arr);
            else if (alg == "zstd") decompressed = ZSTD::DecompressU64(arr);
            else if (alg == "lz4") decompressed = LZ4::DecompressU64(arr);
            else if (alg == "snappy") decompressed = SNAPPY::DecompressU64(arr);
            else if (alg == "deflate") decompressed = DEFLATE::DecompressU64(arr);
            else throw std::runtime_error("Unknown algorithm \"" + args[1].toString() + "\"");

            result.setComplexData(decompressed);
            result.name = n;
            return result;
        }
        if (funcName == "Map::set") {
            if (args.size() < 2) throw std::runtime_error("Expected key");
            if (args.size() < 3) throw std::runtime_error("Expected value");
            if (args[0].type != DataType::MAP) throw std::runtime_error("Expected map");

            if (args[0].isVariable) {
                std::unordered_map<Value, Value> map;
                if (args[0].complex_value) {
                    try {
                        map = args[0].getComplexData<std::unordered_map<Value, Value>>();
                    } catch (...) {
                        map = std::unordered_map<Value, Value>();
                    }
                }
                map[args[1].toPrimitive()] = args[2];
                variables[args[0].variable].setComplexData(map);
                return variables[args[0].variable];
            } else {
                std::unordered_map<Value, Value> map;
                if (args[0].complex_value) {
                    try {
                        map = args[0].getComplexData<std::unordered_map<Value, Value>>();
                    } catch (...) {
                        map = std::unordered_map<Value, Value>();
                    }
                }
                map[args[1].toPrimitive()] = args[2];
                Value result = args[0];
                result.setComplexData(map);
                return result;
            }
        }
        if (funcName == "Map::get") {
            if (args.size() < 2) throw std::runtime_error("Expected key");
            if (args[0].type != DataType::MAP) throw std::runtime_error("Expected map");

            std::unordered_map<Value, Value> map;
            if (args[0].isVariable) {
                map = variables[args[0].variable].getComplexData<std::unordered_map<Value, Value>>();
            } else {
                map = args[0].getComplexData<std::unordered_map<Value, Value>>();
            }
            
            auto it = map.find(args[1].toPrimitive());
            if (it != map.end()) {
                return it->second;
            }
            return Value::createNull();
        }
        if (funcName == "Map::has") {
            if (args.size() < 2) throw std::runtime_error("Expected key");
            if (args[0].type != DataType::MAP) throw std::runtime_error("Expected map");

            std::unordered_map<Value, Value> map;
            if (args[0].isVariable) {
                map = variables[args[0].variable].getComplexData<std::unordered_map<Value, Value>>();
            } else {
                map = args[0].getComplexData<std::unordered_map<Value, Value>>();
            }

            auto it = map.find(args[1].toPrimitive());
            if (it != map.end()) {
                return Value::createBoolean(true);
            }
            return Value::createBoolean(false);
        }
        if (funcName == "Map::delete") {
            if (args.size() < 2) throw std::runtime_error("Expected key");
            if (args[0].type != DataType::MAP) throw std::runtime_error("Expected map");

            if (args[0].isVariable) {
                std::unordered_map<Value, Value> map = variables[args[0].variable].getComplexData<std::unordered_map<Value, Value>>();
                map.erase(args[1].toPrimitive());
                variables[args[0].variable].setComplexData(map);
                return variables[args[0].variable];
            } else {
                std::unordered_map<Value, Value> map = args[0].getComplexData<std::unordered_map<Value, Value>>();
                map.erase(args[1].toPrimitive());
                Value result = args[0];
                result.setComplexData(map);
                return result;
            }
        }
        if (funcName == "Map::clear") {
            if (args[0].type != DataType::MAP) throw std::runtime_error("Expected map");

            std::unordered_map<Value, Value> map;
            if (args[0].isVariable) {
                variables[args[0].variable].setComplexData(map);
                return variables[args[0].variable];
            } else {
                Value result = args[0];
                result.setComplexData(map);
                result.name = "Map";
                return result;
            }
        }
        if (funcName == "Set::add") {
            if (args.size() < 2) throw std::runtime_error("Expected value");
            if (args[0].type != DataType::SET) throw std::runtime_error("Expected set");

            if (args[0].isVariable) {
                std::unordered_set<Value> set;
                if (args[0].complex_value) {
                    try {
                        set = args[0].getComplexData<std::unordered_set<Value>>();
                    } catch (...) {
                        set = std::unordered_set<Value>();
                    }
                }
                set.insert(args[1].toPrimitive());
                variables[args[0].variable].setComplexData(set);
                return variables[args[0].variable];
            } else {
                std::unordered_set<Value> set;
                if (args[0].complex_value) {
                    try {
                        set = args[0].getComplexData<std::unordered_set<Value>>();
                    } catch (...) {
                        set = std::unordered_set<Value>();
                    }
                }
                set.insert(args[1].toPrimitive());
                Value result = args[0];
                result.setComplexData(set);
                return result;
            }
        }
        if (funcName == "Set::has") {
            if (args.size() < 2) throw std::runtime_error("Expected value");
            if (args[0].type != DataType::SET) throw std::runtime_error("Expected set");

            std::unordered_set<Value> set;
            if (args[0].isVariable) {
                set = variables[args[0].variable].getComplexData<std::unordered_set<Value>>();
            } else {
                set = args[0].getComplexData<std::unordered_set<Value>>();
            }

            auto it = set.find(args[1].toPrimitive());
            if (it != set.end()) {
                return Value::createBoolean(true);
            }
            return Value::createBoolean(false);
        }
        if (funcName == "Set::delete") {
            if (args.size() < 2) throw std::runtime_error("Expected value");
            if (args[0].type != DataType::SET) throw std::runtime_error("Expected set");

            if (args[0].isVariable) {
                std::unordered_set<Value> set = variables[args[0].variable].getComplexData<std::unordered_set<Value>>();
                set.erase(args[1].toPrimitive());
                variables[args[0].variable].setComplexData(set);
                return variables[args[0].variable];
            } else {
                std::unordered_set<Value> set = args[0].getComplexData<std::unordered_set<Value>>();
                set.erase(args[1].toPrimitive());
                Value result = args[0];
                result.setComplexData(set);
                return result;
            }
        }
        if (funcName == "Set::clear") {
            if (args[0].type != DataType::SET) throw std::runtime_error("Expected set");

            std::unordered_set<Value> set;
            if (args[0].isVariable) {
                variables[args[0].variable].setComplexData(set);
                return variables[args[0].variable];
            } else {
                Value result = args[0];
                result.setComplexData(set);
                result.name = "Set";
                return result;
            }
        }
        if (funcName == "Object::size") {
            uint64_t size = static_cast<uint64_t>(args[0].properties.size());
            return Value::createNumberWithType(size, NumericType::UINT64);
        }
        if (funcName == "Array::size") {
            uint64_t size = static_cast<uint64_t>(args[0].array_elements.size());
            return Value::createNumberWithType(size, NumericType::UINT64);
        }
        if (funcName == "Map::size" || funcName == "Set::size") {
            if (args[0].complex_value) {
                if (funcName == "Map::size") {
                    auto map = args[0].getComplexData<std::unordered_map<Value, Value>>();
                    return Value::createNumberWithType(static_cast<uint64_t>(map.size()), NumericType::UINT64);
                } else {
                    auto set = args[0].getComplexData<std::unordered_set<Value>>();
                    return Value::createNumberWithType(static_cast<uint64_t>(set.size()), NumericType::UINT64);
                }
            }
            return Value::createNumberWithType(static_cast<uint64_t>(0), NumericType::UINT64);
        }
        if (funcName == "Int8Array::size" || funcName == "Int16Array::size" || 
            funcName == "Int32Array::size" || funcName == "Int64Array::size" ||
            funcName == "UInt8Array::size" || funcName == "UInt16Array::size" || 
            funcName == "UInt32Array::size" || funcName == "UInt64Array::size" ||
            funcName == "CUInt8Array::size" || funcName == "CUInt16Array::size" || 
            funcName == "CUInt32Array::size" || funcName == "CUInt64Array::size" ||
            funcName == "Float32Array::size" || funcName == "Float64Array::size"
        ) {
            if (args[0].complex_value) {
                try {
                    if (args[0].type == DataType::INT8_ARRAY) {
                        auto vec = args[0].getComplexData<std::vector<int8_t>>();
                        return Value::createNumberWithType(static_cast<uint64_t>(vec.size()), NumericType::UINT64);
                    } else if (args[0].type == DataType::INT16_ARRAY) {
                        auto vec = args[0].getComplexData<std::vector<int16_t>>();
                        return Value::createNumberWithType(static_cast<uint64_t>(vec.size()), NumericType::UINT64);
                    } else if (args[0].type == DataType::INT32_ARRAY) {
                        auto vec = args[0].getComplexData<std::vector<int32_t>>();
                        return Value::createNumberWithType(static_cast<uint64_t>(vec.size()), NumericType::UINT64);
                    } else if (args[0].type == DataType::INT64_ARRAY) {
                        auto vec = args[0].getComplexData<std::vector<int64_t>>();
                        return Value::createNumberWithType(static_cast<uint64_t>(vec.size()), NumericType::UINT64);
                    } else if (args[0].type == DataType::UINT8_ARRAY || args[0].type == DataType::CUINT8_ARRAY) {
                        auto vec = args[0].getComplexData<std::vector<uint8_t>>();
                        return Value::createNumberWithType(static_cast<uint64_t>(vec.size()), NumericType::UINT64);
                    } else if (args[0].type == DataType::UINT16_ARRAY || args[0].type == DataType::CUINT16_ARRAY) {
                        auto vec = args[0].getComplexData<std::vector<uint16_t>>();
                        return Value::createNumberWithType(static_cast<uint64_t>(vec.size()), NumericType::UINT64);
                    } else if (args[0].type == DataType::UINT32_ARRAY || args[0].type == DataType::CUINT32_ARRAY) {
                        auto vec = args[0].getComplexData<std::vector<uint32_t>>();
                        return Value::createNumberWithType(static_cast<uint64_t>(vec.size()), NumericType::UINT64);
                    } else if (args[0].type == DataType::UINT64_ARRAY || args[0].type == DataType::CUINT64_ARRAY) {
                        auto vec = args[0].getComplexData<std::vector<uint64_t>>();
                        return Value::createNumberWithType(static_cast<uint64_t>(vec.size()), NumericType::UINT64);
                    } else if (args[0].type == DataType::FLOAT32_ARRAY) {
                        auto vec = args[0].getComplexData<std::vector<float>>();
                        return Value::createNumberWithType(static_cast<uint64_t>(vec.size()), NumericType::UINT64);
                    } else if (args[0].type == DataType::FLOAT64_ARRAY) {
                        auto vec = args[0].getComplexData<std::vector<double>>();
                        return Value::createNumberWithType(static_cast<uint64_t>(vec.size()), NumericType::UINT64);
                    }
                } catch (...) {
                    return Value::createNumberWithType(static_cast<uint64_t>(0), NumericType::UINT64);
                }
            }
            return Value::createNumberWithType(static_cast<uint64_t>(0), NumericType::UINT64);
        }
        if (funcName == "task.sleep") {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(args[0].toNumber())));
            return Value::createNull();
        }
        if (funcName == "Promise") {
            bool doExec = doExecute;
            Value result;
            result.type = DataType::PROMISE;
            result.setComplexData<Promise>(newPromise([this, args, startPos, doExec](Defer d) {
                std::thread([this, args, d, startPos, doExec]() {
                    Value output = Value::createNull();
                    bool rejected = false;
                    bool done = false;
                    
                    try {
                        if (args[0].type == DataType::FUNCTION) {
                            Value resolve = createFunction(
                                [&done, &output](const std::vector<Value>& args) -> Value {
                                    if (!done) {
                                        done = true;
                                        if (!args.empty()) {
                                            output = args[0];
                                        }
                                    }
                                    return Value::createNull();
                                },
                                "resolve"
                            );
                            
                            Value reject = createFunction(
                                [&done, &output, &rejected](const std::vector<Value>& args) -> Value {
                                    if (!done) {
                                        done = true;
                                        rejected = true;
                                        if (!args.empty()) {
                                            output = args[0];
                                        }
                                    }
                                    return Value::createNull();
                                },
                                "reject"
                            );
                            
                            Value funcResult = callFunction(args[0], {resolve, reject}, startPos, doExec);
                            
                            if (funcResult.type != DataType::NULL_TYPE && !done) {
                                done = true;
                                output = funcResult;
                            }
                        } else {
                            output = args[0];
                            done = true;
                        }
                        
                        if (rejected) {
                            d.reject(output);
                        } else if (done) {
                            d.resolve(output);
                        }
                    } catch (const std::exception& e) {
                        d.reject(Value::createString(std::string(e.what())));
                    } catch (...) {
                        d.reject(Value::createString(std::string("Unknown error in Promise")));
                    }
                }).detach();
            }));
            result.name = "Promise";
            promises.push_back(result);
            return result;
        }
        if (funcName == "task.wait") {
            std::vector<Promise> proms;
            if (args.empty()) {
                proms.reserve(promises.size());
                for (const Value& prom : promises) {
                    if (prom.type != DataType::PROMISE) throw std::runtime_error("Promise registry has been corrupted. A non-promise value has been found.");
                    proms.push_back(prom.getComplexData<Promise>());
                }
                promises.clear();
            } else if (args.size() == 1 && args[0].type == DataType::JSON_ARRAY) {
                proms.reserve(args[0].array_elements.size());
                for (const Value& prom : args[0].array_elements) {
                    if (prom.type != DataType::PROMISE) throw std::runtime_error("Expected promise, got " + dataTypeToString(prom.type) + ".");
                    proms.push_back(prom.getComplexData<Promise>());
                }
            } else {
                proms.reserve(args.size());
                for (const Value& prom : args) {
                    if (prom.type != DataType::PROMISE) throw std::runtime_error("Expected promise, got " + dataTypeToString(prom.type) + ".");
                    proms.push_back(prom.getComplexData<Promise>());
                }
            }
            all(proms).then([](std::vector<Promise> results) {});
            return Value::createNull();
        }
        if (funcName == "task.race") {
            std::vector<Promise> proms;
            if (args.size() == 1 && args[0].type == DataType::JSON_ARRAY) {
                proms.reserve(args[0].array_elements.size());
                for (const Value& prom : args[0].array_elements) {
                    if (prom.type != DataType::PROMISE) throw std::runtime_error("Expected promise, got " + dataTypeToString(prom.type) + ".");
                    proms.push_back(prom.getComplexData<Promise>());
                }
            } else {
                proms.reserve(args.size());
                for (const Value& prom : args) {
                    if (prom.type != DataType::PROMISE) throw std::runtime_error("Expected promise, got " + dataTypeToString(prom.type) + ".");
                    proms.push_back(prom.getComplexData<Promise>());
                }
            }
            int32_t resultID = -1;
            race(proms).then([&resultID](int result) {
                resultID = static_cast<int32_t>(result);
            });
            return Value::createNumberWithType(resultID, NumericType::INT32);
        }
        if (funcName == "free") {
            for (const Value& ptr : args) {
                freePointer(ptr);
            }
            return Value::createNull();
        }
        if (funcName == "Error") {
            Value result = stringToValue(args[0].toString());
            result.type = DataType::ERROR;
            return result;
        }
        if (funcName == "memory.global") {
            std::unordered_map<std::string, Value> obj = listGlobals();
            Value result = Value::createJsonObject(obj);
            result.name = funcName;
            return result;
        }
        if (funcName == "memory.classes") {
            std::unordered_map<std::string, Value> obj;
            std::unordered_map<uint64_t, Class> raw = listClasses();
            for (const auto& [key, value] : raw) {
                obj[Utility::uint64ToHexString(key)] = addClass(key);
            }
            Value result = Value::createJsonObject(obj);
            result.name = funcName;
            return result;
        }
        if (funcName == "memory.functions") {
            std::unordered_map<std::string, Value> obj;
            std::unordered_map<uint64_t, std::function<Value(const std::vector<Value>&)>> raw = listFunctions();
            for (const auto& [key, value] : raw) {
                std::string keyHex = Utility::uint64ToHexString(key);
                Value func = createFunction(value, funcName + "()[\"" + keyHex + "\"]");
                func.string_value = "[optimized code]";
                obj[keyHex] = func;
            }
            Value result = Value::createJsonObject(obj);
            result.name = funcName;
            return result;
        }
        if (funcName == "memory.pointers") {
            std::unordered_map<std::string, Value> obj;
            std::unordered_map<uint64_t, Value> raw = listPointers();
            for (const auto& [key, value] : raw) {
                obj[Utility::uint64ToHexString(key)] = value;
            }
            Value result = Value::createJsonObject(obj);
            result.name = funcName;
            return result;
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string(e.what()) + " at " + Utility::position(startPos, input) + ".");
    }

    throw std::runtime_error("\"" + funcName + "\" is not a function.");
}

Value Parser::doubleDot(const Value& left, const Value& right) {
    Value result;

    // concatenate
    if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
        result = stringToValue(left.name + right.name);                                     // "abc .. def" = ""abcdef"", where both "abc" and "def" are not defined.
    } else if (left.type == DataType::UNKNOWN) {
        result = stringToValue(left.name + Utility::value2string(right));                   // "abc .. "def"" = ""abcdef"", where "abc" is not defined.
    } else if (right.type == DataType::UNKNOWN) {
        result = stringToValue(Utility::value2string(left) + right.name);                   // ""abc" .. def" = ""abcdef"", where "def" is not defined.
    } else if (Utility::checkStrings(left, right)) {
        result = stringToValue(Utility::value2string(left) + Utility::value2string(right)); // ""abc" .. "def"" = ""abcdef"".
    } else if (left.type == DataType::JSON_ARRAY && right.type == DataType::JSON_ARRAY) {
        const auto& leftArr = left.array_elements;
        const auto& rightArr = right.array_elements;

        std::vector<Value> concatenated;
        concatenated.reserve(leftArr.size() + rightArr.size());

        concatenated.insert(concatenated.end(), leftArr.begin(), leftArr.end());
        concatenated.insert(concatenated.end(), rightArr.begin(), rightArr.end());

        Value result = Value::createJsonArray(concatenated);
        result.name = "Array";
        return result;
    }
    // merge
    else if (Utility::checkObjects(left, right)) {
        std::unordered_map<std::string, Value> merged;

        for (const auto& [key, val] : left.properties) {
            merged[key] = v(val);
        }
        for (const auto& [key, val] : right.properties) {
            merged[key] = v(val);
        }

        Value result = Value::createJsonObject(merged);
        result.name = "Object";
        return result;
    }
    
    else {
        throw std::runtime_error("Cannot concatenate " + dataTypeToString(left.type) + " with " + dataTypeToString(right.type) + " at " + Utility::position(currentToken().start, input) + ".");
    }

    return result;
}
void Parser::assign(const Value& var, const Value& val, const std::string& pos) {
    std::string vtype = " ";
    if (var.varType == VariableType::GLOBAL) vtype = " global ";
    else if (var.varType == VariableType::LOCAL) vtype = " local ";

    if (var.isConst) throw std::runtime_error("Assignment to" + vtype + "constant variable \"" + var.variable + "\"" + pos);

    Value newVal = val;
    newVal.isVariable = var.isVariable;
    newVal.variable = var.variable;

    variables[var.variable] = newVal;
    switch (var.varType) {
        case VariableType::GLOBAL:
            registerGlobal(var.variable, newVal, false, true);
            break;

        case VariableType::LOCAL:
            setLocal(currentScope, var.variable, newVal, false);
            break;

        case VariableType::VARIABLE:
        default:
            try {
                mutated.erase(var.variable);
            } catch (...) {}
            mutated.try_emplace(var.variable, Mutated(newVal, currentToken().start));
            break;
    }
}
Value Parser::evaluateExpression(const Value& left, const std::string& op, const Value& right, bool doExecute) {
    Value result;
    bool leftBool = left.toBoolean();
    bool rightBool = right.toBoolean();

    std::string errmsg = "Unexpected operator \"" + op + "\" at " + Utility::position(currentToken().start, input) + ".";

    if (op == "+") {
        if (
            (left.type == DataType::STRING  && right.type == DataType::STRING ) ||
            (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) ||
            (left.type == DataType::UNKNOWN && right.type == DataType::STRING ) ||
            (left.type == DataType::STRING  && right.type == DataType::UNKNOWN)
        ) {
            if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
                result = stringToValue(Utility::stringAdd(left.name, right.name));
            } else if (left.type == DataType::UNKNOWN) {
                result = stringToValue(Utility::stringAdd(left.name, right.toString()));
            } else if (right.type == DataType::UNKNOWN) {
                result = stringToValue(Utility::stringAdd(left.toString(), right.name));
            } else {
                result = stringToValue(Utility::stringAdd(left.toString(), right.toString()));
            }
        } else if (left.type == DataType::STRING) {
            throw std::runtime_error("Cannot add string to " + Utility::value2string(right) + " at " + Utility::position(currentToken().start, input) + ".");
        } else if (right.type == DataType::STRING) {
            throw std::runtime_error("Cannot add " + Utility::value2string(left) + " to string at " + Utility::position(currentToken().start, input) + ".");
        } else if (left.type == DataType::NUMBER && right.type == DataType::NUMBER) {
            result = numberToValue(left.toNumber() + right.toNumber());
        } else if (left.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringAdd(left.name, Utility::value2string(right)));
        } else if (right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringAdd(Utility::value2string(left), right.name));
        } else {
            result = stringToValue(Utility::stringAdd(left.toString(), right.toString()));
        }
    }
    else if (op == "minus" || op == "-") {
        if (left.type == DataType::UNKNOWN && Utility::checkNumbers(right, Value::createNumber(0.0))) {
            result = numberToValue(-right.toNumber());
        } else if (Utility::checkNumbers(left, right)) {
            result = numberToValue(left.toNumber() - right.toNumber());
        } else if (left.type == DataType::STRING && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringSub(left.toString(), right.toString()));
        } else if (left.type == DataType::STRING && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringSub(left.toString(), right.name));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringSub(left.name, right.toString()));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringSub(left.name, right.name));
        } else {
            throw std::runtime_error("Unexpected operator \"-\" at " + Utility::position(currentToken().start, input) + ".");
        }
    }
    else if (op == "*") {
        if (Utility::checkNumbers(left, right)) {
            result = numberToValue(left.toNumber() * right.toNumber());
        } else if (left.type == DataType::STRING && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringMul(left.toString(), right.toString()));
        } else if (left.type == DataType::STRING && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringMul(left.toString(), right.name));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringMul(left.name, right.toString()));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringMul(left.name, right.name));
        } else {
            throw std::runtime_error("Unexpected operator \"*\" at " + Utility::position(currentToken().start, input) + ".");
        }
    }
    else if (op == "/" || (op == ":" && Utility::checkNumber(right))) {
        if (Utility::checkNumbers(left, right)) {
            double divisor = right.toNumber();
            if (divisor == 0) {
                result.type = DataType::INFINITE;
                result.name = "infinity";
            } else {
                result = numberToValue(left.toNumber() / divisor);
            }
        } else if (left.type == DataType::STRING && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringDiv(left.toString(), right.toString()));
        } else if (left.type == DataType::STRING && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringDiv(left.toString(), right.name));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringDiv(left.name, right.toString()));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringDiv(left.name, right.name));
        } else {
            throw std::runtime_error("Unexpected operator \"" + op + "\" at " + Utility::position(currentToken().start, input) + ".");
        }
    }
    else if (op == "**") {
        if (Utility::checkNumbers(left, right)) {
            result = numberToValue(std::pow(left.toNumber(), right.toNumber()));
        } else if (left.type == DataType::STRING && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringPow(left.toString(), right.toString()));
        } else if (left.type == DataType::STRING && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringPow(left.toString(), right.name));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringPow(left.name, right.toString()));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringPow(left.name, right.name));
        } else {
            throw std::runtime_error("Unexpected operator \"**\" at " + Utility::position(currentToken().start, input) + ".");
        }
    }
    else if (op == "%") {
        if (Utility::checkNumbers(left, right)) {
            result = numberToValue(std::fmod(left.toNumber(), right.toNumber()));
        } else if (left.type == DataType::STRING && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringFMod(left.toString(), right.toString()));
        } else if (left.type == DataType::STRING && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringFMod(left.toString(), right.name));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringFMod(left.name, right.toString()));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringFMod(left.name, right.name));
        } else {
            throw std::runtime_error("Unexpected operator \"%\" at " + Utility::position(currentToken().start, input) + ".");
        }
    }
    else if (op == "..") {
        result = doubleDot(left, right);
    }

    else if (op == "==" || op == "is") {
        result = booleanToValue(Utility::compareValues(left, right));
    }
    else if (op == "!=" || op == "isn't") {
        result = booleanToValue(!Utility::compareValues(left, right));
    }
    else if (op == "<" && Utility::checkNumbers(left, right)) {
        result = booleanToValue(left.toNumber() < right.toNumber());
    }
    else if (op == ">" && Utility::checkNumbers(left, right)) {
        result = booleanToValue(left.toNumber() > right.toNumber());
    }
    else if (op == "<=" && Utility::checkNumbers(left, right)) {
        result = booleanToValue(left.toNumber() <= right.toNumber());
    }
    else if (op == ">=" && Utility::checkNumbers(left, right)) {
        result = booleanToValue(left.toNumber() >= right.toNumber());
    }

    else if (op == "&" || op == "AND") {
        if (Utility::checkNumbers(left, right)) {
            int leftInt = static_cast<int>(left.toNumber());
            int rightInt = static_cast<int>(right.toNumber());
            result = numberToValue(leftInt & rightInt);
        } else if (left.type == DataType::STRING && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringAnd(left.toString(), right.toString()));
        } else if (left.type == DataType::STRING && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringAnd(left.toString(), right.name));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringAnd(left.name, right.toString()));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringAnd(left.name, right.name));
        } else {
            bool leftBool = left.toBoolean();
            bool rightBool = right.toBoolean();
            int leftInt = leftBool ? 1 : 0;
            int rightInt = rightBool ? 1 : 0;
            result = booleanToValue(leftInt & rightInt);
        }
    }
    else if (op == "|" || op == "OR") {
        if (Utility::checkNumbers(left, right)) {
            int leftInt = static_cast<int>(left.toNumber());
            int rightInt = static_cast<int>(right.toNumber());
            result = numberToValue(leftInt | rightInt);
        } else if (left.type == DataType::STRING && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringOr(left.toString(), right.toString()));
        } else if (left.type == DataType::STRING && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringOr(left.toString(), right.name));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringOr(left.name, right.toString()));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringOr(left.name, right.name));
        } else {
            bool leftBool = left.toBoolean();
            bool rightBool = right.toBoolean();
            int leftInt = leftBool ? 1 : 0;
            int rightInt = rightBool ? 1 : 0;
            result = booleanToValue(leftInt | rightInt);
        }
    }
    else if (op == "^" || op == "XOR") {
        if (Utility::checkNumbers(left, right)) {
            int leftInt = static_cast<int>(left.toNumber());
            int rightInt = static_cast<int>(right.toNumber());
            result = numberToValue(leftInt ^ rightInt);
        } else if (left.type == DataType::STRING && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringXor(left.toString(), right.toString()));
        } else if (left.type == DataType::STRING && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringXor(left.toString(), right.name));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringXor(left.name, right.toString()));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringXor(left.name, right.name));
        } else {
            throw std::runtime_error("Expected numbers or strings for bitwise XOR operation at " + Utility::position(currentToken().start, input) + ".");
        }
    }
    else if (op == "~" || op == "NOT") {
        if (Utility::checkNumber(right)) {
            int num = static_cast<int>(right.toNumber());
            result = numberToValue(~num);
        } else if (right.type == DataType::STRING) {
            result = stringToValue(Utility::stringNot(right.toString()));
        } else if (right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringNot(right.name));
        } else {
            throw std::runtime_error("Expected number or string for bitwise NOT operation at " + Utility::position(currentToken().start, input) + ".");
        }
    }
    else if (op == "<<") {
        if (Utility::checkNumbers(left, right)) {
            int leftInt = static_cast<int>(left.toNumber());
            int rightInt = static_cast<int>(right.toNumber());
            result = numberToValue(leftInt << rightInt);
        } else if (left.type == DataType::STRING && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringLShift(left.toString(), right.toString()));
        } else if (left.type == DataType::STRING && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringLShift(left.toString(), right.name));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringLShift(left.name, right.toString()));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringLShift(left.name, right.name));
        } else {
            throw std::runtime_error("Expected numbers or strings for bitwise left shift operation at " + Utility::position(currentToken().start, input) + ".");
        }
    }
    else if (op == ">>") {
        if (Utility::checkNumbers(left, right)) {
            int leftInt = static_cast<int>(left.toNumber());
            int rightInt = static_cast<int>(right.toNumber());
            result = numberToValue(leftInt >> rightInt);
        } else if (left.type == DataType::STRING && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringRShift(left.toString(), right.toString()));
        } else if (left.type == DataType::STRING && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringRShift(left.toString(), right.name));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::STRING) {
            result = stringToValue(Utility::stringRShift(left.name, right.toString()));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
            result = stringToValue(Utility::stringRShift(left.name, right.name));
        } else {
            throw std::runtime_error("Expected numbers or strings for bitwise right shift operation at " + Utility::position(currentToken().start, input) + ".");
        }
    }

    else if (op == "&&" || op == "and") {
        result = booleanToValue(left.toBoolean() && right.toBoolean());
    }
    else if (op == "!&" || op == "andn't") {
        result = booleanToValue(!(left.toBoolean() && right.toBoolean()));
    }
    else if (op == "||" || op == "or") {
        result = booleanToValue(left.toBoolean() || right.toBoolean());
    }
    else if (op == "!|" || op == "orn't") {
        result = booleanToValue(!(left.toBoolean() || right.toBoolean()));
    }
    else if (op == "!" || op == "not") {
        result = booleanToValue(!right.toBoolean());
    }

    else if (op == "nand") {
        result = booleanToValue(!leftBool && !rightBool);
    }
    else if (op == "nor") {
        result = booleanToValue(!leftBool || !rightBool);
    }
    else if (op == "xor") {
        result = booleanToValue((leftBool && !rightBool) || (!leftBool && rightBool));
    }
    else if (op == "xnor") {
        result = booleanToValue((leftBool && rightBool) || (!leftBool && !rightBool));
    }
    else if (op == "imply") {
        result = booleanToValue(!leftBool || rightBool);
    }
    else if (op == "nimply") {
        result = booleanToValue(leftBool && !rightBool);
    }

    else if (op == "??") {
        switch (left.type) {
            case DataType::UNKNOWN:
            case DataType::NULL_TYPE:
                result = right;
                break;
            default:
                result = left;
                break;
        }
    }
    else if (op == "?:") {
        if (left.toBoolean()) {
            result = left;
        } else {
            result = right;
        }
    }

    else if (op == "|>") {
        std::vector<Value> args;
        args.push_back(left);

        if (match("(")) {
            std::vector<Value> additionalArgs = parseArguments(doExecute);
            args.reserve(args.size() + additionalArgs.size());
            args.insert(args.end(), additionalArgs.begin(), additionalArgs.end());
        }

        if (right.type == DataType::FUNCTION) {
            result = callFunction(right, args, currentToken().start, doExecute);
        } else {
            result = executeFunction(right.toIdentifier(), args, currentToken().start);
        }
    }

    else if (op == "~=" || op == "!~=") {
        if (Utility::checkNumbers(left, right)) {
            result = booleanToValue(Math::Round(left.toNumber()) == Math::Round(right.toNumber()));
        } else if (left.type == DataType::STRING && right.type == DataType::STRING) {
            result = booleanToValue(Unicode::EqualsIgnoreCase(left.toString(), right.toString()));
        } else if (left.type == DataType::STRING && right.type == DataType::UNKNOWN) {
            result = booleanToValue(Unicode::EqualsIgnoreCase(left.toString(), right.name));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::STRING) {
            result = booleanToValue(Unicode::EqualsIgnoreCase(left.name, right.toString()));
        } else if (left.type == DataType::UNKNOWN && right.type == DataType::UNKNOWN) {
            result = booleanToValue(Unicode::EqualsIgnoreCase(left.name, right.name));
        } else {
            result = booleanToValue(Utility::compareValues(left, right));
        }
        if (op == "!~=") result = booleanToValue(!result.toBoolean());
    }
    
    else if (op == "[" && doExecute) {
        if (!match("]")) throw std::runtime_error("Expected \"]\" to close index access at " + Utility::position(currentToken().start, input) + ".");
        advance();

        if (right.type == DataType::STRING) {
            auto it = typeMethods.find(left.type);
            std::string funcName = right.toString();
            if (it != typeMethods.end()) {
                auto itFunc = typeMethods[left.type].find(funcName);
                if (itFunc != typeMethods[left.type].end()) {
                    if (match("(")) {
                        std::vector<Value> args = {left};
                        std::vector<Value> additionalArgs = parseArguments(doExecute);
                        args.reserve(args.size() + additionalArgs.size());
                        args.insert(args.end(), additionalArgs.begin(), additionalArgs.end());
                        result = executeFunction(typeMethods[left.type][funcName], args, currentToken().start);
                    }
                    else throw std::runtime_error("Expected \"(\" for function call at " + Utility::position(currentToken().start, input) + ".");
                } else if (left.type == DataType::JSON_OBJECT || left.type == DataType::JUSTC_OBJECT) {
                    result = accessProperty(left, funcName).first;
                }
            } else if (left.type == DataType::JSON_OBJECT || left.type == DataType::JUSTC_OBJECT) {
                result = accessProperty(left, funcName).first;
            }
        } else {
            size_t index = static_cast<size_t>(right.toNumber());
            switch (left.type) {
                case DataType::STRING:
                case DataType::LINK:
                    result = executeFunction("String::Slice", {
                        stringToValue(left.toString()), 
                        numberToValue(static_cast<double>(index)), 
                        numberToValue(static_cast<double>(index + 1))
                    }, currentToken().start);
                    break;
                case DataType::JSON_ARRAY: 
                    result = index < left.array_elements.size() ? left.array_elements[index] : Value::createNull();
                    break;
                case DataType::INT8_ARRAY: {
                    std::vector<int8_t> a = left.getComplexData<std::vector<int8_t>>();
                    result = index < a.size() ? Value::createNumberWithType(a[index], NumericType::INT8) : Value::createNull();
                    break;
                }
                case DataType::INT16_ARRAY: {
                    std::vector<int16_t> a = left.getComplexData<std::vector<int16_t>>();
                    result = index < a.size() ? Value::createNumberWithType(a[index], NumericType::INT16) : Value::createNull();
                    break;
                }
                case DataType::INT32_ARRAY: {
                    std::vector<int32_t> a = left.getComplexData<std::vector<int32_t>>();
                    result = index < a.size() ? Value::createNumberWithType(a[index], NumericType::INT32) : Value::createNull();
                    break;
                }
                case DataType::INT64_ARRAY: {
                    std::vector<int64_t> a = left.getComplexData<std::vector<int64_t>>();
                    result = index < a.size() ? Value::createNumberWithType(a[index], NumericType::INT64) : Value::createNull();
                    break;
                }
                case DataType::UINT8_ARRAY:
                case DataType::CUINT8_ARRAY: {
                    std::vector<uint8_t> a = left.getComplexData<std::vector<uint8_t>>();
                    result = index < a.size() ? Value::createNumberWithType(a[index], left.type == DataType::UINT8_ARRAY ? NumericType::UINT8 : NumericType::CUINT8) : Value::createNull();
                    break;
                }
                case DataType::UINT16_ARRAY:
                case DataType::CUINT16_ARRAY: {
                    std::vector<uint16_t> a = left.getComplexData<std::vector<uint16_t>>();
                    result = index < a.size() ? Value::createNumberWithType(a[index], left.type == DataType::UINT16_ARRAY ? NumericType::UINT16 : NumericType::CUINT16) : Value::createNull();
                    break;
                }
                case DataType::UINT32_ARRAY:
                case DataType::CUINT32_ARRAY: {
                    std::vector<uint32_t> a = left.getComplexData<std::vector<uint32_t>>();
                    result = index < a.size() ? Value::createNumberWithType(a[index], left.type == DataType::UINT32_ARRAY ? NumericType::UINT32 : NumericType::CUINT32) : Value::createNull();
                    break;
                }
                case DataType::UINT64_ARRAY:
                case DataType::CUINT64_ARRAY: {
                    std::vector<uint64_t> a = left.getComplexData<std::vector<uint64_t>>();
                    result = index < a.size() ? Value::createNumberWithType(a[index], left.type == DataType::UINT64_ARRAY ? NumericType::UINT64 : NumericType::CUINT64) : Value::createNull();
                    break;
                }
                case DataType::FLOAT32_ARRAY: {
                    std::vector<float> a = left.getComplexData<std::vector<float>>();
                    result = index < a.size() ? Value::createNumberWithType(a[index], NumericType::FLOAT32) : Value::createNull();
                    break;
                }
                case DataType::FLOAT64_ARRAY: {
                    std::vector<double> a = left.getComplexData<std::vector<double>>();
                    result = index < a.size() ? Value::createNumberWithType(a[index], NumericType::FLOAT64) : Value::createNull();
                    break;
                }
                case DataType::JSON_OBJECT:
                case DataType::JUSTC_OBJECT:
                case DataType::ENUM:
                    result = accessProperty(left, right.toString()).first;
                    break;
                case DataType::MAP: {
                    std::unordered_map<Value, Value> map = left.getComplexData<std::unordered_map<Value, Value>>();
                    auto it = map.find(right.toPrimitive());
                    if (it != map.end()) {
                        result = it->second;
                    } else result = Value::createNull();
                    break;
                }
                default: break;
            }
        }
    }

    else if ((op == ":" || op == ".") && doExecute) {
        auto it = typeMethods.find(left.type);
        std::string funcName = right.toIdentifier();
        if (it != typeMethods.end()) {
            auto itFunc = typeMethods[left.type].find(funcName);
            if (itFunc != typeMethods[left.type].end()) {
                if (match("(")) {
                    std::vector<Value> args = {left};
                    std::vector<Value> additionalArgs = parseArguments(doExecute);
                    args.reserve(args.size() + additionalArgs.size());
                    args.insert(args.end(), additionalArgs.begin(), additionalArgs.end());
                    result = executeFunction(typeMethods[left.type][funcName], args, currentToken().start);
                }
                else throw std::runtime_error("Expected \"(\" for function call at " + Utility::position(currentToken().start, input) + ".");
            } else if (left.type == DataType::JSON_OBJECT || left.type == DataType::JUSTC_OBJECT) {
                result = accessProperty(left, right.toString()).first;
            } else {
                std::string ogName = left.isVariable ? left.variable : std::string("<" + dataTypeToString(left.type) + ">");
                throw std::runtime_error(ogName + op + funcName + " is not a function. Call attempt at " + Utility::position(currentToken().start, input) + ".");
            }
        } else if (left.type == DataType::JSON_OBJECT || left.type == DataType::JUSTC_OBJECT) {
            result = accessProperty(left, right.toString()).first;
        }
        else throw std::runtime_error(errmsg);

        if (op == ":" && left.isVariable) {
            assign(left, result, " at " + Utility::position(currentToken().start, input) + ".");
        }
    }

    else if (op == "=") {
        result = right;
        if (left.isVariable) {
            assign(left, right, " at " + Utility::position(currentToken().start, input) + ".");
        }
    }

    else throw std::runtime_error(errmsg);
    if (result.type == DataType::UNKNOWN) throw std::runtime_error(errmsg);

    return result;
}

Value Parser::handleInequality(const Value& value) {
    Value result;

    switch (value.type) {
        case DataType::NUMBER:
            result = booleanToValue(value.toNumber() > 0);
            break;
        case DataType::LINK:
            result = stringToValue(value.toString());
            result.type = DataType::STRING;
            break;
        case DataType::BOOLEAN:
            result = booleanToValue(!value.toBoolean());
            break;
        default:
            result = booleanToValue(false);
            break;
    }

    return result;
}

Value Parser::handleConditional(const Value& condition, const Value& thenVal, const Value& elseVal,
                               const std::string& thenOp, const std::string& elseOp) {
    bool cond = condition.toBoolean();

    if (thenOp == "then't" || thenOp == "=!") {
        cond = !cond;
    }

    if (cond) {
        return thenVal;
    } else {
        if (elseOp == "elsen't" || elseOp == "?!") {
            return handleInequality(elseVal);
        }
        return elseVal;
    }
}

void Parser::buildDependencyGraph() {
    for (const auto& node : ast) {
        if (node.type == "VARIABLE_DECLARATION") {
            dependencies[node.identifier] = node.references;
        } else if (node.type == "EXCEPTION") {
            throw std::runtime_error(node.value.toString());
        }
    }
}

bool Parser::detectCycles() {
    std::unordered_map<std::string, bool> visited;
    std::unordered_map<std::string, bool> recStack;
    std::vector<std::string> cyclePath;

    for (const auto& pair : dependencies) {
        if (dfsCycleDetection(pair.first, visited, recStack, cyclePath)) {
            return true;
        }
    }

    return false;
}

bool Parser::dfsCycleDetection(const std::string& node,
                              std::unordered_map<std::string, bool>& visited,
                              std::unordered_map<std::string, bool>& recStack,
                              std::vector<std::string>& cyclePath) {
    if (!visited[node]) {
        visited[node] = true;
        recStack[node] = true;
        cyclePath.push_back(node);

        for (const auto& neighbor : dependencies[node]) {
            if (!visited[neighbor] && dfsCycleDetection(neighbor, visited, recStack, cyclePath)) {
                return true;
            } else if (recStack[neighbor]) {
                cyclePath.push_back(neighbor);
                return true;
            }
        }
    }

    recStack[node] = false;
    if (!cyclePath.empty()) cyclePath.pop_back();
    return false;
}

Value Parser::resolveVariableValue(const std::string& varName, const bool unknownIsString) {
    if (varName == "this") {
        return This();
    } else if (hasGlobal(varName)) {
        return getGlobal(varName);
    } else if (hasLocal(currentScope, varName)) {
        return resolveVariableValueWithScopes(varName, unknownIsString);
    }

    auto it = variables.find(varName);
    if (it != variables.end() && it->second.type != DataType::UNKNOWN) {
        Value var = it->second;
        var.isVariable = true;
        var.variable = varName;
        var.varType = VariableType::VARIABLE;

        auto constIt = constVars.find(varName);
        var.isConst = (constIt != constVars.end() && constIt->second);
        
        return var;
    }

    for (const auto& node : ast) {
        if (node.type == "VARIABLE_DECLARATION" && node.identifier == varName) {
            auto mutatedIt = mutated.find(varName);
            if (mutatedIt != mutated.end()) {
                Mutated newVal = mutatedIt->second;
                if (newVal.startPos > node.startPos) {
                    if (newVal.value.type != DataType::UNKNOWN) {
                        Value var = newVal.value;
                        var.isVariable = true;
                        var.variable = varName;
                        var.varType = VariableType::VARIABLE;

                        auto constIt = constVars.find(varName);
                        var.isConst = (constIt != constVars.end() && constIt->second);

                        return var;
                    } else if (unknownIsString) {
                        Value result;
                        result.type = DataType::STRING;
                        result.name = varName;
                        result.string_value = newVal.value.name;
                        return result;
                    }
                }
            }
            Value var = evaluateASTNode(node);
            var.isVariable = true;
            var.variable = varName;
            var.varType = VariableType::VARIABLE;

            auto constIt = constVars.find(varName);
            var.isConst = (constIt != constVars.end() && constIt->second);

            return var;
        }
    }

    if (unknownIsString) {
        Value result;
        result.type = DataType::STRING;
        result.name = varName;
        result.string_value = varName;
        return result;
    }

    Value result;
    result.type = DataType::UNKNOWN;
    result.name = "unknown";
    return result;
}

void Parser::evaluateAllVariables() {
    if (runAsync && !dependencies.empty()) {
        evaluateAllVariablesAsync();
    } else {
        evaluateAllVariablesSync();
    }
}

std::runtime_error Parser::typeDeclarationError(const DataType left, const DataType right, const ASTNode node) {
    return std::runtime_error("Type declaration error: Cannot convert " + dataTypeToString(left) + " to " + dataTypeToString(right) + " at " + Utility::position(node.startPos, input) + ".");
}

Value Parser::applyTypeDeclaration(const Value value, const ASTNode node) {
    DataType typeDeclaration = node.typeDeclaration;
    Value result = value;
    if (typeDeclaration == result.type) return result;
    switch (typeDeclaration) {
        case DataType::UNKNOWN:
            break;
        case DataType::NUMBER:
        case DataType::HEXADECIMAL:
        case DataType::OCTAL:
        case DataType::BINARY:
            if (typeDeclaration == DataType::BINARY_DATA && result.type == DataType::BINARY) {
                try {
                    result = Binary::Data({result});
                } catch (const std::exception& e) {
                    throw std::runtime_error("Type declaration error: " + std::string(e.what()) + " at " + Utility::position(node.startPos, input) + ".");
                }
            }
            switch (result.type) {
                case DataType::NUMBER:
                case DataType::HEXADECIMAL:
                case DataType::OCTAL:
                case DataType::BINARY:
                    result = Utility::convert(result, typeDeclaration);
                    break;
                default:
                    throw typeDeclarationError(result.type, typeDeclaration, node);
                    break;
            }
            break;
        case DataType::STRING:
            result.type = DataType::STRING;
            result.string_value = Utility::value2string(value);
            break;
        case DataType::LINK:
            if (result.type == DataType::STRING) {
                if (isValidLink(result.string_value)) {
                    result.type = DataType::LINK;
                } else {
                    throw std::runtime_error("Type declaration error: Invalid link: " + result.string_value + " at " + Utility::position(node.startPos, input) + ".");
                }
            }
            break;
        case DataType::BOOLEAN:
            switch (result.type) {
                case DataType::NUMBER:
                case DataType::HEXADECIMAL:
                case DataType::OCTAL:
                case DataType::BINARY:
                    result.boolean_value = (value.number_value > 0);
                    break;
                case DataType::STRING:
                    result.boolean_value = value.toBoolean();
                    break;
                case DataType::NULL_TYPE:
                    result.boolean_value = false;
                    break;
                case DataType::INFINITE:
                    result.boolean_value = true;
                    break;
                case DataType::NINFINITE:
                    result.boolean_value = false;
                    break;
                default:
                    throw typeDeclarationError(result.type, typeDeclaration, node);
                    break;
            }
            result.type = DataType::BOOLEAN;
            break;
        default:
            throw typeDeclarationError(result.type, typeDeclaration, node);
            break;
    }
    return result;
}
std::string Parser::stripUnderscores(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (c != '_') {
            result += c;
        }
    }
    return result;
}
#ifdef __SIZEOF_INT128__
__int128 Parser::parseToInt128(const std::string& str) {
    std::string cleaned = stripUnderscores(str);
    bool isNegative = false;
    size_t start = 0;
    
    if (cleaned[0] == '-') {
        isNegative = true;
        start = 1;
    } else if (cleaned[0] == '+') {
        start = 1;
    }
    
    int base = 10;
    if (cleaned.length() >= start + 2) {
        char second = std::tolower(cleaned[start + 1]);
        if (cleaned[start] == '0') {
            if (second == 'x' || second == 'X') {
                base = 16;
                start += 2;
            } else if (second == 'b' || second == 'B') {
                base = 2;
                start += 2;
            } else if (second == 'o' || second == 'O') {
                base = 8;
                start += 2;
            }
        }
    }
    
    __int128 result = 0;
    for (size_t i = start; i < cleaned.length(); i++) {
        char c = cleaned[i];
        int digit;
        
        if (base == 16) {
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else continue;
        } else if (base == 2) {
            if (c == '0' || c == '1') digit = c - '0';
            else continue;
        } else if (base == 8) {
            if (c >= '0' && c <= '7') digit = c - '0';
            else continue;
        } else {
            if (c >= '0' && c <= '9') digit = c - '0';
            else continue;
        }
        
        if (result > (__int128)(std::numeric_limits<unsigned long long>::max)() / base) {
            #ifdef __EMSCRIPTEN__
                warn_int128(Utility::position(currentToken().start, input).c_str(), getCurrentTimestamp().c_str());
            #else
                std::cout << "C++ int128 overflow at " + Utility::position(currentToken().start, input) + "." << std::endl;
            #endif
        }
        result = result * base + digit;
    }
    
    return isNegative ? -result : result;
}
unsigned __int128 Parser::parseToUInt128(const std::string& str) {
    std::string cleaned = stripUnderscores(str);
    size_t start = 0;
    
    if (cleaned[0] == '+') {
        start = 1;
    }
    
    int base = 10;
    if (cleaned.length() >= start + 2) {
        char second = std::tolower(cleaned[start + 1]);
        if (cleaned[start] == '0') {
            if (second == 'x' || second == 'X') {
                base = 16;
                start += 2;
            } else if (second == 'b' || second == 'B') {
                base = 2;
                start += 2;
            } else if (second == 'o' || second == 'O') {
                base = 8;
                start += 2;
            }
        }
    }
    
    unsigned __int128 result = 0;
    for (size_t i = start; i < cleaned.length(); i++) {
        char c = cleaned[i];
        int digit;
        
        if (base == 16) {
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else continue;
        } else if (base == 2) {
            if (c == '0' || c == '1') digit = c - '0';
            else continue;
        } else if (base == 8) {
            if (c >= '0' && c <= '7') digit = c - '0';
            else continue;
        } else {
            if (c >= '0' && c <= '9') digit = c - '0';
            else continue;
        }
        
        result = result * base + digit;
    }
    
    return result;
}
#else
long long Parser::parseToInt128(const std::string& str) {
    std::string cleaned = stripUnderscores(str);
    try {
        return std::stoll(cleaned);
    } catch (...) {
        return 0;
    }
}
unsigned long long Parser::parseToUInt128(const std::string& str) {
    std::string cleaned = stripUnderscores(str);
    try {
        return std::stoull(cleaned);
    } catch (...) {
        return 0;
    }
}
#endif
#if JUSTC_FLOAT128_SUPPORT
__float128 Parser::parseToFloat128(const std::string& str) {
    std::string cleaned = stripUnderscores(str);
    return strtoflt128(cleaned.c_str(), nullptr);
}
#endif
Value Parser::applyCPPTypeDeclaration(const Value value, const std::string& cpptype, const DataType typeDecl, bool doExecute, const bool canBeDefault, const uint8_t isID) {
    if (cpptype == DEFAULT_CPP_TYPE) return value;

    Value result = value;
    const bool isDefault = canBeDefault && value.type == DataType::NULL_TYPE;

    if (isID == 0 && isCPPNumber(cpptype)) {
        switch (typeDecl) {
            case DataType::UNKNOWN:
            case DataType::NUMBER:
            case DataType::HEXADECIMAL:
            case DataType::BINARY:
            case DataType::OCTAL: {
                std::string cleaned = stripUnderscores(value.name);
                if (isDefault) cleaned = "0";

                if (cpptype == "int8") {
                    result = Value::createNumberWithType(static_cast<int8_t>(std::stoi(cleaned)), NumericType::INT8);
                } else if (cpptype == "int16") {
                    result = Value::createNumberWithType(static_cast<int16_t>(std::stoi(cleaned)), NumericType::INT16);
                } else if (cpptype == "int32") {
                    int32_t num = std::stoi(cleaned);
                    result = Value::createNumberWithType(num, NumericType::INT32);
                } else if (cpptype == "int64") {
                    int64_t num = std::stoll(cleaned);
                    result = Value::createNumberWithType(num, NumericType::INT64);
                } else if (cpptype == "int128") {
                    result = Value::createNumberWithType(parseToInt128(value.name), NumericType::INT128);
                } else if (cpptype == "uint8") {
                    result = Value::createNumberWithType(static_cast<uint8_t>(std::stoul(cleaned)), NumericType::UINT8);
                } else if (cpptype == "uint16") {
                    result = Value::createNumberWithType(static_cast<uint16_t>(std::stoul(cleaned)), NumericType::UINT16);
                } else if (cpptype == "uint32") {
                    uint32_t num = std::stoul(cleaned);
                    result = Value::createNumberWithType(num, NumericType::UINT32);
                } else if (cpptype == "uint64") {
                    uint64_t num = std::stoull(cleaned);
                    result = Value::createNumberWithType(num, NumericType::UINT64);
                } else if (cpptype == "uint128") {
                    result = Value::createNumberWithType(parseToUInt128(value.name), NumericType::UINT128);
                } else if (cpptype == "cuint8") {
                    long long raw = std::stoll(cleaned);
                    uint8_t num;
                    if (raw < 0) num = 0;
                    else if (raw > 255) num = 255;
                    else num = static_cast<uint8_t>(raw);
                    result = Value::createNumberWithType(num, NumericType::CUINT8);
                } else if (cpptype == "cuint16") {
                    long long raw = std::stoll(cleaned);
                    uint16_t num;
                    if (raw < 0) num = 0;
                    else if (raw > 65535) num = 65535;
                    else num = static_cast<uint16_t>(raw);
                    result = Value::createNumberWithType(num, NumericType::CUINT16);
                } else if (cpptype == "cuint32") {
                    long long raw = std::stoll(cleaned);
                    uint32_t num;
                    if (raw < 0) num = 0;
                    else if (raw > 4294967295LL) num = 4294967295U;
                    else num = static_cast<uint32_t>(raw);
                    result = Value::createNumberWithType(num, NumericType::CUINT32);
                } else if (cpptype == "cuint64") {
                    if (!cleaned.empty() && cleaned[0] == '-') {
                        uint64_t num = 0;
                        result = Value::createNumberWithType(num, NumericType::CUINT64);
                    } else {
                        if (cleaned.length() > 19 || (cleaned.length() == 19 && cleaned > "9223372036854775807")) {
                            try {
                                unsigned long long raw_ull = std::stoull(cleaned);
                                uint64_t num = raw_ull;
                                if (raw_ull > 18446744073709551615ULL) {
                                    num = 18446744073709551615ULL;
                                }
                                result = Value::createNumberWithType(num, NumericType::CUINT64);
                            } catch (const std::out_of_range& e) {
                                uint64_t num = 18446744073709551615ULL;
                                result = Value::createNumberWithType(num, NumericType::CUINT64);
                            }
                        } else {
                            try {
                                long long raw_ll = std::stoll(cleaned);
                                if (raw_ll < 0) {
                                    uint64_t num = 0;
                                    result = Value::createNumberWithType(num, NumericType::CUINT64);
                                } else {
                                    uint64_t num = static_cast<uint64_t>(raw_ll);
                                    result = Value::createNumberWithType(num, NumericType::CUINT64);
                                }
                            } catch (const std::out_of_range& e) {
                                uint64_t num = 18446744073709551615ULL;
                                result = Value::createNumberWithType(num, NumericType::CUINT64);
                            }
                        }
                    }
                } else if (cpptype == "float32") {
                    float num = std::stof(cleaned);
                    result = Value::createNumberWithType(num, NumericType::FLOAT32);
                } else if (cpptype == "float128") {
                    #if JUSTC_FLOAT128_SUPPORT
                        __float128 num = parseToFloat128(cleaned);
                        result = Value::createNumberWithType(num, NumericType::FLOAT128);
                    #else
                        long double num = std::stold(cleaned);
                        result = Value::createNumberWithType(num, NumericType::BIGNUM);
                    #endif
                } else { // float64
                    double num = std::stod(cleaned);
                    result = Value::createNumberWithType(num, NumericType::FLOAT64);
                }
                break;
            }
            default:
                throw std::runtime_error("C++ type declaration error: Cannot convert " + dataTypeToString(typeDecl) + " to " + cpptype + " at " + Utility::position(currentToken().start, input) + ".");
                break;
        }
    } else if ((isID == 0 && isStruct(cpptype).first) || isID == 1) {
        Value constructor = isID == 0 ? isStruct(cpptype).second : getStructConstructor(cpptype);
        result = callFunction(constructor, {}, currentToken().start, doExecute);
        for (const Value extend : constructor.array_elements) {
            switch (extend.type) {
                case DataType::STRUCT: {
                    std::string structID = Utility::uint64ToHexString(nextStructConstructor++);
                    structConstructors[structID] = extend;
                    Value parent = applyCPPTypeDeclaration(Value::createNull(), structID, typeDecl, doExecute, canBeDefault, 1);
                    structConstructors.erase(structID);
                    result = doubleDot(parent, result);
                    break;
                }
                case DataType::JSON_OBJECT:
                case DataType::JUSTC_OBJECT:
                    result = doubleDot(extend, result);
                    break;
                case DataType::FUNCTION: {
                    Value parent = callFunction(extend, {}, currentToken().start, doExecute);
                    switch (parent.type) {
                        case DataType::STRUCT: {
                            std::string structID = Utility::uint64ToHexString(nextStructConstructor++);
                            structConstructors[structID] = parent;
                            Value output = applyCPPTypeDeclaration(Value::createNull(), structID, typeDecl, doExecute, canBeDefault, 1);
                            structConstructors.erase(structID);
                            result = doubleDot(output, result);
                            break;
                        }
                        case DataType::JSON_OBJECT:
                        case DataType::JUSTC_OBJECT:
                            result = doubleDot(parent, result);
                            break;
                        default:
                            throw std::runtime_error("Constructor \"" + extend.isVariable ? extend.variable : extend.name + "\" returned a non-object value.");
                    }
                    break;
                }
                case DataType::NULL_TYPE:
                    break;
                default:
                    throw std::runtime_error("Struct \"" + cpptype + "\" cannot extend <" + dataTypeToString(extend.type) + "> - it is not a constructor, object, or null.");
            }
        }
        if (!isDefault) throw std::runtime_error("");
    } else if ((isID == 0 && isClass(cpptype).first) || isID == 2) {
        uint64_t classID = isID == 0 ? isClass(cpptype).second.getNumericValue<uint64_t>() : static_cast<uint64_t>(std::stoull(cpptype));
        Class cls = getClass(classID);
        result = callFunction(cls.constructor, {}, currentToken().start, doExecute);
        if (result.type != DataType::JSON_OBJECT && result.type != DataType::JUSTC_OBJECT)
            throw std::runtime_error("Constructor for class " + Utility::uint64ToHexString(classID) + " returned a non-object value.");
        Value instanceObject = Value::createJsonObject({});
        instanceObject.properties = cls.instanceObject;
        result = doubleDot(instanceObject, result);
    }

    return result;
}

Value Parser::evaluateASTNode(const ASTNode& node) {
    if (node.type == "VARIABLE_DECLARATION") {
        Value result = node.value;

        if (result.type == DataType::VARIABLE) {
            std::string refVar = result.string_value;
            if (refVar == node.identifier) {
                throw std::runtime_error("Variable cannot reference itself: " + node.identifier);
            }
            Value varval = resolveVariableValue(refVar, true);
            return applyTypeDeclaration(varval, node);
        }

        return applyTypeDeclaration(result, node);
    }

    return node.value;
}

void Parser::extractReferences(const Value& value, std::vector<std::string>& references) {
    if (value.type == DataType::VARIABLE) {
        references.push_back(value.string_value);
    }
}

std::future<Value> Parser::functionHTTPAsync(size_t startPos, const std::string& method, const std::vector<Value>& args) {
    return executeAsyncIfEnabled([this, startPos, method, args]() {
        return functionHTTP(startPos, method, args);
    });
}

std::future<Value> Parser::functionFILEAsync(const std::vector<Value>& args) {
    return executeAsyncIfEnabled([this, args]() {
        return functionFILE(args);
    });
}

Value Parser::functionVALUE(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("VALUE function requires one variable argument");
    }
    if (args[0].type != DataType::VARIABLE) {
        return args[0];
    }

    std::string varName = args[0].string_value;
    return resolveVariableValue(varName, true);
}

Value Parser::functionSTRING(const std::vector<Value>& args) {
    if (args.empty()) return stringToValue("");

    return stringToValue(args[0].toString());
}

Value Parser::functionLINK(const std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("LINK function requires one argument");

    std::string str = args[0].toString();
    if (!isValidLink(str)) {
        throw std::runtime_error("Invalid link: " + str);
    }

    return linkToValue(str);
}

Value Parser::functionBINARY(const std::vector<Value>& args) {
    if (args.empty()) return binaryToValue("0");

    double num = args[0].toNumber();
    std::string binary;
    int intNum = static_cast<int>(num);

    if (intNum == 0) return binaryToValue("0");

    while (intNum > 0) {
        binary = (intNum % 2 == 0 ? "0" : "1") + binary;
        intNum /= 2;
    }

    return binaryToValue(binary);
}

Value Parser::functionOCTAL(const std::vector<Value>& args) {
    if (args.empty()) return octalToValue("0");

    double num = args[0].toNumber();
    std::stringstream ss;
    ss << std::oct << static_cast<int>(num);
    return octalToValue(ss.str());
}

Value Parser::functionHEXADECIMAL(const std::vector<Value>& args) {
    if (args.empty()) return hexToValue("0");

    double num = args[0].toNumber();
    std::stringstream ss;
    ss << std::hex << static_cast<int>(num);
    return hexToValue(ss.str());
}

Value Parser::functionTYPEID(const std::vector<Value>& args) {
    if (args.empty()) return numberToValue(static_cast<double>(DataType::UNKNOWN));

    return numberToValue(static_cast<double>(args[0].type));
}

Value Parser::functionTYPEOF(const std::vector<Value>& args) {
    if (args.empty()) return stringToValue("unknown");

    return stringToValue(dataTypeToString(args[0].type));
}

Value Parser::functionECHO(const std::vector<Value>& args) {
    for (const auto& arg : args) {
        std::string message;

        if (arg.type == DataType::VARIABLE) {
            Value resolved = resolveVariableValue(arg.string_value, false);
            if (resolved.type != DataType::UNKNOWN) {
                message = Utility::value2string(resolved);
            } else {
                message = arg.string_value;
            }
        } else {
            message = arg.toString();
        }

        addLog("ECHO", message, currentToken().start);
        std::cout << message << std::endl;
    }
    return Value();
}

Value Parser::functionJSON(const std::vector<Value>& args) { return Value(); }

Value Parser::functionHTTP(size_t startPos, const std::string& method, const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("Expected one argument at function HTTPTEXT at " + Utility::position(startPos, input) + ".");
    } else if (args[0].type != DataType::LINK) {
        throw std::runtime_error("Expected TYPEOF( argument 0 )=\"Link\" at function HTTPTEXT at " + Utility::position(startPos, input) + ", got \"" + dataTypeToString(args[0].type) + "\".");
    }

    std::string url = args[0].toString();
    std::string headersStr = args[1].toString();
    std::string body = args[2].toString();
    std::unordered_map<std::string, std::string> headers = Utility::ParseHeaders(headersStr);
    if (headers.find("Accept") == headers.end()) {
        headers["Accept"] = Utility::defaultHTTPAccept;
    }

    Value result;
    if (method == "POST") {
        result = HTTP::POST(url, headers, body);
    } else if (method == "PUT") {
        result = HTTP::PUT(url, headers, body);
    } else if (method == "PATCH") {
        result = HTTP::PATCH(url, headers, body);
    } else if (method == "DELETE") {
        result = (HTTP::DELETE_)(url, headers);
    } else if (method == "HEAD") {
        result = HTTP::HEAD(url, headers);
    } else if (method == "OPTIONS") {
        result = HTTP::OPTIONS(url, headers);
    } else {
        result = HTTP::GET(url, headers);
    }
    if (!body.empty() && method != "POST" && method != "PUT" && method != "PATCH") {
        Utility::Warn("HTTP: Cannot send body with method \"" + method + "\" at " + Utility::position(startPos, input) + ".");
    }
    if ((match(".") || match(":")) && peekToken().type == "identifier") {
        advance();
        std::string funcName = currentToken().value;
        advance();
        if (currentToken().type == "(" && peekToken().type == ")") {
            position += 2;
            if (result.object_value.find(funcName) != result.object_value.end()) {
                return result.object_value[funcName];
            } else {
                throw std::runtime_error("HTTP.Response: Unknown function \"" + funcName + "\" at " + Utility::position(currentToken().start, input) + ".");
            }
        } else throw std::runtime_error("Expected function call at " + Utility::position(currentToken().start, input) + ".");
    } else return result;
}

Value Parser::merger(const std::vector<Value>& args) {
    std::string key = args[0].toString();
    Value value = args[1];
    variables[key] = value;
    return Value::createNull();
}
Value Parser::isolated(const std::string& code, bool doExecute, size_t startPos, const std::unordered_map<std::string, Value>* context, const std::string name, bool merge, bool silent, ParserType ptype, const std::vector<ParserToken> ptokens) {
    try {
        std::vector<ParserToken> inputT;
        bool useInput = false;
        if (ptokens.size() > 0) {
            inputT.reserve(ptokens.size());
            inputT.insert(inputT.begin(), ptokens.begin(), ptokens.end());
            useInput = true;
        } else {
            auto lexerResult = Lexer::parse(code);
            inputT.reserve(lexerResult.second.size());
            inputT.insert(inputT.begin(), lexerResult.second.begin(), lexerResult.second.end());
        }

        std::string currName = "function";
        bool isFunction = true;
        if (context == nullptr) {
            currName = "JUSTC";
            isFunction = false;
        }
        if (name != "auto") {
            currName = name;
        }

        Parser isolatedParser(
            inputT,
            doExecute && this->doExecute,
            this->runAsync,
            useInput ? input : code,
            this->allowJavaScript,
            this->canAllowJS,
            this->scriptName + "::" + currName,
            currName,
            this->allowLuau,
            this->canAllowLuau,
            isFunction,
            context,
            chartype,
            ptype
        );

        ParseResult result;

        isolatedParser.userFunctions = this->userFunctions;
        isolatedParser.userFunctionsConst = this->userFunctionsConst;

        if (merge) {
            isolatedParser.variableUpdateListener([this](const std::vector<Value>& args) {
                return this->merger(args);
            });
            if (context) {
                isolatedParser.variableUpdateListener([this, context](const std::vector<Value>& args) {
                    if (args.size() < 2) return Value::createNull();
                    std::string key = args[0].toString();
                    Value value = args[1];
                    const_cast<std::unordered_map<std::string, Value>*>(context)->operator[](key) = value;
                    return Value::createNull();
                });
            }
        }

        isolatedParser.throwError = true;

        result = isolatedParser.parse(doExecute);

        Value isolatedObject;
        isolatedObject.type = DataType::JUSTC_OBJECT;
        isolatedObject.object_type = DataType::JUSTC_OBJECT;
        isolatedObject.name = "Object";

        if (isolatedParser.outputMode == "everything") {
            isolatedObject.properties = pmap(result.returnValues);
        } else if (isolatedParser.outputMode == "specified") {
            for (size_t i = 0; i < isolatedParser.outputVariables.size(); i++) {
                const auto& varName = isolatedParser.outputVariables[i];
                std::string outputName = (i < isolatedParser.outputNames.size()) ? isolatedParser.outputNames[i] : varName;
                if (result.returnValues.find(varName) != result.returnValues.end()) {
                    if (outputName != "_") {
                        isolatedObject.properties[outputName] = p(result.returnValues.at(varName));
                    } else {
                        isolatedObject.properties[varName] = p(result.returnValues.at(varName));
                    }
                }
            }
        } else if (isolatedParser.outputMode == "disabled" && isFunction && result.returnValues.empty()) {
            isolatedObject.properties["return"] = p(Value::createNull());
        }

        if (isolatedObject.properties.empty() && !result.returnValues.empty()) {
            isolatedObject.properties = pmap(result.returnValues);
        }

        auto objectContext = std::make_shared<ObjectContext>();
        objectContext->parser = std::make_shared<Parser>(isolatedParser);
        objectContext->variables = result.returnValues;
        objectContext->outputMode = isolatedParser.outputMode;
        objectContext->outputVariables = isolatedParser.outputVariables;
        objectContext->allowJavaScript = isolatedParser.allowJavaScript;
        objectContext->allowLuau = isolatedParser.allowLuau;
        isolatedObject.object_context = objectContext;

        if (!silent) {
            for (const auto& log : result.logs) {
                addLog(log.type, log.message, log.position);
            }
        }
        for (const auto& importLog : result.importLogs) {
            addImportLog(importLog[0], importLog[1], importLog[2]);
        }

        if (merge) {
            if (result.variables) {
                for (const auto& [key, value] : *result.variables) {
                    auto parentConstIt = constVars.find(key);
                    if ((parentConstIt != constVars.end() && parentConstIt->second) || isBuiltinVariable(key)) {
                        continue;
                    }

                    variables[key] = value;
                    try {
                        mutated.erase(key);
                    } catch (...) {}
                    mutated.try_emplace(key, Mutated(value, startPos));
                    if (result.constants) {
                        auto childConstIt = result.constants->find(key);
                        if (childConstIt != result.constants->end()) {
                            constVars[key] = childConstIt->second;
                        }
                    }
                }
            }
            if (result.constants) {
                for (const auto& [key, isConst] : *result.constants) {
                    if (isBuiltinVariable(key)) {
                        continue;
                    }
                    auto parentVarIt = variables.find(key);
                    if (parentVarIt != variables.end()) {
                        auto parentConstIt = constVars.find(key);
                        if (parentConstIt != constVars.end() && parentConstIt->second) {
                            continue;
                        }
                    }
                    constVars[key] = isConst;
                }
            }
        }

        return isolatedObject;

    } catch (const std::runtime_error& e) {
        throw std::runtime_error(std::string(e.what()) + " (at \"" + this->scriptName + "\" " + Utility::position(startPos, input) + ")");
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string(e.what()) + " (at \"" + this->scriptName + "\" " + Utility::position(startPos, input) + ")");
    }
}
Value Parser::shared(const std::string& code, bool doExecute, size_t startPos, const std::unordered_map<std::string, Value>* context, const std::string name, bool merge, bool silent, ParserType ptype, const std::vector<ParserToken> ptokens) {
    std::unordered_map<std::string, Value> ctx;
    if (context) {
        ctx = *context;
    }

    Value result = isolated(code, doExecute, startPos, &ctx, name, merge, silent, ptype, ptokens);

    if (merge) {
        for (const auto& [key, value] : ctx) {
            auto constIt = constVars.find(key);
            if (constIt != constVars.end() && constIt->second) {
                continue;
            }
            if (isBuiltinVariable(key)) {
                continue;
            }

            variables[key] = value;

            if (constVars.find(key) == constVars.end()) {
                constVars[key] = false;
            }
        }
    }

    return result;
}

Value Parser::emptyJUSTC() {
    auto emptyContext = std::make_shared<ObjectContext>();
    emptyContext->allowJavaScript = this->allowJavaScript;
    emptyContext->allowLuau = this->allowLuau;
    emptyContext->outputMode = "everything";

    Value emptyObject = Value::createJustcObject(emptyContext);
    emptyObject.name = "Object";
    return emptyObject;
}
Value Parser::functionJUSTC(const std::vector<Value>& args, size_t startPos) {
    if (args.empty()) {
        return emptyJUSTC();
    }

    std::string code;

    if (args[0].type == DataType::STRING) {
        code = args[0].string_value;
    } else if (args[0].type == DataType::VARIABLE) {
        Value resolved = resolveVariableValue(args[0].string_value, true);
        if (resolved.type == DataType::STRING) {
            code = resolved.string_value;
        } else {
            code = args[0].toString();
        }
    } else {
        code = args[0].toString();
    }

    bool execute = this->doExecute;
    if (args.size() > 1) {
        execute = args[1].toBoolean();
    }

    return isolated(code, execute, startPos);
}
Value Parser::functionJUSTC2(const std::string& code, bool doExecute, size_t startPos) {
    bool execute = doExecute;
    if (!this->doExecute) {
        execute = false;
    }

    return isolated(code, execute, startPos);
}

Value Parser::ParseJUSTO(const std::string& code) {
    JUSTO::JUSTOParser parser;
    return parser.parse(code);
}
Value Parser::functionJUSTO(const std::vector<Value>& args) {
    if (args.empty()) {
        return emptyJUSTC();
    }

    std::string code;

    if (args[0].type == DataType::STRING) {
        code = args[0].string_value;
    } else if (args[0].type == DataType::VARIABLE) {
        Value resolved = resolveVariableValue(args[0].string_value, true);
        if (resolved.type == DataType::STRING) {
            code = resolved.string_value;
        } else {
            code = args[0].toString();
        }
    } else {
        code = args[0].toString();
    }

    return ParseJUSTO(code);
}
Value Parser::toJUSTO(const std::vector<Value>& args) {
    if (args.empty()) {
        return Value::createString("");
    }
    return Value::createString(JUSTO::valueToJUSTO(args[0]));
}

Value Parser::i2v(Value fromIsolated) { // isolatedToValue
    return v(fromIsolated.properties["return"]);
}
std::string Parser::t2i(ParserToken toIsolated) { // tokenToIsolated
    std::string out;
    if (toIsolated.type == "string") {
        out = "\"" + toIsolated.value + "\"";
    } else if (toIsolated.type == "link") {
        out = "<" + toIsolated.value + ">";
    } else if (toIsolated.type == "Luau") {
        out = "<<" + toIsolated.value + ">>";
    } else if (toIsolated.type == "JavaScript") {
        out = "{{" + toIsolated.value + "}}";
    } else {
        out = toIsolated.value;
    }
    return out + " ";
}

Value Parser::parseCondition(bool doExecute, bool wasIsolated) {
    size_t startPos = currentToken().start;
    int conditionType = 0; // 0 = if; 1 = for; 2 = while; 3 = elseif; 4 = try
    std::string errMsg = "Expected \"if\"/\"for\"/\"while\"/\"try\" keyword at " + Utility::position(startPos, input) + ".";
    bool isIsolated = wasIsolated;

    if (match("keyword", "isolated")) {
        isIsolated = true;
        advance();
    }

    if (match("keyword", "if")) {
        conditionType = 0;
    } else if (match("keyword", "for")) {
        conditionType = 1;
    } else if (match("keyword", "while")) {
        conditionType = 2;
    } else if (match("keyword", "elseif")) {
        conditionType = 3;
    } else if (match("keyword", "try")) {
        conditionType = 4;
    } else {
        throw std::runtime_error(errMsg);
    }
    advance();

    std::unordered_map<std::string, Value> conditionContext;
    std::unordered_map<std::string, Value> conditionBodyContext;
    for (const auto& [key, value] : this->variables) {
        try {
            conditionContext[key] = resolveVariableValue(key, false);
        } catch (...) {
            conditionContext[key] = value;
        }
    }
    if (!isIsolated) {
        conditionBodyContext = conditionContext;
    }

    std::vector<Value> importedContext = parseLambda(doExecute, currentToken().start);
    for (Value importedVar : importedContext) {
        conditionContext[importedVar.name] = importedVar;
        conditionBodyContext[importedVar.name] = importedVar;
    }

    if (conditionType != 4) {
        if (!match("(")) {
            std::string currKeyword = "if";
            switch (conditionType) {
                case 1:
                    currKeyword = "for";
                    break;
                case 2:
                    currKeyword = "while";
                    break;
                case 3:
                    currKeyword = "elseif";
                    break;
                default:
                    currKeyword = "if";
                    break;
            }
            throw std::runtime_error("Expected '(' after '" + currKeyword + "' at " + Utility::position(currentToken().start, input) + ".");
        }
        advance();
    }

    std::stringstream first;
    std::stringstream second;
    std::stringstream third;
    int ssnum = 1;

    if (conditionType != 4) {
        int braceCount = 1;
        int braceCount2= 0;
        int braceCount3= 0;
        while (braceCount > 0 && !isEnd()) {
            if (match("(")) braceCount++;
            else if (match(")")) braceCount--;

            if (braceCount > 0) {
                if (braceCount == 1 && match(";") && braceCount2 == 0 && braceCount3 == 0) {
                    ssnum++;
                } else {
                    if (match("{")) braceCount2++;
                    else if (match("}")) braceCount2--;
                    else if (match("[")) braceCount3++;
                    else if (match("]")) braceCount3--;

                    std::string out = t2i(currentToken());
                    switch (ssnum) {
                        case 1:
                            first << out;
                            break;
                        case 2:
                            second << out;
                            break;
                        default:
                            third << out;
                            break;
                    }
                }
                if (ssnum > 3) throw std::runtime_error("Unexpected ';' at " + Utility::position(currentToken().start, input) + ".");
            }
            advance();
        }

        if (braceCount != 0) {
            throw std::runtime_error("Expected ')' after condition at " + Utility::position(currentToken().start, input) + ".");
        }
    }
    std::string conditionBodyErr = "Expected '{' for condition body at " + Utility::position(currentToken().start, input) + ".";
    if (!match("{")) {
        throw std::runtime_error(conditionBodyErr);
    }
    advance();

    enterScope();
    std::stringstream body;

    int braceCount = 1;
    while (!isEnd() && braceCount > 0) {
        if (match("{")) braceCount++;
        else if (match("}")) braceCount--;

        if (braceCount > 0) {
            body << t2i(currentToken());
        }
        advance();
    }

    std::string unclosedBody = "Unclosed condition body at " + Utility::position(currentToken().start, input) + ".";
    if (braceCount != 0) {
        throw std::runtime_error(unclosedBody);
    }

    std::string conditionBody = body.str();

    switch (conditionType) {
        case 0: case 3: { // if/elseif
            std::string currOp = conditionType == 0 ? "if" : "elseif";
            bool conditionResult = i2v(isolated("return " + first.str() + " .", doExecute, startPos, &conditionContext, "'" + currOp + "' condition at " + Utility::position(currentToken().start, input))).toBoolean();
            exitScope();

            if (conditionResult) {
                return shared(conditionBody, doExecute, startPos, &conditionBodyContext, "'" + currOp + "' body at " + Utility::position(currentToken().start, input), !isIsolated);
            } else if (match("keyword", "else")) {
                advance();
                if (peekToken().type == "keyword" && peekToken().value == "if") {
                    return parseCondition(doExecute, isIsolated);
                }

                std::unordered_map<std::string, Value> elseContext = conditionBodyContext;
                std::vector<Value> importedContext2 = parseLambda(doExecute, currentToken().start);
                for (Value importedVar : importedContext2) {
                    elseContext[importedVar.name] = importedVar;
                }
                
                if (!match("{")) {
                    throw std::runtime_error(conditionBodyErr);
                }
                advance();

                std::stringstream elsebody;

                int braceCount4 = 1;
                while (!isEnd() && braceCount4 > 0) {
                    if (match("{")) braceCount4++;
                    else if (match("}")) braceCount4--;

                    if (braceCount4 > 0) {
                        elsebody << t2i(currentToken());
                    }
                    advance();
                }
                if (braceCount4 != 0) throw std::runtime_error(unclosedBody);

                return shared(elsebody.str(), doExecute, startPos, &elseContext, "'else' body at " + Utility::position(currentToken().start, input), !isIsolated);
            } else if (match("keyword", "elseif")) {
                return parseCondition(doExecute, isIsolated);
            } else return Value::createNull();
        } case 2: { // while
            std::string conditionStr = "return " + first.str() + " .";
            bool conditionResult = i2v(isolated(conditionStr, doExecute, startPos, &conditionContext, "'while' condition at " + Utility::position(currentToken().start, input))).toBoolean();
            exitScope();
            Value lastResult = Value::createNull();
            while (conditionResult) {
                lastResult = shared(conditionBody, doExecute, startPos, &conditionBodyContext, "'while' body at " + Utility::position(currentToken().start, input), !isIsolated);
                for (const auto& [key, value] : this->variables) {
                    try {
                        conditionContext[key] = resolveVariableValue(key, false);
                    } catch (...) {
                        conditionContext[key] = value;
                    }
                }
                enterScope();
                conditionResult = i2v(isolated(conditionStr, doExecute, startPos, &conditionContext, "'while' condition at " + Utility::position(currentToken().start, input))).toBoolean();
                exitScope();
            }
            return lastResult;
        } case 4: { // try
            exitScope();
            Value result = Value::createNull();
            bool failed = false;
            std::string err = "";
            try {
                result = shared(conditionBody, doExecute, startPos, &conditionBodyContext, "'try' body at " + Utility::position(currentToken().start, input), !isIsolated);
            } catch (const std::runtime_error& e) {
                failed = true;
                err = std::string(e.what());
            } catch (const std::exception& e) {
                failed = true;
                err = std::string(e.what());
            } catch (...) {
                failed = true;
                err = "Unknown error.";
            }
            if (failed && match("keyword", "catch")) {
                advance();

                std::unordered_map<std::string, Value> catchContext = conditionBodyContext;
                std::vector<Value> importedContext2 = parseLambda(doExecute, currentToken().start);
                for (Value importedVar : importedContext2) {
                    catchContext[importedVar.name] = importedVar;
                }

                if (match("(")) {
                    advance();
                    if (match(")")) {
                        advance();
                    } else {
                        Value errval = Value::createString(err);
                        errval.type = DataType::ERROR;
                        catchContext[currentToken().value] = errval;
                        advance();
                        if (!match(")")) throw std::runtime_error("Expected \")\" at " + Utility::position(currentToken().start, input) + ".");
                        advance();
                    }
                }

                if (!match("{")) throw std::runtime_error(conditionBodyErr);
                advance();

                std::stringstream catchbody;

                int braceCount4 = 1;
                while (!isEnd() && braceCount4 > 0) {
                    if (match("{")) braceCount4++;
                    else if (match("}")) braceCount4--;

                    if (braceCount4 > 0) {
                        catchbody << t2i(currentToken());
                    }
                    advance();
                }
                if (braceCount4 != 0) throw std::runtime_error(unclosedBody);

                result = shared(catchbody.str(), doExecute, startPos, &catchContext, "'catch' body at " + Utility::position(currentToken().start, input), !isIsolated);
            }
            if (match("keyword", "finally")) {
                advance();

                std::unordered_map<std::string, Value> finallyContext = conditionBodyContext;
                std::vector<Value> importedContext2 = parseLambda(doExecute, currentToken().start);
                for (Value importedVar : importedContext2) {
                    finallyContext[importedVar.name] = importedVar;
                }

                if (!match("{")) throw std::runtime_error(conditionBodyErr);
                advance();

                std::stringstream finallybody;

                int braceCount4 = 1;
                while (!isEnd() && braceCount4 > 0) {
                    if (match("{")) braceCount4++;
                    else if (match("}")) braceCount4--;

                    if (braceCount4 > 0) {
                        finallybody << t2i(currentToken());
                    }
                    advance();
                }
                if (braceCount4 != 0) throw std::runtime_error(unclosedBody);

                result = shared(finallybody.str(), doExecute, startPos, &finallyContext, "'finally' body at " + Utility::position(currentToken().start, input), !isIsolated);
            }
            return result;
        } default:
            throw std::runtime_error(errMsg);
    }
}

Value Parser::parseFunctionDeclaration(bool doExecute, std::string funcName, bool requireName) {
    bool isIsolated = false;

    if (match("keyword", "isolated")) {
        isIsolated = true;
        advance();
    }
    if (!match("keyword", "function")) {
        throw std::runtime_error("Expected 'function' keyword at " + Utility::position(currentToken().start, input));
    }
    advance();

    if (requireName) {
        if (!match("identifier")) {
            throw std::runtime_error("Expected function name at " + Utility::position(currentToken().start, input));
        }
        funcName = currentToken().value;
        advance();
    } else {
        if (match("identifier")) {
            funcName = currentToken().value;
            advance();
        }
    }

    std::vector<Value> importedContext = parseLambda(doExecute, currentToken().start);

    if (!match("(")) {
        throw std::runtime_error("Expected '(' after function name at " + Utility::position(currentToken().start, input));
    }
    advance();

    FunctionInfo funcInfo;
    funcInfo.isIsolated = isIsolated;
    std::vector<std::string> paramNames;

    while (!match(")") && !isEnd()) {
        if (match("identifier")) {
            std::string paramName = currentToken().value;
            advance();

            DataType paramType = DataType::UNKNOWN;
            Value defaultValue;
            bool hasDefault = false;

            if (match(":")) {
                advance();
                if (match("identifier")) {
                    std::string typeName = currentToken().value;
                    try {
                        paramType = Utility::typeDeclaration2dataType(typeName, Utility::position(currentToken().start, input));
                    } catch (...) {
                        paramType = DataType::UNKNOWN;
                    }
                    advance();
                }
            }

            if (match("=") || match("keyword", "is")) {
                advance();
                defaultValue = parseExpression(doExecute);
                hasDefault = true;
            }

            funcInfo.paramNames.push_back(paramName);
            funcInfo.paramTypes.push_back(paramType);
            funcInfo.defaultValues.push_back(hasDefault ? defaultValue : Value::createNull());

            if (match(",")) {
                advance();
            }
        } else {
            throw std::runtime_error("Expected parameter name at " + Utility::position(currentToken().start, input));
        }
    }

    if (!match(")")) {
        throw std::runtime_error("Expected ')' after parameters at " + Utility::position(currentToken().start, input));
    }
    advance();

    if (!match("{")) {
        throw std::runtime_error("Expected '{' for function body at " + Utility::position(currentToken().start, input));
    }
    advance();

    std::stringstream body;
    int braceCount = 1;

    while (!isEnd() && braceCount > 0) {
        if (match("{")) braceCount++;
        else if (match("}")) braceCount--;

        if (braceCount > 0) {
            funcInfo.tokens.push_back(currentToken());
            body << t2i(currentToken());
        }
        advance();
    }

    if (braceCount != 0) {
        throw std::runtime_error("Unclosed function body at " + Utility::position(currentToken().start, input));
    }

    std::string functionBody = body.str();

    Value result;
    result.type = DataType::FUNCTION;
    result.string_value = functionBody;
    result.name = funcName;
    result.function_info = funcInfo;
    result.array_elements = importedContext;

    auto closureContext = std::make_shared<ObjectContext>();
    if (!isIsolated) {
        for (const auto& [key, value] : this->variables) {
            closureContext->variables[key] = value;
        }
    }
    closureContext->allowJavaScript = this->allowJavaScript;
    closureContext->allowLuau = this->allowLuau;
    result.closure_context = closureContext;

    return result;
}

Value Parser::callFunction(const Value& function, const std::vector<Value>& args, size_t startPos, bool doExecute) {
    if (function.type != DataType::FUNCTION && function.type != DataType::STRUCT && function.type != DataType::CLASS && function.type != DataType::ENUM) {
        throw std::runtime_error("Cannot call non-function value at " + Utility::position(startPos, input));
    } else if (!doExecute) {
        return onExecDisabled(startPos, function.name);
    } else if (function.native) {
        return executeFunction(function.name, args, startPos);
    }

    const auto& funcInfo = function.function_info;

    std::unordered_map<std::string, Value> functionContext;

    if (function.closure_context) {
        for (const auto& [key, value] : function.closure_context->variables) {
            functionContext[key] = value;
        }
    }

    if (!funcInfo.isIsolated) {
        for (const auto& [key, value] : this->variables) {
            try {
                functionContext[key] = resolveVariableValue(key, false);
            } catch (...) {
                functionContext[key] = value;
            }
        }
    }

    for (Value importedVar : function.array_elements) {
        functionContext[importedVar.name] = importedVar;
    }

    for (size_t i = 0; i < funcInfo.paramNames.size(); i++) {
        Value paramValue;

        if (i < args.size()) {
            paramValue = args[i];

            if (funcInfo.paramTypes[i] != DataType::UNKNOWN) {
                ASTNode typeNode("TYPE_CHECK", "", startPos);
                typeNode.typeDeclaration = funcInfo.paramTypes[i];
                paramValue = applyTypeDeclaration(paramValue, typeNode);
            }
        } else if (funcInfo.defaultValues[i].type != DataType::NULL_TYPE) {
            paramValue = funcInfo.defaultValues[i];
        } else {
            throw std::runtime_error("Missing required argument '" + funcInfo.paramNames[i] + "' for function '" + function.name + "' at " + Utility::position(startPos, input));
        }

        functionContext[funcInfo.paramNames[i]] = paramValue;
    }

    ParserType ptype = ParserType::SCRIPT;
    switch (function.type) {
        case DataType::STRUCT:
            ptype = ParserType::STRUCT;
            break;
        case DataType::CLASS:
            ptype = ParserType::CLASS;
            break;
        case DataType::ENUM:
            ptype = ParserType::ENUM;
            break;
        default: break;
    }
    Value result = isolated(funcInfo.tokens.size() > 0 ? "" : function.string_value, true, startPos, &functionContext, "auto", false, false, ptype, funcInfo.tokens);

    if (!result.properties.empty()) {
        auto it = result.properties.find("return");
        if (it != result.properties.end()) {
            return v(it->second);
        }

        if (result.properties.size() == 1) {
            return v(result.properties.begin()->second);
        }

        return result;
    }

    return Value::createNull();
}

Value Parser::functionFILE(const std::vector<Value>& args) { return Value(); }
Value Parser::functionSTAT(const std::vector<Value>& args) { return Value(); }
Value Parser::functionCONFIG(const std::vector<Value>& args) { return Value(); }

Value Parser::stringToValue(const std::string& str) {
    Value result;
    result.type = DataType::STRING;
    result.string_value = str;
    result.name = "\"" + str + "\"";
    return result;
}

Value Parser::numberToValue(double num) {
    Value result;
    result.type = DataType::NUMBER;
    result.number_value = num;
    result.name = Utility::doubleToString(num);
    return result;
}

Value Parser::booleanToValue(bool b) {
    Value result;
    result.type = DataType::BOOLEAN;
    result.boolean_value = b;
    result.name = b;
    return result;
}

Value Parser::linkToValue(const std::string& link) {
    Value result;
    result.type = DataType::LINK;
    result.string_value = link;
    result.name = "<" + link + ">";
    return result;
}

Value Parser::pathToValue(const std::string& path) {
    Value result;
    result.type = DataType::PATH;
    result.string_value = path;
    result.name = path;
    return result;
}

Value Parser::hexToValue(const std::string& hexStr) {
    Value result;
    result.type = DataType::HEXADECIMAL;

    std::string cleanHex = hexStr;
    bool isBigNumber = false;

    if (!cleanHex.empty() && std::tolower(cleanHex.back()) == 'b') {
        isBigNumber = true;
        cleanHex.pop_back();
    }

    if (!cleanHex.empty() && cleanHex[0] == '0' &&
        cleanHex.length() > 1 && std::tolower(cleanHex[1]) == 'x') {
        cleanHex = cleanHex.substr(2);
    } else if (!cleanHex.empty() && cleanHex[0] == '#') {
        cleanHex = cleanHex.substr(1);
    } else if (!cleanHex.empty() && cleanHex[0] == 'x') {
        cleanHex = cleanHex.substr(1);
    }

    try {
        if (isBigNumber) {
            unsigned long long num;
            std::stringstream ss;
            ss << std::hex << cleanHex;
            ss >> num;
            result.number_value = static_cast<double>(num);
        } else {
            unsigned int num;
            std::stringstream ss;
            ss << std::hex << cleanHex;
            ss >> num;
            result.number_value = static_cast<double>(num);
        }
    } catch (...) {
        result.number_value = 0.0;
    }

    result.name = Utility::double2hexString(result.number_value);
    if (isBigNumber) {
        result.name += "B";
    }
    return result;
}

Value Parser::binaryToValue(const std::string& binStr) {
    Value result;
    result.type = DataType::BINARY;

    std::string cleanBin = binStr;
    bool isBigNumber = false;

    if (!cleanBin.empty() && std::tolower(cleanBin.back()) == 'b') {
        isBigNumber = true;
        cleanBin.pop_back();
    }

    if (!cleanBin.empty() && cleanBin[0] == '0' &&
        cleanBin.length() > 1 && std::tolower(cleanBin[1]) == 'b') {
        cleanBin = cleanBin.substr(2);
    } else if (!cleanBin.empty() && (cleanBin[0] == 'b' || cleanBin[0] == 'B')) {
        cleanBin = cleanBin.substr(1);
    }

    try {
        if (isBigNumber) {
            unsigned long long num = 0;
            for (char c : cleanBin) {
                num = (num << 1) | (c == '1' ? 1 : 0);
            }
            result.number_value = static_cast<double>(num);
        } else {
            unsigned int num = 0;
            for (char c : cleanBin) {
                num = (num << 1) | (c == '1' ? 1 : 0);
            }
            result.number_value = static_cast<double>(num);
        }
    } catch (...) {
        result.number_value = 0.0;
    }

    result.name = Utility::double2binString(result.number_value);
    if (isBigNumber) {
        result.name += "B";
    }
    return result;
}

Value Parser::octalToValue(const std::string& octStr) {
    Value result;
    result.type = DataType::OCTAL;

    std::string cleanOct = octStr;
    bool isBigNumber = false;

    if (!cleanOct.empty() && std::tolower(cleanOct.back()) == 'b') {
        isBigNumber = true;
        cleanOct.pop_back();
    }

    if (!cleanOct.empty() && cleanOct[0] == '0' &&
        cleanOct.length() > 1 && std::tolower(cleanOct[1]) == 'o') {
        cleanOct = cleanOct.substr(2);
    } else if (!cleanOct.empty() && (cleanOct[0] == 'o' || cleanOct[0] == 'O')) {
        cleanOct = cleanOct.substr(1);
    }

    try {
        if (isBigNumber) {
            unsigned long long num;
            std::stringstream ss;
            ss << std::oct << cleanOct;
            ss >> num;
            result.number_value = static_cast<double>(num);
        } else {
            unsigned int num = std::stoi(cleanOct, nullptr, 8);
            result.number_value = static_cast<double>(num);
        }
    } catch (...) {
        result.number_value = 0.0;
    }

    result.name = Utility::double2octString(result.number_value);
    if (isBigNumber) {
        result.name += "B";
    }
    return result;
}

void Parser::evaluateAllVariablesSync() {
    bool changed;
    uint64_t passes = 0;
    const uint64_t MAX_PASSES = std::min(
        static_cast<uint64_t>(0xFF) * variables.size(), 
        (std::numeric_limits<uint64_t>::max)()
    );

    do {
        changed = false;
        passes++;

        for (auto& [varName, mut] : mutated) {
            if (isBuiltinVariable(varName) || hasLocal(currentScope, varName)) {
                continue;
            }
            if (mut.applied) continue;

            auto constIt = constVars.find(varName);
            if (constIt != constVars.end() && constIt->second) {
                continue;
            }

            ASTNode* originalNode = nullptr;
            for (auto& node : ast) {
                if (node.type == "VARIABLE_DECLARATION" && node.identifier == varName) {
                    originalNode = &node;
                    break;
                }
            }

            if (originalNode && mut.startPos > originalNode->startPos) {
                if (variables[varName].toString() != mut.value.toString()) {
                    variables[varName] = mut.value;
                    constVars[varName] = false;
                    changed = true;
                    mut.applied = true;
                    triggerVariableUpdate(varName, mut.value);
                }
            }
        }

        for (auto& node : ast) {
            if (node.type == "VARIABLE_DECLARATION") {
                std::string varName = node.identifier;
                if (isBuiltinVariable(varName) || hasLocal(currentScope, varName)) {
                    continue;
                }

                auto mutIt = mutated.find(varName);
                if (mutIt != mutated.end() && !mutIt->second.applied) {
                    continue;
                }

                bool isConst = node.constant;
                auto constIt = constVars.find(varName);
                if (constIt != constVars.end() && constIt->second && variables[varName].type != DataType::UNKNOWN) {
                    continue;
                }

                Value newValue = evaluateASTNode(node);

                if (newValue.type == DataType::VARIABLE && newValue.string_value == varName) {
                    throw std::runtime_error("Variable cannot reference itself: " + varName);
                }

                if (newValue.type != DataType::UNKNOWN) {
                    if (variables[varName].type == DataType::UNKNOWN || variables[varName].toString() != newValue.toString()) {
                        variables[varName] = newValue;
                        if (isConst) constVars[varName] = true;
                        changed = true;
                        triggerVariableUpdate(varName, newValue);
                    }
                }
            }
        }

    } while (changed && passes < MAX_PASSES);

    mutated.clear();

    if (passes >= MAX_PASSES) {
        throw std::runtime_error("Cannot resolve variable dependencies - possible circular reference.");
    }
}

void Parser::evaluateAllVariablesAsync() {
#ifndef __EMSCRIPTEN__
    std::unordered_map<std::string, std::future<Value>> futures;

    for (auto& node : ast) {
        if (node.type == "VARIABLE_DECLARATION") {
            std::string varName = node.identifier;
            if (dependencies[varName].empty()) {
                futures[varName] = executeAsyncIfEnabled([this, node]() {
                    return evaluateASTNode(node);
                });
            }
        }
    }

    for (auto it = futures.begin(); it != futures.end(); ++it) {
        variables[it->first] = it->second.get();
    }

    evaluateAllVariablesSync();
#else
    evaluateAllVariablesSync();
#endif
}

std::shared_ptr<ObjectContext> Parser::createObjectContext(bool inheritFromParent) {
    auto context = std::make_shared<ObjectContext>();

    if (inheritFromParent) {
        context->allowJavaScript = allowJavaScript;
        context->allowLuau = allowLuau;
    } else {
        context->allowJavaScript = true;
        context->allowLuau = true;
    }

    context->outputMode = "everything";
    return context;
}
Value Parser::parseJustcObject(bool doExecute) {
    if (!match("|")) {
        throw std::runtime_error("Expected '|' for object.");
    }
    advance();

    auto objectContext = createObjectContext(true);

    std::string objectContent;
    int pipeCount = 1;
    bool inString = false;
    bool inComment = false;
    char stringChar = 0;

    while (!isEnd() && pipeCount > 0) {
        ParserToken current = currentToken();
        std::string currentValue = current.value;

        if (!inComment && current.type == "string") {
            inString = !inString;
        }

        if (!inString && !inComment) {
            if (current.type == "|") {
                pipeCount--;
                if (pipeCount == 0) {
                    advance();
                    break;
                }
            } else if (current.type == "{" && peekToken().type == "{") {
                advance();
                advance();

                int jsBraces = 1;
                while (!isEnd() && jsBraces > 0) {
                    if (match("{")) jsBraces++;
                    else if (match("}")) jsBraces--;
                    advance();
                }
                continue;
            } else if (current.type == "<" && peekToken().type == "<") {
                advance();
                advance();

                int luauAngles = 1;
                while (!isEnd() && luauAngles > 0) {
                    if (match("<") && peekToken().type == "<") {
                        advance();
                        advance();
                        luauAngles++;
                    } else if (match(">") && peekToken().type == ">") {
                        advance();
                        advance();
                        luauAngles--;
                    } else {
                        advance();
                    }
                }
                continue;
            } else if (current.type == "|") {
                pipeCount++;
            }
        }

        objectContent += current.value + " ";
        advance();
    }

    if (pipeCount > 0) {
        throw std::runtime_error("Unclosed object at " + Utility::position(currentToken().start, input) + ".");
    }

    auto lexerResult = Lexer::parse(objectContent, false);

    std::unordered_map<std::string, Value> currentContext;
    for (const auto& [key, value] : this->variables) {
        try {
            currentContext[key] = resolveVariableValue(key, false);
        } catch (...) {
            currentContext[key] = value;
        }
    }

    auto objectParser = std::make_shared<Parser>(
        lexerResult.second,
        doExecute,
        runAsync,
        objectContent,
        objectContext->allowJavaScript,
        canAllowJS,
        scriptName + "::object",
        "object",
        objectContext->allowLuau,
        canAllowLuau,
        false,
        &currentContext,
        chartype
    );

    objectContext->parser = objectParser;

    ParseResult objectResult = objectParser->parse(doExecute);

    objectContext->variables = objectResult.returnValues;
    objectContext->outputMode = objectParser->outputMode;
    objectContext->outputVariables = objectParser->outputVariables;

    Value result = Value::createJustcObject(objectContext);

    if (objectParser->outputMode == "everything") {
        result.properties = pmap(objectResult.returnValues);
    } else if (objectParser->outputMode == "specified") {
        for (size_t i = 0; i < objectParser->outputVariables.size(); i++) {
            const auto& varName = objectParser->outputVariables[i];
            std::string outputName = (i < objectParser->outputNames.size()) ?
                                     objectParser->outputNames[i] : varName;

            if (objectResult.returnValues.find(varName) != objectResult.returnValues.end()) {
                if (outputName != "_") {
                    result.properties[outputName] = p(objectResult.returnValues.at(varName));
                } else {
                    result.properties[varName] = p(objectResult.returnValues.at(varName));
                }
            }
        }
    }

    result.name = "Object";
    return result;
}

Value Parser::parseJsonObject(bool doExecute) {
    if (!match("{")) {
        throw std::runtime_error("Expected \"{\" for object.");
    }
    advance();

    std::unordered_map<std::string, Value> properties;

    skipCommas();
    while (!match("}") && !isEnd()) {
        Value keyVal = parseExpression(doExecute, true);
        std::string key;

        if (keyVal.type == DataType::STRING) {
            key = keyVal.string_value;
        } else {
            key = keyVal.toString();
        }

        if (match(":") || match("=") || match("-") || match("keyword", "is")) {
            advance();
        } else if (!CanIgnoreNoAssignmentOperator()) {
            throw std::runtime_error("Expected \":\" after key in object at " + Utility::position(currentToken().start, input) + ".");
        }

        Value valueVal = parseExpression(doExecute);
        properties[key] = valueVal;

        skipCommas();
        if (match(",")) {
            advance();
            skipCommas();
        }
    }

    if (!match("}")) {
        throw std::runtime_error("Expected \"}\" to close object at " + Utility::position(currentToken().start, input) + ".");
    }
    advance();

    auto jsonContext = createObjectContext(true);

    Value result = Value::createJsonObject(properties);
    result.object_context = jsonContext;
    result.name = "Object";

    return result;
}
Value Parser::parseJsonArray(bool doExecute) {
    if (!match("[")) {
        throw std::runtime_error("Expected '[' for array.");
    }
    advance();

    std::vector<Value> elements;

    skipCommas();
    while (!match("]") && !isEnd()) {
        Value element = parseExpression(doExecute);
        elements.push_back(element);

        skipCommas();
        if (match(",")) {
            advance();
            skipCommas();
        }
    }

    if (!match("]")) {
        throw std::runtime_error("Expected ']' to close array at " + Utility::position(currentToken().start, input) + ".");
    }
    advance();

    auto arrayContext = createObjectContext(true);

    Value result = Value::createJsonArray(elements);
    result.object_context = arrayContext;
    result.name = "Array";

    return result;
}
Value Parser::parseObjectPropertyAccess(bool doExecute, bool set) {
    std::vector<std::variant<std::string, size_t>> accessChain;
    std::stringstream accessChainStr;

    std::string firstIdentifier = currentToken().value;
    accessChain.push_back(firstIdentifier);
    advance();

    while ((match(".") || match("[")) && position + 1 < tokens.size()) {
        accessChainStr << t2i(currentToken());
        if (match(".")) {
            advance();
            if (!match("identifier") && !match("keyword") && !isEnd()) {
                throw std::runtime_error("Expected property name after \".\" at " + Utility::position(currentToken().start, input) + ".");
            }
            std::string propName = currentToken().value;
            accessChain.push_back(propName);
            advance();
        } else if (match("[")) {
            advance();
            Value indexVal = parseExpression(doExecute);
            if (indexVal.type == DataType::STRING) {
                accessChain.push_back(indexVal.string_value);
            } else if (indexVal.type == DataType::NUMBER) {
                accessChain.push_back(static_cast<size_t>(indexVal.toNumber()));
            } else {
                throw std::runtime_error("Expected string or numeric index in bracket access, got <" + dataTypeToString(indexVal.type) + "> at " + Utility::position(currentToken().start, input) + ".");
            }
            if (!match("]")) {
                throw std::runtime_error("Expected \"]\" to close array access, got \"" + currentToken().value + "\" at " + Utility::position(currentToken().start, input) + ".");
            }
            advance();
        }
    }

    if (set) return updateObjectProperty(accessChain, accessChainStr.str());

    std::string rootName = std::get<std::string>(accessChain[0]);
    Value currentValue = resolveVariableValue(rootName, false);

    std::string error = "\"" + rootName + "\" is not an object. Attempt to access property or index of not an object";
    auto checkTypeMethods = [&]() -> Value {
        if (doExecute) {
            auto it = typeMethods.find(currentValue.type);
            auto last = accessChain.back();
            if (it != typeMethods.end() && std::holds_alternative<std::string>(last)) {
                std::string funcName = std::get<std::string>(last);
                auto itFunc = typeMethods[currentValue.type].find(funcName);
                if (itFunc != typeMethods[currentValue.type].end()) {
                    if (match("(")) {
                        std::vector<Value> args = {currentValue};
                        std::vector<Value> additionalArgs = parseArguments(doExecute);
                        args.reserve(args.size() + additionalArgs.size());
                        args.insert(args.end(), additionalArgs.begin(), additionalArgs.end());
                        return executeFunction(typeMethods[currentValue.type][funcName], args, currentToken().start);
                    } else {
                        return executeFunction(typeMethods[currentValue.type][funcName], {currentValue}, currentToken().start);
                    }
                }
            }
            if ((currentValue.type == DataType::STRING || currentValue.type == DataType::LINK) && std::holds_alternative<size_t>(last)) {
                size_t index = std::get<size_t>(last);
                currentValue = stringToValue(currentValue.toString());
                return executeFunction("String::Slice", {
                    currentValue, 
                    numberToValue(static_cast<double>(index)), 
                    numberToValue(static_cast<double>(index + 1))
                }, currentToken().start);
            }
        }
        throw std::runtime_error(error + " at " + Utility::position(currentToken().start, input) + ".");
    };

    if (!currentValue.isObject() && accessChain.size() > 1) return checkTypeMethods();
    error = "Cannot access property of non-object";

    for (size_t i = 1; i < accessChain.size() - 1; i++) {
        if (std::holds_alternative<std::string>(accessChain[i])) {
            std::string propName = std::get<std::string>(accessChain[i]);
            currentValue = accessProperty(currentValue, propName).first;
        } else if (std::holds_alternative<size_t>(accessChain[i])) {
            size_t index = std::get<size_t>(accessChain[i]);
            currentValue = accessIndex(currentValue, index);
        }

        if (!currentValue.isObject()) return checkTypeMethods();
    }

    auto last = accessChain.back();

    if (match("(")) { // function
        std::string funcName;
        if (std::holds_alternative<std::string>(last)) {
            funcName = std::get<std::string>(last);
        }

        Value func = accessProperty(currentValue, funcName).first;

        if (func.type == DataType::FUNCTION) { // user funciton
            return callFunction(func, parseArguments(doExecute), currentToken().start, doExecute);
        } else { // built-in function
            return executeFunction(funcName, parseArguments(doExecute), currentToken().start);
        }
    } else { // property/index
        if (std::holds_alternative<std::string>(last)) { // object
            return accessProperty(currentValue, std::get<std::string>(last)).first;
        } else { // array
            return accessIndex(currentValue, std::get<size_t>(last));
        }
    }
}
std::pair<Value, Value::Property> Parser::accessProperty(const Value& obj, const std::string& propName, const Access& requestAccess) {
    if (obj.type == DataType::JUSTC_OBJECT && (obj.object_context && obj.object_context->parser)) {
        if (obj.object_context->parser->outputMode == "disabled") {
            throw std::runtime_error("Attempt to access \"" + propName + "\" of a closure (Object with output mode \"disabled\") at " + Utility::position(currentToken().start, input) + ".");
        }

        auto it = obj.properties.find(propName);
        if (it != obj.properties.end()) {
            return vp(it->second, requestAccess);
        }

        auto& parserVars = obj.object_context->variables;
        auto varIt = parserVars.find(propName);
        if (varIt != parserVars.end()) {
            Value val = varIt->second;
            Value::Property empty(val, Access::READ_WRITE);
            return {val, empty};
        }

        auto itM = typeMethods[DataType::JUSTC_OBJECT].find(propName);
        if (itM != typeMethods[DataType::JUSTC_OBJECT].end()) {
            const size_t currPos = currentToken().start;
            Value funcVal = createFunction([this, obj, propName, currPos](const std::vector<Value>& args) -> Value {
                auto itM2 = typeMethods[DataType::JUSTC_OBJECT].find(propName);
                if (itM2 != typeMethods[DataType::JUSTC_OBJECT].end()) {
                    std::vector<Value> additionalArgs;
                    additionalArgs.reserve(1 + args.size());
                    additionalArgs.push_back(obj);
                    additionalArgs.insert(additionalArgs.end(), args.begin(), args.end());
                    return this->executeFunction(typeMethods[DataType::JUSTC_OBJECT][propName], additionalArgs, currPos);
                } else throw std::runtime_error("\"" + propName + "\" is not a function.");
            }, typeMethods[DataType::JUSTC_OBJECT][propName]);
            Value::Property prop(funcVal, Access::READ_ONLY);
            return {funcVal, prop};
        }

        throw std::runtime_error("Property '" + propName + "' not found in object at " + Utility::position(currentToken().start, input) + ".");
    } else if (Utility::checkObject(obj)) {
        auto it = obj.properties.find(propName);
        if (it != obj.properties.end()) {
            return vp(it->second, requestAccess);
        }

        auto itM = typeMethods[obj.type].find(propName);
        if (itM != typeMethods[obj.type].end()) {
            const size_t currPos = currentToken().start;
            Value funcVal = createFunction([this, obj, propName, currPos](const std::vector<Value>& args) -> Value {
                auto itM2 = typeMethods[obj.type].find(propName);
                if (itM2 != typeMethods[obj.type].end()) {
                    std::vector<Value> additionalArgs;
                    additionalArgs.reserve(1 + args.size());
                    additionalArgs.push_back(obj);
                    additionalArgs.insert(additionalArgs.end(), args.begin(), args.end());
                    return this->executeFunction(typeMethods[obj.type][propName], additionalArgs, currPos);
                } else throw std::runtime_error("\"" + propName + "\" is not a function.");
            }, typeMethods[obj.type][propName]);
            Value::Property prop(funcVal, Access::READ_ONLY);
            return {funcVal, prop};
        }

        throw std::runtime_error("Property '" + propName + "' not found in object at " + Utility::position(currentToken().start, input) + ".");
    } else if (Utility::checkArray(obj)) {
        throw std::runtime_error("Cannot access property '" + propName + "' on array at " + Utility::position(currentToken().start, input) + ".");
    }

    throw std::runtime_error("Cannot access property '" + propName + "' on non-object at " + Utility::position(currentToken().start, input) + ".");
}
Value Parser::accessIndex(const Value& arr, size_t index) {
    if (arr.type == DataType::JSON_ARRAY) {
        if (index < arr.array_elements.size()) {
            return arr.array_elements[index];
        }
        return Value::createNull();
    }
    if (arr.type == DataType::INT8_ARRAY) {
        std::vector<int8_t> a = arr.getComplexData<std::vector<int8_t>>();
        if (index < a.size()) {
            return Value::createNumberWithType(a[index], NumericType::INT8);
        }
        return Value::createNull();
    }
    if (arr.type == DataType::INT16_ARRAY) {
        std::vector<int16_t> a = arr.getComplexData<std::vector<int16_t>>();
        if (index < a.size()) {
            return Value::createNumberWithType(a[index], NumericType::INT16);
        }
        return Value::createNull();
    }
    if (arr.type == DataType::INT32_ARRAY) {
        std::vector<int32_t> a = arr.getComplexData<std::vector<int32_t>>();
        if (index < a.size()) {
            return Value::createNumberWithType(a[index], NumericType::INT32);
        }
        return Value::createNull();
    }
    if (arr.type == DataType::INT64_ARRAY) {
        std::vector<int64_t> a = arr.getComplexData<std::vector<int64_t>>();
        if (index < a.size()) {
            return Value::createNumberWithType(a[index], NumericType::INT64);
        }
        return Value::createNull();
    }
    if (arr.type == DataType::UINT8_ARRAY || arr.type == DataType::CUINT8_ARRAY) {
        std::vector<uint8_t> a = arr.getComplexData<std::vector<uint8_t>>();
        if (index < a.size()) {
            return Value::createNumberWithType(a[index], arr.type == DataType::UINT8_ARRAY ? NumericType::UINT8 : NumericType::CUINT8);
        }
        return Value::createNull();
    }
    if (arr.type == DataType::UINT16_ARRAY || arr.type == DataType::CUINT16_ARRAY) {
        std::vector<uint16_t> a = arr.getComplexData<std::vector<uint16_t>>();
        if (index < a.size()) {
            return Value::createNumberWithType(a[index], arr.type == DataType::UINT16_ARRAY ? NumericType::UINT16 : NumericType::CUINT16);
        }
        return Value::createNull();
    }
    if (arr.type == DataType::UINT32_ARRAY || arr.type == DataType::CUINT32_ARRAY) {
        std::vector<uint32_t> a = arr.getComplexData<std::vector<uint32_t>>();
        if (index < a.size()) {
            return Value::createNumberWithType(a[index], arr.type == DataType::UINT32_ARRAY ? NumericType::UINT32 : NumericType::CUINT32);
        }
        return Value::createNull();
    }
    if (arr.type == DataType::UINT64_ARRAY || arr.type == DataType::CUINT64_ARRAY) {
        std::vector<uint64_t> a = arr.getComplexData<std::vector<uint64_t>>();
        if (index < a.size()) {
            return Value::createNumberWithType(a[index], arr.type == DataType::UINT64_ARRAY ? NumericType::UINT64 : NumericType::CUINT64);
        }
        return Value::createNull();
    }
    if (arr.type == DataType::FLOAT32_ARRAY) {
        std::vector<float> a = arr.getComplexData<std::vector<float>>();
        if (index < a.size()) {
            return Value::createNumberWithType(a[index], NumericType::FLOAT32);
        }
        return Value::createNull();
    }
    if (arr.type == DataType::FLOAT64_ARRAY) {
        std::vector<double> a = arr.getComplexData<std::vector<double>>();
        if (index < a.size()) {
            return Value::createNumberWithType(a[index], NumericType::FLOAT64);
        }
        return Value::createNull();
    }
    throw std::runtime_error("Cannot access index " + std::to_string(index) + " on non-array at " + Utility::position(currentToken().start, input) + ".");
}
std::vector<Value> Parser::parseArguments(bool doExecute) {
    std::vector<Value> args;
    if (!match("(")) throw std::runtime_error("Expected '(' for function arguments at " + Utility::position(currentToken().start, input) + ".");
    advance();

    while (!match(")") && !isEnd()) {
        args.push_back(parseExpression(doExecute));
        if (match(",") || match(";")) advance();
    }

    if (!match(")")) {
        throw std::runtime_error("Expected ')' after function arguments at " + Utility::position(currentToken().start, input) + ".");
    }
    advance();

    return args;
}

void Parser::initializeCPPTypes() {
    cpptypes = ::cpptypes;
    cppnumbers = ::cppnumbers;
}
bool Parser::isCPPType() {
    if (!match("keyword")) return false;
    return std::find(cpptypes.begin(), cpptypes.end(), currentToken().value) != cpptypes.end();
}
bool Parser::isCPPNumber(const std::string& cpptype) {
    return std::find(cppnumbers.begin(), cppnumbers.end(), cpptype) != cppnumbers.end();
}
void Parser::initializeBuiltIns() {
    builtins = ::builtins;
}
bool Parser::isBuiltinVariable(const std::string& name) const {
    return std::find(builtins.begin(), builtins.end(), name) != builtins.end();
}
void Parser::handleBuiltinVariableAssignment(const std::string& name, const Value& value, size_t startPos) {
    if (name == "charType") {
        updateCharType(value.toString(), startPos);
    } else if (name == "JUSTC") {
        throw std::runtime_error("Attempt to redefine readonly built-in variable \"" + name + "\" at " + Utility::position(startPos, input) + ".");
    }
}
void Parser::removeBuiltinVariablesFromOutput() {
    for (const auto& name : builtins) {
        variables.erase(name);
        constVars.erase(name);
    }
}
void Parser::removeStructsFromOutput() {
    for (const auto& [name, val] : structures) {
        variables.erase(name);
        constVars.erase(name);
    }
}
void Parser::removeClassesFromOutput() {
    for (const auto& [name, val] : classes) {
        variables.erase(name);
        constVars.erase(name);
    }
}
void Parser::finalizeOutput() {
    for (const std::string name : outputExcludeVariables) {
        variables.erase(name);
        constVars.erase(name);
    }
}

void Parser::updateCharType(const std::string& newType, size_t startPos) {
    bool success = true;
    if (newType == "grapheme") {
        chartype = CharType::GRAPHEME;
    } else if (newType == "codepoint") {
        chartype = CharType::CODEPOINT;
    } else if (newType == "byte") {
        chartype = CharType::BYTE;
    } else {
        success = false;
    }
    if (success) {
        addLog("CHARTYPE", newType, startPos);
    } else {
        throw std::runtime_error("Invalid chartype: " + newType + ". Must be 'grapheme', 'codepoint', or 'byte' at " + Utility::position(startPos, input));
    }
}

void Parser::registerFunction(const std::string& name, Function func, bool isConst) {
    userFunctions[name] = func;
    userFunctionsConst[name] = isConst;
}
void Parser::registerFunctions(const std::unordered_map<std::string, Function>& functions, bool isConst) {
    for (const auto& [name, func] : functions) {
        userFunctions[name] = func;
        userFunctionsConst[name] = isConst;
    }
}
void Parser::unregisterFunction(const std::string& name) {
    userFunctions.erase(name);
    userFunctionsConst.erase(name);
}
bool Parser::hasFunction(const std::string& name) const {
    return userFunctions.find(name) != userFunctions.end();
}

void Parser::variableUpdateListener(Function func) {
    variableUpdateListeners.push_back(func);
}
void Parser::triggerVariableUpdate(const std::string& name, const Value& value) {
    Value key = Value::createString(name);
    std::vector<Value> args = {key, value};
    for (Function func : variableUpdateListeners) {
        try {
            func(args);
        } catch (const std::exception& e) {
            std::cout << std::string(e.what()) << std::endl;
        }
    }
}

ASTNode Parser::typeDeclarationNode(std::string typeDecl, size_t pos) {
    ASTNode node("TYPE_CHECK", "", pos);
    node.typeDeclaration = Utility::typeDeclaration2dataType(typeDecl, Utility::position(pos, input));
    return node;
}
std::vector<Value> Parser::parseLambda(bool doExecute, size_t pos) {
    std::vector<std::string> names;
    std::vector<Value> vars;
    std::vector<std::string> renames;
    std::vector<Value> output;

    if (match("[")) {
        advance();
        while ((match("identifier") || match("string")) && !isEnd()) {
            names.push_back(currentToken().value);
            Value var = parseExpression(doExecute, true);
            if (match(":") && !(position + 1 >= tokens.size())) {
                advance();
                std::string typeDecl = currentToken().value;
                ASTNode typeNode = typeDeclarationNode(typeDecl, pos);
                var = applyTypeDeclaration(var, typeNode);
            }
            vars.push_back(var);
            while ((match(",") || match(";")) && !isEnd()) {
                advance();
            }
        }
        if (isEnd()) {
            throw std::runtime_error("Unclosed lambda at " + Utility::position(pos, input) + ".");
        }
        if (!match("]")) {
            throw std::runtime_error("Expected ']' to close lambda at " + Utility::position(pos, input) + ".");
        }
        advance();
        if (match("keyword", "as") || match(":")) {
            advance();
            if (isEnd()) {
                throw std::runtime_error("Expected '[' at " + Utility::position(pos, input) + ".");
            } else if (match("[")) {
                advance();
                while (!match("]") && !isEnd()) {
                    renames.push_back(currentToken().value);
                    advance();
                    while ((match(",") || match(";")) && !isEnd()) {
                        advance();
                    }
                }
                if (isEnd()) {
                    throw std::runtime_error("Unclosed lambda at " + Utility::position(pos, input) + ".");
                }
                if (!match("]")) {
                    throw std::runtime_error("Expected ']' at " + Utility::position(pos, input) + ".");
                }
                advance();
            } else {
                Value arr = parseExpression(doExecute);
                if (arr.type != DataType::JSON_ARRAY) {
                    throw std::runtime_error("Expected array at " + Utility::position(pos, input) + ".");
                }
                for (Value arrItem : arr.array_elements) {
                    renames.push_back(arrItem.toString());
                }
            }
        }
    } else if (match("identifier") || match("string")) {
        names.push_back(currentToken().value);
        Value var = parseExpression(doExecute, true);
        if (match(":") && !(position + 1 >= tokens.size())) {
            advance();
            std::string typeDecl = currentToken().value;
            ASTNode typeNode = typeDeclarationNode(typeDecl, pos);
            var = applyTypeDeclaration(var, typeNode);
        }
        vars.push_back(var);
        if ((match("keyword", "as") || match(":")) && (peekToken().type == "identifier" || peekToken().type == "string")) {
            advance();
            renames.push_back(currentToken().value);
            advance();
        }
    } else if (match("keyword", "lambda")) {
        advance();
        Value obj = parseExpression(doExecute);
        switch (obj.type) {
            case DataType::JSON_ARRAY: {
                for (Value arrItem : obj.array_elements) {
                    names.push_back(arrItem.name);
                    vars.push_back(arrItem);
                }
                break;
            }
            case DataType::JSON_OBJECT:
            case DataType::JUSTC_OBJECT: {
                for (const auto& [key, value] : obj.properties) {
                    names.push_back(key);
                    vars.push_back(v(value));
                }
                break;
            }
            default:
                throw std::runtime_error("Expected array or object for lambda at " + Utility::position(pos, input) + ".");
        }
    }

    output.reserve(vars.size());
    for (size_t i = 0; i < vars.size(); ++i) {
        Value var = vars[i];
        std::string oldName = (i < names.size()) ? names[i] : var.name;
        std::string newName = (i < renames.size()) ? renames[i] : oldName;
        var.name = newName;
        output.push_back(var);
    }

    return output;
}

void Parser::clearUserFunctions() {
    userFunctions.clear();
    userFunctionsConst.clear();
}

void Parser::registerGlobal(const std::string& name, const Value& value, bool isConst, bool isJUSTC) {
    if (isGlobalConst(name) && !(!isGlobalJUSTC(name) && !isJUSTC)) {
        if (isJUSTC) throw std::runtime_error("Assignment to global constant variable \"" + name + "\" at " + Utility::position(currentToken().start, input) + ".");
        else throw std::runtime_error("Attempt to re-register global constant variable \"" + name + "\".");
    }
    setGlobal(name, value, isConst, isJUSTC);
}
Value Parser::getGlobal(const std::string& name) {
    Value var = getGlobal_(name);
    var.isVariable = true;
    var.variable = name;
    var.varType = VariableType::GLOBAL;
    var.isConst = isGlobalConst(name);
    return var;
}
bool Parser::hasGlobal(const std::string& name) {
    return hasGlobal_(name);
}
void Parser::unregisterGlobal(const std::string& name) {
    removeGlobal(name);
}
void Parser::clearGlobals() {
    clearGlobals_();
}

std::string Parser::getCurrentScopeName() const {
    if (currentScope == rootIndex) {
        return "root_" + std::to_string(rootIndex);
    }
    return "scope_" + std::to_string(currentScope);
}
void Parser::enterScope() {
    static uint64_t scopeIdCounter = 0;
    uint64_t newScope = ++scopeIdCounter;
    
    scopeStack.push_back(currentScope);
    currentScope = newScope;
    
    localScopes[newScope] = std::unordered_map<std::string, Value>();
    localConstVars[newScope] = std::unordered_map<std::string, bool>();
}
void Parser::exitScope() {
    if (!scopeStack.empty()) {
        localScopes.erase(currentScope);
        localConstVars.erase(currentScope);
        
        currentScope = scopeStack.back();
        scopeStack.pop_back();
    }
}
void Parser::setLocal(uint64_t scope, const std::string& name, const Value& value, bool isConst) {
    auto it = localScopes.find(scope);
    if (it != localScopes.end()) {
        it->second[name] = value;
        localConstVars[scope][name] = isConst;
    }
}
Value Parser::getLocal(uint64_t scope, const std::string& name) const {
    auto it = localScopes.find(scope);
    if (it != localScopes.end()) {
        auto varIt = it->second.find(name);
        if (varIt != it->second.end()) {
            Value var = varIt->second;
            var.isVariable = true;
            var.variable = name;
            var.varType = VariableType::LOCAL;
            var.isConst = isLocalConst(scope, name);
            return var;
        }
    }
    return Value::createNull();
}
bool Parser::hasLocal(uint64_t scope, const std::string& name) const {
    auto it = localScopes.find(scope);
    if (it != localScopes.end()) {
        return it->second.find(name) != it->second.end();
    }
    return false;
}
bool Parser::isLocalConst(uint64_t scope, const std::string& name) const {
    auto it = localConstVars.find(scope);
    if (it != localConstVars.end()) {
        auto constIt = it->second.find(name);
        if (constIt != it->second.end()) {
            return constIt->second;
        }
    }
    return false;
}
Value Parser::resolveVariableValueWithScopes(const std::string& varName, const bool unknownIsString) {
    if (hasLocal(currentScope, varName)) {
        return getLocal(currentScope, varName);
    }
    
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        if (hasLocal(*it, varName)) {
            return getLocal(*it, varName);
        }
    }
    
    if (hasLocal(rootIndex, varName)) {
        return getLocal(rootIndex, varName);
    }
    
    if (hasGlobal(varName)) {
        return getGlobal(varName);
    }
    
    auto it = variables.find(varName);
    if (it != variables.end() && it->second.type != DataType::UNKNOWN) {
        Value var = it->second;
        var.isVariable = true;
        var.variable = varName;
        var.varType = VariableType::VARIABLE;

        auto constIt = constVars.find(varName);
        var.isConst = (constIt != constVars.end() && constIt->second);

        return var;
    }
    
    if (unknownIsString) {
        Value result;
        result.type = DataType::STRING;
        result.name = varName;
        result.string_value = varName;
        return result;
    }
    
    Value result;
    result.type = DataType::UNKNOWN;
    result.name = "unknown";
    return result;
}

Value Parser::parseJSXElement(const std::string& jsxStr) {
    Value element = isolated("return " + jsxStr + " .", doExecute, currentToken().start, nullptr, "JSX parse", false);
    element.type = DataType::JSX_ELEMENT;
    return element;
}

std::string Parser::renderJSX(const Value& jsxElement) {
    if (!Utility::checkElement(jsxElement)) {
        return jsxElement.toString();
    }
    
    std::string type = this->v(jsxElement.getProperty("type", Value::createString(""))).toString();
    Value props = this->v(jsxElement.getProperty("props", Value::createNull()));
    Value children = this->v(jsxElement.getProperty("children", Value::createNull()));
    
    if (type.empty()) return "";
    
    std::string result = "<" + type;
    
    if (props.type == DataType::JSON_OBJECT) {
        for (const auto& [key, value_] : props.properties) {
            const Value& value = v(value_);
            if (key == "className" || key == "class") {
                result += " class=\"" + value.toString() + "\"";
            } else if (key == "id") {
                result += " id=\"" + value.toString() + "\"";
            } else if (key == "style") {
                if (value.type == DataType::JSON_OBJECT) {
                    result += " style=\"";
                    bool first = true;
                    for (const auto& [prop, val] : value.properties) {
                        if (!first) result += ";";
                        first = false;
                        std::string cssProp = prop;
                        for (size_t i = 0; i < cssProp.length(); i++) {
                            if (isupper(cssProp[i])) {
                                cssProp.insert(i, "-");
                                cssProp[i+1] = tolower(cssProp[i+1]);
                                i++;
                            }
                        }
                        result += cssProp + ":" + v(val).toString();
                    }
                    result += "\"";
                } else {
                    result += " style=\"" + value.toString() + "\"";
                }
            } else if (key == "href") {
                result += " href=\"" + value.toString() + "\"";
            } else if (key == "src") {
                result += " src=\"" + value.toString() + "\"";
            } else if (key == "alt") {
                result += " alt=\"" + value.toString() + "\"";
            } else if (key == "onClick" || key == "onClick") {
                result += " onclick=\"" + value.toString() + "\"";
            } else if (key == "onChange") {
                result += " onchange=\"" + value.toString() + "\"";
            } else if (key == "value") {
                result += " value=\"" + value.toString() + "\"";
            } else if (key == "placeholder") {
                result += " placeholder=\"" + value.toString() + "\"";
            } else if (value.toBoolean() && (key == "disabled" || key == "checked" || key == "selected")) {
                result += " " + key;
            } else if (key != "children") {
                result += " " + key + "=\"" + value.toString() + "\"";
            }
        }
    }
    
    static const std::vector<std::string> selfClosing = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr"
    };
    
    if (std::find(selfClosing.begin(), selfClosing.end(), type) != selfClosing.end()) {
        result += " />";
        return result;
    }
    
    result += ">";
    
    if (children.type == DataType::JSON_ARRAY) {
        for (const auto& child_ : children.array_elements) {
            const Value& child = v(child_);
            switch (child.type) {
                case DataType::STRING:
                    result += child.string_value;
                    break;
                case DataType::JSON_OBJECT:
                case DataType::JUSTC_OBJECT:
                case DataType::JSX_ELEMENT:
                    result += renderJSX(child);
                    break;
                default:
                    result += child.toString();
                    break;
            }
        }
    }
    
    result += "</" + type + ">";
    return result;
}

Value Parser::p2v(const Value::Property& property, const Access& requestAccess) { // propertyToValue
    switch (requestAccess) {
        case Access::READ_WRITE: {
            switch (property.access) {
                case Access::READ_WRITE: {
                    if (property.hasGetter && property.getter) {
                        Value getter = *property.getter;
                        if (getter.type == DataType::FUNCTION && doExecute) return callFunction(getter, {}, position, doExecute);
                        else if (doExecute) throw std::runtime_error("Invalid getter type.");
                    }
                    return property.value;
                }
                default: throw std::runtime_error("Access denied.");
            }
        }
        case Access::WRITE_ONLY: {
            switch (property.access) {
                case Access::READ_WRITE:
                case Access::WRITE_ONLY: {
                    if (property.hasGetter && property.getter && property.access == Access::READ_WRITE) {
                        Value getter = *property.getter;
                        if (getter.type == DataType::FUNCTION && doExecute) return callFunction(getter, {}, position, doExecute);
                        else if (doExecute) throw std::runtime_error("Invalid getter type.");
                    }
                    return property.value;
                }
                default: throw std::runtime_error("Access denied.");
            }
        }
        case Access::READ_ONLY: 
        default: {
            switch (property.access) {
                case Access::READ_WRITE:
                case Access::READ_ONLY: {
                    if (property.hasGetter && property.getter) {
                        Value getter = *property.getter;
                        if (getter.type == DataType::FUNCTION && doExecute) return callFunction(getter, {}, position, doExecute);
                        else if (doExecute) throw std::runtime_error("Invalid getter type.");
                    }
                    return property.value;
                }
                default: throw std::runtime_error("Access denied.");
            }
        }
    }
}
Value::Property Parser::v2p(const Value& value, const Access& access, const Value& getter, const Value& setter) { // valueToProperty
    Value::Property prop(value, access);
    if (getter.type == DataType::FUNCTION) {
        prop.hasGetter = true;
        prop.getter = std::make_shared<Value>(getter);
    }
    if (setter.type == DataType::FUNCTION) {
        prop.hasSetter = true;
        prop.setter = std::make_shared<Value>(setter);
    }
    return prop;
}

Value Parser::v(const Value& value, const Access& requestAccess) { // value
    return value;
}
Value Parser::v(const Value::Property& value, const Access& requestAccess) { // value
    return p2v(value, requestAccess);
}

std::pair<Value, Value::Property> Parser::vp(const Value& value, const Access& requestAccess) { // value & property
    Value::Property empty(value, Access::READ_WRITE);
    return {value, empty};
}
std::pair<Value, Value::Property> Parser::vp(const Value::Property& value, const Access& requestAccess) { // value & property
    return {p2v(value, requestAccess), value};
}

Value::Property Parser::p(const Value& value) { // property
    return v2p(value);
}
Value::Property Parser::p(const Value::Property& value) { // property
    return value;
}

std::unordered_map<std::string, Value> Parser::vmap(const std::unordered_map<std::string, Value>& props) { // valueMap
    return props;
}
std::unordered_map<std::string, Value> Parser::vmap(const std::unordered_map<std::string, Value::Property>& props) { // valueMap
    std::unordered_map<std::string, Value> values;
    for (const auto& [key, value] : props) {
        values[key] = p2v(value);
    }
    return values;
}

std::unordered_map<std::string, Value::Property> Parser::pmap(const std::unordered_map<std::string, Value>& values) { // propertyMap
    std::unordered_map<std::string, Value::Property> props;
    for (const auto& [key, value] : values) {
        props[key] = v2p(value);
    }
    return props;
}
std::unordered_map<std::string, Value::Property> Parser::pmap(const std::unordered_map<std::string, Value::Property>& values) { // propertyMap
    return values;
}

Value Parser::parseStructDeclaration(bool doExecute, std::string structName, bool requireName) {
    if (!match("keyword", "struct") && !match("keyword", "class") && !match("keyword", "enum")) {
        throw std::runtime_error("Expected \"struct\"/\"class\"/\"enum\" keyword at " + Utility::position(currentToken().start, input));
    }
    bool isStruct_ = match("keyword", "struct");
    bool isEnum_ = match("keyword", "enum");
    std::string typeStr = isStruct_ ? "Struct" : isEnum_ ? "Enum" : "Class";
    std::string typeStrL= isStruct_ ? "struct" : isEnum_ ? "enum" : "class";
    advance();

    if (requireName) {
        if (!match("identifier")) {
            throw std::runtime_error("Expected " + typeStrL + " name at " + Utility::position(currentToken().start, input));
        }
        structName = currentToken().value;
        advance();
    } else {
        if (match("identifier")) {
            structName = currentToken().value;
            advance();
        }
    }

    Value result;
    result.type = isStruct_ ? DataType::STRUCT : isEnum_ ? DataType::ENUM : DataType::CLASS;
    result.name = structName;

    if (match("keyword", "extends") && !isEnum_) {
        advance();
        Value extends = parseExpression(doExecute, true);
        switch (extends.type) {
            case DataType::STRUCT:
                result.array_elements.push_back(
                    extends.isVariable && isStruct(extends.variable).first ? isStruct(extends.variable).second : extends
                );
                break;
            case DataType::JSON_OBJECT:
            case DataType::JUSTC_OBJECT:
            case DataType::FUNCTION:
                result.array_elements.push_back(extends);
                break;
            case DataType::JSON_ARRAY: {
                for (const Value item : extends.array_elements) {
                    switch (item.type) {
                        case DataType::STRUCT:
                            result.array_elements.push_back(
                                item.isVariable && isStruct(item.variable).first ? isStruct(item.variable).second : item
                            );
                            break;
                        case DataType::JSON_OBJECT:
                        case DataType::JUSTC_OBJECT:
                        case DataType::FUNCTION:
                            result.array_elements.push_back(item);
                            break;
                        case DataType::NULL_TYPE: break;
                        case DataType::CLASS: {
                            if (!isStruct_) {
                                result.array_elements.push_back(item);
                                break;
                            } else throw std::runtime_error("Struct cannot extend class.");
                        }
                        default:
                            throw std::runtime_error(typeStr + " \"" + structName + "\" cannot extend <" + dataTypeToString(item.type) + "> - it is not a constructor, object, or null.");
                    }
                }
                break;
            }
            case DataType::NULL_TYPE: break;
            case DataType::CLASS: {
                if (!isStruct_) {
                    result.array_elements.push_back(extends);
                    break;
                } else throw std::runtime_error("Struct cannot extend class.");
            }
            default:
                throw std::runtime_error(typeStr + " \"" + structName + "\" cannot extend <" + dataTypeToString(extends.type) + "> - it is not a constructor, object, or null.");
        }
    }

    if (!match("{")) {
        throw std::runtime_error("Expected \"{\" for " + typeStrL + " body at " + Utility::position(currentToken().start, input));
    }
    advance();

    std::stringstream body;
    int braceCount = 1;

    while (!isEnd() && braceCount > 0) {
        if (match("{")) braceCount++;
        else if (match("}")) braceCount--;

        if (braceCount > 0) {
            result.function_info.tokens.push_back(currentToken());
            body << t2i(currentToken());
        }
        advance();
    }

    if (braceCount != 0) {
        throw std::runtime_error("Unclosed " + typeStrL + " body at " + Utility::position(currentToken().start, input));
    }

    result.string_value = body.str();

    auto closureContext = std::make_shared<ObjectContext>();
    for (const auto& [key, value] : this->variables) {
        closureContext->variables[key] = value;
    }
    closureContext->allowJavaScript = this->allowJavaScript;
    closureContext->allowLuau = this->allowLuau;
    result.closure_context = closureContext;
    
    if (isEnum_) return parseEnumDeclaration(result, doExecute, structName);
    if (!isStruct_) return parseClassDeclaration(result, doExecute, structName);

    structures[structName] = result;
    return result;
}
Value Parser::parseClassDeclaration(const Value& value, bool doExecute, std::string className) {
    Class cls;
    Value body = callFunction(value, {}, currentToken().start, doExecute);
    cls.constructor = accessProperty(body, "constructor").first;
    cls.destructor = accessProperty(body, "destructor").first;
    cls.instanceObject = accessProperty(body, "instance").first.properties;
    cls.staticObject = accessProperty(body, "static").first.properties;
    return createClass(cls, true, className);
}
Value Parser::parseEnumDeclaration(const Value& value, bool doExecute, std::string enumName) {
    Value result = callFunction(value, {}, currentToken().start, doExecute);
    result.type = DataType::ENUM;
    result.name = enumName;
    return result;
}

std::pair<bool, Value> Parser::isStruct(const std::string& name) {
    auto it = structures.find(name);
    if (it != structures.end()) {
        return {true, it->second};
    }
    return {false, Value::createNull()};
}
std::pair<bool, Value> Parser::isClass(const std::string& name) {
    auto it = classes.find(name);
    if (it != classes.end()) {
        return {true, it->second};
    }
    return {false, Value::createNull()};
}

Value Parser::updateObjectProperty(const std::vector<std::variant<std::string, size_t>>& accessChain, std::string accessChainStr) {
    if (accessChain.empty()) {
        throw std::runtime_error("Empty property path");
    }
    
    std::string rootName = std::get<std::string>(accessChain[0]);
    Value root = resolveVariableValue(rootName, false);
    Value current = root;
    Value setter = Value::createNull();

    Access neededAccess = Access::WRITE_ONLY;
    if (match("--") || match("++") || match("#") || match("!") || match("~") || match("$")) neededAccess = Access::READ_WRITE;

    std::vector<PropertyPathNode> pathNodes;
    
    for (size_t i = 1; i < accessChain.size(); i++) {
        PropertyPathNode node;
        if (std::holds_alternative<std::string>(accessChain[i])) {
            node.isProperty = true;
            node.name = std::get<std::string>(accessChain[i]);
            
            if (i < accessChain.size() - 1) {
                auto result = accessProperty(current, node.name, neededAccess);
                current = result.first;
                if (result.second.hasSetter) {
                    setter = *result.second.setter;
                }
            }
            pathNodes.push_back(node);
        } else if (std::holds_alternative<size_t>(accessChain[i])) {
            node.isIndex = true;
            node.index = std::get<size_t>(accessChain[i]);
            
            if (i < accessChain.size() - 1) {
                current = accessIndex(current, node.index);
            }
            pathNodes.push_back(node);
        }
    }

    if (pathNodes.empty()) {
        throw std::runtime_error("No property/index to update");
    }

    Value newValue;
    std::string assignOp;
    
    if (match("keyword", "to") || match("=")) {
        assignOp = currentToken().value;
        advance();
        
        newValue = parseExpression(doExecute);
        if (pathNodes.back().typeNode.has_value()) {
            newValue = applyTypeDeclaration(newValue, pathNodes.back().typeNode.value());
        }
    }
    else if (match("keyword", "isn't") || match("!=")) {
        assignOp = currentToken().value;
        advance();
        
        newValue = parseExpression(doExecute);
        newValue = handleInequality(newValue);
    }
    else if (match("keyword", "isif") || match("?")) {
        assignOp = currentToken().value;
        advance();
        
        newValue = parseConditional(doExecute);
    }
    else if (match("--") || match("++") || match("#") || match("!") || match("~") || match("$")) {
        std::string unaryOp = currentToken().value;
        advance();
        
        Value currentVal;
        if (pathNodes.back().isProperty) {
            currentVal = accessProperty(current, pathNodes.back().name, Access::READ_ONLY).first;
        } else {
            currentVal = accessIndex(current, pathNodes.back().index);
        }
        
        if (unaryOp == "--" || unaryOp == "++") {
            newValue = Value::createNumber(currentVal.toNumber() + (unaryOp == "++" ? 1 : -1));
        } else if (unaryOp == "#") {
            newValue = evaluateLengthOperator(currentVal);
        } else if (unaryOp == "!") {
            newValue = booleanToValue(!currentVal.toBoolean());
        } else if (unaryOp == "~") {
            if (Utility::checkNumber(currentVal)) {
                int num = static_cast<int>(currentVal.toNumber());
                newValue = numberToValue(~num);
            } else if (currentVal.type == DataType::STRING) {
                newValue = stringToValue(Utility::stringNot(currentVal.toString()));
            } else {
                throw std::runtime_error("Expected number or string for bitwise NOT operation");
            }
        } else if (unaryOp == "$") {
            newValue = evaluateNameOperator(currentVal);
        }
    }
    else {
        if (isEnd()) {
            throw std::runtime_error("Expected assignment operator at " + Utility::position(currentToken().start, input) + ", got EOF.");
        } else if (CanIgnoreNoAssignmentOperator()) {
            newValue = parseExpression(doExecute);
        } else {
            throw std::runtime_error("Expected assignment operator at " + Utility::position(currentToken().start, input) + ", got \"" + currentToken().value +"\".");
        }
    }

    if (setter.type == DataType::FUNCTION && doExecute) {
        newValue = callFunction(setter, {newValue}, currentToken().start, doExecute);
    }

    Value updatedObject = updateObjectPropertyRecursive(root, pathNodes, 0, newValue);
    
    assign(root, updatedObject, " at " + Utility::position(currentToken().start, input) + ".");
    
    newValue.name = accessChainStr;
    return newValue;
}

Value Parser::updateObjectPropertyRecursive(const Value& obj, const std::vector<PropertyPathNode>& pathNodes, size_t depth, const Value& newValue) {
    if (depth >= pathNodes.size()) {
        return newValue;
    }
    
    const PropertyPathNode& node = pathNodes[depth];
    Value result = obj;
    
    if (node.isProperty) {
        if (obj.type == DataType::JSON_OBJECT || obj.type == DataType::JUSTC_OBJECT) {
            std::unordered_map<std::string, Value::Property> newProperties = obj.properties;
            
            if (depth == pathNodes.size() - 1) {
                newProperties[node.name] = p(newValue);
            } else {
                if (newProperties.find(node.name) != newProperties.end()) {
                    Value child = v(newProperties[node.name]);
                    Value updatedChild = updateObjectPropertyRecursive(
                        child, 
                        pathNodes, 
                        depth + 1, 
                        newValue
                    );
                    newProperties[node.name] = p(updatedChild);
                } else {
                    Value newChild;
                    if (depth + 1 < pathNodes.size() && pathNodes[depth + 1].isProperty) {
                        newChild = Value::createJsonObject({});
                    } else {
                        newChild = Value::createJsonArray({});
                    }
                    newProperties[node.name] = p(updateObjectPropertyRecursive(
                        newChild, 
                        pathNodes, 
                        depth + 1, 
                        newValue
                    ));
                }
            }
            
            if (obj.type == DataType::JUSTC_OBJECT) {
                result = Value::createJustcObject(obj.object_context);
            } else {
                result = Value::createJsonObject({});
            }
            result.properties = newProperties;
            result.name = obj.name;
        } else {
            throw std::runtime_error("Cannot set property \"" + node.name + "\" on non-object");
        }
    } else if (node.isIndex) {
        if (obj.type == DataType::JSON_ARRAY) {
            std::vector<Value> newArray = obj.array_elements;
            
            if (node.index >= newArray.size()) {
                newArray.resize(node.index + 1, Value::createNull());
            }
            
            if (depth == pathNodes.size() - 1) {
                newArray[node.index] = newValue;
            } else {
                Value updatedChild = updateObjectPropertyRecursive(
                    newArray[node.index], 
                    pathNodes, 
                    depth + 1, 
                    newValue
                );
                newArray[node.index] = updatedChild;
            }
            
            result = Value::createJsonArray(newArray);
        } else if (obj.type == DataType::JSON_OBJECT || obj.type == DataType::JUSTC_OBJECT) {
            std::string indexStr = std::to_string(node.index);
            std::unordered_map<std::string, Value::Property> newProperties = obj.properties;
            
            if (depth == pathNodes.size() - 1) {
                newProperties[indexStr] = p(newValue);
            } else {
                if (newProperties.find(indexStr) != newProperties.end()) {
                    Value child = v(newProperties[indexStr]);
                    Value updatedChild = updateObjectPropertyRecursive(
                        child, 
                        pathNodes, 
                        depth + 1, 
                        newValue
                    );
                    newProperties[indexStr] = p(updatedChild);
                } else {
                    Value newChild;
                    if (depth + 1 < pathNodes.size() && pathNodes[depth + 1].isProperty) {
                        newChild = Value::createJsonObject({});
                    } else {
                        newChild = Value::createJsonArray({});
                    }
                    newProperties[indexStr] = p(updateObjectPropertyRecursive(
                        newChild, 
                        pathNodes, 
                        depth + 1, 
                        newValue
                    ));
                }
            }
            
            if (obj.type == DataType::JUSTC_OBJECT) {
                result = Value::createJustcObject(obj.object_context);
            } else {
                result = Value::createJsonObject({});
            }
            result.properties = newProperties;
            result.name = obj.name;
        } else {
            throw std::runtime_error("Cannot access index on non-array/non-object");
        }
    }
    
    return result;
}

Value Parser::getStructConstructor(const std::string& structID) {
    auto it = structConstructors.find(structID);
    if (it == structures.end()) throw std::runtime_error("Struct registry has been corrupted. Failed to access struct " + structID + ".");
    return it->second;
}

uint64_t Parser::registerClass(const Class& value) {
    return setClass(value);
}
Class Parser::getClass(const uint64_t& classID) {
    return getClass_(classID, Utility::uint64ToHexString(classID));
}
void Parser::unregisterClass(const uint64_t& classID) {
    removeClass(classID);
}
void Parser::clearClasses() {
    clearClasses_();
    builtinClasses();
}

uint64_t Parser::builtinClass(const std::string& name) {
    Class cls(builtinObjectFunction(name));
    Value val = createClass(cls, true, name);
    variables[name] = val;
    constVars[name] = true;
    return val.getNumericValue<uint64_t>();
}
void Parser::builtinClasses() {
    std::unordered_map<std::string, uint64_t> BIM;
    if (!getBIC()) {
        #ifndef __EMSCRIPTEN__
            BIM["Window"] = builtinClass("Window");
        #endif
        BIM["Promise"] = builtinClass("Promise");
        BIM["Error"] = builtinClass("Error");
        setBIM(BIM);
        setBIC(true);
    } else {
        BIM = getBIM();
        #ifndef __EMSCRIPTEN__
            fromBIM(BIM, "Window");
        #endif
        fromBIM(BIM, "Promise");
        fromBIM(BIM, "Error");
    }
}
void Parser::fromBIM(const std::unordered_map<std::string, uint64_t>& BIM, const std::string& name) {
    auto it = BIM.find(name);
    if (it == BIM.end()) throw std::runtime_error("Class registry has been corrupted. Failed to register built-in class \"" + name + "\".");
    uint64_t ClassID = it->second;

    Value val = addClass(ClassID, true, name);
    variables[name] = val;
    constVars[name] = true;
}

uint64_t Parser::registerUserFunction(std::function<Value(const std::vector<Value>&)> func) {
    return ::registerFunction(func);
}

std::function<Value(const std::vector<Value>&)> Parser::getUserFunction(uint64_t id) {
    return ::getFunction(id, Utility::uint64ToHexString(id));
}

void Parser::unregisterUserFunction(uint64_t id) {
    ::removeFunction(id);
}

void Parser::clearUserFunctions_() {
    ::clearFunctions_();
}

Value Parser::createFunction(std::function<Value(const std::vector<Value>&)> func, const std::string& name) {
    uint64_t id = registerUserFunction(func);
    
    Value result;
    result.type = DataType::FUNCTION;
    result.name = name.empty() ? FUNCTION_PREFIX + Utility::uint64ToHexString(id) : name;
    result.string_value = "[native code]";
    result.object_type = DataType::FUNCTION;
    result.native = true;
    result.function_info.paramNames.push_back(FUNCTION_ID_NAME);
    result.function_info.paramTypes.push_back(DataType::NUMBER);
    result.function_info.defaultValues.push_back(Value::createNumberWithType(id, NumericType::UINT64));
    
    return result;
}

bool Parser::isFunctionValue(const Value& value) const {
    return value.type == DataType::FUNCTION && 
           value.string_value == "[native code]" && 
           value.native;
}

bool Parser::isFunctionId(const std::string& name) const {
    return name.find(FUNCTION_PREFIX) == 0;
}

uint64_t Parser::extractFunctionId(const std::string& name) const {
    if (!isFunctionId(name)) {
        throw std::runtime_error("Not a function ID: " + name);
    }
    std::string hexStr = name.substr(std::string(FUNCTION_PREFIX).length());
    return std::stoull(hexStr);
}

uint64_t Parser::setPointer(const Value& value) {
    return setPointer_(value);
}
Value Parser::getPointer(const uint64_t& pointer) {
    return getPointer_(pointer, Utility::uint64ToHexString(pointer));
}
void Parser::freePointer(const uint64_t& pointer) {
    freePointer_(pointer);
}
void Parser::clearPointers() {
    clearPointers_();
}
bool Parser::hasPointer(const uint64_t& pointer) {
    return hasPointer_(pointer);
}
uint64_t Parser::setPointer(const uint64_t& pointer, const Value& value) {
    return setPointer_(pointer, Utility::uint64ToHexString(pointer), value);
}
Value Parser::makePointer(const Value& value) {
    uint64_t id = setPointer(value);
    Value result = Value::createNumberWithType(id, NumericType::UINT64);
    result.type = DataType::POINTER;
    result.name = value.name;
    return result;
}
void Parser::updatePointer(const Value& pointer, const Value& value) {
    uint64_t id = Utility::checkPointer(pointer);
    setPointer(id, value);
}
Value Parser::getPointer(const Value& pointer) {
    uint64_t id = Utility::checkPointer(pointer);
    return getPointer(id);
}
void Parser::freePointer(const Value& pointer) {
    uint64_t id = Utility::checkPointer(pointer);
    freePointer(id);
}

void Parser::cleanup() {
    cleanupGlobal();
}

std::string Parser::parseStringInterpolation(bool doExecute) {
    if (!match("string_i") && !match("string_ri") && !match("Luau_i") && !match("JavaScript_i") && !match("JUSTC_i") && !match("JUSTO_i")) {
        throw std::runtime_error("Expected string interpolation at " + Utility::position(currentToken().start, input) + ".");
    }
    
    std::string str = currentToken().value;
    bool isRaw = match("string_ri");
    advance();
    
    std::vector<StringPart> parts;
    size_t pos = 0;
    std::string currentLiteral = "";
    int braceDepth = 0;
    std::string exprBuffer = "";
    bool inExpression = false;
    
    while (pos < str.length()) {
        char ch = str[pos];
        
        if (ch == '\\' && pos + 1 < str.length()) {
            if (str[pos + 1] == '{') {
                if (inExpression) {
                    exprBuffer += "\\{";
                } else {
                    currentLiteral += "{";
                }
                pos += 2;
                continue;
            } else {
                if (inExpression) {
                    exprBuffer += ch;
                    exprBuffer += str[pos + 1];
                } else {
                    currentLiteral += ch;
                    currentLiteral += str[pos + 1];
                }
                pos += 2;
                continue;
            }
        }
        
        if (ch == '{' && !inExpression) {
            inExpression = true;
            braceDepth = 1;
            exprBuffer = "";
            pos++;
            continue;
        }
        
        if (inExpression) {
            if (ch == '{') {
                braceDepth++;
                exprBuffer += ch;
            } else if (ch == '}') {
                braceDepth--;
                if (braceDepth == 0) {
                    inExpression = false;
                    
                    if (!currentLiteral.empty()) {
                        parts.emplace_back(true, currentLiteral);
                        currentLiteral = "";
                    }
                    
                    if (!exprBuffer.empty()) {
                        std::string exprResult = evaluateInterpolationExpression(exprBuffer, doExecute);
                        parts.emplace_back(false, exprResult);
                    }
                    
                    exprBuffer = "";
                    pos++;
                    continue;
                } else {
                    exprBuffer += ch;
                }
            } else {
                exprBuffer += ch;
            }
            pos++;
            continue;
        }
        
        currentLiteral += ch;
        pos++;
    }
    
    if (!currentLiteral.empty()) {
        parts.emplace_back(true, currentLiteral);
    }
    
    std::string result = "";
    for (const auto& part : parts) {
        if (part.isLiteral && !isRaw) {
            result += StringEscape::unescape(part.content);
        } else {
            result += part.content;
        }
    }
    
    return result;
}

std::string Parser::evaluateInterpolationExpression(const std::string& expr, bool doExecute) {
    auto lexerResult = Lexer::parse(expr, false);

    size_t savedPos = position;
    
    Parser tempParser(
        lexerResult.second,
        doExecute,
        this->runAsync,
        expr,
        this->allowJavaScript,
        this->canAllowJS,
        this->scriptName + "::interpolation",
        "interpolation",
        this->allowLuau,
        this->canAllowLuau,
        false,
        nullptr,
        this->chartype,
        this->parsertype
    );
    
    tempParser.variables = this->variables;
    tempParser.constVars = this->constVars;
    tempParser.mutated = this->mutated;
    tempParser.throwError = true;

    Value result = tempParser.parseExpression(doExecute);
    
    this->position = savedPos;
    
    return result.toString();
}

Value Parser::This() {
    std::unordered_map<std::string, Value> obj;
    for (const auto& [key, value] : this->variables) {
        try {
            Value val = resolveVariableValue(key, false);
            if (val.type == DataType::UNKNOWN) throw std::runtime_error("");
            obj[key] = val;
        } catch (...) {
            obj[key] = value;
        }
    }
    Value result = Value::createJsonObject(obj);
    result.name = "this";
    return result;
}

ParseResult Parser::parseTokens(const std::vector<ParserToken>& tokens, bool doExecute, bool runAsync, const std::string& input, const bool allowJavaScript, const bool canAllowJS, const std::string scriptName, const std::string scriptType, const bool allowLuau, const bool canAllowLuau) {
    #ifndef __EMSCRIPTEN__
    try {
    #endif

        Parser parser(tokens, doExecute, runAsync, input, allowJavaScript, canAllowJS, scriptName, scriptType, allowLuau, canAllowLuau, false, nullptr, CharType::GRAPHEME);
        return parser.parse(doExecute);

    #ifndef __EMSCRIPTEN__
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string(e.what()) + "\n\nJUSTC v" + JUSTC_VERSION);
    }
    #endif
}
