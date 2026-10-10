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

/*

Just an Ultimate Site Tool Configuration language v0.3.0 (dev)

*/

console.log("\n\nThe JUSTC Programming Language v0.3.0\n\nhttps://just.js.org/justc/\n\nUsage:\n  justc interpret            [options] [ script.justc ]            [arguments]\n  justc compile   [format]   [options] [ input.justc ]  [ output ] [arguments]\n  justc evaluate             [options] [ script ]                  [arguments]\n  justc execute              [options] [ input.justb ]\n  justc serialize [format]   [options] [ input.justc ]  [ output ] [arguments]\n  justc transpile [language] [options] [ input.justc ]  [ output ] [arguments]\n\n  justc json->justc [options] [ input.json ]  [ output.justc ]\n  justc json->justo [options] [ input.json ]  [ output.justo ]\n  justc justb->justc [options] [ input.justb ] [ output.justc ]\n  justc justc->js    [options] [ input.justc ] [ output.js ]    [arguments]\n  justc justc->json  [options] [ input.justc ] [ output.json ]  [arguments]\n  justc justc->justb [options] [ input.justc ] [ output.justb ] [arguments]\n  justc justc->justo [options] [ input.justc ] [ output.justo ] [arguments]\n  justc justc->xml   [options] [ input.justc ] [ output.xml ]   [arguments]\n  justc justc->yaml  [options] [ input.justc ] [ output.yaml ]  [arguments]\n  justc justo->json  [options] [ input.justo ] [ output.json ]\n  justc justo->justc [options] [ input.justo ] [ output.justc ]\n\nOptions:\n  --                                    Indicate the end of JUSTC options\n  --async-evaluation                    Asynchronous variable evaluation\n  -c, --check                           Validate JUSTC/JUSTO/JUSTB input\n  --disallow-javascript                 Disallow JavaScript\n  --disallow-luau                       Disallow Luau\n  -h, --help                            Print JUSTC command line options\n  --license                             Print JUSTC license\n  --no-compression                      Disable compiler output compression\n  -p, --print                           Print the result\n  --raw-version                         Print JUSTC version in \"x.y.z\" format\n  -s, --silent                          Suppress all logs except errors\n  -v, --version                         Print JUSTC version\n\nAvailable formats to compile JUSTC to:\n  justb\n\nAvailable formats to serialize JUSTC to:\n  json, justo, xml, yaml\n\nAvailable languages to transpile JUSTC to:\n  js, lua\n\nJS    - JavaScript\nJSON  - JavaScript Object Notation\nJUSTB - JUSTC's Universal Structuren't Temporary Bytecode\nJUSTC - JUSTC's Universal Serialization, Transpilation & Compilation\nJUSTO - JUSTC's Universal Simple Text Object\nXML   - eXtensible Markup Language\nYAML  - YAML Ain't Markup Language\n\nDocumentation can be found at https://just.js.org/justc/\nMade for Just an Ultimate Site Tool ( https://just.js.org/ )\n\nJUSTC, JUSTB and JUSTO,\nlicensed under the MIT License.\nCopyright (c) 2025-2026 JustStudio.\n(The dot in \"JustStudio.\" is part of the name.)\nhttps://just.js.org/justc\nhttps://juststudio.is-a.dev\n\nDependencies:\n  JUSTC uses C++ as its implementation language.\n  The entire project requires C++17.\n  JUSTC incorporates code from the following open-source projects:\n\n  Core Dependencies (Native Builds - Linux/macOS/Windows):\n    - Cereal by iLab @ USC - Licensed under the BSD-3-Clause License:\n      Copyright (c) 2013-2022, Randolph Voorhies, Shane Grant.\n      https://github.com/USCiLab/cereal/blob/master/LICENSE\n    - CPR by libcpr - Licensed under the MIT License:\n      Copyright (c) 2017-2021 Huu Nguyen.\n      Copyright (c) 2022 libcpr and many other contributors.\n      https://github.com/libcpr/cpr/blob/master/LICENSE\n    - ICU by Unicode - Licensed under the UNICODE LICENSE V3:\n      Copyright (c) 2016-2025 Unicode, Inc.\n      https://github.com/unicode-org/icu/blob/main/LICENSE\n    - Luau by Roblox - Licensed under the MIT License:\n      Copyright (c) 2019-2025 Roblox Corporation.\n      Copyright (c) 1994-2019 Lua.org, PUC-Rio.\n      https://github.com/luau-lang/luau/blob/master/LICENSE.txt\n    - nlohmann/json by Niels Lohmann - Licensed under the MIT License:\n      Copyright (c) 2013-2026 Niels Lohmann.\n      https://github.com/nlohmann/json/blob/develop/LICENSE.MIT\n    - QuickJS by bellard - Licensed under the MIT License:\n      Copyright (c) 2017-2021 Fabrice Bellard.\n      Copyright (c) 2017-2021 Charlie Gordon.\n      https://github.com/bellard/quickjs/blob/master/LICENSE\n    - QuickJS CMake by Rob Loach - Licensed under the MIT License:\n      Copyright (c) 2022 Rob Loach.\n      https://github.com/RobLoach/quickjs-cmake/blob/master/LICENSE\n\n  WebAssembly:\n    Does not use CPR (HTTP Requests support via XHR API)\n    Does not use nlohmann/json\n    Does not use QuickJS and QuickJS CMake\n    Does not use ICU (Unicode support via Intl API)\n\n  Reference / Inspiration:\n    - Lua by PUC-Rio - Licensed under the MIT License:\n      Copyright (c) 1994-2025 Lua.org, PUC-Rio.\n      https://www.lua.org/license.html\n      (Luau is a fork of Lua)\n\nAcknowledgements:\n  - JavaScript is a trademark of Oracle Corporation.\n    For more information, see https://www.oracle.com/legal/trademarks/\n  - Special thanks to the authors and maintainers of:\n    Cereal, CPR, ICU, Lua, Luau, nlohmann/json, QuickJS and QuickJS CMake\n    for their excellent work, which made JUSTC possible.");
