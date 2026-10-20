> [!WARNING]
> ### JUSTC language is currently in development and is experimental!
> Documentation is coming after release of new Just an Ultimate Site Tool update. Currently, you can use [experimental JUSTC demo website](https://just.js.org/justc/development/test.html).

<div align="center">
    <img src="./.github/image.png" alt="JUSTC Banner" height="500"><br>
</div> <br><br>

# <img align="top" src="https://just.js.org/justc/logo-50.svg" alt="JUSTC Logo" width="40" height="40"> JUSTC ![Latest version](https://img.shields.io/npm/v/justc?label=&color=6e3bf3) [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://just.js.org/justc/license.txt) [![Compile](https://github.com/js-just/JUSTC/actions/workflows/compile.yml/badge.svg)](https://github.com/js-just/JUSTC/actions/workflows/compile.yml)
**JUSTC** is a powerful scripting language designed for automation, with JavaScript™ and Luau included, designed to be backwards compatible with JSON. <br>

**JUSTB** is a bytecode format and a part of JUSTC, designed for faster loading and smaller file sizes. <br>
**JUSTO** is an object notation language with simple syntax, designed for easy and fast parsing, also a part of JUSTC.

JUSTC is a free and open-source software, made for Just an Ultimate Site Tool, the Web, and everything else.

## Features
- JUSTC is **compilable** to `.justb` (JUSTB);
- **Interpretable**;
- **Serializable** to JSON, JUSTO, XML, YAML;
- **Cross-platform**: Windows, Ubuntu, macOS, WebAssembly (JavaScript&nbsp;- Node.js and browsers).

# Quick Start

### CLI

```bash
justc --help
```

### Web

```html
<script src="https://unpkg.com/justc"></script>
<script>
    await JUSTC.initialize();
    await JUSTC.execute(`
        echo "Hello, World!".
    `);
</script>
```

### Node.js

```bash
npm install justc
```

```js
const JUSTC = require("justc");

JUSTC.execute(`
    echo "Hello, World!".
`).then(result => console.log(result));
```

---

# License

JUSTC, JUSTB, and JUSTO are open-source software,
licensed under the [MIT License](https://just.js.org/justc/license.txt).

Copyright (c) 2025-2026 JustStudio.
(The dot in "JustStudio." is part of the name.)

### For Web Developers

If you are using the official JUSTC distribution from
[just.js.org/justc](https://just.js.org/justc),
[unpkg.com/justc](https://unpkg.com/justc) or npm,
**attribution is not required** in your website's UI or source code.

We have already included:
- License headers in all scripts
- Source maps with license headers
- `LICENSE` and `README.md` files in the distribution
- Copyright notices in WASM binary custom sections

Otherwise, or if your website blocks source map loading in some way,
please include the Desktop/Mobile Applications notice.

### For Desktop/Mobile Applications

If your application **uses** JUSTC, or at least JUSTB or JUSTO,
please include the following notice in your credits or about screen:

```
This software uses JUSTC, JUSTB and JUSTO,
licensed under the MIT License.
Copyright (c) 2025-2026 JustStudio.
(The dot in "JustStudio." is part of the name.)
https://just.js.org/justc
https://juststudio.is-a.dev
```

### For Developers Building from Source

If you compile JUSTC from source or distribute it as part of
your project, you must include the MIT License notice.
See [LICENSE](./LICENSE) for details.

### Logo and banner

Attribution making use of the [JUSTC logo](https://github.com/js-just/website/blob/main/justc/logo.svg) or [JUSTC banner](./.github/image.png) is also encouraged when reasonable.

---

# Dependencies
JUSTC uses C++ as its implementation language. The entire project requires C++17.

JUSTC incorporates code from the following open-source projects:

### Core Dependencies (Native Builds - Windows/Ubuntu/macOS)

- [bzip2](https://sourceware.org/bzip2/) by Julian Seward — Licensed under the bzip2 and libbzip2 License: <br>
Copyright (C) 1996-2019 Julian R. Seward. <br>
https://sourceware.org/git/?p=bzip2.git;a=blob;f=LICENSE;hb=HEAD

- [Cereal](https://github.com/USCiLab/cereal) by [iLab @ USC](https://github.com/USCiLab) — Licensed under the BSD-3-Clause license: <br>
Copyright (c) 2013-2022, Randolph Voorhies, Shane Grant. All rights reserved. <br>
https://github.com/USCiLab/cereal/blob/master/LICENSE

- [CPR](https://github.com/libcpr/cpr) by [libcpr](https://github.com/libcpr) — Licensed under the MIT License: <br>
Copyright (c) 2017-2021 Huu Nguyen. <br>
Copyright (c) 2022 libcpr and many other contributors. <br>
https://github.com/libcpr/cpr/blob/master/LICENSE

- [ICU](https://icu.unicode.org/) by [Unicode](https://www.unicode.org/) — Licensed under the UNICODE LICENSE V3: <br>
Copyright (c) 2016-2025 Unicode, Inc. <br>
https://github.com/unicode-org/icu/blob/main/LICENSE

- [Luau](https://luau-lang.org/) by [Roblox](https://github.com/Roblox) — Licensed under the MIT License: <br>
Copyright (c) 2019-2025 Roblox Corporation. <br>
Copyright (c) 1994–2019 Lua.org, PUC-Rio. <br>
https://github.com/luau-lang/luau/blob/master/LICENSE.txt

- [LZ4](https://www.lz4.org/) by Yann Collet — Licensed under the BSD-2-Clause License: <br>
Copyright (c) 2011-2020, Yann Collet. All rights reserved. <br>
https://github.com/lz4/lz4/blob/dev/lib/LICENSE

- [nlohmann/json](https://github.com/nlohmann/json) by [Niels Lohmann](https://github.com/nlohmann) — Licensed under the MIT License: <br>
Copyright (c) 2013-2026 Niels Lohmann. <br>
https://github.com/nlohmann/json/blob/develop/LICENSE.MIT

- [Promise-cpp](https://github.com/xhawk18/promise-cpp) by [xhawk18](https://github.com/xhawk18) — Licensed under the MIT License: <br>
Copyright (c) 2018 xhawk18@gmail.com. <br>
https://github.com/xhawk18/promise-cpp/blob/master/LICENSE.md

- [QuickJS](https://bellard.org/quickjs/) by [bellard](https://github.com/bellard) — Licensed under the MIT License: <br>
Copyright (c) 2017-2021 Fabrice Bellard. <br>
Copyright (c) 2017-2021 Charlie Gordon. <br>
https://github.com/bellard/quickjs/blob/master/LICENSE

- [QuickJS CMake](https://github.com/robloach/quickjs-cmake) by [Rob Loach](https://robloach.net/) — Licensed under the MIT License: <br>
Copyright (c) 2022 Rob Loach. <br>
https://github.com/RobLoach/quickjs-cmake/blob/master/LICENSE

- [Snappy](https://github.com/google/snappy) by [Google](https://github.com/google) — Licensed under the BSD-3-Clause license: <br>
Copyright 2011, Google Inc. All rights reserved. <br>

- [XZ Utils](https://github.com/tukaani-project/xz) by The XZ Utils authors and contributors — Licensing varies by component and version. <br>
See the [official licensing information](https://github.com/tukaani-project/xz/blob/master/COPYING).

- [zlib](https://zlib.net/) by Jean-loup Gailly and [Mark Adler](https://github.com/madler) — Licensed under the zlib License: <br>
Copyright (c) 1995-2026 Jean-loup Gailly and Mark Adler. <br>
https://github.com/madler/zlib/blob/develop/LICENSE

- [Zstandard](https://github.com/facebook/zstd) by [Meta](https://github.com/facebook) — Dual-licensed under the BSD License **OR** GPLv2. **Used under the BSD License in JUSTC:** <br>
Copyright (c) Meta Platforms, Inc. and affiliates. All rights reserved. <br>
https://github.com/facebook/zstd/blob/dev/LICENSE <br>
https://github.com/facebook/zstd/blob/dev/COPYING

### WebAssembly 
- Does **not** use CPR (HTTP Requests support via XHR API)
- Does **not** use nlohmann/json
- Does **not** use QuickJS and QuickJS CMake
- Does **not** use ICU (Unicode support via Intl API)

### Build type 1 and build type 3
- Does **not** use Luau

### Build type 2 and build type 3
- Does **not** use Cereal

### Reference / Inspiration
- [Lua](https://www.lua.org/) by [PUC-Rio](https://www.puc-rio.br/) — Licensed under the MIT License:
  Copyright (c) 1994-2025 Lua.org, PUC-Rio.
  https://www.lua.org/license.html
  (*Luau is a fork of Lua*)

---

# Acknowledgements

- **JavaScript™** is a trademark of Oracle Corporation.
  For more information, see https://www.oracle.com/legal/trademarks/

- Special thanks to the authors and maintainers of:
  **bzip2**, **Cereal**, **CPR**, **ICU**, **Lua**, 
  **Luau**, **LZ4**, **nlohmann/json**, **Promise-cpp**, 
  **QuickJS**, **QuickJS CMake**, **Snappy**, **XZ Utils**,
  **zlib** and **Zstandard**
  for their excellent work, which made JUSTC possible.
