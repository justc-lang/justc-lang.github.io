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

#include "justb.hpp"
#include "../justb.hpp"
#include <cereal/archives/binary.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <sstream>
#include <algorithm>
#include <zlib.h>
#include <zstd.h>
#include <lz4.h>
#include <snappy.h>
#include <bzlib.h>
#include <lzma.h>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include "justb.emscripten.h"
#endif

int JustbCompiler::compressionLevel = 6;
bool JustbCompiler::autoSelect = true;
double JustbCompiler::minCompressionRatio = 0.01;

std::vector<uint8_t> JustbCompiler::compressNone(const std::vector<uint8_t>& data) {
    return data;
}

std::vector<uint8_t> JustbCompiler::compressZlib(const std::vector<uint8_t>& data, int level) {
    std::vector<uint8_t> result;
    uLongf compressedSize = compressBound(static_cast<uLong>(data.size()));
    result.resize(compressedSize);
    
    int ret = compress2(result.data(), &compressedSize, data.data(), static_cast<uLong>(data.size()), level);
    if (ret == Z_OK) {
        result.resize(compressedSize);
        return result;
    }
    return {};
}

std::vector<uint8_t> JustbCompiler::compressGzip(const std::vector<uint8_t>& data, int level) {
    std::vector<uint8_t> result;
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    
    int ret = deflateInit2(&stream, level, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) return {};
    
    stream.next_in = const_cast<Bytef*>(data.data());
    stream.avail_in = static_cast<uInt>(data.size());
    
    uLongf compressedSize = deflateBound(&stream, data.size());
    result.resize(compressedSize);
    stream.next_out = result.data();
    stream.avail_out = static_cast<uInt>(result.size());
    
    ret = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);
    
    if (ret == Z_STREAM_END) {
        result.resize(stream.total_out);
        return result;
    }
    return {};
}

std::vector<uint8_t> JustbCompiler::compressBzip2(const std::vector<uint8_t>& data, int level) {
    std::vector<uint8_t> result;
    unsigned int compressedSize = data.size() + 1024;
    result.resize(compressedSize);
    
    int ret = BZ2_bzBuffToBuffCompress(
        reinterpret_cast<char*>(result.data()), &compressedSize,
        const_cast<char*>(reinterpret_cast<const char*>(data.data())), 
        static_cast<int>(data.size()),
        level, 0, 30
    );
    
    if (ret == BZ_OK) {
        result.resize(compressedSize);
        return result;
    }
    return {};
}

std::vector<uint8_t> JustbCompiler::compressLzma(const std::vector<uint8_t>& data, int level) {
    std::vector<uint8_t> result;
    lzma_stream stream = LZMA_STREAM_INIT;
    
    lzma_options_lzma options;
    lzma_lzma_preset(&options, level);
    
    int ret = lzma_alone_encoder(&stream, &options);
    if (ret != LZMA_OK) return {};
    
    stream.next_in = data.data();
    stream.avail_in = data.size();
    
    result.resize(data.size() + 1024);
    stream.next_out = result.data();
    stream.avail_out = result.size();
    
    ret = lzma_code(&stream, LZMA_FINISH);
    lzma_end(&stream);
    
    if (ret == LZMA_STREAM_END) {
        result.resize(stream.total_out);
        return result;
    }
    return {};
}

std::vector<uint8_t> JustbCompiler::compressZstd(const std::vector<uint8_t>& data, int level) {
    std::vector<uint8_t> result;
    size_t compressedSize = ZSTD_compressBound(data.size());
    result.resize(compressedSize);
    
    size_t ret = ZSTD_compress(result.data(), result.size(), data.data(), data.size(), level);
    if (!ZSTD_isError(ret)) {
        result.resize(ret);
        return result;
    }
    return {};
}

std::vector<uint8_t> JustbCompiler::compressLz4(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> result;
    int maxSize = LZ4_compressBound(static_cast<int>(data.size()));
    result.resize(maxSize);
    
    int compressedSize = LZ4_compress_default(
        reinterpret_cast<const char*>(data.data()),
        reinterpret_cast<char*>(result.data()),
        static_cast<int>(data.size()),
        maxSize
    );
    
    if (compressedSize > 0) {
        result.resize(compressedSize);
        return result;
    }
    return {};
}

std::vector<uint8_t> JustbCompiler::compressSnappy(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> result;
    size_t compressedSize = snappy::MaxCompressedLength(data.size());
    result.resize(compressedSize);
    
    snappy::RawCompress(
        reinterpret_cast<const char*>(data.data()),
        data.size(),
        reinterpret_cast<char*>(result.data()),
        &compressedSize
    );
    
    result.resize(compressedSize);
    return result;
}

std::vector<uint8_t> JustbCompiler::compressDeflate(const std::vector<uint8_t>& data, int level) {
    std::vector<uint8_t> result;
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    
    int ret = deflateInit(&stream, level);
    if (ret != Z_OK) return {};
    
    stream.next_in = const_cast<Bytef*>(data.data());
    stream.avail_in = static_cast<uInt>(data.size());
    
    uLongf compressedSize = deflateBound(&stream, data.size());
    result.resize(compressedSize);
    stream.next_out = result.data();
    stream.avail_out = static_cast<uInt>(result.size());
    
    ret = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);
    
    if (ret == Z_STREAM_END) {
        result.resize(stream.total_out);
        return result;
    }
    return {};
}

CompressionResult JustbCompiler::tryCompression(const std::vector<uint8_t>& data, CompressionAlgorithm algorithm, int level) {
    CompressionResult result;
    result.algorithm = algorithm;
    result.originalSize = data.size();
    result.data = {};

    if (data.empty()) {
        result.compressedSize = 0;
        result.ratio = 1.0;
        return result;
    }

    if (data.size() < 64 && algorithm != CompressionAlgorithm::NONE) {
        result.compressedSize = data.size();
        result.ratio = 1.0;
        return result;
    }

    int lvl = (level > 0) ? level : compressionLevel;

    switch (algorithm) {
        case CompressionAlgorithm::NONE:
            result.data = compressNone(data);
            break;
        case CompressionAlgorithm::ZLIB:
            result.data = compressZlib(data, lvl);
            break;
        case CompressionAlgorithm::GZIP:
            result.data = compressGzip(data, lvl);
            break;
        case CompressionAlgorithm::BZIP2:
            result.data = compressBzip2(data, lvl);
            break;
        case CompressionAlgorithm::LZMA:
            result.data = compressLzma(data, lvl);
            break;
        case CompressionAlgorithm::ZSTD:
            result.data = compressZstd(data, lvl);
            break;
        case CompressionAlgorithm::LZ4:
            result.data = compressLz4(data);
            break;
        case CompressionAlgorithm::SNAPPY:
            result.data = compressSnappy(data);
            break;
        case CompressionAlgorithm::DEFLATE:
            result.data = compressDeflate(data, lvl);
            break;
        default:
            result.data = compressNone(data);
            break;
    }

    if (result.data.empty()) {
        result.data = compressNone(data);
        result.algorithm = CompressionAlgorithm::NONE;
        result.compressedSize = data.size();
        result.ratio = 1.0;
        return result;
    }

    result.compressedSize = result.data.size();
    result.ratio = static_cast<double>(result.compressedSize) / static_cast<double>(data.size());
    
    return result;
}

CompressionResult JustbCompiler::selectBestCompression(const std::vector<uint8_t>& data, bool noCompression) {
    if (!autoSelect || data.empty() || noCompression) {
        return tryCompression(data, CompressionAlgorithm::NONE);
    }

    std::vector<CompressionAlgorithm> algorithms = {
        CompressionAlgorithm::ZSTD,
        CompressionAlgorithm::ZLIB,
        CompressionAlgorithm::DEFLATE,
        CompressionAlgorithm::GZIP,
        CompressionAlgorithm::LZMA,
        CompressionAlgorithm::BZIP2,
        CompressionAlgorithm::LZ4,
        CompressionAlgorithm::SNAPPY,
    };

    CompressionResult best = tryCompression(data, CompressionAlgorithm::NONE);
    best.algorithm = CompressionAlgorithm::NONE;

    for (auto algo : algorithms) {
        if (data.size() < 1024 && (algo == CompressionAlgorithm::LZMA || algo == CompressionAlgorithm::BZIP2)) {
            continue;
        }

        CompressionResult current = tryCompression(data, algo);
        if (current.data.empty()) continue;

        if (current.compressedSize < best.compressedSize) {
            best = current;
        }
    }

    if (best.ratio > (1.0 - minCompressionRatio)) {
        best = tryCompression(data, CompressionAlgorithm::NONE);
    }

    return best;
}

bool JustbCompiler::compile(const ParseResult& result, const std::string& outputPath, bool noCompression) {
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) return false;
    return compile(result, out, noCompression);
}

bool JustbCompiler::compile(const ParseResult& result, std::ostream& out, bool noCompression) {
    try {
        std::stringstream buffer(std::ios::binary | std::ios::in | std::ios::out);
        {
            cereal::BinaryOutputArchive archive(buffer);
            archive(result.returnValues);
        }
        
        std::string bufferStr = buffer.str();
        std::vector<uint8_t> data(bufferStr.begin(), bufferStr.end());
        
        CompressionResult compressed;
        if (autoSelect) {
            compressed = selectBestCompression(data, noCompression);
        } else {
            compressed = tryCompression(data, CompressionAlgorithm::NONE);
        }
        
        uint8_t compressionType = static_cast<uint8_t>(compressed.algorithm);
        if (!JUSTB::writeHeader(out, JUSTC_VERSION, compressionType, 0)) {
            return false;
        }
        
        uint64_t originalSize = data.size();
        out.write(reinterpret_cast<const char*>(&originalSize), sizeof(originalSize));
        out.write(reinterpret_cast<const char*>(compressed.data.data()), compressed.data.size());

        return out.good();
    } catch (const std::exception& e) {
        #ifdef __EMSCRIPTEN__
            error_compile_justb(std::string(e.what()).c_str());
        #endif
        return false;
    }
}
