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

#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <future>
#include <cstdint>
#include <variant>
#include "lexer.h"
#include "version.h"
#include <functional>
#include <cstring>
#include <iomanip>
#include <limits>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <optional>

#ifdef _MSC_VER
    #define JUSTC_HAS_INT128 0
    #define JUSTC_HAS_UINT128 0
    #define JUSTC_HAS_FLOAT128 0
    using int128_t = long long;
    using uint128_t = unsigned long long;
#elif defined(__SIZEOF_INT128__)
    #define JUSTC_HAS_INT128 1
    #define JUSTC_HAS_UINT128 1
    using int128_t = __int128;
    using uint128_t = unsigned __int128;
    #ifdef __SIZEOF_FLOAT128__
        #if JUSTC_HAS_QUADMATH
            #define JUSTC_HAS_FLOAT128 1
            #include <quadmath.h>
        #endif
    #else
        #define JUSTC_HAS_FLOAT128 0
    #endif
#else
    #define JUSTC_HAS_INT128 0
    #define JUSTC_HAS_UINT128 0
    #define JUSTC_HAS_FLOAT128 0
    using int128_t = long long;
    using uint128_t = unsigned long long;
#endif

#ifndef DEFAULT_CPP_TYPE
#define DEFAULT_CPP_TYPE "_"
#endif

struct Value;
class Parser;

struct ObjectContext {
    std::shared_ptr<Parser> parser;
    std::string outputMode;
    std::vector<std::string> outputVariables;
    std::unordered_map<std::string, Value> variables;
    bool allowJavaScript;
    bool allowLuau;

    std::shared_ptr<ObjectContext> parent;
    std::unordered_map<std::string, std::shared_ptr<ObjectContext>> childObjects;
};

enum class DataType : int8_t {
    JUSTC_OBJECT =  0,
    NUMBER       =  1,
    STRING       =  2,
    LINK         =  3,
    BOOLEAN      =  4,
    JSON_OBJECT  =  5,
    JSON_ARRAY   =  6,
    NULL_TYPE    =  7,
    HEXADECIMAL  =  9,
    BINARY       = 11,
    PATH         = 12,
    ERROR        = 13,
    VARIABLE     = 14,
    FUNCTION     = 15,
    NOT_A_NUMBER = 17,
    INFINITE     = 18,
    SYNTAX_ERROR = 19,
    OCTAL        = 20,
    CLASS        = 21,
    SPACE        = 22,
    BINARY_DATA  = 23,
    BIGNUM       = 24,
    INTEGER      = 25,
    BASE64       = 26,
    HTTP_ERROR   = 27,
    JSX_ELEMENT  = 28,
    STRUCT       = 29,
    MAP          = 30,
    SET          = 31,
    PROMISE      = 32,
    ENUM         = 33,
    POINTER      = 34,
    NINFINITE    = 35,

    INT8_ARRAY   =-2,
    INT16_ARRAY  =-3,
    INT32_ARRAY  =-4,
    INT64_ARRAY  =-5,
    UINT8_ARRAY  =-6,
    UINT16_ARRAY =-7,
    UINT32_ARRAY =-8,
    UINT64_ARRAY =-9,
    CUINT8_ARRAY =-10,
    CUINT16_ARRAY=-11,
    CUINT32_ARRAY=-12,
    CUINT64_ARRAY=-13,
    FLOAT32_ARRAY=-14,
    FLOAT64_ARRAY=-15,

    UNKNOWN      =-1
};

inline std::string dataTypeToString(DataType type) {
    switch (type) {
        case DataType::JUSTC_OBJECT: return "Object";
        case DataType::NUMBER:       return "Number";
        case DataType::STRING:       return "String";
        case DataType::LINK:         return "Link";
        case DataType::BOOLEAN:      return "Boolean";
        case DataType::JSON_OBJECT:  return "Object";
        case DataType::JSON_ARRAY:   return "Array";
        case DataType::NULL_TYPE:    return "Null";
        case DataType::HEXADECIMAL:  return "Hexadecimal number";
        case DataType::BINARY:       return "Binary number";
        case DataType::PATH:         return "Path";
        case DataType::ERROR:        return "Error";
        case DataType::VARIABLE:     return "Variable";
        case DataType::FUNCTION:     return "Function";
        case DataType::NOT_A_NUMBER: return "NaN";
        case DataType::INFINITE:     return "Positive Infinity";
        case DataType::SYNTAX_ERROR: return "Syntax Error";
        case DataType::OCTAL:        return "Octal number";
        case DataType::CLASS:        return "Class";
        case DataType::SPACE:        return "Space";
        case DataType::BINARY_DATA:  return "Data";
        case DataType::BIGNUM:       return "Big number";
        case DataType::INTEGER:      return "Integer";
        case DataType::BASE64:       return "Base64 number";
        case DataType::HTTP_ERROR:   return "HTTP Error";
        case DataType::JSX_ELEMENT:  return "Element";
        case DataType::STRUCT:       return "Struct";
        case DataType::MAP:          return "Map";
        case DataType::SET:          return "Set";
        case DataType::PROMISE:      return "Promise";
        case DataType::ENUM:         return "Enum";
        case DataType::POINTER:      return "Pointer";
        case DataType::NINFINITE:    return "Negative Infinity";

        case DataType::INT8_ARRAY:   return "Int8 Array";
        case DataType::INT16_ARRAY:  return "Int16 Array";
        case DataType::INT32_ARRAY:  return "Int32 Array";
        case DataType::INT64_ARRAY:  return "Int64 Array";
        case DataType::UINT8_ARRAY:  return "UInt8 Array";
        case DataType::UINT16_ARRAY: return "UInt16 Array";
        case DataType::UINT32_ARRAY: return "UInt32 Array";
        case DataType::UINT64_ARRAY: return "UInt64 Array";
        case DataType::CUINT8_ARRAY: return "UInt8 clamped Array";
        case DataType::CUINT16_ARRAY:return "UInt16 clamped Array";
        case DataType::CUINT32_ARRAY:return "UInt32 clamped Array";
        case DataType::CUINT64_ARRAY:return "UInt32 clamped Array";
        case DataType::FLOAT32_ARRAY:return "Float32 Array";
        case DataType::FLOAT64_ARRAY:return "Float64 Array";

        case DataType::UNKNOWN:      return "unknown";
        default:                     return "invalid";
    }
};
inline std::string dataTypeToTypeDecl(DataType type) {
    switch (type) {
        case DataType::JUSTC_OBJECT: return "object";
        case DataType::NUMBER:       return "number";
        case DataType::STRING:       return "string";
        case DataType::LINK:         return "Link";
        case DataType::BOOLEAN:      return "boolean";
        case DataType::JSON_OBJECT:  return "object";
        case DataType::JSON_ARRAY:   return "array";
        case DataType::HEXADECIMAL:  return "number";
        case DataType::BINARY:       return "number";
        case DataType::OCTAL:        return "number";
        case DataType::BINARY_DATA:  return "data";
        case DataType::BIGNUM:       return "number";
        case DataType::JSX_ELEMENT:  return "element";
        case DataType::MAP:          return "map";
        case DataType::SET:          return "set";
        default:                     return "auto";
    }
};

enum class NumericType : uint8_t {
    NONE = 0,
    FLOAT32 = 1,
    FLOAT64 = 2,
    BIGNUM = 3,
    FLOAT128 = 4,
    INT8 = 5,
    INT16 = 6,
    INT32 = 7,
    INT64 = 8,
    INT128 = 9,
    UINT8 = 10,
    UINT16 = 11,
    UINT32 = 12,
    UINT64 = 13,
    UINT128 = 14,
    CUINT8 = 15,
    CUINT16 = 16,
    CUINT32 = 17,
    CUINT64 = 18
};

struct NumericValue {
    NumericType type;
    double value;
    void* data;
    
    NumericValue() : type(NumericType::NONE), value(0.0), data(nullptr) {}
    
    template<typename T>
    NumericValue(T val) : type(NumericType::NONE), value(0.0), data(nullptr) {
        set(val);
    }
    
    template<typename T>
    void set(T val) {
        data = malloc(sizeof(T));
        if (data) {
            memcpy(data, &val, sizeof(T));
            value = static_cast<double>(val);
            setType<T>();
        }
    }
    
    template<typename T>
    T get() const {
        if (!data) return T{0};
        T result;
        memcpy(&result, data, sizeof(T));
        return result;
    }
    
    template<typename T>
    void setType() {
        if constexpr (std::is_same_v<T, float>) type = NumericType::FLOAT32;
        else if constexpr (std::is_same_v<T, double>) type = NumericType::FLOAT64;
        else if constexpr (std::is_same_v<T, long double>) type = NumericType::BIGNUM;
        else if constexpr (std::is_same_v<T, int8_t>) type = NumericType::INT8;
        else if constexpr (std::is_same_v<T, int16_t>) type = NumericType::INT16;
        else if constexpr (std::is_same_v<T, int32_t>) type = NumericType::INT32;
        else if constexpr (std::is_same_v<T, int64_t>) type = NumericType::INT64;
        else if constexpr (std::is_same_v<T, uint8_t>) type = NumericType::UINT8;
        else if constexpr (std::is_same_v<T, uint16_t>) type = NumericType::UINT16;
        else if constexpr (std::is_same_v<T, uint32_t>) type = NumericType::UINT32;
        else if constexpr (std::is_same_v<T, uint64_t>) type = NumericType::UINT64;
        #if JUSTC_HAS_INT128
        else if constexpr (std::is_same_v<T, __int128>) type = NumericType::INT128;
        else if constexpr (std::is_same_v<T, unsigned __int128>) type = NumericType::UINT128;
        #endif
        #if JUSTC_HAS_FLOAT128
        else if constexpr (std::is_same_v<T, __float128>) type = NumericType::FLOAT128;
        #endif
        else type = NumericType::FLOAT64;
    }
    
    ~NumericValue() {
        if (data) {
            free(data);
            data = nullptr;
        }
    }
    
    NumericValue(const NumericValue& other) {
        type = other.type;
        value = other.value;
        if (other.data) {
            size_t size = getTypeSize(type);
            data = malloc(size);
            if (data) {
                memcpy(data, other.data, size);
            }
        } else {
            data = nullptr;
        }
    }
    
    NumericValue& operator=(const NumericValue& other) {
        if (this != &other) {
            if (data) {
                free(data);
                data = nullptr;
            }
            type = other.type;
            value = other.value;
            if (other.data) {
                size_t size = getTypeSize(type);
                data = malloc(size);
                if (data) {
                    memcpy(data, other.data, size);
                }
            }
        }
        return *this;
    }
    
    static size_t getTypeSize(NumericType type) {
        switch (type) {
            case NumericType::FLOAT32: return sizeof(float);
            case NumericType::FLOAT64: return sizeof(double);
            case NumericType::BIGNUM: return sizeof(long double);
            #if JUSTC_HAS_FLOAT128
            case NumericType::FLOAT128: return 16; // __float128
            #endif
            case NumericType::INT8: return sizeof(int8_t);
            case NumericType::INT16: return sizeof(int16_t);
            case NumericType::INT32: return sizeof(int32_t);
            case NumericType::INT64: return sizeof(int64_t);
            #if JUSTC_HAS_INT128
            case NumericType::INT128: return sizeof(__int128);
            case NumericType::UINT128: return sizeof(unsigned __int128);
            #endif
            case NumericType::UINT8: case NumericType::CUINT8: return sizeof(uint8_t);
            case NumericType::UINT16: case NumericType::CUINT16: return sizeof(uint16_t);
            case NumericType::UINT32: case NumericType::CUINT32: return sizeof(uint32_t);
            case NumericType::UINT64: case NumericType::CUINT64: return sizeof(uint64_t);
            default: return sizeof(double);
        }
    }

    template <class Archive>
    void serialize(Archive& archive) {
        int typeInt = static_cast<int>(type);
        archive(typeInt);
        type = static_cast<NumericType>(typeInt);
        
        archive(value);
        
        if (Archive::is_loading::value) {
            if (data) {
                free(data);
                data = nullptr;
            }
            size_t size = getTypeSize(type);
            data = malloc(size);
            if (data) {
                switch (type) {
                    case NumericType::FLOAT32:
                        *(float*)data = static_cast<float>(value);
                        break;
                    case NumericType::FLOAT64:
                        *(double*)data = value;
                        break;
                    case NumericType::BIGNUM:
                        *(long double*)data = static_cast<long double>(value);
                        break;
                    #if JUSTC_HAS_FLOAT128
                    case NumericType::FLOAT128:
                        *(__float128*)data = static_cast<__float128>(value);
                        break;
                    #endif
                    case NumericType::INT8:
                        *(int8_t*)data = static_cast<int8_t>(value);
                        break;
                    case NumericType::INT16:
                        *(int16_t*)data = static_cast<int16_t>(value);
                        break;
                    case NumericType::INT32:
                        *(int32_t*)data = static_cast<int32_t>(value);
                        break;
                    case NumericType::INT64:
                        *(int64_t*)data = static_cast<int64_t>(value);
                        break;
                    #if JUSTC_HAS_INT128
                    case NumericType::INT128:
                        *(__int128*)data = static_cast<__int128>(value);
                        break;
                    #endif
                    #if JUSTC_HAS_UINT128
                    case NumericType::UINT128:
                        *(unsigned __int128*)data = static_cast<unsigned __int128>(value);
                        break;
                    #endif
                    case NumericType::UINT8:
                    case NumericType::CUINT8:
                        *(uint8_t*)data = static_cast<uint8_t>(value);
                        break;
                    case NumericType::UINT16:
                    case NumericType::CUINT16:
                        *(uint16_t*)data = static_cast<uint16_t>(value);
                        break;
                    case NumericType::UINT32:
                    case NumericType::CUINT32:
                        *(uint32_t*)data = static_cast<uint32_t>(value);
                        break;
                    case NumericType::UINT64:
                    case NumericType::CUINT64:
                        *(uint64_t*)data = static_cast<uint64_t>(value);
                        break;
                    default:
                        *(double*)data = value;
                        break;
                }
            }
        }
    }
};

struct FunctionInfo {
    std::string code;
    std::vector<std::string> paramNames;
    std::vector<DataType> paramTypes;
    std::vector<struct Value> defaultValues;
    bool hasVarArgs;
    bool isIsolated;

    FunctionInfo() : hasVarArgs(false), isIsolated(false) {}

    template <class Archive>
    void serialize(Archive& archive) {
        std::vector<int> paramTypesInt;
        if (Archive::is_loading::value) {
            archive(paramTypesInt);
            paramTypes.resize(paramTypesInt.size());
            for (size_t i = 0; i < paramTypesInt.size(); ++i) {
                paramTypes[i] = static_cast<DataType>(paramTypesInt[i]);
            }
        } else {
            paramTypesInt.resize(paramTypes.size());
            for (size_t i = 0; i < paramTypes.size(); ++i) {
                paramTypesInt[i] = static_cast<int>(paramTypes[i]);
            }
            archive(paramTypesInt);
        }
        
        archive(
            code,
            paramNames,
            defaultValues,
            hasVarArgs,
            isIsolated
        );
    }
};

enum class VariableType : uint8_t {
    VARIABLE = 0,
    GLOBAL = 1,
    LOCAL = 2
};

enum class Access : uint8_t {
    READ_WRITE = 0,
    READ_ONLY  = 1,
    WRITE_ONLY = 2,
    PRIVATE    = 3,
};

struct Value {
    DataType type;
    std::string cpptype;

    union {
        double number_value;
        bool boolean_value;
    };
    std::string string_value;
    std::shared_ptr<void> complex_value;
    std::string name;
    std::unordered_map<std::string, Value> object_value;
    std::vector<unsigned char> binary_data;

    bool isVariable;
    std::string variable;
    VariableType varType;
    bool isConst;

    struct Property;

    std::shared_ptr<ObjectContext> object_context;
    std::unordered_map<std::string, Property> properties;
    std::vector<Value> array_elements;
    DataType object_type;

    FunctionInfo function_info;
    std::shared_ptr<ObjectContext> closure_context;

    bool native;
    
    std::shared_ptr<NumericValue> numeric_data;

    Value() : type(DataType::UNKNOWN), cpptype(DEFAULT_CPP_TYPE), number_value(0), name("unknown"), object_type(DataType::UNKNOWN), native(false), isVariable(false), varType(VariableType::VARIABLE) {}
    Value(DataType t) : type(t), cpptype(DEFAULT_CPP_TYPE), number_value(0), name(dataTypeToString(t)), object_type(DataType::UNKNOWN), native(false), isVariable(false), varType(VariableType::VARIABLE) {}
    Value(DataType t, std::string s) : type(t), cpptype(DEFAULT_CPP_TYPE), string_value(s), name(dataTypeToString(t)), object_type(DataType::UNKNOWN), native(false), isVariable(false), varType(VariableType::VARIABLE) {}

    std::string toString() const;
    std::string toIdentifier() const;
    double toNumber() const;
    bool toBoolean() const;

    std::wstring toWString() const {
        try {
            return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(string_value);
        } catch (...) {
            return L"";
        }
    }

    bool isObject() const {
        return type == DataType::JUSTC_OBJECT ||
               type == DataType::JSON_OBJECT ||
               type == DataType::JSON_ARRAY ||
               type == DataType::ENUM;
    }
    Property* getProperty(const std::string& name);
    Value* getArrayElement(size_t index);

    Property getProperty(const std::string& name, Value placeholder);
    Property getProperty(const std::string& name, Value placeholder) const;

    static Value createNumber(double num);
    static Value createString(const std::string& str);
    static Value createBoolean(bool b);
    static Value createNull();
    static Value createLink(const std::string& link);
    static Value createPath(const std::string& path);
    static Value createVariable(const std::string& varName);
    static Value createHexadecimal(double num);
    static Value createBinary(double num);
    static Value createOctal(double num);
    static Value createBinaryData(const std::vector<unsigned char>& data);
    static Value createJustcObject(const std::shared_ptr<ObjectContext>& context);
    static Value createJsonObject(const std::unordered_map<std::string, Value>& obj);
    static Value createJsonArray(const std::vector<Value>& arr);

    static Value createString(const std::wstring& wstr) {
        Value result;
        result.type = DataType::STRING;
        try {
            result.string_value = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wstr);
            result.name = "\"" + result.string_value + "\"";
        } catch (...) {
            result.string_value = "";
            result.name = "\"\"";
        }
        return result;
    }

    template<typename T>
    static Value createNumberWithType(T num, NumericType numType);
    
    template<typename T>
    T getNumericValue() const {
        if (numeric_data) {
            return numeric_data->get<T>();
        }
        return static_cast<T>(number_value);
    }
    
    NumericType getNumericType() const {
        if (numeric_data) {
            return numeric_data->type;
        }
        return NumericType::FLOAT64;
    }
    
    double toDouble() const {
        if (numeric_data) {
            return numeric_data->value;
        }
        return number_value;
    }
    
    std::string toNumericString() const;

    Value toPrimitive() const;

    template<typename T>
    void setComplexData(const T& data) {
        complex_value = std::make_shared<T>(data);
    }
    
    template<typename T>
    T getComplexData() const {
        if (!complex_value) return T{};
        auto ptr = std::static_pointer_cast<T>(complex_value);
        if (!ptr) return T{};
        return *ptr;
    }

    template <class Archive>
    void serialize(Archive& archive) {
        int typeInt = static_cast<int>(type);
        archive(typeInt);
        type = static_cast<DataType>(typeInt);
        archive(cpptype);

        switch (type) {
            case DataType::NUMBER:
            case DataType::HEXADECIMAL:
            case DataType::BINARY:
            case DataType::OCTAL: {
                bool hasNumericData = static_cast<bool>(numeric_data);
                archive(hasNumericData);
                if (hasNumericData) {
                    archive(*numeric_data);
                } else {
                    archive(number_value);
                }
                break;
            }
            case DataType::STRING:
            case DataType::LINK:
            case DataType::PATH:
            case DataType::VARIABLE:
                archive(string_value);
                break;
            case DataType::BOOLEAN:
                archive(boolean_value);
                break;
            case DataType::JSON_OBJECT:
            case DataType::JUSTC_OBJECT:
                archive(properties);
                break;
            case DataType::JSON_ARRAY:
                archive(array_elements);
                break;
            case DataType::FUNCTION:
            case DataType::STRUCT:
                archive(name, string_value, function_info);
                break;
            case DataType::BINARY_DATA:
                archive(binary_data);
                break;
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
                //archive(*complex_value);
                break;
            default:
                break;
        }
    }

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;
};
namespace std {
    template<>
    struct hash<Value> {
        size_t operator()(const Value& value) const;
    };
}
struct Value::Property {
    Access access = Access::READ_WRITE;
    std::shared_ptr<Value> getter;
    std::shared_ptr<Value> setter;
    bool hasGetter = false;
    bool hasSetter = false;
    Value value;

    Property() : value(Value::createNull()) {}
    Property(const Value& val, const Access& acs): value(val), access(acs) {}

    template <class Archive>
    void serialize(Archive& archive) {
        int accessInt = static_cast<int>(access);
        archive(accessInt);
        access = static_cast<Access>(accessInt);
        archive(value);
    }
};
inline Value::Property* Value::getProperty(const std::string& name) {
    if (properties.find(name) != properties.end()) {
        return &properties[name];
    }
    return nullptr;
}
inline Value* Value::getArrayElement(size_t index) {
    if (type == DataType::JSON_ARRAY && index < array_elements.size()) {
        return &array_elements[index];
    }
    return nullptr;
}
inline Value::Property Value::getProperty(const std::string& name, Value placeholder) {
    if (properties.find(name) != properties.end()) {
        return properties[name];
    }
    Property prop;
    prop.value = placeholder;
    return prop;
}
inline Value::Property Value::getProperty(const std::string& name, Value placeholder) const {
    auto it = properties.find(name);
    if (it != properties.end()) {
        return it->second;
    }
    Property prop;
    prop.value = placeholder;
    return prop;
}

struct LogEntry {
    std::string type;
    std::string message;
    size_t position;
    std::string timestamp;

    LogEntry(const std::string& t, const std::string& m, size_t p, const std::string& ts = "")
        : type(t), message(m), position(p), timestamp(ts) {}
};

struct ParseResult {
    std::unordered_map<std::string, Value> returnValues;
    std::vector<LogEntry> logs;
    std::string logFilePath;
    std::string logFileContent;
    std::string error;
    std::vector<std::vector<std::string>> importLogs;
    bool array;

    std::shared_ptr<std::unordered_map<std::string, Value>> variables;
    std::shared_ptr<std::unordered_map<std::string, bool>> constants;
    std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> dependencies;

    ParseResult() : logFilePath(""), logFileContent(""), error(""), array(false) {}
};

struct JSONObject {
    std::unordered_map<std::string, Value::Property> properties;
};

struct JSONArray {
    std::vector<Value> elements;
};

struct JUSTCObject {
    std::unordered_map<std::string, Value> variables;
    std::vector<std::string> outputOrder;
};

struct ASTNode {
    std::string type;
    std::string identifier;
    Value value;
    std::vector<std::string> references;
    std::vector<std::string> tokens;
    size_t startPos;
    DataType typeDeclaration;
    bool constant;
    bool local;

    ASTNode(const std::string& t, const std::string& id = "", size_t start = 0)
        : type(t), identifier(id), startPos(start), typeDeclaration(DataType::UNKNOWN), local(false) {}
};

enum class CharType : uint8_t {
    GRAPHEME  = 0,
    CODEPOINT = 1,
    BYTE      = 2
};

struct Mutated {
    Value value;
    size_t startPos;
    bool applied = false;

    Mutated() : value(), startPos(0), applied(false) {}
    Mutated(Value v, size_t p) : value(v), startPos(p), applied(false) {}
};

using Function = std::function<Value(const std::vector<Value>&)>;

struct Class {
    Value constructor;
    Value destructor;
    std::unordered_map<std::string, Value::Property> staticObject;
    std::unordered_map<std::string, Value::Property> instanceObject;

    Class() : constructor(Value::createNull()), destructor(Value::createNull()) {}
    Class(Value constructor) : constructor(constructor), destructor(Value::createNull()) {}
    Class(Value constructor, Value destructor) : constructor(constructor), destructor(destructor) {}
};

enum class ParserType : uint8_t {
    SCRIPT = 0,
    STRUCT = 1,
    CLASS = 2,
    ENUM = 3,
};

struct PropertyPathNode {
    bool isProperty = false;
    bool isIndex = false;
    std::string name;
    size_t index = 0;
    std::optional<ASTNode> typeNode;
};

enum class CompressionAlgorithm : uint8_t {
    NONE = 0,
    ZLIB = 1,
    GZIP = 2,
    BZIP2 = 3,
    LZMA = 4,
    ZSTD = 5,
    LZ4 = 6,
    SNAPPY = 7,
    LZMA2 = 8,
    DEFLATE = 9,
};

struct StringPart {
    bool isLiteral;
    std::string content;

    StringPart(bool lit, const std::string& cont) : isLiteral(lit), content(cont) {}
};

class Parser {
private:
    bool doExecute;
    bool runAsync;
    std::vector<ParserToken> tokens;
    std::vector<ASTNode> ast;
    size_t position;
    std::string input;

    std::unordered_map<std::string, Value> variables;
    std::unordered_map<std::string, Mutated> mutated;
    std::unordered_map<std::string, bool> constVars;
    std::unordered_map<std::string, std::vector<std::string>> dependencies;
    std::vector<std::string> outputVariables;
    std::vector<std::string> outputNames;
    Value returnValue;
    std::string outputMode;
    bool allowJavaScript;
    bool globalScope;
    bool strictMode;
    bool canAllowJS;
    bool allowLuau;
    bool canAllowLuau;

    std::vector<LogEntry> logs;
    std::string logFilePath;
    std::string logFileContent;
    bool hasLogFile;

    std::string scriptName;
    std::string scriptType;

    bool asJSON;
    bool isJSONArray;
    std::string endOfScript;
    std::vector<Value> arrayItems;

    bool isFunction;

    CharType chartype;

    std::unordered_map<std::string, Function> userFunctions;
    std::unordered_map<std::string, bool> userFunctionsConst;
    std::vector<Function> variableUpdateListeners;

    ParserToken currentToken() const;
    ParserToken peekToken(size_t offset = 1) const;
    void advance();
    bool match(const std::string& type) const;
    bool match(const std::string& type, const std::string& value) const;
    bool isEnd() const;
    void skipCommas();

    std::string toLower(const std::string& str) const {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    std::vector<std::vector<std::string>> importLogs;

    std::unordered_map<uint64_t, std::unordered_map<std::string, Value>> localScopes;
    std::unordered_map<uint64_t, std::unordered_map<std::string, bool>> localConstVars;
    std::vector<uint64_t> scopeStack;
    uint64_t currentScope;
    uint64_t rootIndex;
    
    std::unordered_map<DataType, std::unordered_map<std::string, std::string>> typeMethods;

    ParserType parsertype;

    std::unordered_map<std::string, Value> structures;
    std::unordered_map<std::string, Value> structConstructors;
    uint64_t nextStructConstructor;

    std::unordered_map<std::string, Value> classes;
    std::unordered_map<std::string, Value> staticValues;
    
    std::vector<std::string> outputExcludeVariables;

    std::vector<Value> promises;

    size_t nextIndex;

    bool hasWindow;

    // logs
    void addLog(const std::string& type, const std::string& message, size_t position = 0);
    void setLogFile(const std::string& path);
    void appendToLogFile(const std::string& content);
    void addImportLog(const std::string& path, const std::string& script, const std::string& type);

    Value astNodeToValue(const ASTNode& node);

    Value getIdentifier();

    Value parsePrimary(bool doExecute, bool doFunctionCall = true);
    Value parseConditional(bool doExecute, bool identifierMode = false, bool doFunctionCall = true, bool ignoreColon = false);
    Value parseBitwiseOR(bool doExecute, bool identifierMode = false, bool doFunctionCall = true, bool ignoreColon = false);
    Value parseBitwiseXOR(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon);
    Value parseBitwiseAND(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon);
    Value parseBitwiseSHIFT(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon);
    Value parseBitwiseNOT(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon);
    Value parsePipelineOrMethodCall(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon);
    Value parseElvisOrNullCoalescing(bool doExecute, bool identifierMode, bool doFunctionCall = true, bool ignoreColon = false);
    Value parseLogicalOR(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon = false);
    Value parseLogicalXOR(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon = false);
    Value parseLogicalAND(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon = false);
    Value parseLogicalIMPLY(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon = false);
    Value parseEquality(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon = false);
    Value parseComparison(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon = false);
    Value parseTerm(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon = false);
    Value parseFactor(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon = false);
    Value parsePower(bool doExecute, bool identifierMode, bool doFunctionCall, bool ignoreColon = false);
    Value parseUnary(bool doExecute, bool identifierMode = false, bool doFunctionCall = true, bool ignoreColon = false);
    Value parseFunctionCall(bool doExecute, bool doFunctionCall = true);
    Value parseSpaceCall(bool doExecute, bool doFunctionCall = true);
    std::vector<Value> parseLambda(bool doExecute, size_t pos);

    ASTNode typeDeclarationNode(std::string typeDecl, size_t pos);

    Value parseJustcObject(bool doExecute);
    Value parseJsonObject(bool doExecute);
    Value parseJsonArray(bool doExecute);
    Value parseObjectPropertyAccess(bool doExecute, bool set = false);
    std::shared_ptr<ObjectContext> createObjectContext(bool inheritFromParent);

    std::pair<Value, Value::Property> accessProperty(const Value& obj, const std::string& propName, const Access& requestAccess = Access::READ_ONLY);
    Value accessIndex(const Value& arr, size_t index);
    std::vector<Value> parseArguments(bool doExecute);

    std::string readVariableName();
    void checkVariableNameAvailable(std::string name);

    ASTNode parseStatement(bool doExecute);
    bool CanIgnoreNoAssignmentOperator();
    Value makeValue(Value value, bool b);
    ASTNode parseGlobal(bool doExecute, bool constant = false);
    ASTNode parseVariableDeclaration(bool doExecute, bool constant = false, bool local = false, bool global = false);
    ASTNode parseCommand(bool doExecute);
    ASTNode parseScopeCommand();
    ASTNode parseOutputCommand();
    ASTNode parseReturnCommand();
    ASTNode parseAllowCommand();
    ASTNode parseImportCommand();

    Value executeFunction(const std::string& funcName, const std::vector<Value>& args, size_t startPos);
    Value doubleDot(const Value& left, const Value& right);
    Value evaluateExpression(const Value& left, const std::string& op, const Value& right, bool doExecute);
    Value handleInequality(const Value& value);
    Value handleConditional(const Value& condition, const Value& thenVal, const Value& elseVal,
                           const std::string& thenOp, const std::string& elseOp);

    void buildDependencyGraph();
    bool detectCycles();
    bool dfsCycleDetection(const std::string& node,
                          std::unordered_map<std::string, bool>& visited,
                          std::unordered_map<std::string, bool>& recStack,
                          std::vector<std::string>& cyclePath);

    Value resolveVariableValue(const std::string& varName, const bool unknownIsString);
    void evaluateAllVariables();
    void evaluateAllVariablesSync();
    void evaluateAllVariablesAsync();
    std::runtime_error typeDeclarationError(const DataType left, const DataType right, const ASTNode node);
    Value applyTypeDeclaration(const Value value, const ASTNode node);
    Value applyCPPTypeDeclaration(const Value value, const std::string& cpptype, const DataType typeDecl, bool doExecute, const bool canBeDefault = false, const uint8_t isID = 0);
    Value evaluateASTNode(const ASTNode& node);
    void extractReferences(const Value& value, std::vector<std::string>& references);

    std::string stripUnderscores(const std::string& str);
    #ifdef __SIZEOF_INT128__
        __int128 parseToInt128(const std::string& str);
        unsigned __int128 parseToUInt128(const std::string& str);
    #else
        long long parseToInt128(const std::string& str);
        unsigned long long parseToUInt128(const std::string& str);
    #endif
    #ifdef __SIZEOF_FLOAT128__
    __float128 parseToFloat128(const std::string& str);
    #endif

    Value numberToValue(double num);
    Value booleanToValue(bool b);
    Value linkToValue(const std::string& link);
    Value pathToValue(const std::string& path);
    Value hexToValue(const std::string& hexStr);
    Value binaryToValue(const std::string& binStr);
    Value octalToValue(const std::string& octStr);

    Value convertToDecimal(const Value& value);

    template<class Func>
    auto executeAsyncIfEnabled(Func&& func) -> std::future<decltype(func())> {
        typedef decltype(func()) ResultType;

        if (runAsync) {
    #ifdef __EMSCRIPTEN__
            return std::async(std::launch::deferred, std::forward<Func>(func));
    #else
            return std::async(std::launch::async, std::forward<Func>(func));
    #endif
        } else {
            ResultType result = func();
            return std::async(std::launch::deferred, [result]() { return result; });
        }
    }

    Value isolated(const std::string& code, bool doExecute, size_t startPos, const std::unordered_map<std::string, Value>* context = nullptr, const std::string name = "auto", bool merge = false, bool silent = false, const ParserType ptype = ParserType::SCRIPT);
    Value shared(const std::string& code, bool doExecute, size_t startPos, const std::unordered_map<std::string, Value>* context, const std::string name = "auto", bool merge = true, bool silent = false, const ParserType ptype = ParserType::SCRIPT);

    Value parseFunctionDeclaration(bool doExecute, std::string funcName = "anonymous", bool requireName = true);
    Value emptyJUSTC();

    Value parseCondition(bool doExecute, bool wasIsolated = false);
    Value i2v(Value fromIsolated);
    std::string t2i(ParserToken toIsolated);

    // built-in
    std::future<Value> functionHTTPAsync(size_t startPos, const std::string& method, const std::vector<Value>& args);
    std::future<Value> functionFILEAsync(const std::vector<Value>& args);
    Value functionVALUE(const std::vector<Value>& args);
    Value functionSTRING(const std::vector<Value>& args);
    Value functionLINK(const std::vector<Value>& args);
    Value functionBINARY(const std::vector<Value>& args);
    Value functionOCTAL(const std::vector<Value>& args);
    Value functionHEXADECIMAL(const std::vector<Value>& args);
    Value functionTYPEID(const std::vector<Value>& args);
    Value functionTYPEOF(const std::vector<Value>& args);
    Value functionECHO(const std::vector<Value>& args);
    Value functionJSON(const std::vector<Value>& args);
    Value functionHTTP(size_t startPos, const std::string& method, const std::vector<Value>& args);
    Value functionJUSTC(const std::vector<Value>& args, size_t startPos);
    Value functionJUSTC2(const std::string& code, bool doExecute, size_t startPos);
    Value functionFILE(const std::vector<Value>& args);
    Value functionSTAT(const std::vector<Value>& args);
    Value functionCONFIG(const std::vector<Value>& args);
    Value functionJUSTO(const std::vector<Value>& args);
    Value toJUSTO(const std::vector<Value>& args);

    Value onExecDisabled(size_t startPos, std::string name);
    void parseOutputCommandError(const std::string mode);
    void parseScopeCommandError(const std::string scope);
    void parseAllowCommandError();

    Value evaluateLengthOperator(const Value& value);
    Value evaluateNameOperator(const Value& value);
    Value evaluateAddressOfOperator(const Value& value);
    Value evaluateDereferenceOperator(const Value& value);

    static std::vector<double> values2numbers(const std::vector<Value>& values) {
        std::vector<double> result;
        result.reserve(values.size());

        for (size_t i = 0; i < values.size(); ++i) {
            const auto& value = values[i];
            switch (value.type) {
                case DataType::NUMBER:
                case DataType::HEXADECIMAL:
                case DataType::BINARY:
                case DataType::OCTAL:
                    result.push_back(value.number_value);
                    break;
                default:
                    throw std::runtime_error(
                        "Expected number at argument " + std::to_string(i) +
                        ", got <" + dataTypeToString(value.type) + ">"
                    );
                    break;
            }
        }
        return result;
    }

    std::vector<std::string> builtins;
    std::vector<std::string> cpptypes;
    std::vector<std::string> cppnumbers;
    void initializeBuiltIns();
    void initializeCPPTypes();
    bool isBuiltinVariable(const std::string& name) const;
    bool isCPPType();
    bool isCPPNumber(const std::string& cpptype);
    void handleBuiltinVariableAssignment(const std::string& name, const Value& value, size_t startPos);
    void removeBuiltinVariablesFromOutput();
    void builtinObject(const std::string& name, std::unordered_map<std::string, Value> props);
    Value builtinObjectFunction(const std::string& name);

    void updateCharType(const std::string& newType, size_t startPos);

    void triggerVariableUpdate(const std::string& name, const Value& value);
    Value merger(const std::vector<Value>& args);

    std::string getCurrentScopeName() const;
    uint64_t getCurrentScope() const { return currentScope; }
    uint64_t getRootScope() const { return rootIndex; }
    void enterScope();
    void exitScope();
    void setLocal(uint64_t scope, const std::string& name, const Value& value, bool isConst = false);
    Value getLocal(uint64_t scope, const std::string& name) const;
    bool hasLocal(uint64_t scope, const std::string& name) const;
    bool isLocalConst(uint64_t scope, const std::string& name) const;
    Value resolveVariableValueWithScopes(const std::string& varName, const bool unknownIsString);

    void assign(const Value& var, const Value& val, const std::string& pos = ".");
    bool isInBracketedExpression();

    Value parseJSXElement(const std::string& jsxStr);
    std::string renderJSX(const Value& jsxElement);

    Value parseStructDeclaration(bool doExecute, std::string structName = "anonymous", bool requireName = true);
    std::pair<bool, Value> isStruct(const std::string& name);
    void removeStructsFromOutput();
    Value getStructConstructor(const std::string& structID);

    Value updateObjectProperty(const std::vector<std::variant<std::string, size_t>>& accessChain, std::string accessChainStr);
    Value updateObjectPropertyRecursive(const Value& obj, const std::vector<PropertyPathNode>& pathNodes, size_t depth, const Value& newValue);

    void finalizeOutput();

    Value createClass(const Class& value, bool hasName = false, std::string className = "");
    Value parseClassDeclaration(const Value& value, bool doExecute, std::string className);
    std::pair<bool, Value> isClass(const std::string& name);
    void removeClassesFromOutput();
    uint64_t builtinClass(const std::string& name);
    void builtinClasses();
    Value addClass(const uint64_t& classID, bool hasName = false, std::string className = "");
    void fromBIM(const std::unordered_map<std::string, uint64_t>& BIM, const std::string& name);
    Value parseEnumDeclaration(const Value& value, bool doExecute, std::string enumName);

    std::string parseStringInterpolation(bool doExecute);
    std::string evaluateInterpolationExpression(const std::string& expr, bool doExecute);

public:
    static std::string getCurrentTimestamp();
    static Value stringToValue(const std::string& str);
    Parser(const std::vector<ParserToken>& tokens, bool doExecute = true, bool runAsync = false, const std::string& input = "", const bool allowJavaScript = true, const bool canAllowJS = true, const std::string scriptName = "", const std::string scriptType = "script", const bool allowLuau = true, const bool canAllowLuau = true, const bool isFunction = false, const std::unordered_map<std::string, Value>* initialContext = nullptr, const CharType chartype = CharType::GRAPHEME, const ParserType parsertype = ParserType::SCRIPT);
    ParseResult parse(bool doExecute = true);
    static ParseResult parseTokens(const std::vector<ParserToken>& tokens, bool doExecute = true, bool runAsync = false, const std::string& input = "", const bool allowJavaScript = true, const bool canAllowJS = true, const std::string scriptName = "", const std::string scriptType = "script", const bool allowLuau = true, const bool canAllowLuau = true);

    Value parseExpression(bool doExecute, bool identifierMode = false, bool doFunctionCall = true, bool ignoreColon = false);
    Value callFunction(const Value& function, const std::vector<Value>& args, size_t startPos, bool doExecute);
    void registerFunction(const std::string& name, Function func, bool isConst = true);
    void registerFunctions(const std::unordered_map<std::string, Function>& functions, bool isConst = true);
    void unregisterFunction(const std::string& name);
    bool hasFunction(const std::string& name) const;
    void clearUserFunctions();

    void variableUpdateListener(Function func);

    void registerGlobal(const std::string& name, const Value& value, bool isConst = true, bool isJUSTC = false);
    Value getGlobal(const std::string& name);
    bool hasGlobal(const std::string& name);
    void unregisterGlobal(const std::string& name);
    void clearGlobals();

    Value ParseJUSTO(const std::string& code);

    Value p2v(const Value::Property& property, const Access& requestAccess = Access::READ_ONLY);
    Value::Property v2p(const Value& value, const Access& access = Access::READ_WRITE, const Value& getter = Value::createNull(), const Value& setter = Value::createNull());
    Value v(const Value& value, const Access& requestAccess = Access::READ_ONLY);
    Value v(const Value::Property& value, const Access& requestAccess = Access::READ_ONLY);
    Value::Property p(const Value& value);
    Value::Property p(const Value::Property& value);
    std::unordered_map<std::string, Value> vmap(const std::unordered_map<std::string, Value>& props);
    std::unordered_map<std::string, Value> vmap(const std::unordered_map<std::string, Value::Property>& props);
    std::unordered_map<std::string, Value::Property> pmap(const std::unordered_map<std::string, Value>& values);
    std::unordered_map<std::string, Value::Property> pmap(const std::unordered_map<std::string, Value::Property>& values);
    std::pair<Value, Value::Property> vp(const Value& value, const Access& requestAccess);
    std::pair<Value, Value::Property> vp(const Value::Property& value, const Access& requestAccess);

    uint64_t registerClass(const Class& value);
    Class getClass(const uint64_t& classID);
    void unregisterClass(const uint64_t& classID);
    void clearClasses();

    uint64_t registerUserFunction(std::function<Value(const std::vector<Value>&)> func);
    std::function<Value(const std::vector<Value>&)> getUserFunction(uint64_t id);
    void unregisterUserFunction(uint64_t id);
    void clearUserFunctions_();
    Value createFunction(std::function<Value(const std::vector<Value>&)> func, const std::string& name = "");
    bool isFunctionValue(const Value& value) const;
    bool isFunctionId(const std::string& name) const;
    uint64_t extractFunctionId(const std::string& name) const;

    uint64_t setPointer(const Value& value);
    Value getPointer(const uint64_t& pointer);
    void freePointer(const uint64_t& pointer);
    void clearPointers();
    bool hasPointer(const uint64_t& pointer);
    uint64_t setPointer(const uint64_t& pointer, const Value& value);
    Value makePointer(const Value& value);
    void updatePointer(const Value& pointer, const Value& value);
    Value getPointer(const Value& pointer);
    void freePointer(const Value& pointer);

    void cleanup();

    bool throwError;
    
    Value This();
};

#endif
