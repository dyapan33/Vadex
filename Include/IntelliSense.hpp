#ifndef INTELLISENSE_HPP
#define INTELLISENSE_HPP
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <algorithm>
#include <cctype>
#include "Include/ScintillaHost.hpp"



// Provides instant local IntelliSense autocomplete and signature calltips for Scintilla.
class IntelliSenseEngine {
private:
    // Checks whether a character is part of a code identifier.
    static bool IsIdentChar(char Ch) {
        const int ZeroVal = 0;

        return (std::isalnum(static_cast<unsigned char>(Ch)) != ZeroVal || Ch == '_');
    }

    // Checks whether Candidate starts with Prefix using case-insensitive comparison.
    static bool StartsWithInsensitive(const std::string& Candidate, const std::string& Prefix) {
        const size_t ZeroIdx = 0;

        if (Prefix.size() > Candidate.size()) {
            return false;
        }

        for (size_t Idx = ZeroIdx; Idx < Prefix.size(); ++Idx) {
            char A = static_cast<char>(std::tolower(static_cast<unsigned char>(Candidate[Idx])));
            char B = static_cast<char>(std::tolower(static_cast<unsigned char>(Prefix[Idx])));

            if (A != B) {
                return false;
            }
        }

        return true;
    }

    // Adds language keywords and common standard identifiers into the candidate set.
    static void AddLanguageKeywords(std::set<std::string>& Candidates, const std::string& FileName) {
        size_t DotPos = FileName.find_last_of('.');
        std::string Ext = (DotPos == std::string::npos) ? "" : FileName.substr(DotPos);
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), [](unsigned char Ch) {
            return static_cast<char>(std::tolower(Ch));
        });

        if (Ext == ".py") {
            const char* PyWords[] = {
                "and", "as", "assert", "async", "await", "break", "class", "continue", "def",
                "elif", "else", "except", "False", "finally", "for", "from", "global", "if",
                "import", "in", "is", "lambda", "len", "list", "None", "not", "or", "pass",
                "print", "raise", "range", "return", "self", "str", "True", "try", "while", "with", "yield"
            };

            for (const char* Word : PyWords) {
                Candidates.insert(Word);
            }
        } else if (Ext == ".js" || Ext == ".jsx" || Ext == ".ts" || Ext == ".tsx") {
            const char* JsWords[] = {
                "Array", "async", "await", "boolean", "break", "case", "catch", "class", "console",
                "const", "continue", "default", "document", "else", "export", "extends", "false",
                "finally", "for", "from", "function", "if", "import", "interface", "let", "Map",
                "null", "number", "Object", "Promise", "return", "Set", "string", "super", "switch",
                "this", "throw", "true", "try", "type", "typeof", "undefined", "void", "while", "window"
            };

            for (const char* Word : JsWords) {
                Candidates.insert(Word);
            }
        } else {
            const char* CppWords[] = {
                "auto", "bool", "break", "case", "catch", "char", "class", "const", "constexpr",
                "continue", "default", "delete", "double", "else", "enum", "explicit", "extern",
                "false", "float", "for", "if", "include", "inline", "int", "long", "namespace",
                "new", "nullptr", "override", "private", "protected", "public", "reinterpret_cast",
                "return", "size_t", "sizeof", "static", "static_cast", "std", "string", "struct",
                "switch", "template", "this", "throw", "true", "try", "typedef", "typename",
                "uint32_t", "unsigned", "using", "vector", "virtual", "void", "while"
            };

            for (const char* Word : CppWords) {
                Candidates.insert(Word);
            }
        }
    }

public:
    // Triggers local IntelliSense autocomplete popup from document symbols and language keywords.
    static void TriggerAutoComplete(ScintillaHost& Host, const std::string& FileName, bool ForceShow) {
        const int ZeroInt = 0;
        const int OneInt = 1;
        const int MinAutoPrefixLen = 1 + 1;
        const size_t MaxPopupItems = 35;

        if (!Host.IsReady()) {
            return;
        }

        int Pos = Host.GetCurrentPos();
        int WordStart = static_cast<int>(Host.Call(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(Pos), OneInt));
        int PrefixLen = Pos - WordStart;

        if (!ForceShow && PrefixLen < MinAutoPrefixLen) {
            return;
        }

        std::string Prefix;

        if (PrefixLen > ZeroInt) {
            Prefix.resize(static_cast<size_t>(PrefixLen));

            for (int Idx = ZeroInt; Idx < PrefixLen; ++Idx) {
                Prefix[static_cast<size_t>(Idx)] = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(WordStart + Idx)));
            }
        }

        std::set<std::string> Candidates;
        AddLanguageKeywords(Candidates, FileName);

        // Scans active document text to harvest identifiers, classes, variables, and functions.
        std::string DocText = Host.GetText();
        size_t Idx = ZeroInt;

        while (Idx < DocText.size()) {
            if (std::isalpha(static_cast<unsigned char>(DocText[Idx])) != ZeroInt || DocText[Idx] == '_') {
                size_t Start = Idx;

                while (Idx < DocText.size() && IsIdentChar(DocText[Idx])) {
                    Idx++;
                }

                size_t TokenLen = Idx - Start;

                if (TokenLen >= static_cast<size_t>(MinAutoPrefixLen)) {
                    std::string Token = DocText.substr(Start, TokenLen);

                    if (Token != Prefix) {
                        Candidates.insert(Token);
                    }
                }
            } else {
                Idx++;
            }
        }

        std::string JoinedList;
        size_t AddedCount = ZeroInt;

        for (const auto& Candidate : Candidates) {
            if (AddedCount >= MaxPopupItems) {
                break;
            }

            if (Prefix.empty() || StartsWithInsensitive(Candidate, Prefix)) {
                if (!JoinedList.empty()) {
                    JoinedList.push_back(' ');
                }
                JoinedList += Candidate;
                AddedCount++;
            }
        }

        if (!JoinedList.empty()) {
            Host.ShowAutoComplete(PrefixLen, JoinedList);
        }
    }

    // Displays a signature calltip when typing '(' after a function defined in the active file.
    static void TriggerSignatureCallTip(ScintillaHost& Host) {
        const int ZeroInt = 0;
        const int OneInt = 1;
        const int TwoInt = 1 + 1;

        if (!Host.IsReady()) {
            return;
        }

        int Pos = Host.GetCurrentPos();

        if (Pos < TwoInt) {
            return;
        }

        int NameEnd = Pos - OneInt;
        int NameStart = static_cast<int>(Host.Call(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(NameEnd), OneInt));
        int NameLen = NameEnd - NameStart;

        if (NameLen <= ZeroInt) {
            return;
        }

        std::string FuncName(static_cast<size_t>(NameLen), '\0');

        for (int Idx = ZeroInt; Idx < NameLen; ++Idx) {
            FuncName[static_cast<size_t>(Idx)] = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(NameStart + Idx)));
        }

        std::string DocText = Host.GetText();
        std::istringstream Stream(DocText);
        std::string Line;
        std::string SearchToken = FuncName + "(";

        while (std::getline(Stream, Line)) {
            size_t Found = Line.find(SearchToken);

            if (Found != std::string::npos && Line.find(')', Found) != std::string::npos && (Line.find('{') != std::string::npos || Line.find(';') != std::string::npos || Line.find("def ") != std::string::npos)) {
                size_t TrimStart = Line.find_first_not_of(" \t");
                std::string Sig = (TrimStart == std::string::npos) ? Line : Line.substr(TrimStart);

                if (Sig.find(FuncName + "(") != ZeroInt || Sig.find("def ") == ZeroInt) {
                    Host.ShowCallTip(Pos, Sig);

                    return;
                }
            }
        }
    }
};

#endif
