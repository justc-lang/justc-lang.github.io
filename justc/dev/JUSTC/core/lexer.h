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

#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <codecvt>
#include <locale>
#include "keywords.h"

struct ParserToken {
    std::string type;
    std::string value;
    size_t start;

    ParserToken() : type(""), value(""), start(0) {}
    ParserToken(const std::string& t, const std::string& v, size_t s)
        : type(t), value(v), start(s) {}

    template <class Archive>
    void serialize(Archive& archive) {
        archive(type);
        archive(value);
        int startInt = static_cast<int>(start);
        archive(startInt);
        start = static_cast<size_t>(startInt);
    }
};

class Lexer {
private:
    std::string input;
    bool warn;
    size_t position;
    std::vector<ParserToken> tokens;
    bool requireDot;
    bool useDollarBefore;

    std::vector<std::string> keywords;
    std::unordered_map<std::string, std::string> smallkeywords;
    std::unordered_map<std::string, std::string> bigkeywords;
    std::vector<std::string> skw;
    std::vector<std::string> bkw;
    std::vector<size_t> gotopos;

    void initializeKeywords();
    bool isWhitespace(char ch) const;
    bool isLetter(char ch) const;
    bool isDigit(char ch) const;
    bool isHexDigit(char ch) const;
    bool isBase64Char(char ch) const;

    bool isUnicodeLetter(char ch) const;
    bool isIdentifierStart(char ch) const;
    bool isIdentifierContinue(char ch) const;
    std::string readUnicodeChar();

    char peek(size_t offset = 1) const;
    void readComment();
    void readMultiLineComment();
    ParserToken readString(char quote, bool raw = false);
    ParserToken readLink(bool isType = false);
    ParserToken readNumber();
    ParserToken readIdentifier();
    void tokenize();

    void invalidInput();
    void invalidUsage();

    bool dollarBefore;
    void addDollarBefore();
    bool isNonAscii(char c);

    ParserToken readJSX();
    ParserToken readJSXOpeningTag();
    std::string readJSXAttributeValue();
    bool isJSXIdentifier(char ch) const;
    void skipWhitespace();

public:
    Lexer(const std::string& input, const bool& warn, const bool& requireDot = false, const bool& useDollarBefore = false);
    std::vector<ParserToken> getTokens() const;
    static std::pair<std::string, std::vector<ParserToken>> parse(const std::string& input, const bool& warn = false);

    static bool isValidUTF8(const std::string& str);
    static std::string toUTF8(const std::wstring& wstr);
    static std::wstring fromUTF8(const std::string& str);
};

#endif
