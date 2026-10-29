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

#include "lexer.h"
#include "keywords.h"
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <regex>
#include <string>
#include <cstring>
#include <sstream>
#include "utility.h"
#include <iostream>
#include <codecvt>
#include <locale>

#ifdef __EMSCRIPTEN__
#include "parser.h"
#include "lexer.emscripten.h"
#endif

namespace {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
}

Lexer::Lexer(const std::string& input, const bool& warn, const bool& requireDot, const bool& useDollarBefore) : input(input), warn(warn), position(0), dollarBefore(false), requireDot(requireDot), useDollarBefore(useDollarBefore), braceDepth(0), bracketDepth(0), parenthesisDepth(0), waitFor(0), firstBrace(0), firstBracket(0), firstParenthesis(0) {
    if (input.empty()) {
        throw std::invalid_argument("Invalid Input.");
    }
    if (!isValidUTF8(input)) {
        throw std::invalid_argument("Out of range. JUSTC supports only UTF-8.");
    }
    initializeKeywords();
    tokenize();
}

bool Lexer::isValidUTF8(const std::string& str) {
    try {
        std::wstring wstr = converter.from_bytes(str);
        return true;
    } catch (...) {
        return false;
    }
}

std::string Lexer::toUTF8(const std::wstring& wstr) {
    return converter.to_bytes(wstr);
}

std::wstring Lexer::fromUTF8(const std::string& str) {
    return converter.from_bytes(str);
}

bool Lexer::isUnicodeLetter(char ch) const {
    return static_cast<unsigned char>(ch) > 127;
}

bool Lexer::isIdentifierStart(char ch) const {
    return isLetter(ch) || isUnicodeLetter(ch) || ch == '_';
}

bool Lexer::isIdentifierContinue(char ch) const {
    return isIdentifierStart(ch) || isDigit(ch) || ch == '\'';
}

std::string Lexer::readUnicodeChar() {
    if (position >= input.length()) return "";

    char first = input[position];

    int charLen = 1;
    if ((first & 0xE0) == 0xC0) charLen = 2;
    else if ((first & 0xF0) == 0xE0) charLen = 3;
    else if ((first & 0xF8) == 0xF0) charLen = 4;

    if (position + charLen > input.length()) {
        throw std::runtime_error("Encoding error: Invalid UTF-8 sequence at " + Utility::position(position, input));
    }

    std::string result = input.substr(position, charLen);
    position += charLen;
    return result;
}

void Lexer::initializeKeywords() {
    keywords = ::keywords;
}

void Lexer::invalidInput() {
    throw std::invalid_argument("Invalid Input.");
}

void Lexer::invalidUsage() {
    throw std::invalid_argument("Invalid Usage.");
}

bool Lexer::isWhitespace(char ch) const {
    return std::isspace(static_cast<unsigned char>(ch));
}

bool Lexer::isLetter(char ch) const {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool Lexer::isDigit(char ch) const {
    return std::isdigit(static_cast<unsigned char>(ch));
}

bool Lexer::isHexDigit(char ch) const {
    return std::isxdigit(static_cast<unsigned char>(ch));
}

bool Lexer::isBase64Char(char ch) const {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '+' || ch == '/' || ch == '=';
}

char Lexer::peek(size_t offset) const {
    if (position + offset < input.length()) {
        return input[position + offset];
    }
    return '\0';
}

// single-line comment: -- comment
void Lexer::readComment() {
    while (position < input.length() && input[position] != '\n') {
        ++position;
    }
    if (position < input.length() && input[position] == '\n') {
        ++position;
    }
}
// multi-line comment: -{ comment }-
void Lexer::readMultiLineComment() {
    while (position < input.length() && input[position] != '}' && peek() != '-') {
        ++position;
    }
    if (position < input.length() && input[position] == '}' && peek() == '-') {
        position += 2;
    }
}

ParserToken Lexer::readString(char quote, bool raw) {
    size_t start = ++position;
    std::string value = "";
    bool interpolation = quote == '`';
    
    while (position < input.length()) {
        char ch = input[position];
        
        if (ch == '\\' && position + 1 < input.length()) {
            if (input[position + 1] == quote) {
                position += 2;
                value += quote;
            } else {
                value += ch;
                value += input[position + 1];
                position += 2;
            }
        } else if (ch == quote) {
            ++position;
            break;
        } else {
            value += ch;
            ++position;
        }
    }
    
    if (!raw && !interpolation) value = StringEscape::unescape(value);
    return ParserToken{std::string("string") + std::string(interpolation ? "_i" : ""), value, start};
}

ParserToken Lexer::readLink(bool isType) {
    size_t start = ++position;
    std::string value = "";
    while (position < input.length() && input[position] != '>') {
        value += input[position++];
    }
    ++position;
    return ParserToken{isType ? "type" : "link", value, start};
}

ParserToken Lexer::readNumber() {
    size_t start = position;
    bool point = false;
    bool isBin = false;
    bool isOct = false;
    bool isHex = false;
    bool isB64 = false;
    bool enableType = false;
    bool allowCommaDecimal = true;

    enum class NumType {
        big,
        int_,
        exp,
    };
    NumType numtype = NumType::big;
    static const std::unordered_map<char, NumType> NumTypes = {
        {'b', NumType::big},
        {'i', NumType::int_},
        {'e', NumType::exp},
    };

    if (braceDepth > 0 || bracketDepth > 0 || parenthesisDepth > 0) {
        allowCommaDecimal = false;
    }

    if (position + 1 < input.length() && input[position] == '0') {
        char next = std::tolower(input[position + 1]);
        if (next == 'b') {
            isBin = true;
            position += 2;
        } else if (next == 'o') {
            isOct = true;
            position += 2;
        } else if (next == 'x') {
            isHex = true;
            position += 2;
        }
    }

    while (position < input.length()) {
        char ch = input[position];
        char c = static_cast<char>(ch);
        bool isValidChar = false;
        auto it = NumTypes.find(c);

        if (start == position && (ch == '#' || ch == '&')) {
            isValidChar = true;
            if (ch == '#') {
                isHex = true;
            } else {
                isB64 = true;
            }
        } else if (isBin) {
            isValidChar = (ch == '0' || ch == '1');
        } else if (isOct) {
            isValidChar = (ch >= '0' && ch <= '7');
        } else if (isHex) {
            isValidChar = isHexDigit(ch);
        } else {
            isValidChar = isDigit(ch) ||
                         (ch == '.' && position + 1 < input.length()) ||
                         (ch == ',' && allowCommaDecimal && position + 1 < input.length()) ||
                         ch == '_' ||
                         (it != NumTypes.end() && position > start && isDigit(input[position - 1])) ||
                         (numtype == NumType::exp && (ch == '+' || ch == '-'));
        }

        if (isValidChar) {
            if (it != NumTypes.end() && !isBin && !isOct && !isHex && !isB64) {
                enableType = true;
                numtype = it->second;
            }

            if (ch == ',') {
                if (!point && allowCommaDecimal) {
                    point = true;
                    input[position] = '.';
                } else {
                    break;
                }
            } else if (ch == '.') {
                if (!point) {
                    point = true;
                } else {
                    break;
                }
            } else if (ch == '_') {
                ++position;
                continue;
            }

            ++position;
        } else {
            break;
        }
    }

    std::string numStr = input.substr(start, position - start);
    numStr.erase(std::remove(numStr.begin(), numStr.end(), '_'), numStr.end());
    std::string checkstr = numStr;
    std::transform(checkstr.begin(), checkstr.end(), checkstr.begin(), ::tolower);

    std::string type;
    if (isBin) {
        type = "binary";
    } else if (isOct) {
        type = "octal";
    } else if (isHex || checkstr[0] == '#') {
        type = "hex";
    } else if (isB64 || checkstr[0] == '&') {
        type = "base64";
    } else {
        type = "number";

        if (enableType && !checkstr.empty() && NumTypes.find(std::tolower(checkstr.back())) != NumTypes.end()) {
            checkstr.pop_back();
            numStr.pop_back();
            switch (numtype) {
                case NumType::big:
                    type = "big";
                    break;

                case NumType::int_:
                    type = "int";
                    break;

                case NumType::exp:
                    type = "exp";
                    break;
            }
        }
    }

    ParserToken token(type, numStr, start);
    return token;
}

ParserToken Lexer::readIdentifier() {
    size_t start = position;

    if (dollarBefore) {
        dollarBefore = false;
        start = position - 1;
    }

    std::string id;
    while (position < input.length()) {
        char ch = input[position];

        if (isIdentifierContinue(ch)) {
            if (isUnicodeLetter(ch)) {
                size_t before = position;
                std::string unicodeChar = readUnicodeChar();
                id += unicodeChar;
            } else {
                id += ch;
                ++position;
            }
        } else if (ch == ',' || ch == ';' || ch == '.' || ch == '=' || ch == ':' || ch == '-') {
            break;
        } else {
            break;
        }
    }

    std::string idWithoutDollar = id;
    if (!id.empty() && id[0] == '$') {
        idWithoutDollar = id.substr(1);
    }

    if (std::find(keywords.begin(), keywords.end(), idWithoutDollar) != keywords.end()) {
        return ParserToken{"keyword", id, start};
    }

    std::regex keyword_regex("^is$|^isn't$|^isif$|^then$|^elseif$|^else$|^isifn't$|^elseifn't$|^then't$|^elsen't$|^or$|^orn't$|^and$|^andn't$|^not$|^AND$|^OR$|^XOR$|^NOT$|^nand$|^nor$|^xor$|^imply$|^nimply$");
    std::regex boolean_regex("^true$|^True$|^TRUE$|^yes$|^Yes$|^YES$|^false$|^False$|^FALSE$|^no$|^No$|^NO$|^Y$|^y$|^N$|^n$");
    std::regex null_regex("^null$|^Null$|^NULL$|^nil$|^Nil$|^NIL$");
    std::regex undefined_regex("^undefined$");

    if (std::regex_match(idWithoutDollar, keyword_regex)) {
        return ParserToken{"keyword", id, start};
    } else if (std::regex_match(idWithoutDollar, boolean_regex)) {
        return ParserToken{"boolean", id, start};
    } else if (std::regex_match(idWithoutDollar, null_regex)) {
        return ParserToken{"null", id, start};
    } else if (std::regex_match(idWithoutDollar, undefined_regex)) {
        return ParserToken{"undefined", id, start};
    } else {
        bool isAllDigits = !id.empty();
        for (char c : id) {
            if (!std::isdigit(c)) {
                isAllDigits = false;
                break;
            }
        }

        if (isAllDigits) {
            return ParserToken{"number", id, start};
        } else {
            return ParserToken{"identifier", id, start};
        }
    }
}

void Lexer::skipWhitespace() {
    while (position < input.length() && isWhitespace(input[position])) {
        ++position;
        continue;
    }
}

bool Lexer::isJSXIdentifier(char ch) const {
    return isLetter(ch) || isDigit(ch) || ch == '_' || ch == '-' || ch == '.';
}

std::string Lexer::readJSXAttributeValue() {
    std::string value;
    if (input[position] == '"' || input[position] == '\'') {
        char quote = input[position];
        ++position;
        while (position < input.length() && input[position] != quote) {
            if (input[position] == '\\' && position + 1 < input.length()) {
                value += input[position];
                ++position;
                value += input[position];
                ++position;
            } else {
                value += input[position];
                ++position;
            }
        }
        if (input[position] == quote) ++position;
        return value;
    } else if (input[position] == '{') {
        ++position;
        int braceDepth_ = 1;
        std::string expr;
        while (position < input.length() && braceDepth_ > 0) {
            if (input[position] == '{') ++braceDepth_;
            else if (input[position] == '}') --braceDepth_;
            if (braceDepth_ > 0) {
                expr += input[position];
                ++position;
            }
        }
        if (input[position] == '}') ++position;
        return "{" + expr + "}";
    }
    return "";
}

ParserToken Lexer::readJSXOpeningTag() {
    size_t start = position;
    std::string tagName;
    
    while (position < input.length() && isJSXIdentifier(input[position])) {
        tagName += input[position];
        ++position;
    }
    
    std::unordered_map<std::string, std::string> attributes;
    bool selfClosing = false;
    
    while (position < input.length() && input[position] != '>' && input[position] != '/') {
        skipWhitespace();
        if (input[position] == '/') {
            ++position;
            if (input[position] == '>') {
                selfClosing = true;
                ++position;
                break;
            }
            continue;
        }
        
        std::string attrName;
        while (position < input.length() && isJSXIdentifier(input[position])) {
            attrName += input[position];
            ++position;
        }
        
        skipWhitespace();
        if (input[position] == '=') {
            ++position;
            skipWhitespace();
            std::string attrValue = readJSXAttributeValue();
            attributes[attrName] = attrValue;
        } else {
            attributes[attrName] = "true";
        }
        
        skipWhitespace();
    }
    
    if (!selfClosing && input[position] == '>') {
        ++position;
    }
    
    std::string attrsStr = "o'o{";
    bool first = true;
    for (const auto& [key, value] : attributes) {
        if (!first) attrsStr += ";";
        first = false;
        if (value.find('{') == 0 && value.back() == '}') {
            attrsStr += key + ": " + value.substr(1, value.length() - 2);
        } else {
            attrsStr += key + ": \"" + value + "\"";
        }
    }
    attrsStr += "}'";
    
    std::string result = "{\"type\":\"" + tagName + "\",\"props\":" + attrsStr + ",\"children\":";
    
    if (selfClosing) {
        result += "[]}";
        return ParserToken("jsx_element", result, start);
    }
    
    std::string children;
    int depth = 1;
    while (position < input.length() && depth > 0) {
        skipWhitespace();
        if (position < input.length() && input[position] == '<') {
            if (position + 1 < input.length() && peek() == '/') {
                --depth;
                if (depth == 0) break;
            } else if (peek() != ' ') {
                ++depth;
            }
        }
        
        if (depth > 0) {
            children += input[position];
            ++position;
        }
    }
    
    if (position + 1 < input.length() && input[position] == '<' && peek() == '/') {
        position += 2;
        while (position < input.length() && isJSXIdentifier(input[position])) {
            ++position;
        }
        if (input[position] == '>') ++position;
    }
    
    result += "[" + children + "]}";
    return ParserToken("jsx_element", result, start);
}

ParserToken Lexer::readJSX() {
    size_t start = position;
    ++position;
    
    if (position >= input.length()) {
        throw std::runtime_error("Unexpected EOF at Element at " + Utility::position(position, input));
    }
    
    if (input[position] == '/') {
        ++position;
        std::string tagName;
        while (position < input.length() && isJSXIdentifier(input[position])) {
            tagName += input[position];
            ++position;
        }
        if (input[position] == '>') ++position;
        return ParserToken("jsx_closing", tagName, start);
    }
    
    if (input[position] == '>') {
        ++position;
        return ParserToken("jsx_fragment_open", "", start);
    }
    
    return readJSXOpeningTag();
}

void Lexer::addDollarBefore() {
    if (dollarBefore) {
        dollarBefore = false;
        position--;
    }
}

std::string Lexer::waitForToString() {
    switch (waitFor) {
        case 1:
            return "}";
        case 2:
            return "]";
        case 3:
            return ")";
        default:
            return "";
    }
}

void Lexer::tokenize() {
    std::string warnPrefix = "";
    #ifndef __EMSCRIPTEN__
    if (Utility::isGitHubActions()) {
        warnPrefix = "::warning::";
    }
    #endif

    braceDepth = 0;
    bracketDepth = 0;
    parenthesisDepth = 0;
    waitFor = 0;
    firstBrace = 0;
    firstBracket = 0;
    firstParenthesis = 0;

    while (position < input.length()) {
        char ch = input[position];

        if (isWhitespace(ch)) {
            addDollarBefore();
            ++position;
            continue;
        }

        if (ch == '-' && peek() == '-') {
            if ((isDigit(input[position - 1]) || isLetter(input[position - 1])) && (peek(2) == ',' || peek(2) == '.' || peek(2) == ')')) {
                addDollarBefore();
                tokens.push_back(ParserToken{"--", "--", position});
                position += 2;
                continue;
            } else {
                addDollarBefore();
                readComment();
                continue;
            }
        }
        if (ch == '-' && peek() == '{') {
            addDollarBefore();
            readMultiLineComment();
            continue;
        }

        if (ch == '/' && peek() == '/') {
            if ((isDigit(input[position - 1]) || isLetter(input[position - 1])) && (peek(2) == ',' || peek(2) == '.' || peek(2) == ')')) {
                addDollarBefore();
                tokens.push_back(ParserToken{"//", "//", position});
                position += 2;
                continue;
            } else {
                addDollarBefore();
                position += 2;
                while (position < input.length() && input[position] != '\n') {
                    ++position;
                }
                if (position < input.length() && input[position] == '\n') {
                    ++position;
                }
                continue;
            }
        }
        if (ch == '/' && peek() == '*') {
            addDollarBefore();
            position += 2;
            while (position < input.length()) {
                if (input[position] == '*' && peek() == '/') {
                    position += 2;
                    break;
                }
                ++position;
            }
            continue;
        }

        if (ch == '"' || ch == '\'' || ch == '`') {
            addDollarBefore();
            tokens.push_back(readString(ch, ch == '\''));
            continue;
        }

        if (ch == '<' && peek() != '<' && peek() != '=') {
            addDollarBefore();
            tokens.push_back((position - 1) < 0 || ((position - 1) >= 0 && input[position - 1] == '(') ? readJSX() : readLink(true));
            continue;
        }

        if (ch == 'l' && peek() == '<') {
            addDollarBefore();
            ++position;
            tokens.push_back(readLink());
            continue;
        }
        if (ch == 't' && peek() == '<') {
            addDollarBefore();
            ++position;
            tokens.push_back(readLink(true));
            continue;
        }
        if (ch == 'e' && peek() == '<') {
            addDollarBefore();
            ++position;
            tokens.push_back(readJSX());
            continue;
        }

        if (ch == '=' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"==", "==", position});
            position += 2;
            continue;
        }

        if (ch == '?' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"?=", "?=", position});
            position += 2;
            continue;
        }

        if (ch == '?' && peek() == '?') {
            addDollarBefore();
            tokens.push_back(ParserToken{"??", "??", position});
            position += 2;
            continue;
        }

        if (ch == '?' && peek() == ':') {
            addDollarBefore();
            tokens.push_back(ParserToken{"?:", "?:", position});
            position += 2;
            continue;
        }

        if (ch == '=' && peek() == '!') {
            addDollarBefore();
            tokens.push_back(ParserToken{"=!", "=!", position});
            position += 2;
            continue;
        }

        if (ch == '?' && peek() == '!') {
            addDollarBefore();
            tokens.push_back(ParserToken{"?!", "?!", position});
            position += 2;
            continue;
        }

        if (ch == '!' && peek() == '?') {
            if (peek(1) == '?') {
                addDollarBefore();
                tokens.push_back(ParserToken{"!??", "!??", position});
                position += 3;
            } else {
                addDollarBefore();
                tokens.push_back(ParserToken{"!?", "!?", position});
                position += 2;
            }
            continue;
        }

        if (ch == '.' && peek() == '.' && peek(2) == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"..=", "..=", position});
            position += 2;
            continue;
        }
        if (ch == '.' && peek() == '.' && (position + 2) < input.length()) {
            addDollarBefore();
            tokens.push_back(ParserToken{"..", "..", position});
            position += 2;
            continue;
        }

        if (ch == '<' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"<=", "<=", position});
            position += 2;
            continue;
        }

        if (ch == '>' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{">=", ">=", position});
            position += 2;
            continue;
        }

        if (ch == '!' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"!=", "!=", position});
            position += 2;
            continue;
        }

        if (ch == '|' && peek() == '|' && peek(2) == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"||=", "||=", position});
            position += 3;
            continue;
        }
        if (ch == '|' && peek() == '|') {
            addDollarBefore();
            tokens.push_back(ParserToken{"||", "||", position});
            position += 2;
            continue;
        }

        if (ch == '|' && peek() == '>') {
            addDollarBefore();
            tokens.push_back(ParserToken{"|>", "|>", position});
            position += 2;
            continue;
        }

        if (ch == '!' && peek() == '|' && peek(2) == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"!|=", "!|=", position});
            position += 3;
            continue;
        }
        if (ch == '!' && peek() == '|') {
            addDollarBefore();
            tokens.push_back(ParserToken{"!|", "!|", position});
            position += 2;
            continue;
        }

        if (ch == '&' && peek() == '&' && peek(2) == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"&&=", "&&=", position});
            position += 3;
            continue;
        }
        if (ch == '&' && peek() == '&') {
            addDollarBefore();
            tokens.push_back(ParserToken{"&&", "&&", position});
            position += 2;
            continue;
        }

        if (ch == '!' && peek() == '&' && peek(2) == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"!&=", "!&=", position});
            position += 3;
            continue;
        }
        if (ch == '!' && peek() == '&') {
            addDollarBefore();
            tokens.push_back(ParserToken{"!&", "!&", position});
            position += 2;
            continue;
        }

        if (isDigit(ch) || (
            ch == '#' && isDigit(peek())
        )) {
            addDollarBefore();
            tokens.push_back(readNumber());
            continue;
        }
        if (ch == '#' && peek() == '&' && isDigit(peek(2))) {
            addDollarBefore();
            ++position;
            tokens.push_back(readNumber());
            continue;
        }

        if ((ch == 'l' || ch == 'j' || ch == 'c' || ch == 'o') && (peek() == '"' || peek() == '\'' || peek() == '`')) {
            addDollarBefore();
            char qch = peek();
            ++position;
            ParserToken str = readString(qch, qch == '\'');
            std::string type = ch == 'l' ? "Luau" : ch == 'j' ? "JavaScript" : ch == 'c' ? "JUSTC" : "JUSTO";
            tokens.push_back(ParserToken{type + (qch == '`' ? "_i" : ""), str.value, str.start});
            if (warn) {
                #ifdef __EMSCRIPTEN__
                warn_lexer_lang(Parser::getCurrentTimestamp().c_str(), Utility::position(position, input).c_str(), type.c_str());
                #else
                std::cout << warnPrefix + "Warning: " + type + " may be corrupted in the lexer output." << std::endl;
                #endif
            }
            continue;
        }

        if (ch == 'r' && (peek() == '"' || peek() == '\'' || peek() == '`')) {
            addDollarBefore();
            char qch = peek();
            ++position;
            bool isRaw = qch != '\'';
            ParserToken str = readString(qch, isRaw);
            if (isRaw && qch == '`') str.type = "string_ri";
            tokens.push_back(str);
            continue;
        }

        if (ch == '{') {
            addDollarBefore();
            ++braceDepth;
            tokens.push_back(ParserToken{"{", "{", position});
            if (firstBrace == 0) firstBrace = position;
            ++position;
            waitFor = 1;
            continue;
        }
        if (ch == '}') {
            if (braceDepth <= 0) throw std::runtime_error("Unmatched closing delimiter \"}\" at " + Utility::position(position, input));
            if (waitFor != 1 && waitFor != 0) throw std::runtime_error("Mismatched closing delimiter \"}\" (expected \"" + waitForToString() + "\") at " + Utility::position(position, input));
            addDollarBefore();
            --braceDepth;
            tokens.push_back(ParserToken{"}", "}", position});
            if (braceDepth <= 0) firstBrace = 0;
            ++position;
            waitFor = 0;
            continue;
        }

        if (ch == '[') {
            addDollarBefore();
            ++bracketDepth;
            tokens.push_back(ParserToken{"[", "[", position});
            if (firstBracket == 0) firstBracket = position;
            ++position;
            waitFor = 2;
            continue;
        }
        if (ch == ']') {
            if (bracketDepth <= 0) throw std::runtime_error("Unmatched closing delimiter \"]\" at " + Utility::position(position, input));
            if (waitFor != 2 && waitFor != 0) throw std::runtime_error("Mismatched closing delimiter \"]\" (expected \"" + waitForToString() + "\") at " + Utility::position(position, input));
            addDollarBefore();
            --bracketDepth;
            tokens.push_back(ParserToken{"]", "]", position});
            if (bracketDepth <= 0) firstBracket = 0;
            ++position;
            waitFor = 0;
            continue;
        }

        if (ch == '(') {
            addDollarBefore();
            ++parenthesisDepth;
            tokens.push_back(ParserToken{"(", "(", position});
            if (firstParenthesis == 0) firstParenthesis = position;
            ++position;
            waitFor = 3;
            continue;
        }
        if (ch == ')') {
            if (parenthesisDepth <= 0) throw std::runtime_error("Unmatched closing delimiter \")\" at " + Utility::position(position, input));
            if (waitFor != 3 && waitFor != 0) throw std::runtime_error("Mismatched closing delimiter \")\" (expected \"" + waitForToString() + "\") at " + Utility::position(position, input));
            addDollarBefore();
            --parenthesisDepth;
            tokens.push_back(ParserToken{")", ")", position});
            if (parenthesisDepth <= 0) firstParenthesis = 0;
            ++position;
            waitFor = 0;
            continue;
        }

        if (ch == ',' || ch == '.') {
            addDollarBefore();
            tokens.push_back(ParserToken{std::string(1, ch), std::string(1, ch), position});
            ++position;
            continue;
        }

        if (isIdentifierStart(ch)) {
            const ParserToken currToken = readIdentifier();
            const size_t currPos = position;

            if (currToken.type == "keyword") {
                if (currToken.value == "lgt") {
                    if (std::find(gotopos.begin(), gotopos.end(), currPos) != gotopos.end()) {
                        #ifdef __EMSCRIPTEN__
                        warn_lexer_goto(Parser::getCurrentTimestamp().c_str(), Utility::position(position, input).c_str());
                        #else
                        std::cout << warnPrefix + "Warning: Found goto loop at " + Utility::position(position, input) << std::endl;
                        #endif
                        continue;
                    }
                    gotopos.push_back(currPos);
                    try {
                        while(position < input.length() &&
                            isWhitespace(input[position]))
                        {
                            ++position;
                        }

                        ParserToken target = readNumber();
                        position = static_cast<size_t>(std::stod(target.value));

                        continue;
                    } catch (...) {
                        throw std::runtime_error("Invalid goto usage at " + Utility::position(position, input));
                    }
                }
            }

            tokens.push_back(currToken);
            continue;
        }

        if (ch == '-') {
            addDollarBefore();
            tokens.push_back(ParserToken{"minus", "-", position});
            ++position;
            continue;
        }

        if (ch == '$' && useDollarBefore) {
            dollarBefore = true;
            ++position;
            continue;
        }

        if (ch == '<' && peek() == '<') {
            addDollarBefore();
            tokens.push_back(ParserToken{"<<", "<<", position});
            position += 2;
            continue;
        }

        if (ch == '>' && peek() == '>') {
            addDollarBefore();
            tokens.push_back(ParserToken{">>", ">>", position});
            position += 2;
            continue;
        }

        if (ch == '*' && peek() == '*' && peek(2) == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"**=", "**=", position});
            position += 3;
            continue;
        }
        if (ch == '*' && peek() == '*') {
            addDollarBefore();
            tokens.push_back(ParserToken{"**", "**", position});
            position += 2;
            continue;
        }
        
        if (ch == '+' && peek() == '+') {
            addDollarBefore();
            tokens.push_back(ParserToken{"++", "++", position});
            position += 2;
            continue;
        }
        if (ch == '+' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"+=", "+=", position});
            position += 2;
            continue;
        }
        if (ch == '-' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"-=", "-=", position});
            position += 2;
            continue;
        }
        if (ch == '/' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"/=", "/=", position});
            position += 2;
            continue;
        }
        if (ch == '*' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"*=", "*=", position});
            position += 2;
            continue;
        }
        if (ch == '&' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"&=", "&=", position});
            position += 2;
            continue;
        }
        if (ch == '|' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"|=", "|=", position});
            position += 2;
            continue;
        }
        if (ch == '^' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"^=", "^=", position});
            position += 2;
            continue;
        }
        if (ch == '%' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"%=", "%=", position});
            position += 2;
            continue;
        }

        if (ch == '=' || ch == '?' || ch == '!' || ch == '<' ||
            ch == '>' || ch == '|' || ch == '&' || ch == '+' ||
            ch == '*' || ch == '/' || ch == '%' || ch == '^') {
            addDollarBefore();
            tokens.push_back(ParserToken{std::string(1, ch), std::string(1, ch), position});
            ++position;
            continue;
        }

        if (ch == ':' && peek() == ':') {
            addDollarBefore();
            tokens.push_back(ParserToken{"::", "::", position});
            position += 2;
            continue;
        }

        if (ch == '~' && peek() == '=') {
            addDollarBefore();
            tokens.push_back(ParserToken{"~=", "~=", position});
            position += 2;
            continue;
        }

        if (ch == '~') {
            addDollarBefore();
            tokens.push_back(ParserToken{"~", "~", position});
            ++position;
            continue;
        }

        if (ch == '^') {
            addDollarBefore();
            tokens.push_back(ParserToken{"^", "^", position});
            ++position;
            continue;
        }

        addDollarBefore();
        tokens.push_back(ParserToken{std::string(1, ch), std::string(1, ch), position});
        ++position;
    }

    if (braceDepth > 0) throw std::runtime_error("Unclosed delimiter \"{\" at " + Utility::position(firstBrace, input));
    if (bracketDepth > 0) throw std::runtime_error("Unclosed delimiter \"[\" at " + Utility::position(firstBracket, input));
    if (parenthesisDepth > 0) throw std::runtime_error("Unclosed delimiter \"(\" at " + Utility::position(firstParenthesis, input));

    addDollarBefore();
}

std::vector<ParserToken> Lexer::getTokens() const {
    if (!tokens.empty()) {
        const auto& lastToken = tokens.back();
        if (lastToken.type != "." && lastToken.value != "." && !(
            (tokens[0].type == "{" && lastToken.type == "}") ||
            (tokens[0].type == "[" && lastToken.type == "]")
        ) && requireDot) {
            throw std::runtime_error("Expected \".\", got EOF at " + Utility::position(lastToken.start, input));
        }
    }
    return tokens;
}

std::pair<std::string, std::vector<ParserToken>> Lexer::parse(const std::string& input, const bool& warn) {
    Lexer lexer(input, warn);
    return std::make_pair(input, lexer.getTokens());
}
