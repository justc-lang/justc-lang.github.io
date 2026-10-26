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

#include "builtins.h"

const std::vector<std::string> builtins = {
    "charType", "JUSTC", "JUSTO", "Luau",
    "JavaScript", "math", "script", "JSON",
    "HTTP", "system", "task", "memory"
};
const std::vector<std::string> builtinfunctions = {
    "TIME", "backslash", "version", "valueof", 
    "String", "Link", "Binary", "Octal", "Set", "Map",
    "Hexadecimal", "typeid", "typeof", "echo", 
    "Number", "JSON", "JSON.parse", "file", "size", 
    "config", "Window", "Element::render", "hash",
    "system.env", "Promise", "free", "Error",

    "JUSTC", "JUSTC.version", "JUSTC.parse", 
    "JUSTC.execute", "JUSTC.stringify",

    "JUSTO", "JUSTO.version", "JUSTO.stringify",
    "JUSTO.parse",

    "HTTP.GET", "HTTP.PUT", "HTTP.POST", "HTTP.PATCH",
    "HTTP.DELETE", "HTTP.HEAD", "HTTP.OPTIONS",

    "JavaScript",
    "JavaScript.isAllowed", "JavaScript.execute",

    "Luau",
    "Luau.isAllowed", "Luau.execute", "Luau.compile",

    "math.pi", "math.e", "math.ln2", "math.ln10",
    "math.sqrt2", "math.sqrt1_2", "math.log2e",
    "math.log10e", "math.random", "math.abs",
    "math.acos", "math.asin", "math.atan",
    "math.atan2", "math.ceil", "math.cos", 
    "math.clamp", "math.double", "math.exp",
    "math.factorial", "math.floor", "math.hypot",
    "math.isPrime", "math.lerp", "math.log",
    "math.log10", "math.max", "math.min",
    "math.pow", "math.round", "math.sign",
    "math.sin", "math.sqrt", "math.square",
    "math.tan", "math.toDegrees", "math.cube",
    "math.toRadians", "parseNum", "parseInt",

    "String::reverse", "String::graphemeReverse",
    "String::codePointReverse", "String::byteReverse",
    "String::slice", "String::graphemeSlice",
    "String::codePointSlice", "String::byteSlice",
    "String::length", "String::graphemeLength",
    "String::codePointLength", "String::byteLength",
    "String::size", "String::trim", "String::repeat",
    "String::startsWith", "String::endsWith",
    "String::split", "String::lower", "String::upper",
    "String::normalize", "String::equalsIgnoreCase",
    "String::isWhitespace", "String::hash",

    "Array::join", "Array::includes", "Array::indexOf",
    "Array::lastIndexOf", "Array::forEach", "Array::push",
    "Array::reverse", "Array::filter", "Array::unshift",

    "Binary::toText", "Binary::fromText",
    "Binary::toDataURL", "Binary::fromDataURL",

    "task.wait", "task.sleep", "task.race",

    "memory.global", "memory.classes", 
    "memory.functions", "memory.pointers",

    "Int8Array", "Int16Array", "Int32Array", "Int64Array",
    "UInt8Array", "UInt16Array", "UInt32Array", "UInt64Array",
    "CUInt8Array", "CUInt16Array", "CUInt32Array", "CUInt64Array",
    "Float32Array", "Float64Array",
    
    "Int8Array::compress", "Int16Array::compress",
    "Int32Array::compress", "Int64Array::compress",
    "UInt8Array::compress", "UInt16Array::compress",
    "UInt32Array::compress", "UInt64Array::compress",
    "CUInt8Array::compress", "CUInt16Array::compress",
    "CUInt32Array::compress", "CUInt64Array::compress",
    
    "Int8Array::decompress", "Int16Array::decompress",
    "Int32Array::decompress", "Int64Array::decompress",
    "UInt8Array::decompress", "UInt16Array::decompress",
    "UInt32Array::decompress", "UInt64Array::decompress",
    "CUInt8Array::decompress", "CUInt16Array::decompress",
    "CUInt32Array::decompress", "CUInt64Array::decompress",

    "Map::set", "Map::get", "Map::has", "Map::delete", "Map::clear",

    "Set::add", "Set::has", "Set::delete", "Set::clear",

    "Object::size", "Array::size", "Map::size", "Set::size",
    "Int8Array::size", "Int16Array::size", "Int32Array::size", 
    "Int64Array::size", "UInt8Array::size", "UInt16Array::size",
    "UInt32Array::size", "UInt64Array::size", "CUInt8Array::size", 
    "CUInt16Array::size", "CUInt32Array::size", "CUInt64Array::size",
    "Float32Array::size", "Float64Array::size",
};
