#ifndef SYMBOL_OUTLINE_HPP
#define SYMBOL_OUTLINE_HPP
#include <string>
#include <vector>
#include <sstream>
#include <cctype>



// Represents a single navigable symbol extracted from source code.
struct OutlineSymbol {
    std::string Kind;
    std::string Name;
    int LineNumber;
};

// Extracts classes, structs, enums, functions, and headings for the Symbol Outline view.
class SymbolOutline {
private:
    // Trims leading and trailing ASCII whitespace.
    static std::string Trim(const std::string& Text) {
        const size_t OneOffset = 1;
        size_t Start = Text.find_first_not_of(" \t\r\n");

        if (Start == std::string::npos) {
            return "";
        }

        size_t End = Text.find_last_not_of(" \t\r\n");

        return Text.substr(Start, End - Start + OneOffset);
    }

    // Extracts the first identifier word starting at StartPos.
    static std::string ReadIdentifier(const std::string& Text, size_t StartPos) {
        const int ZeroInt = 0;
        size_t Pos = Text.find_first_not_of(" \t", StartPos);

        if (Pos == std::string::npos) {
            return "";
        }

        size_t End = Pos;

        while (End < Text.size() && (std::isalnum(static_cast<unsigned char>(Text[End])) != ZeroInt || Text[End] == '_' || Text[End] == ':' || Text[End] == '~')) {
            End++;
        }

        return Text.substr(Pos, End - Pos);
    }

public:
    // Scans source text and returns a list of top-level and member symbols with line numbers.
    static std::vector<OutlineSymbol> Extract(const std::string& SourceText, const std::string& FileName) {
        const int OneLine = 1;
        const size_t ZeroIdx = 0;
        const size_t ClassLen = 5;
        const size_t StructLen = 5 + 1;
        const size_t EnumLen = 5 - 1;
        const size_t DefLen = 3;
        const size_t FuncLen = 9 - 1;

        std::vector<OutlineSymbol> Symbols;
        std::istringstream Stream(SourceText);
        std::string RawLine;
        int LineNum = OneLine;
        bool IsMarkdown = (FileName.size() >= DefLen && FileName.substr(FileName.size() - DefLen) == ".md");

        while (std::getline(Stream, RawLine)) {
            std::string Line = Trim(RawLine);

            if (Line.empty()) {
                LineNum++;
                continue;
            }

            if (IsMarkdown) {
                if (Line[ZeroIdx] == '#') {
                    Symbols.push_back({ "H", Line, LineNum });
                }

                LineNum++;
                continue;
            }

            if (Line.rfind("//", ZeroIdx) == ZeroIdx || Line[ZeroIdx] == '#' || Line[ZeroIdx] == '*') {
                LineNum++;
                continue;
            }

            if (Line.rfind("class ", ZeroIdx) == ZeroIdx) {
                std::string Name = ReadIdentifier(Line, ClassLen);

                if (!Name.empty()) {
                    Symbols.push_back({ "class", Name, LineNum });
                }
            } else if (Line.rfind("struct ", ZeroIdx) == ZeroIdx) {
                std::string Name = ReadIdentifier(Line, StructLen);

                if (!Name.empty()) {
                    Symbols.push_back({ "struct", Name, LineNum });
                }
            } else if (Line.rfind("enum ", ZeroIdx) == ZeroIdx) {
                std::string Name = ReadIdentifier(Line, EnumLen);

                if (!Name.empty()) {
                    Symbols.push_back({ "enum", Name, LineNum });
                }
            } else if (Line.rfind("def ", ZeroIdx) == ZeroIdx) {
                std::string Name = ReadIdentifier(Line, DefLen);

                if (!Name.empty()) {
                    Symbols.push_back({ "def", Name, LineNum });
                }
            } else if (Line.rfind("function ", ZeroIdx) == ZeroIdx) {
                std::string Name = ReadIdentifier(Line, FuncLen);

                if (!Name.empty()) {
                    Symbols.push_back({ "fn", Name, LineNum });
                }
            } else {
                // Detects C/C++/JS function definitions containing '(' and ending with '{'.
                size_t ParenPos = Line.find('(');

                if (ParenPos != std::string::npos && Line.back() == '{') {
                    std::string Prefix = Trim(Line.substr(ZeroIdx, ParenPos));
                    size_t LastSpace = Prefix.find_last_of(" \t*&");
                    const size_t OneStep = 1;
                    std::string FuncName = (LastSpace == std::string::npos) ? Prefix : Prefix.substr(LastSpace + OneStep);

                    if (!FuncName.empty() && FuncName != "if" && FuncName != "for" && FuncName != "while" && FuncName != "switch" && FuncName != "catch") {
                        Symbols.push_back({ "fn", FuncName, LineNum });
                    }
                }
            }

            LineNum++;
        }

        return Symbols;
    }
};

#endif
