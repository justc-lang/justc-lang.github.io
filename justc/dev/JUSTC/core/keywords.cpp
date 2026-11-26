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

#include "keywords.h"

/*
const std::vector<std::string> keywords = {
    "TYPE", "GLOBAL", "LOCAL", "STRICT", "JSON",
    "HTTPJSON", "HTTPTEXT", "HTTPJUSTC", "JUSTC",
    "IMPORT", "EXPORT", "EXPORTS", "REQUIRE", "ENV",
    "CONFIG", "RUN", "VALUE", "FILE", "SIZE", "STRING",
    "LINK", "STRINGNUM", "STRINGB64", "STRINGBIN",
    "STRINGHEX", "TYPEID", "TYPEOF", "OUTPUT", "RETURN",
    "V", "D", "SQ", "CU", "P", "M", "S", "C", "T", "N",
    "ECHO", "LOGFILE", "LOG", "PARSEJUSTC", "PARSEJSON",
    "SPECIFIED", "EVERYTHING", "DISABLED", "AS",
    "STRINGOCT", "NUMBER", "BINARY", "OCTAL", "BASE64",
    "HEXADECIMAL", "PARSEHOCON", "HTTPHOCON", "BASE32",
    "STRINGB32", "ABSOLUTE", "CEIL", "FLOOR", "ALLOW",
    "DISALLOW", "JAVASCRIPT", "SAFE", "LUAU", "CLASS"
};*/

const std::vector<std::string> keywords = {
    "type", "global", "local", "strict",
    "import", "export", "exports", "require",
    "run", "output", "return", "specified",
    "everything", "disabled", "as", "allow",
    "disallow", "JavaScript", "safe", "Luau",
    "class", "function", "public", "private",
    "static", "const", "define", "undefine",
    "echo", "log", "logfile", "space", "var",
    "new", "lgt", "goto", "isolated", "if",
    "for", "while", "lambda", "from", "options",
    "struct", "set", "to", "extends", "await",
    "async", "try", "catch", "finally", "enum",
    "throw", "this",

    // cpp type keywords
    "int8", "int16", "int32", "int64", "int128",
    "uint8", "uint16", "uint32", "uint64",
    "uint128", "float32", "float64", "float128",
    "cuint8", "cuint16", "cuint32", "cuint64",
    "float256", "float512", "float1024", "float16",
    "bfloat16",
};
