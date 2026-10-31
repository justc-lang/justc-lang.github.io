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

#include "utility.h"
#include "parser.h"
#include <cmath>
#include <string>
#include <iomanip>
#include <bitset>
#include <cstring>
#include <sstream>
#include <cstddef>
#include <unordered_map>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <type_traits>
#include <limits>
#ifdef __EMSCRIPTEN__
#include "utility.emscripten.h"
#endif

std::string Utility::numberValue2string(const Value& value) {
    if (static_cast<bool>(value.numeric_data)) {
        return value.toNumericString();
    } else {
        return doubleToString(value.number_value);
    }
}

std::string Utility::value2string(const Value& value) {
    switch (value.type) {
        case DataType::NUMBER:
        case DataType::HEXADECIMAL:
        case DataType::BINARY:
        case DataType::OCTAL:
            return numberValue2string(value);
        case DataType::JUSTC_OBJECT:
            if (value.name == "HTTP.Responce") {
                auto text = value.object_value.find("text");
                if (text != value.object_value.end()) return value2string(text->second);
                else return value.toString();
            } else return value.toString();
        default:
            return value.toString();
    }
}

std::string Utility::double2hexString(const double d) {
    uint64_t bits;
    std::memcpy(&bits, &d, sizeof(double));
    std::stringstream ss;
    ss << std::hex << bits;
    return ss.str();
}

std::string Utility::double2octString(const double d) {
    uint64_t bits;
    std::memcpy(&bits, &d, sizeof(double));
    std::stringstream ss;
    ss << std::oct << bits;
    return ss.str();
}

std::string Utility::double2binString(const double d) {
    uint64_t bits;
    std::memcpy(&bits, &d, sizeof(double));
    return std::bitset<64>(bits).to_string();
}

bool Utility::checkNumber(const Value& val) {
    switch (val.type) {
        case DataType::NUMBER:
        case DataType::HEXADECIMAL:
        case DataType::BINARY:
        case DataType::OCTAL:
        case DataType::BIGNUM:
        case DataType::BASE64:
            return true;
        default:
            return false;
    }
}
bool Utility::checkNumbers(const Value& left, const Value& right) {
    return (checkNumber(left) && checkNumber(right));
}
bool Utility::checkObject(const Value& val) {
    switch (val.type) {
        case DataType::JSON_OBJECT:
        case DataType::JUSTC_OBJECT:
        case DataType::ENUM:
            return true;
        default:
            return false;
    }
}
bool Utility::checkObjects(const Value& left, const Value& right) {
    return (checkObject(left) && checkObject(right));
}
bool Utility::checkString(const Value& val) {
    switch (val.type) {
        case DataType::STRING:
        case DataType::UNKNOWN:
            return true;
        default:
            return false;
    }
}
bool Utility::checkStrings(const Value& left, const Value& right) {
    return (checkString(left) && checkString(right));
}

std::pair<size_t, size_t> Utility::pos(const size_t& pos, const std::string& script) {
    if (script.empty() || pos >= script.length()) {
        return {1, 1};
    }

    size_t line = 1;
    size_t column = 1;
    size_t current_pos = 0;

    while (current_pos < pos && current_pos < script.length()) {
        char current_char = script[current_pos];

        if (current_char == '\n') {                                                     //      \n
            line++;
            column = 1;
            current_pos++;
        } else if (current_char == '\r') {
            if (current_pos + 1 < script.length() && script[current_pos + 1] == '\n') { //      \r\n
                line++;
                column = 1;
                current_pos += 2;
            } else {                                                                    //      \r
                line++;
                column = 1;
                current_pos++;
            }
        } else {
            column++;
            current_pos++;
        }
    }

    return {line, column};
}

std::string Utility::position(const size_t& pos_, const std::string& script) {
    std::pair<size_t, size_t> position = pos(pos_, script);
    size_t line = position.first;
    size_t column = position.second;
    return "line " + std::to_string(line) + ", column " + std::to_string(column);
}

DataType Utility::typeDeclaration2dataType(const std::string& typeDeclaration, const std::string& position) {
    static const std::unordered_map<std::string, DataType> typeMap = {
        { "number",      DataType::NUMBER       },     { "num",  DataType::NUMBER       },
        { "string",      DataType::STRING       },     { "str",  DataType::STRING       },
        { "boolean",     DataType::BOOLEAN      },     { "bool", DataType::BOOLEAN      },
        { "null",        DataType::NULL_TYPE    },     { "nil",  DataType::NULL_TYPE    },
        { "link",        DataType::LINK         },
        { "path",        DataType::PATH         },
        { "binary",      DataType::BINARY       },     { "bin",  DataType::BINARY       },
        { "octal",       DataType::OCTAL        },     { "oct",  DataType::OCTAL        },
        { "hexadecimal", DataType::HEXADECIMAL  },     { "hex",  DataType::HEXADECIMAL  },
        { "object",      DataType::JUSTC_OBJECT },     { "obj",  DataType::JUSTC_OBJECT },
        { "json",        DataType::JSON_OBJECT  },
        { "array",       DataType::JSON_ARRAY   },
        { "nan",         DataType::NOT_A_NUMBER },
        { "infinity",    DataType::INFINITE     },     { "inf",  DataType::INFINITE     },
        { "data",        DataType::BINARY_DATA  },
        { "element",     DataType::JSX_ELEMENT  },
        { "map",         DataType::MAP          },
        { "set",         DataType::SET          },
        { "auto",        DataType::UNKNOWN      },
    };

    auto it = typeMap.find(typeDeclaration);
    if (it != typeMap.end()) {
        return it->second;
    }

    throw std::runtime_error("Invalid type declaration \"" + typeDeclaration + "\" at " + position + ".");
}

Value Utility::convert(const Value value, const DataType type) {
    Value result = value;
    result.type = type;
    switch (type) {
        case DataType::NUMBER:
            break;
        case DataType::BINARY:
            result.name = double2binString(value.number_value);
            break;
        case DataType::HEXADECIMAL:
            result.name = double2hexString(value.number_value);
            break;
        case DataType::OCTAL:
            result.name = double2octString(value.number_value);
            break;
        default: // warning: 15 enumeration values not handled in switch: 'UNKNOWN', 'JUSTC_OBJECT', 'STRING'... [-Wswitch]
            throw std::runtime_error("JUSTC/core/utility.cpp error: Incorrect usage.");
    }
    return result;
}

Value Utility::ParseResult2Value(const ParseResult parseresult) {
    Value result;
    result.type = DataType::JUSTC_OBJECT;
    result.object_value = parseresult.returnValues;
    result.name = "(Object)";
    return result;
}

std::pair<bool, std::string> Utility::env(const std::string& name) {
    const char* env_p = std::getenv(name.c_str());
    return {(env_p ? true : false), (env_p ? std::string(env_p) : "")};
}

bool Utility::isGitHubActions() {
    const std::pair<bool, std::string> githubActions = env("GITHUB_ACTIONS");
    return (githubActions.first && githubActions.second == "true");
}

std::unordered_map<std::string, std::string> Utility::ParseHeaders(const std::string& headers) {
    std::unordered_map<std::string, std::string> output;
    std::istringstream lines_stream(headers);
    std::string line;
    while (std::getline(lines_stream, line, '\n')) {
        std::istringstream pair_stream(line);
        std::string key;
        std::string value;
        if (std::getline(pair_stream, key, ':')) {
            if (std::getline(pair_stream, value)) {
                output[key] = value;
            }
        }
    }
    return output;
}

std::string Utility::defaultHTTPAccept = "text/*, application/x-justc, application/json, application/lua, application/hocon, application/xml, application/yaml, */*";

void Utility::Warn(const std::string& warning) {
    #ifdef __EMSCRIPTEN__
    console_warn(Parser::getCurrentTimestamp().c_str(), warning.c_str());
    #else
    if (isGitHubActions()) {
        std::cout << "::warning::" + warning << std::endl;
    } else {
        std::cout << "JUSTC: Warning: " + warning << std::endl;
    }
    #endif
}

std::string Utility::escapeJUSTCString(const std::string& str) {
    return str; // TODO: 1. escape sequences in JUSTC; 2. this function
}

std::string Utility::_stringifyValue(const Value& value, int indentLevel) {
    std::string indent(indentLevel * 2, ' ');
    std::string nextIndent((indentLevel + 1) * 2, ' ');

    switch (value.type) {
        case DataType::NUMBER:
            return numberValue2string(value);

        case DataType::HEXADECIMAL:
            return "0x" + double2hexString(value.number_value);

        case DataType::BINARY:
            return "0b" + double2binString(value.number_value);

        case DataType::OCTAL:
            return "0o" + double2octString(value.number_value);

        case DataType::STRING:
            return "\"" + StringEscape::escape(value.string_value) + "\"";

        case DataType::LINK:
            return "<" + escapeJUSTCString(value.string_value) + ">";

        case DataType::PATH:
            return value.string_value;

        case DataType::BOOLEAN:
            return value.boolean_value ? "y" : "n";

        case DataType::NULL_TYPE:
            return "";

        case DataType::NOT_A_NUMBER:
            return "NaN";

        case DataType::INFINITE:
            return "Infinity";

        case DataType::JUSTC_OBJECT:
        case DataType::JSON_OBJECT:
        case DataType::ENUM: {
            std::string result = "{" + nextIndent;
            bool first = true;

            const auto& props = value.properties;
            for (const auto& [key, val] : props) {
                if (!first) result += "," + nextIndent;
                first = false;
                result += "\"" + escapeJUSTCString(key) + "\":" + _stringifyValue(val, indentLevel + 1);
            }

            if (value.object_context && !value.object_context->variables.empty()) {
                for (const auto& [key, val] : value.object_context->variables) {
                    if (props.find(key) != props.end()) continue;

                    if (!first) result += "," + nextIndent;
                    first = false;
                    result += "\"" + escapeJUSTCString(key) + "\":" + _stringifyValue(val, indentLevel + 1);
                }
            }

            result += indent + "}";
            return result;
        }

        case DataType::JSON_ARRAY: {
            std::string result = "[" + nextIndent;
            for (size_t i = 0; i < value.array_elements.size(); i++) {
                if (i > 0) result += "," + nextIndent;
                result += _stringifyValue(value.array_elements[i], indentLevel + 1);
            }
            result += indent + "]";
            return result;
        }

        case DataType::VARIABLE:
            return value.string_value;

        case DataType::FUNCTION: {
            if (value.native) {
                return value.name;
            }

            std::string result;
            if (value.function_info.isIsolated) {
                result = "isolated ";
            }
            result += "function " + value.name + "(";
            for (size_t i = 0; i < value.function_info.paramNames.size(); i++) {
                if (i > 0) result += ", ";
                result += value.function_info.paramNames[i];
            }
            result += "){" + nextIndent;
            result += value.string_value;
            result += indent + "}";
            return result;
        }

        case DataType::BINARY_DATA: {
            std::string binData;
            for (size_t i = 0; i < value.binary_data.size(); i++) {
                binData += std::to_string(static_cast<int>(value.binary_data[i]));
            }
            return "Binary::FromText(\"" + escapeJUSTCString(binData) + "\")";
        }

        default:
            return "nil";
    }
}
std::string Utility::_stringifyValue(const Value::Property& value, int indentLevel) {
    return _stringifyValue(value.value, indentLevel);
}

std::string Utility::stringifyValue(const Value& value) {
    return "return " + _stringifyValue(value, 0) + " .";
}

char StringEscape::hexToChar(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

std::string StringEscape::codepointToUTF8(uint32_t cp) {
    std::string result;
    if (cp < 0x80) {
        result += static_cast<char>(cp);
    } else if (cp < 0x800) {
        result += static_cast<char>(0xC0 | (cp >> 6));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        result += static_cast<char>(0xE0 | (cp >> 12));
        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        result += static_cast<char>(0xF0 | (cp >> 18));
        result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return result;
}

std::string StringEscape::unescape(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); i++) {
        char c = str[i];
        
        if (c == '\\' && i + 1 < str.length()) {
            char next = str[i + 1];
            i++;
            
            switch (next) {
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case 'v':  result += '\v'; break;
                case 'f':  result += '\f'; break;
                case 'b':  result += '\b'; break;
                case 'a':  result += '\a'; break;
                case '\\': result += '\\'; break;
                case '"':  result += '"';  break;
                case '\'': result += '\''; break;
                case '?':  result += '?';  break;
                
                case '0': case '1': case '2': case '3':
                case '4': case '5': case '6': case '7': {
                    std::string octal;
                    octal += next;
                    
                    for (int j = 0; j < 2 && i + 1 < str.length(); j++) {
                        char nextChar = str[i + 1];
                        if (nextChar >= '0' && nextChar <= '7') {
                            octal += nextChar;
                            i++;
                        } else {
                            break;
                        }
                    }
                    
                    int value = 0;
                    for (char oct : octal) {
                        value = (value << 3) + (oct - '0');
                    }
                    
                    if (value <= 0xFF) {
                        result += static_cast<char>(value);
                    } else {
                        result += codepointToUTF8(value);
                    }
                    break;
                }
                
                case 'u': {
                    if (i + 4 < str.length()) {
                        std::string hex = str.substr(i + 1, 4);
                        bool valid = true;
                        for (char h : hex) {
                            if (!((h >= '0' && h <= '9') || 
                                  (h >= 'a' && h <= 'f') || 
                                  (h >= 'A' && h <= 'F'))) {
                                valid = false;
                                break;
                            }
                        }
                        
                        if (valid) {
                            uint32_t cp = 0;
                            for (char h : hex) {
                                cp = (cp << 4) + hexToChar(h);
                            }
                            result += codepointToUTF8(cp);
                            i += 4;
                        } else {
                            result += next;
                        }
                    } else {
                        result += next;
                    }
                    break;
                }
                
                case 'U': {
                    if (i + 8 < str.length()) {
                        std::string hex = str.substr(i + 1, 8);
                        bool valid = true;
                        for (char h : hex) {
                            if (!((h >= '0' && h <= '9') || 
                                  (h >= 'a' && h <= 'f') || 
                                  (h >= 'A' && h <= 'F'))) {
                                valid = false;
                                break;
                            }
                        }
                        
                        if (valid) {
                            uint32_t cp = 0;
                            for (char h : hex) {
                                cp = (cp << 4) + hexToChar(h);
                            }
                            result += codepointToUTF8(cp);
                            i += 8;
                        } else {
                            result += next;
                        }
                    } else {
                        result += next;
                    }
                    break;
                }
                
                case 'x': {
                    if (i + 2 < str.length()) {
                        std::string hex = str.substr(i + 1, 2);
                        bool valid = true;
                        for (char h : hex) {
                            if (!((h >= '0' && h <= '9') || 
                                  (h >= 'a' && h <= 'f') || 
                                  (h >= 'A' && h <= 'F'))) {
                                valid = false;
                                break;
                            }
                        }
                        
                        if (valid) {
                            int value = (hexToChar(hex[0]) << 4) + hexToChar(hex[1]);
                            result += static_cast<char>(value);
                            i += 2;
                        } else {
                            result += next;
                        }
                    } else {
                        result += next;
                    }
                    break;
                }
                
                default:
                    result += next;
                    break;
            }
        } else {
            result += c;
        }
    }
    
    return result;
}

std::string StringEscape::escape(const std::string& str) {
    std::string result;
    result.reserve(str.length() * 2);
    
    for (char c : str) {
        switch (c) {
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '\v': result += "\\v"; break;
            case '\f': result += "\\f"; break;
            case '\b': result += "\\b"; break;
            case '\a': result += "\\a"; break;
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            case '\'': result += "\\'"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[5];
                    snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }
    
    return result;
}

bool StringEscape::isValidEscape(const std::string& str) {
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '\\') {
            if (i + 1 >= str.length()) return false;
            
            char next = str[i + 1];
            std::string validEscapes = "nrtvfba\\\"\'?01234567uUx";
            if (validEscapes.find(next) == std::string::npos) {
                return false;
            }
        }
    }
    return true;
}

std::string Utility::floatToString(float value) {
    std::ostringstream oss;
    oss << std::setprecision(std::numeric_limits<float>::max_digits10)
        << std::noshowpoint << value;
    return oss.str();
}
std::string Utility::doubleToString(double value) {
    std::ostringstream oss;
    oss << std::setprecision(std::numeric_limits<double>::max_digits10)
        << std::noshowpoint << value;
    return oss.str();
}
std::string Utility::longDoubleToString(long double value) {
    std::ostringstream oss;
    oss << std::setprecision(std::numeric_limits<long double>::max_digits10)
        << std::noshowpoint << value;
    return oss.str();
}

bool Utility::compareValues(const Value& left, const Value& right) {
    if (left.type != right.type && !checkObjects(left, right) && !checkStrings(left, right) && !checkNumbers(left, right)) return false;

    if (checkNumbers(left, right)) return left.number_value == right.number_value;
    if (checkStrings(left, right)) return left.toString() == right.toString();

    switch (left.type) {
        case DataType::NUMBER:
        case DataType::HEXADECIMAL:
        case DataType::BINARY:
        case DataType::OCTAL:
            return left.number_value == right.number_value;
        
        case DataType::STRING:
        case DataType::LINK:
        case DataType::PATH:
        case DataType::VARIABLE:
            return left.string_value == right.string_value;
        
        case DataType::BOOLEAN:
            return left.boolean_value == right.boolean_value;
        
        case DataType::NULL_TYPE:
            return true;

        case DataType::NOT_A_NUMBER:
        case DataType::INFINITE:
            return left.toNumber() == right.toNumber();

        case DataType::JUSTC_OBJECT:
        case DataType::JSON_OBJECT:
        case DataType::ENUM:
            if (left.type == DataType::ENUM && right.type != DataType::ENUM) return false;
            if (right.type == DataType::ENUM && left.type != DataType::ENUM) return false;
            if (left.properties.size() != right.properties.size()) return false;
            for (const auto& [key, val] : left.properties) {
                auto it = right.properties.find(key);
                if (it == right.properties.end()) return false;
                if (!compareValues(val, it->second)) return false;
            }
            return true;

        case DataType::JSON_ARRAY:
            if (left.array_elements.size() != right.array_elements.size()) return false;
            for (size_t i = 0; i < left.array_elements.size(); i++) {
                if (!compareValues(left.array_elements[i], right.array_elements[i])) return false;
            }
            return true;

        case DataType::BINARY_DATA:
            return left.binary_data == right.binary_data;

        case DataType::FUNCTION:
            return _stringifyValue(left) == _stringifyValue(right);

        default: return left.toBoolean() == right.toBoolean();
    }
}
bool Utility::compareValues(const Value::Property& left, const Value& right) {
    if (left.hasGetter) return false;
    return compareValues(left.value, right);
}
bool Utility::compareValues(const Value& left, const Value::Property& right) {
    if (right.hasGetter) return false;
    return compareValues(left, right.value);
}
bool Utility::compareValues(const Value::Property& left, const Value::Property& right) {
    if (left.hasGetter || right.hasGetter) return false;
    return compareValues(left.value, right.value);
}

bool Utility::checkElement(const Value& val) {
    switch (val.type) {
        case DataType::JSX_ELEMENT:
        case DataType::JSON_OBJECT:
        case DataType::JUSTC_OBJECT:
            return true;
        default:
            return false;
    }
}

bool Utility::checkArray(const Value& val) {
    switch (val.type) {
        case DataType::JSON_ARRAY:
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
            return true;
        default:
            return false;
    }
}
bool Utility::checkArrays(const Value& left, const Value& right) {
    return (checkArray(left) && checkArray(right));
}

std::string Utility::hashString(const Value& val) {
    std::stringstream ss;
    ss << static_cast<int>(val.type);
    ss << val.cpptype;
    ss << (val.boolean_value ? "1" : "0");
    ss << val.toNumericString();
    ss << uint64ToHexString(static_cast<uint64_t>(val.object_value.size()));
    for (const auto& [key, item] : val.object_value) {
        ss << key;
        ss << hashString(item);
    }
    ss << uint64ToHexString(static_cast<uint64_t>(val.properties.size()));
    for (const auto& [key, item] : val.properties) {
        ss << key;
        ss << hashString(item);
    }
    ss << uint64ToHexString(static_cast<uint64_t>(val.array_elements.size()));
    for (const auto& item : val.array_elements) {
        ss << hashString(item);
    }
    ss << val.string_value;
    return ss.str();
}
std::string Utility::hashString(const Value::Property& val) {
    std::stringstream ss;
    ss << static_cast<int>(val.access);
    ss << (val.hasGetter && val.hasSetter ? "a" : val.hasGetter ? "b" : val.hasSetter ? "c" : "d");
    ss << hashString(val.value);
    return ss.str();
}

namespace {
    template<typename From, typename To>
    static inline To bit_cast(const From& from) {
        static_assert(sizeof(From) == sizeof(To), "Sizes must match for bit_cast");
        To to;
        std::memcpy(&to, &from, sizeof(To));
        return to;
    }

    template<typename T>
    static constexpr bool is_integral_v = std::is_integral_v<T>;
}

template<typename To, typename From>
To Utility::bitCast(const From& from) {
    return bit_cast<From, To>(from);
}

template<typename To, typename From>
std::vector<To> Utility::expandVector(const std::vector<From>& input) {
    static_assert(is_integral_v<From> && is_integral_v<To>, 
                  "Integral types required");
    static_assert(sizeof(To) >= sizeof(From), "To must be larger or equal");
    
    constexpr size_t N = sizeof(To) / sizeof(From);
    static_assert(N >= 1, "N must be >= 1");
    
    if (input.size() % N != 0) {
        throw std::invalid_argument("Input size must be multiple of " + std::to_string(N));
    }
    
    std::vector<To> result;
    result.reserve(input.size() / N);
    
    if constexpr (N == 1) {
        for (const auto& val : input) {
            result.push_back(bit_cast<From, To>(val));
        }
    } else {
        for (size_t i = 0; i < input.size(); i += N) {
            To value = 0;
            for (size_t j = 0; j < N; ++j) {
                const size_t shift = j * 8 * sizeof(From);
                using UnsignedFrom = std::make_unsigned_t<From>;
                const auto unsigned_val = static_cast<UnsignedFrom>(input[i + j]);
                value |= (static_cast<To>(unsigned_val) << shift);
            }
            result.push_back(value);
        }
    }
    
    return result;
}

template<typename To, typename From>
std::vector<To> Utility::shrinkVector(const std::vector<From>& input) {
    static_assert(is_integral_v<From> && is_integral_v<To>, "Integral types required");
    static_assert(sizeof(From) >= sizeof(To), "From must be larger or equal");
    
    constexpr size_t N = sizeof(From) / sizeof(To);
    static_assert(N >= 1, "N must be >= 1");
    
    std::vector<To> result;
    result.reserve(input.size() * N);
    
    if constexpr (N == 1) {
        for (const auto& val : input) {
            result.push_back(bit_cast<From, To>(val));
        }
    } else {
        for (const auto& val : input) {
            using UnsignedFrom = std::make_unsigned_t<From>;
            const auto unsigned_val = static_cast<UnsignedFrom>(val);
            constexpr auto mask = static_cast<UnsignedFrom>(std::numeric_limits<To>::max());
            
            for (size_t j = 0; j < N; ++j) {
                const size_t shift = j * 8 * sizeof(To);
                const To part = static_cast<To>((unsigned_val >> shift) & mask);
                result.push_back(part);
            }
        }
    }
    
    return result;
}

template<typename To, typename From>
std::vector<To> Utility::reinterpretSign(const std::vector<From>& input) {
    static_assert(is_integral_v<From> && is_integral_v<To>, "Integral types required");
    static_assert(sizeof(From) == sizeof(To), "Sizes must match for sign conversion");
    
    std::vector<To> result;
    result.reserve(input.size());
    
    for (const auto& val : input) {
        result.push_back(bit_cast<From, To>(val));
    }
    
    return result;
}

template<typename To, typename From>
std::vector<To> Utility::convertVector(const std::vector<From>& input) {
    static_assert(is_integral_v<From> && is_integral_v<To>, "Integral types required");
    
    constexpr size_t from_size = sizeof(From);
    constexpr size_t to_size = sizeof(To);
    
    if constexpr (from_size == to_size) {
        return reinterpretSign<To, From>(input);
    } else if constexpr (from_size < to_size) {
        return expandVector<To, From>(input);
    } else {
        return shrinkVector<To, From>(input);
    }
}

std::vector<uint8_t> Utility::toUint8(const std::vector<int8_t>& input) {
    return reinterpretSign<uint8_t, int8_t>(input);
}

std::vector<uint16_t> Utility::toUint16(const std::vector<int16_t>& input) {
    return reinterpretSign<uint16_t, int16_t>(input);
}

std::vector<uint32_t> Utility::toUint32(const std::vector<int32_t>& input) {
    return reinterpretSign<uint32_t, int32_t>(input);
}

std::vector<uint64_t> Utility::toUint64(const std::vector<int64_t>& input) {
    return reinterpretSign<uint64_t, int64_t>(input);
}

std::vector<int8_t> Utility::toInt8(const std::vector<uint8_t>& input) {
    return reinterpretSign<int8_t, uint8_t>(input);
}

std::vector<int16_t> Utility::toInt16(const std::vector<uint16_t>& input) {
    return reinterpretSign<int16_t, uint16_t>(input);
}

std::vector<int32_t> Utility::toInt32(const std::vector<uint32_t>& input) {
    return reinterpretSign<int32_t, uint32_t>(input);
}

std::vector<int64_t> Utility::toInt64(const std::vector<uint64_t>& input) {
    return reinterpretSign<int64_t, uint64_t>(input);
}

#define INSTANTIATE_CONVERT(To, From) \
    template std::vector<To> Utility::convertVector<To, From>(const std::vector<From>&);

INSTANTIATE_CONVERT(uint8_t,  int8_t)
INSTANTIATE_CONVERT(uint8_t,  uint8_t)
INSTANTIATE_CONVERT(uint8_t,  int16_t) 
INSTANTIATE_CONVERT(uint8_t,  uint16_t)
INSTANTIATE_CONVERT(uint8_t,  int32_t)
INSTANTIATE_CONVERT(uint8_t,  uint32_t)
INSTANTIATE_CONVERT(uint8_t,  int64_t)
INSTANTIATE_CONVERT(uint8_t,  uint64_t)


INSTANTIATE_CONVERT(uint16_t, int8_t)
INSTANTIATE_CONVERT(uint16_t, uint8_t)
INSTANTIATE_CONVERT(uint16_t, int16_t)
INSTANTIATE_CONVERT(uint16_t, uint16_t)
INSTANTIATE_CONVERT(uint16_t, int32_t)
INSTANTIATE_CONVERT(uint16_t, uint32_t)
INSTANTIATE_CONVERT(uint16_t, int64_t)
INSTANTIATE_CONVERT(uint16_t, uint64_t)


INSTANTIATE_CONVERT(uint32_t, int8_t)
INSTANTIATE_CONVERT(uint32_t, uint8_t)
INSTANTIATE_CONVERT(uint32_t, int16_t)
INSTANTIATE_CONVERT(uint32_t, uint16_t)
INSTANTIATE_CONVERT(uint32_t, int32_t)
INSTANTIATE_CONVERT(uint32_t, uint32_t)
INSTANTIATE_CONVERT(uint32_t, int64_t)
INSTANTIATE_CONVERT(uint32_t, uint64_t)


INSTANTIATE_CONVERT(uint64_t, int8_t)
INSTANTIATE_CONVERT(uint64_t, uint8_t)
INSTANTIATE_CONVERT(uint64_t, int16_t)
INSTANTIATE_CONVERT(uint64_t, uint16_t)
INSTANTIATE_CONVERT(uint64_t, int32_t)
INSTANTIATE_CONVERT(uint64_t, uint32_t)
INSTANTIATE_CONVERT(uint64_t, int64_t)
INSTANTIATE_CONVERT(uint64_t, uint64_t)


INSTANTIATE_CONVERT(int8_t,   uint8_t)
INSTANTIATE_CONVERT(int8_t,   int8_t)
INSTANTIATE_CONVERT(int8_t,   uint16_t)
INSTANTIATE_CONVERT(int8_t,   int16_t)
INSTANTIATE_CONVERT(int8_t,   uint32_t)
INSTANTIATE_CONVERT(int8_t,   int32_t)
INSTANTIATE_CONVERT(int8_t,   uint64_t)
INSTANTIATE_CONVERT(int8_t,   int64_t)


INSTANTIATE_CONVERT(int16_t,  uint8_t)
INSTANTIATE_CONVERT(int16_t,  int8_t)
INSTANTIATE_CONVERT(int16_t,  uint16_t)
INSTANTIATE_CONVERT(int16_t,  int16_t)
INSTANTIATE_CONVERT(int16_t,  uint32_t)
INSTANTIATE_CONVERT(int16_t,  int32_t)
INSTANTIATE_CONVERT(int16_t,  uint64_t)
INSTANTIATE_CONVERT(int16_t,  int64_t)


INSTANTIATE_CONVERT(int32_t,  uint8_t)
INSTANTIATE_CONVERT(int32_t,  int8_t)
INSTANTIATE_CONVERT(int32_t,  uint16_t)
INSTANTIATE_CONVERT(int32_t,  int16_t)
INSTANTIATE_CONVERT(int32_t,  uint32_t)
INSTANTIATE_CONVERT(int32_t,  int32_t)
INSTANTIATE_CONVERT(int32_t,  uint64_t)
INSTANTIATE_CONVERT(int32_t,  int64_t)


INSTANTIATE_CONVERT(int64_t,  uint8_t)
INSTANTIATE_CONVERT(int64_t,  int8_t)
INSTANTIATE_CONVERT(int64_t,  uint16_t)
INSTANTIATE_CONVERT(int64_t,  int16_t)
INSTANTIATE_CONVERT(int64_t,  uint32_t)
INSTANTIATE_CONVERT(int64_t,  int32_t)
INSTANTIATE_CONVERT(int64_t,  uint64_t)
INSTANTIATE_CONVERT(int64_t,  int64_t)

#undef INSTANTIATE_CONVERT

#define INSTANTIATE_BIT_CAST(To, From) \
    template To Utility::bitCast<To, From>(const From&);

INSTANTIATE_BIT_CAST(uint8_t,  int8_t)
INSTANTIATE_BIT_CAST(uint8_t,  uint8_t)
INSTANTIATE_BIT_CAST(int8_t,   uint8_t)
INSTANTIATE_BIT_CAST(int8_t,   int8_t)

INSTANTIATE_BIT_CAST(uint16_t, int16_t)
INSTANTIATE_BIT_CAST(uint16_t, uint16_t)
INSTANTIATE_BIT_CAST(int16_t,  uint16_t)
INSTANTIATE_BIT_CAST(int16_t,  int16_t)

INSTANTIATE_BIT_CAST(uint32_t, int32_t)
INSTANTIATE_BIT_CAST(uint32_t, uint32_t)
INSTANTIATE_BIT_CAST(int32_t,  uint32_t)
INSTANTIATE_BIT_CAST(int32_t,  int32_t)

INSTANTIATE_BIT_CAST(uint64_t, int64_t)
INSTANTIATE_BIT_CAST(uint64_t, uint64_t)
INSTANTIATE_BIT_CAST(int64_t,  uint64_t)
INSTANTIATE_BIT_CAST(int64_t,  int64_t)

#undef INSTANTIATE_BIT_CAST

uint64_t Utility::checkPointer(const Value& val) {
    if (val.type != DataType::POINTER) throw std::runtime_error("Expected pointer.");
    return val.getNumericValue<uint64_t>();
}
