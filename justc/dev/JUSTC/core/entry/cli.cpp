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

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <tuple>
#include <algorithm>
#include "../lexer.h"
#include "../parser.h"
#include "../json.hpp"
#include "../version.h"
#include "../utility.h"
#include "../justo.hpp"
#include "../compiler/justb.hpp"
#include "../loader/justb.hpp"
#include "../justb.hpp"

void logError(const std::string& error) {
    if (Utility::isGitHubActions()) {
        std::cerr << "::error::" << error << std::endl;
    } else {
        std::cerr << "Error: " << error << std::endl;
    }
}

void logWarning(const std::string& warning) {
    if (Utility::isGitHubActions()) {
        std::cerr << "::warning::" << warning << std::endl;
    } else {
        std::cerr << "Warning: " << warning << std::endl;
    }
}

void throwError(const std::string& error) {
    logError(error);
    std::exit(1);
}

void setupGlobalExceptionHandler() {
    std::set_terminate([]() {
        logError("Terminate called due to uncaught exception");

        auto current = std::current_exception();
        if (current) {
            try {
                std::rethrow_exception(current);
            } catch (const std::exception& e) {
                logError(std::string("Exception: ") + e.what());
            } catch (...) {
                logError("Unknown exception type");
            }
        } else {
            logError("No current exception available");
        }

        std::abort();
    });
}
class OutputRedirector {
private:
    std::streambuf* originalCout;
    std::streambuf* originalCerr;
    std::stringstream silentBuffer;
    bool silent;
    bool redirectErrors;

public:
    OutputRedirector(bool silentMode, bool redirectErrors = false) : silent(silentMode), redirectErrors(redirectErrors) {
        if (silent) {
            originalCout = std::cout.rdbuf();
            std::cout.rdbuf(silentBuffer.rdbuf());

            if (redirectErrors) {
                originalCerr = std::cerr.rdbuf();
                std::cerr.rdbuf(silentBuffer.rdbuf());
            }
        }
    }

    ~OutputRedirector() {
        restore();
    }

    void restore() {
        if (silent) {
            std::cout.rdbuf(originalCout);
            if (redirectErrors) {
                std::cerr.rdbuf(originalCerr);
            }
        }
    }

    void outputError(const std::string& error) {
        if (silent) {
            restore();
            std::cerr << error << std::endl;
            std::cout.rdbuf(silentBuffer.rdbuf());
            if (redirectErrors) {
                std::cerr.rdbuf(silentBuffer.rdbuf());
            }
        } else {
            logError(std::string(error));
        }
    }
};

inline bool doPrint = true;

void printLicense() {
    if (doPrint) {
        doPrint = false;
        std::cout << R"(

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

)" << std::endl;
    }
}

void printVersion() {
    if (doPrint) {
        doPrint = false;
        std::cout << "The JUSTC Programming Language v" << JUSTC_VERSION << std::endl;
        std::cout << "Copyright (c) 2025-2026 JustStudio." << std::endl;
        std::cout << "MIT License." << std::endl;
        std::cout << "https://just.js.org/justc/" << std::endl;
    }
}

void printUsage() {
    if (doPrint) {
        doPrint = false;
        std::cout << R"(

The JUSTC Programming Language v)" << JUSTC_VERSION << R"(

https://just.js.org/justc/

Usage:
  justc interpret            [options] [ script.justc ]            [arguments]
  justc compile   [format]   [options] [ input.justc ]  [ output ] [arguments]
  justc evaluate             [options] [ script ]                  [arguments]
  justc execute              [options] [ input.justb ]
  justc serialize [format]   [options] [ input.justc ]  [ output ] [arguments]
  justc transpile [language] [options] [ input.justc ]  [ output ] [arguments]

  justc json->justc [options] [ input.json ]  [ output.justc ]
  justc json->justo [options] [ input.json ]  [ output.justo ]
  justc justb->justc [options] [ input.justb ] [ output.justc ]
  justc justc->js    [options] [ input.justc ] [ output.js ]    [arguments]
  justc justc->json  [options] [ input.justc ] [ output.json ]  [arguments]
  justc justc->justb [options] [ input.justc ] [ output.justb ] [arguments]
  justc justc->justo [options] [ input.justc ] [ output.justo ] [arguments]
  justc justc->xml   [options] [ input.justc ] [ output.xml ]   [arguments]
  justc justc->yaml  [options] [ input.justc ] [ output.yaml ]  [arguments]
  justc justo->json  [options] [ input.justo ] [ output.json ]
  justc justo->justc [options] [ input.justo ] [ output.justc ]

Options:
  --                                    Indicate the end of JUSTC options
  --async-evaluation                    Asynchronous variable evaluation
  -c, --check                           Validate JUSTC/JUSTO/JUSTB input
  --disallow-javascript                 Disallow JavaScript
  --disallow-luau                       Disallow Luau
  -h, --help                            Print JUSTC command line options
  --license                             Print JUSTC license
  --no-compression                      Disable compiler output compression
  -p, --print                           Print the result
  --raw-version                         Print JUSTC version in "x.y.z" format
  -s, --silent                          Suppress all logs except errors
  -v, --version                         Print JUSTC version

Available formats to compile JUSTC to:
  justb

Available formats to serialize JUSTC to:
  json, justo, xml, yaml

Available languages to transpile JUSTC to:
  js, lua

JS    - JavaScript
JSON  - JavaScript Object Notation
JUSTB - JUSTC's Universal Structuren't Temporary Bytecode
JUSTC - JUSTC's Universal Serialization, Transpilation & Compilation
JUSTO - JUSTC's Universal Simple Text Object
XML   - eXtensible Markup Language
YAML  - YAML Ain't Markup Language

Documentation can be found at https://just.js.org/justc/
Made for Just an Ultimate Site Tool ( https://just.js.org/ )

JUSTC, JUSTB and JUSTO,
licensed under the MIT License.
Copyright (c) 2025-2026 JustStudio.
(The dot in "JustStudio." is part of the name.)
https://just.js.org/justc
https://juststudio.is-a.dev

Dependencies:
  JUSTC uses C++ as its implementation language.
  The entire project requires C++17.
  JUSTC incorporates code from the following open-source projects:

  Core Dependencies (Native Builds - Linux/macOS/Windows):
    - Cereal by iLab @ USC - Licensed under the BSD-3-Clause License:
      Copyright (c) 2013-2022, Randolph Voorhies, Shane Grant.
      https://github.com/USCiLab/cereal/blob/master/LICENSE
    - CPR by libcpr - Licensed under the MIT License:
      Copyright (c) 2017-2021 Huu Nguyen.
      Copyright (c) 2022 libcpr and many other contributors.
      https://github.com/libcpr/cpr/blob/master/LICENSE
    - ICU by Unicode - Licensed under the UNICODE LICENSE V3:
      Copyright (c) 2016-2025 Unicode, Inc.
      https://github.com/unicode-org/icu/blob/main/LICENSE
    - Luau by Roblox - Licensed under the MIT License:
      Copyright (c) 2019-2025 Roblox Corporation.
      Copyright (c) 1994-2019 Lua.org, PUC-Rio.
      https://github.com/luau-lang/luau/blob/master/LICENSE.txt
    - nlohmann/json by Niels Lohmann - Licensed under the MIT License:
      Copyright (c) 2013-2026 Niels Lohmann.
      https://github.com/nlohmann/json/blob/develop/LICENSE.MIT
    - QuickJS by bellard - Licensed under the MIT License:
      Copyright (c) 2017-2021 Fabrice Bellard.
      Copyright (c) 2017-2021 Charlie Gordon.
      https://github.com/bellard/quickjs/blob/master/LICENSE
    - QuickJS CMake by Rob Loach - Licensed under the MIT License:
      Copyright (c) 2022 Rob Loach.
      https://github.com/RobLoach/quickjs-cmake/blob/master/LICENSE

  WebAssembly:
    Does not use CPR (HTTP Requests support via XHR API)
    Does not use nlohmann/json
    Does not use QuickJS and QuickJS CMake
    Does not use ICU (Unicode support via Intl API)

  Reference / Inspiration:
    - Lua by PUC-Rio - Licensed under the MIT License:
      Copyright (c) 1994-2025 Lua.org, PUC-Rio.
      https://www.lua.org/license.html
      (Luau is a fork of Lua)

Acknowledgements:
  - JavaScript is a trademark of Oracle Corporation.
    For more information, see https://www.oracle.com/legal/trademarks/
  - Special thanks to the authors and maintainers of:
    Cereal, CPR, ICU, Lua, Luau, nlohmann/json, QuickJS and QuickJS CMake
    for their excellent work, which made JUSTC possible.

)" << std::endl;
    }
}

std::string readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throwError("Unable to read the file: " + filename);
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void writeFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throwError("Unable to write the file: " + filename);
    }
    file << content;
    if (!file.good()) {
        throwError("Error occurred while writing to file: " + filename);
    }
}

struct CommandLineFlags {
    bool silent = false;
    bool print = false;
    bool check = false;
    bool async = false;
    bool endOfOptions = false;

    bool allowJS = true;
    bool allowLuau = true;

    bool noCompression = false;

    bool hasInp = false;

    std::string command;
    std::string format;
    std::string language;
    std::string input;
    std::string output;

    std::vector<std::string> arguments;
};

CommandLineFlags parseArguments(int argc, char* argv[]) {
    CommandLineFlags flags;

    if (argc < 2) {
        printUsage();
        return flags;
    }

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    size_t i = 0;
    bool commandParsed = false;
    int wait = 0; // 1 = compile format; 2 = serialize format; 3 = transpile lang

    while (i < args.size()) {
        const std::string& arg = args[i];

        if (wait == 1 || wait == 2) {
            flags.format = arg;
            ++i;
            wait = 0;
            continue;
        } else if (wait == 3) {
            flags.language = arg;
            ++i;
            wait = 0;
            continue;
        }

        if (arg == "--") {
            flags.endOfOptions = true;
            ++i;
            while (i < args.size()) {
                flags.arguments.push_back(args[i]);
                ++i;
            }
            break;
        }

        if (arg == "-h" || arg == "--help") {
            printUsage();
            flags.hasInp = true;
            ++i;
        } else if (arg == "-v" || arg == "--version") {
            printVersion();
            flags.hasInp = true;
            ++i;
        } else if (arg == "--license") {
            printLicense();
            flags.hasInp = true;
            ++i;
        } else if (arg == "--raw-version") {
            std::cout << JUSTC_VERSION << std::endl;
            flags.hasInp = true;
            ++i;
        } else if (arg == "-s" || arg == "--silent") {
            flags.silent = true;
            ++i;
        } else if (arg == "-p" || arg == "--print") {
            flags.print = true;
            ++i;
        } else if (arg == "-c" || arg == "--check") {
            flags.check = true;
            ++i;
        } else if (arg == "--async-evaluation") {
            flags.async = true;
            ++i;
        } else if (arg == "--disallow-javascript") {
            flags.allowJS = false;
            ++i;
        } else if (arg == "--disallow-luau") {
            flags.allowLuau = false;
            ++i;
        } else if (arg == "--no-compression") {
            flags.noCompression = true;
            ++i;
        } else if (arg[0] == '-') {
            throwError("Unknown option: " + arg);
        } else {
            if (!commandParsed) {
                flags.command = arg;
                commandParsed = true;
                if (arg == "compile") {
                    wait = 1;
                } else if (arg == "serialize") {
                    wait = 2;
                } else if (arg == "transpile") {
                    wait = 3;
                }
                ++i;
            } else if (flags.input.empty()) {
                flags.input = arg;
                ++i;
            } else if (flags.output.empty()) {
                flags.output = arg;
                ++i;
            } else {
                flags.arguments.push_back(arg);
                ++i;
            }
        }
    }

    if (wait != 0) {
        printUsage();
        return flags;
    }

    return flags;
}

std::string getOutputFormat(const std::string& format) {
    if (format == "json" || format == "justo" || format == "xml" || format == "yaml") {
        return format;
    }
    return "json";
}

std::string serializeResult(const ParseResult& result, const std::string& format) {
    if (format == "xml") {
        return XmlSerializer::serialize(result);
    } else if (format == "yaml") {
        return YamlSerializer::serialize(result);
    } else if (format == "justo") {
        return JUSTOSerializer::serialize(result);
    } else {
        return JsonSerializer::serialize(result);
    }
}

auto lexer(std::string code) {
    try {
        return Lexer::parse(code);
    } catch (const std::exception& e) {
        throwError(std::string(e.what()) + "\n\nJUSTC v" + JUSTC_VERSION);
    }
}

void handleExecute(const CommandLineFlags& flags) {
    if (flags.input.empty()) {
        throwError("No input file specified for execution");
    }

    std::string code = readFile(flags.input);
    auto lexerResult = lexer(code);
    ParseResult result = Parser::parseTokens(lexerResult.second, true, flags.async, code, flags.allowJS, flags.allowJS, flags.input, "script", flags.allowLuau, flags.allowLuau);

    if (!result.error.empty()) {
        throwError(result.error);
    }

    if (flags.print) {
        std::cout << serializeResult(result, "json") << std::endl;
    }
}

void handleSerialize(const CommandLineFlags& flags) {
    if (flags.input.empty()) {
        throwError("No input file specified for serialization");
    }

    std::string format = getOutputFormat(flags.format);
    std::string code = readFile(flags.input);
    auto lexerResult = lexer(code);
    ParseResult result = Parser::parseTokens(lexerResult.second, false, flags.async, code, flags.allowJS, flags.allowJS, flags.input, "script", flags.allowLuau, flags.allowLuau);

    if (!result.error.empty()) {
        throwError(result.error);
    }

    std::string serialized = serializeResult(result, format);

    if (!flags.output.empty()) {
        writeFile(flags.output, serialized);
    } else {
        std::cout << serialized << std::endl;
    }
}

void handleCompileJustb(const CommandLineFlags& flags) {
    if (flags.input.empty()) {
        throwError("No input file specified for compilation");
    }

    std::string code = readFile(flags.input);
    auto lexerResult = lexer(code);
    ParseResult result = Parser::parseTokens(lexerResult.second, true, flags.async, code, flags.allowJS, flags.allowJS, flags.input, "script", flags.allowLuau, flags.allowLuau);

    if (!result.error.empty()) {
        throwError(result.error);
    }

    std::stringstream ss;
    if (!JustbCompiler::compile(result, ss)) {
        throwError("Failed to compile to JUSTB");
    }

    if (!flags.output.empty()) {
        writeFile(flags.output, ss.str());
    } else {
        logWarning("No output file specified for JUSTB compilation");
    }
}

void handleExecuteJustb(const CommandLineFlags& flags) {
    if (flags.input.empty()) {
        throwError("No input file specified for JUSTB execution");
    }

    std::string data = readFile(flags.input);
    std::stringstream ss(data);
    ParseResult result = JustbLoader::load(ss);

    if (!result.error.empty()) {
        throwError(result.error);
    }

    if (flags.print) {
        std::cout << JsonSerializer::serialize(result) << std::endl;
    }
}

void handleCheck(const CommandLineFlags& flags) {
    if (flags.input.empty()) {
        throwError("No input file specified for validation");
    }

    std::string ext = flags.input.substr(flags.input.find_last_of('.') + 1);

    if (ext == "justc") {
        std::string code = readFile(flags.input);
        auto lexerResult = lexer(code);
        ParseResult result = Parser::parseTokens(lexerResult.second, false, flags.async, code, flags.allowJS, flags.allowJS, flags.input, "script", flags.allowLuau, flags.allowLuau);

        if (!result.error.empty()) {
            throwError(result.error);
        }
    } else if (ext == "justb") {
        std::string data = readFile(flags.input);
        std::stringstream ss(data);
        JUSTB::Header header;
        if (!JUSTB::readHeader(ss, header) || !JUSTB::validateHeader(header)) {
            throwError("Invalid JUSTB file: " + flags.input);
        }
    } else if (ext != "justo") {
        throwError("Unsupported file type for validation: " + ext);
    }
}

void handleJsonToJustc(const CommandLineFlags& flags) {
    if (flags.input.empty()) {
        throwError("No input file specified for JSON to JUSTC conversion");
    }

    std::string json = readFile(flags.input);
    std::string justc = JsonParser::stringify(json);
    
    if (!flags.output.empty()) {
        writeFile(flags.output, justc);
    }

    if (flags.print) {
        std::cout << justc << std::endl;
    }
}

void handelJsonToJusto(const CommandLineFlags& flags) {
    if (flags.input.empty()) {
        throwError("No input file specified for JSON to JUSTO conversion");
    }

    std::string json = readFile(flags.input);
    std::string justc = JsonParser::stringify(json);
    auto lexerResult = lexer(justc);
    ParseResult result = Parser::parseTokens(lexerResult.second, false, flags.async, justc, flags.allowJS, flags.allowJS, flags.input, "script", flags.allowLuau, flags.allowLuau);
    std::string justo = serializeResult(result, "justo");

    if (!flags.output.empty()) {
        writeFile(flags.output, justo);
    }

    if (flags.print) {
        std::cout << justo << std::endl;
    }
}

int main(int argc, char* argv[]) {
    setupGlobalExceptionHandler();
    OutputRedirector* outputRedirector = nullptr;

    try {
        CommandLineFlags flags = parseArguments(argc, argv);

        if (flags.silent) {
            outputRedirector = new OutputRedirector(true, false);
        }

        if (flags.input.empty() && !flags.hasInp) {
            printUsage();
            return 1;
        }

        if (flags.command == "interpret") {
            handleExecute(flags);
        } else if (flags.command == "execute") {
            handleExecuteJustb(flags);
        } else if (flags.command == "serialize") {
            handleSerialize(flags);
        } else if (flags.command == "compile") {
            if (flags.format == "justb") {
                handleCompileJustb(flags);
            } else {
                throwError("Unknown compile format: " + flags.format);
            }
        } else if (flags.command == "evaluate") {
            std::string code = flags.input;
            auto lexerResult = lexer(code);
            ParseResult result = Parser::parseTokens(lexerResult.second, true, flags.async, code, flags.allowJS, flags.allowJS, "<eval>", "script", flags.allowLuau, flags.allowLuau);

            if (!result.error.empty()) {
                throwError(result.error);
            }

            if (flags.print) {
                std::cout << JsonSerializer::serialize(result) << std::endl;
            }
        } else if (flags.command == "check" || flags.check) {
            handleCheck(flags);
        } else if (flags.command == "json->justc") {
            handleJsonToJustc(flags);
        } else if (flags.command == "json->justo") {
            handelJsonToJusto(flags);
        } else if (
            flags.command == "justc->json" ||
            flags.command == "justc->justo" ||
            flags.command == "justc->xml" ||
            flags.command == "justc->yaml"
        ) {
            size_t pos = flags.command.find("->");
            if (pos != std::string::npos) {
                std::string format = flags.command.substr(pos + 2);
                flags.format = format;
                handleSerialize(flags);
            } else {
                throwError("Invalid command format");
            }
        } else {
            if (!flags.input.empty()) {
                handleExecute(flags);
            } else if (!flags.hasInp) {
                printUsage();
                return 1;
            }
        }

        if (outputRedirector) {
            outputRedirector->restore();
            delete outputRedirector;
        }

    } catch (const std::exception& e) {
        logError(e.what());
        return 1;
    } catch (...) {
        logError("Unknown fatal error");
        return 1;
    }
    
    return 0;
}
