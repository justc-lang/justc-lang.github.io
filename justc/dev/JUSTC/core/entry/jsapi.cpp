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

#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include "../lexer.h"
#include "../parser.h"
#include "../json.hpp"
#include "../fetch.h"
#include "../version.h"
#include <tuple>
#include "../justo.hpp"
#include <functional>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include "../compiler/justb.hpp"
#include "../loader/justb.hpp"
#include <cstdint>
#include "../lang/luau.hpp"
#include "../built-in/compression/compression.hpp"
#include <stdexcept>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(void, console_error, (const char* what), {
    console.error('[JUSTC]', UTF8ToString(what));
});

#include <cmath>
extern "C" __attribute__((used)) long double scalbnl(long double x, int n) {
    return static_cast<long double>(std::scalbn(static_cast<double>(x), n));
}

#endif

std::string outputString(const std::string& mode, const ParseResult& result) {
    if (mode == "xml") {
        return XmlSerializer::serialize(result);
    } else if (mode == "yaml") {
        return YamlSerializer::serialize(result);
    } else if (mode == "justo") {
        return JUSTOSerializer::serialize(result);
    } else if (mode == "justb") {
        if (!result.error.empty()) {
            return "{\"error\":\"" + JsonSerializer::escapeJsonString(result.error) + "\"}";
        } else {
            std::stringstream ss;
            JustbCompiler::compile(result, ss);
            
            std::stringstream json;
            json << "{";

            json << "\"type\":\"justb\",\"return\":\"" << JsonSerializer::escapeJsonString(ss.str()) << "\",";
            json << "\"logs\":" << JsonSerializer::serialize(result.logs) << ",";

            // logfile object
            json << "\"logfile\":{";
            json << "\"file\":\"" << JsonSerializer::escapeJsonString(result.logFilePath) << "\",";
            json << "\"logs\":\"" << JsonSerializer::escapeJsonString(result.logFileContent) << "\"";
            json << "},";

            // import logs array
            json << "\"imported\":";
            json << JsonSerializer::serialize(result.importLogs);

            json << "}";
            return json.str();
        }
    } else {
        return JsonSerializer::serialize(result);
    }
}
std::string outputString(const std::string& mode, const std::vector<ParserToken>& tokens, const std::string& input) {
    if (mode == "xml") {
        return XmlSerializer::serialize(tokens, input);
    } else if (mode == "yaml") {
        return YamlSerializer::serialize(tokens, input);
    } else if (mode == "justo") {
        return JUSTOSerializer::serialize(tokens, input);
    } else {
        return JsonSerializer::serialize(tokens, input);
    }
}

static std::unique_ptr<Parser> globalParser = nullptr;
static std::mutex globalParserMutex;

static std::unordered_map<std::string, Value> justoPointers;
static std::mutex justoPointersMutex;

static std::vector<std::function<void(const std::string&, const Value&)>> varUpdateListeners;
static std::mutex varUpdateListenersMutex;

static std::unordered_map<std::string, int> jsFunctions;
static std::mutex jsFunctionsMutex;

void ensureGlobalParser() {
    if (!globalParser) {
        globalParser = std::make_unique<Parser>(
            std::vector<ParserToken>(), true, false, "", true, true,
            "global", "global", true, true, false, nullptr, CharType::GRAPHEME
        );
    }
}

void initializeJUSTOPointers() {
    std::lock_guard<std::mutex> lock(justoPointersMutex);

    Value nanVal;
    nanVal.type = DataType::NOT_A_NUMBER;
    nanVal.name = "NaN";
    justoPointers["nan"] = nanVal;

    Value infVal;
    infVal.type = DataType::INFINITE;
    infVal.name = "Infinity";
    justoPointers["inf"] = infVal;
}

Value justoToValue(const std::string& justo) {
    JUSTO::JUSTOParser parser;
    for (const auto& [key, value] : justoPointers) {
        parser.registerPointer(key, value);
    }
    return parser.parse(justo);
}

std::string argsToJUSTOArray(const std::vector<Value>& args) {
    std::string result = "a[";
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) result += ",";
        result += JUSTO::valueToJUSTO(args[i]);
    }
    result += "]";
    return result;
}

void triggerVariableUpdate(const std::string& name, const Value& value) {
    std::lock_guard<std::mutex> lock(varUpdateListenersMutex);
    for (const auto& listener : varUpdateListeners) {
        try {
            listener(name, value);
        } catch (const std::exception& e) {}
    }
}

class Base64 {
public:
    static std::string encode(const std::vector<uint8_t>& data) {
        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        
        std::string result;
        result.reserve(((data.size() + 2) / 3) * 4);
        
        size_t i = 0;
        while (i < data.size()) {
            uint32_t n = 0;
            size_t bytes = 0;
            
            for (size_t j = 0; j < 3 && i + j < data.size(); ++j) {
                n = (n << 8) | data[i + j];
                bytes++;
            }
            i += bytes;
            
            n <<= (3 - bytes) * 8;
            
            result.push_back(chars[(n >> 18) & 0x3F]);
            result.push_back(chars[(n >> 12) & 0x3F]);
            
            if (bytes >= 2) {
                result.push_back(chars[(n >> 6) & 0x3F]);
            } else {
                result.push_back('=');
            }
            
            if (bytes >= 3) {
                result.push_back(chars[n & 0x3F]);
            } else {
                result.push_back('=');
            }
        }
        
        return result;
    }
    
    static std::vector<uint8_t> decode(const std::string& base64) {
        static const uint8_t decode_table[256] = {
            0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
            0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
            0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x3E,0x40,0x40,0x40,0x3F,
            0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x40,0x40,0x40,0x40,0x40,0x40,
            0x40,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,
            0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x40,0x40,0x40,0x40,0x40,
            0x40,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
            0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x40,0x40,0x40,0x40,0x40,
            0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
            0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
            0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
            0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
            0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
            0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
            0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
            0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40
        };
        
        std::string clean;
        clean.reserve(base64.size());
        for (char c : base64) {
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                clean.push_back(c);
            }
        }
        
        if (clean.empty()) {
            return {};
        }
        
        if (clean.size() % 4 != 0) {
            throw std::runtime_error("Invalid Base64 length");
        }
        
        for (char c : clean) {
            if (c != '=' && decode_table[static_cast<unsigned char>(c)] == 0x40) {
                throw std::runtime_error("Invalid Base64 character");
            }
        }
        
        std::vector<uint8_t> result;
        result.reserve((clean.size() / 4) * 3);
        
        for (size_t i = 0; i < clean.size(); i += 4) {
            uint32_t n = 0;
            size_t padding = 0;
            
            for (size_t j = 0; j < 4; ++j) {
                char c = clean[i + j];
                if (c == '=') {
                    padding++;
                } else {
                    uint8_t val = decode_table[static_cast<unsigned char>(c)];
                    n = (n << 6) | val;
                }
            }
            
            n <<= (padding * 6);
            
            result.push_back((n >> 16) & 0xFF);
            if (padding < 2) {
                result.push_back((n >> 8) & 0xFF);
            }
            if (padding < 1) {
                result.push_back(n & 0xFF);
            }
        }
        
        return result;
    }
    
    static std::string encode_string(const std::string& data) {
        return encode(std::vector<uint8_t>(data.begin(), data.end()));
    }
    
    static std::string decode_to_string(const std::string& base64) {
        auto bytes = decode(base64);
        return std::string(bytes.begin(), bytes.end());
    }
    
    static char* encode_to_char(const char* data, size_t len) {
        std::vector<uint8_t> bytes(data, data + len);
        std::string encoded = encode(bytes);
        char* result = static_cast<char*>(malloc(encoded.size() + 1));
        if (result) {
            std::memcpy(result, encoded.c_str(), encoded.size() + 1);
        }
        return result;
    }
    
    static char* decode_to_char(const char* base64, size_t len) {
        try {
            std::string input(base64, len);
            auto decoded = decode(input);
            char* result = static_cast<char*>(malloc(decoded.size() + 1));
            if (result) {
                std::memcpy(result, decoded.data(), decoded.size());
                result[decoded.size()] = '\0';
            }
            return result;
        } catch (const std::exception&) {
            return nullptr;
        }
    }
};

std::vector<uint8_t> c2v(const char* c) {
    return Base64::decode(std::string(c));
}
char* v2c(std::vector<uint8_t>& v) {
    thread_local std::string cached;
    cached = Base64::encode(v);
    return const_cast<char*>(cached.c_str());
}
const char* v2c(const std::vector<uint8_t>& v) {
    thread_local std::string cached;
    cached = Base64::encode(v);
    return cached.c_str();
}

#ifdef __EMSCRIPTEN__
extern "C" {
    extern void jsCallFunction(const char* name, const char* argsJUSTO, char** resultJUSTO);
}
#endif

extern "C" {

char* lexer(const char* input, const char* outputMode) {
    if (input == nullptr) return nullptr;
    std::string mode(outputMode == nullptr || outputMode == "justb" ? "justo" : outputMode);

    try {
        auto parsed = Lexer::parse(input, true);
        std::string json = outputString(mode, parsed.second, parsed.first);
        return strdup(json.c_str());

    } catch (const std::exception& e) {
        std::string error = "{\"error\":\"" + JsonSerializer::escapeJsonString(std::string(e.what())) + "\",\"lexer\":true}";
        return strdup(error.c_str());
    }
}

char* parser(const char* tokensJUSTO, const char* outputMode) {
    if (tokensJUSTO == nullptr) return nullptr;
    std::string mode(outputMode == nullptr ? "json" : outputMode);

    try {
        std::vector<ParserToken> parserTokens;
        std::string input = "";

        if (JUSTO_Parser::parseJUSTOTokens(tokensJUSTO, parserTokens, input)) {
            ParseResult result = Parser::parseTokens(parserTokens, false, false, input);
            std::string json = outputString(mode, result);
            return strdup(json.c_str());
        } else {
            std::string error = "{\"error\":\"Failed to parse JUSTO\"}";
            return strdup(error.c_str());
        }

    } catch (const std::exception& e) {
        std::string error = "{\"error\":\"" + JsonSerializer::escapeJsonString(std::string(e.what())) + "\",\"parser\":true}";
        return strdup(error.c_str());
    }
}

char* parse(const char* input, const bool execute, const bool runAsync, const char* outputMode) {
    if (input == nullptr) return nullptr;
    std::string mode(outputMode == nullptr ? "json" : outputMode);

    try {
        auto lexerResult = Lexer::parse(input);
        ParseResult result = Parser::parseTokens(lexerResult.second, execute, runAsync, input);

        if (result.variables) {
            for (const auto& [key, value] : *result.variables) {
                if (globalParser && globalParser->hasGlobal(key)) {
                    Value oldVal = globalParser->getGlobal(key);
                    if (oldVal.toString() != value.toString()) {
                        globalParser->registerGlobal(key, value);
                        triggerVariableUpdate(key, value);
                    }
                }
            }
        }

        std::string json = outputString(mode, result);
        return strdup(json.c_str());

    } catch (const std::exception& e) {
        std::string error = "{\"error\":\"" + JsonSerializer::escapeJsonString(std::string(e.what())) + "\"}";
        return strdup(error.c_str());
    }
}

void free_string(char* str) {
    if (str != nullptr) {
        free(str);
    }
}

char* version() {
    return strdup(JUSTC_VERSION.c_str());
}

int registerGlobal(const char* name, const char* justoValue) {
    if (name == nullptr || justoValue == nullptr) return 0;

    std::lock_guard<std::mutex> lock(globalParserMutex);
    ensureGlobalParser();

    try {
        Value val = justoToValue(std::string(justoValue));
        globalParser->registerGlobal(std::string(name), val);
        triggerVariableUpdate(std::string(name), val);
        return 1;
    } catch (const std::exception& e) {
        return 0;
    }
}

char* getGlobal(const char* name) {
    if (name == nullptr) return strdup(";");

    std::lock_guard<std::mutex> lock(globalParserMutex);
    ensureGlobalParser();

    try {
        Value val = globalParser->getGlobal(std::string(name));
        std::string justo = JUSTO::valueToJUSTO(val);
        return strdup(justo.c_str());
    } catch (const std::exception& e) {
        return strdup(";");
    }
}

int hasGlobal(const char* name) {
    if (name == nullptr) return 0;

    std::lock_guard<std::mutex> lock(globalParserMutex);
    ensureGlobalParser();

    return globalParser->hasGlobal(std::string(name)) ? 1 : 0;
}

int unregisterGlobal(const char* name) {
    if (name == nullptr) return 0;

    std::lock_guard<std::mutex> lock(globalParserMutex);
    ensureGlobalParser();

    globalParser->unregisterGlobal(std::string(name));
    triggerVariableUpdate(std::string(name), Value::createNull());
    return 1;
}

void clearGlobals() {
    std::lock_guard<std::mutex> lock(globalParserMutex);
    ensureGlobalParser();
    globalParser->clearGlobals();
}

int registerPointer(const char* name, const char* justoValue) {
    if (name == nullptr || justoValue == nullptr) return 0;

    std::lock_guard<std::mutex> lock(justoPointersMutex);

    try {
        JUSTO::JUSTOParser parser;
        for (const auto& [key, value] : justoPointers) {
            parser.registerPointer(key, value);
        }
        Value val = parser.parse(std::string(justoValue));
        justoPointers[std::string(name)] = val;
        return 1;
    } catch (const std::exception& e) {
        return 0;
    }
}

char* getPointer(const char* name) {
    if (name == nullptr) return strdup(";");

    std::lock_guard<std::mutex> lock(justoPointersMutex);

    auto it = justoPointers.find(std::string(name));
    if (it != justoPointers.end()) {
        std::string justo = JUSTO::valueToJUSTO(it->second);
        return strdup(justo.c_str());
    }
    return strdup(";");
}

int unregisterPointer(const char* name) {
    if (name == nullptr) return 0;

    std::lock_guard<std::mutex> lock(justoPointersMutex);
    justoPointers.erase(std::string(name));
    return 1;
}

void clearPointers() {
    std::lock_guard<std::mutex> lock(justoPointersMutex);
    justoPointers.clear();
    initializeJUSTOPointers();
}

int registerFunction(const char* name, int jsCallbackPtr, int isConst) {
    if (name == nullptr || jsCallbackPtr == 0) return 0;

    std::lock_guard<std::mutex> lock(globalParserMutex);
    std::lock_guard<std::mutex> lock2(jsFunctionsMutex);
    ensureGlobalParser();

    try {
        jsFunctions[std::string(name)] = jsCallbackPtr;

        globalParser->registerFunction(std::string(name),
            [name, jsCallbackPtr](const std::vector<Value>& args) -> Value {
                std::string argsJUSTO = argsToJUSTOArray(args);

                char* resultJUSTO = nullptr;

                #ifdef __EMSCRIPTEN__
                    using JSFunc = void (*)(const char*, const char*, char**);
                    JSFunc jsFunc = reinterpret_cast<JSFunc>(jsCallbackPtr);
                    jsFunc(name, argsJUSTO.c_str(), &resultJUSTO);
                #else
                    resultJUSTO = strdup(";");
                #endif

                Value result = justoToValue(std::string(resultJUSTO));
                free(resultJUSTO);

                return result;
            },
            isConst != 0
        );
        return 1;
    } catch (const std::exception& e) {
        return 0;
    }
}

int unregisterFunction(const char* name) {
    if (name == nullptr) return 0;

    std::lock_guard<std::mutex> lock(globalParserMutex);
    std::lock_guard<std::mutex> lock2(jsFunctionsMutex);
    ensureGlobalParser();

    globalParser->unregisterFunction(std::string(name));
    jsFunctions.erase(std::string(name));
    return 1;
}

void clearUserFunctions() {
    std::lock_guard<std::mutex> lock(globalParserMutex);
    std::lock_guard<std::mutex> lock2(jsFunctionsMutex);
    ensureGlobalParser();
    globalParser->clearUserFunctions();
    jsFunctions.clear();
}

int addVariableUpdateListener(void (*callback)(const char* name, const char* valueJUSTO)) {
    if (callback == nullptr) return 0;

    std::lock_guard<std::mutex> lock(varUpdateListenersMutex);

    varUpdateListeners.push_back([callback](const std::string& name, const Value& value) {
        std::string justo = JUSTO::valueToJUSTO(value);
        callback(name.c_str(), justo.c_str());
    });

    return 1;
}

void clearVariableUpdateListeners() {
    std::lock_guard<std::mutex> lock(varUpdateListenersMutex);
    varUpdateListeners.clear();
}

char* justoParse(const char* justoString) {
    if (justoString == nullptr) return strdup(";");

    try {
        Value val = justoToValue(std::string(justoString));
        std::string justo = JUSTO::valueToJUSTO(val);
        return strdup(justo.c_str());
    } catch (const std::exception& e) {
        return strdup(";");
    }
}

char* load(const unsigned char* bytes, size_t length, const char* outputMode) {
    if (bytes == nullptr) return nullptr;
    std::string mode(outputMode == nullptr ? "json" : outputMode);

    std::string data(reinterpret_cast<const char*>(bytes), length);
    std::stringstream ss(data);
    ParseResult result = JustbLoader::load(ss);

    std::string json = outputString(mode, result);
    return strdup(json.c_str());
}

int unregisterClass(const char* raw_classID_str) {
    if (raw_classID_str == nullptr) return 0;

    std::lock_guard<std::mutex> lock(globalParserMutex);
    ensureGlobalParser();

    std::string classID_str = std::string(raw_classID_str);
    unsigned long long raw_classID = std::stoull(classID_str);
    uint64_t classID = raw_classID;

    globalParser->unregisterClass(classID);

    return 1;
}

void clearClasses() {
    std::lock_guard<std::mutex> lock(globalParserMutex);
    ensureGlobalParser();
    globalParser->clearClasses();
}

int freePointer(const char* raw_pointer_str) {
    if (!raw_pointer_str) return 0;

    std::lock_guard<std::mutex> lock(globalParserMutex);
    ensureGlobalParser();

    std::string pointer_str = std::string(raw_pointer_str);
    unsigned long long raw_pointer = std::stoull(pointer_str);
    uint64_t pointer = raw_pointer;

    globalParser->freePointer(pointer);

    return 1;
}

void clearJUSTCPointers() {
    std::lock_guard<std::mutex> lock(globalParserMutex);
    ensureGlobalParser();
    globalParser->clearPointers();
}

void cleanup() {
    std::lock_guard<std::mutex> lock(globalParserMutex);
    ensureGlobalParser();
    globalParser->cleanup();
}

int has_luau() {
    #ifdef JUSTC_NOLUAU
        return 0;
    #else
        return 1;
    #endif
}

char* internal_luau_eval(const char* code) {
    try {
        std::pair<std::string, int> res = RunLuau::runScriptWithResult(code);
        std::stringstream ss;
        ss << res.second << res.first;
        std::string out = ss.str();
        return strdup(out.c_str());
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        std::stringstream ss;
        ss << "6" << e.what();
        std::string out = ss.str();
        return strdup(out.c_str());
    }
}

int internal_luau_comp(const char* code) {
    try {
        std::string err;
        bool res = RunLuau::compileScript(code, err);
        return res ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

char* internal_zlib_comp(const char* data, int level) {
    try {
        return strdup(v2c(ZLIB::CompressU8(c2v(data), level)));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_zlib_dcmp(const char* data) {
    try {
        return strdup(v2c(ZLIB::DecompressU8(c2v(data))));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_gzip_comp(const char* data, int level) {
    try {
        return strdup(v2c(GZIP::CompressU8(c2v(data), level)));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_gzip_dcmp(const char* data) {
    try {
        return strdup(v2c(GZIP::DecompressU8(c2v(data))));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_bzip2_comp(const char* data, int level) {
    try {
        return strdup(v2c(BZIP2::CompressU8(c2v(data), level)));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_bzip2_dcmp(const char* data) {
    try {
        return strdup(v2c(BZIP2::DecompressU8(c2v(data))));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_lzma_comp(const char* data, int level) {
    try {
        return strdup(v2c(LZMA::CompressU8(c2v(data), level)));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_lzma_dcmp(const char* data) {
    try {
        return strdup(v2c(LZMA::DecompressU8(c2v(data))));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_zstd_comp(const char* data, int level) {
    try {
        return strdup(v2c(ZSTD::CompressU8(c2v(data), level)));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_zstd_dcmp(const char* data) {
    try {
        return strdup(v2c(ZSTD::DecompressU8(c2v(data))));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_lz4_comp(const char* data, int level) {
    try {
        return strdup(v2c(LZ4::CompressU8(c2v(data), level)));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_lz4_dcmp(const char* data) {
    try {
        return strdup(v2c(LZ4::DecompressU8(c2v(data))));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_snappy_comp(const char* data, int level) {
    try {
        return strdup(v2c(SNAPPY::CompressU8(c2v(data), level)));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_snappy_dcmp(const char* data) {
    try {
        return strdup(v2c(SNAPPY::DecompressU8(c2v(data))));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_deflate_comp(const char* data, int level) {
    try {
        return strdup(v2c(DEFLATE::CompressU8(c2v(data), level)));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

char* internal_deflate_dcmp(const char* data) {
    try {
        return strdup(v2c(DEFLATE::DecompressU8(c2v(data))));
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            console_error(e.what());
        #endif
        return strdup(e.what());
    }
}

}

struct JUSTOInitializer {
    JUSTOInitializer() {
        initializeJUSTOPointers();
    }
} justoInitializer;
