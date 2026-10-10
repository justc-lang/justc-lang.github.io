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
#include <zlib.h>
#include <zstd.h>
#include <lz4.h>
#include <snappy.h>
#include <bzlib.h>
#include <lzma.h>
#include "../parser.h"

std::vector<uint8_t> JustbLoader::decompressData(const std::vector<uint8_t>& compressedData, uint8_t compressionType, size_t originalSize) {
    if (compressionType == 0) { // NONE
        return compressedData;
    }

    std::vector<uint8_t> result(originalSize);

    switch (static_cast<CompressionAlgorithm>(compressionType)) {
        case CompressionAlgorithm::ZLIB: {
            uLongf destLen = originalSize;
            int ret = uncompress(result.data(), &destLen,
                               compressedData.data(), compressedData.size());
            if (ret == Z_OK) {
                result.resize(destLen);
                return result;
            }
            break;
        }
        case CompressionAlgorithm::GZIP: {
            z_stream stream;
            memset(&stream, 0, sizeof(stream));
            inflateInit2(&stream, 31);
            stream.next_in = const_cast<Bytef*>(compressedData.data());
            stream.avail_in = compressedData.size();
            stream.next_out = result.data();
            stream.avail_out = originalSize;
            int ret = inflate(&stream, Z_FINISH);
            inflateEnd(&stream);
            if (ret == Z_STREAM_END) {
                result.resize(stream.total_out);
                return result;
            }
            break;
        }
        case CompressionAlgorithm::BZIP2: {
            unsigned int destLen = originalSize;
            int ret = BZ2_bzBuffToBuffDecompress(
                reinterpret_cast<char*>(result.data()), &destLen,
                const_cast<char*>(reinterpret_cast<const char*>(compressedData.data())), 
                static_cast<int>(compressedData.size()),
                0, 0
            );
            if (ret == BZ_OK) {
                result.resize(destLen);
                return result;
            }
            break;
        }
        case CompressionAlgorithm::LZMA: {
            lzma_stream stream = LZMA_STREAM_INIT;
            lzma_ret ret = lzma_alone_decoder(&stream, UINT64_MAX);
            if (ret != LZMA_OK) break;
            
            stream.next_in = compressedData.data();
            stream.avail_in = compressedData.size();
            stream.next_out = result.data();
            stream.avail_out = originalSize;
            
            ret = lzma_code(&stream, LZMA_FINISH);
            lzma_end(&stream);
            if (ret == LZMA_STREAM_END) {
                result.resize(stream.total_out);
                return result;
            }
            break;
        }
        case CompressionAlgorithm::ZSTD: {
            size_t ret = ZSTD_decompress(result.data(), originalSize,
                                       compressedData.data(), compressedData.size());
            if (!ZSTD_isError(ret)) {
                result.resize(ret);
                return result;
            }
            break;
        }
        case CompressionAlgorithm::LZ4: {
            int ret = LZ4_decompress_safe(
                reinterpret_cast<const char*>(compressedData.data()),
                reinterpret_cast<char*>(result.data()),
                static_cast<int>(compressedData.size()),
                static_cast<int>(originalSize)
            );
            if (ret > 0) {
                result.resize(ret);
                return result;
            }
            break;
        }
        case CompressionAlgorithm::SNAPPY: {
            size_t destLen = originalSize;
            if (snappy::RawUncompress(
                reinterpret_cast<const char*>(compressedData.data()),
                compressedData.size(),
                reinterpret_cast<char*>(result.data())
            )) {
                return result;
            }
            break;
        }
        case CompressionAlgorithm::DEFLATE: {
            z_stream stream;
            memset(&stream, 0, sizeof(stream));
            inflateInit(&stream);
            stream.next_in = const_cast<Bytef*>(compressedData.data());
            stream.avail_in = compressedData.size();
            stream.next_out = result.data();
            stream.avail_out = originalSize;
            int ret = inflate(&stream, Z_FINISH);
            inflateEnd(&stream);
            if (ret == Z_STREAM_END) {
                result.resize(stream.total_out);
                return result;
            }
            break;
        }
        default:
            return compressedData;
    }

    return compressedData;
}

ParseResult JustbLoader::load(const std::string& inputPath, const std::string& entryFunction) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open JUSTB file");
    return load(in);
}

ParseResult JustbLoader::load(std::istream& in, const std::string& entryFunction) {
    JUSTB::Header header;
    if (!JUSTB::readHeader(in, header) || !JUSTB::validateHeader(header)) {
        throw std::runtime_error("Invalid JUSTB header");
    }

    uint64_t originalSize = 0;
    in.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));
    if (!in.good()) {
        throw std::runtime_error("Invalid JUSTB header");
    }

    std::vector<uint8_t> compressedData;
    compressedData.resize(originalSize);
    in.read(reinterpret_cast<char*>(compressedData.data()), compressedData.size());
    size_t actualSize = in.gcount();
    compressedData.resize(actualSize);

    std::vector<uint8_t> decompressedData = decompressData(compressedData, header.compression, originalSize);

    ParseResult result;
    std::string dataStr(decompressedData.begin(), decompressedData.end());
    std::stringstream buffer(dataStr, std::ios::binary | std::ios::in | std::ios::out);
    
    {
        cereal::BinaryInputArchive archive(buffer);
        archive(result.returnValues);
    }

    auto it = result.returnValues.find(entryFunction);
    if (it != result.returnValues.end()) {
        Value func = it->second;
        if (func.type != DataType::FUNCTION) throw std::runtime_error("\"" + entryFunction + "\" is defined, but it is not a function.");
        Parser parser({});
        for (const auto& [key, value] : result.returnValues) {
            parser.registerGlobal(key, value);
        }
        parser.callFunction(func, {}, 0, true);
    }

    return result;
}
