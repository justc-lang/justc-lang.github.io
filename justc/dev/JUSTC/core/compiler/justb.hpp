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

#pragma once

#include "../parser.h"
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

struct CompressionResult {
    CompressionAlgorithm algorithm;
    std::vector<uint8_t> data;
    size_t originalSize;
    size_t compressedSize;
    double ratio;
};

class JustbCompiler {
public:
    static bool compile(const ParseResult& result, const std::string& outputPath, bool noCompression = false);
    static bool compile(const ParseResult& result, std::ostream& out, bool noCompression = false);

    static void setCompressionLevel(int level) { compressionLevel = std::max(1, std::min(9, level)); }
    static int getCompressionLevel() { return compressionLevel; }

    static void setAutoSelect(bool enable) { autoSelect = enable; }
    static bool getAutoSelect() { return autoSelect; }

    static void setMinCompressionRatio(double ratio) { minCompressionRatio = std::max(0.0, std::min(1.0, ratio)); }
    static double getMinCompressionRatio() { return minCompressionRatio; }

    static std::vector<uint8_t> compressNone(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> compressZlib(const std::vector<uint8_t>& data, int level);
    static std::vector<uint8_t> compressGzip(const std::vector<uint8_t>& data, int level);
    static std::vector<uint8_t> compressBzip2(const std::vector<uint8_t>& data, int level);
    static std::vector<uint8_t> compressLzma(const std::vector<uint8_t>& data, int level);
    static std::vector<uint8_t> compressZstd(const std::vector<uint8_t>& data, int level);
    static std::vector<uint8_t> compressLz4(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> compressSnappy(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> compressDeflate(const std::vector<uint8_t>& data, int level);

private:
    static int compressionLevel;
    static bool autoSelect;
    static double minCompressionRatio;

    static CompressionResult tryCompression(const std::vector<uint8_t>& data, CompressionAlgorithm algorithm, int level = -1);
    static CompressionResult selectBestCompression(const std::vector<uint8_t>& data, bool noCompression = false);
};
