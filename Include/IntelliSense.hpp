#ifndef INTELLISENSE_HPP
#define INTELLISENSE_HPP
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <algorithm>
#include <cctype>
#include "Include/ScintillaHost.hpp"



// Provides instant local IntelliSense autocomplete and signature calltips for every language.
class IntelliSenseEngine {
private:
    // Extracts lowercase file extension including leading dot.
    static std::string ExtractLowerExt(const std::string& FileName) {
        size_t DotPos = FileName.find_last_of('.');

        if (DotPos == std::string::npos) {
            return "";
        }

        std::string Ext = FileName.substr(DotPos);
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), [](unsigned char Ch) {
            return static_cast<char>(std::tolower(Ch));
        });

        return Ext;
    }

    // Checks whether a character is part of a code identifier (allowing '-' in CSS/HTML).
    static bool IsIdentChar(char Ch, bool AllowHyphen) {
        const int ZeroVal = 0;

        return (std::isalnum(static_cast<unsigned char>(Ch)) != ZeroVal || Ch == '_' || (AllowHyphen && Ch == '-'));
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

    // Inserts a fixed array of keywords into the candidate set.
    template <size_t Count>
    static void InsertWordArray(std::set<std::string>& Candidates, const char* (&Words)[Count]) {
        for (const char* Word : Words) {
            Candidates.insert(Word);
        }
    }

    // Adds language keywords, tags, properties, and common standard identifiers into the candidate set.
    static void AddLanguageKeywords(std::set<std::string>& Candidates, const std::string& FileName) {
        std::string Ext = ExtractLowerExt(FileName);

        if (Ext == ".html" || Ext == ".htm" || Ext == ".xml" || Ext == ".svg") {
            const char* HtmlWords[] = {
                "a", "article", "aside", "audio", "blockquote", "body", "br", "button", "canvas",
                "charset", "class", "code", "content", "data-", "details", "dialog", "disabled",
                "div", "doctype", "em", "footer", "form", "h1", "h2", "h3", "h4", "h5", "h6",
                "head", "header", "height", "hr", "href", "html", "id", "iframe", "img", "input",
                "label", "lang", "li", "link", "main", "meta", "name", "nav", "ol", "onchange",
                "onclick", "onsubmit", "option", "p", "path", "placeholder", "pre", "readonly",
                "rel", "required", "script", "section", "select", "span", "src", "strong", "style",
                "summary", "svg", "table", "target", "tbody", "td", "textarea", "th", "thead",
                "title", "tr", "type", "ul", "value", "video", "viewport", "width"
            };
            InsertWordArray(Candidates, HtmlWords);
        } else if (Ext == ".css" || Ext == ".scss" || Ext == ".less") {
            const char* CssWords[] = {
                "absolute", "active", "align-items", "animation", "auto", "background",
                "background-color", "block", "border", "border-bottom", "border-color",
                "border-radius", "bottom", "box-shadow", "box-sizing", "calc", "center", "color",
                "column", "cursor", "display", "fixed", "flex", "flex-direction", "flex-wrap",
                "focus", "font-family", "font-size", "font-weight", "gap", "grid",
                "grid-template-columns", "height", "hidden", "hover", "inherit", "inline",
                "inline-block", "justify-content", "keyframes", "left", "line-height",
                "linear-gradient", "margin", "max-height", "max-width", "media", "min-height",
                "min-width", "none", "opacity", "outline", "overflow", "padding", "pointer",
                "position", "relative", "rgba", "right", "row", "solid", "sticky", "text-align",
                "text-decoration", "top", "transform", "transition", "transparent", "var",
                "white-space", "width", "z-index"
            };
            InsertWordArray(Candidates, CssWords);
        } else if (Ext == ".py") {
            const char* PyWords[] = {
                "and", "as", "assert", "async", "await", "bool", "break", "case", "class",
                "classmethod", "continue", "def", "del", "dict", "elif", "else", "enumerate",
                "except", "False", "finally", "float", "for", "from", "global", "if", "import",
                "in", "int", "is", "isinstance", "lambda", "len", "list", "match", "None",
                "nonlocal", "not", "open", "or", "pass", "print", "property", "raise", "range",
                "return", "self", "set", "staticmethod", "str", "super", "True", "try", "tuple",
                "while", "with", "yield", "zip"
            };
            InsertWordArray(Candidates, PyWords);
        } else if (Ext == ".js" || Ext == ".jsx" || Ext == ".ts" || Ext == ".tsx" || Ext == ".mjs") {
            const char* JsWords[] = {
                "addEventListener", "Array", "async", "await", "boolean", "break", "case", "catch",
                "class", "console", "const", "constructor", "continue", "default", "document",
                "else", "enum", "export", "extends", "false", "fetch", "finally", "for", "from",
                "function", "getElementById", "if", "implements", "import", "interface", "JSON",
                "let", "localStorage", "Map", "null", "number", "Object", "private", "Promise",
                "protected", "public", "querySelector", "querySelectorAll", "readonly", "return",
                "Set", "setInterval", "setTimeout", "static", "string", "super", "switch", "this",
                "throw", "true", "try", "type", "typeof", "undefined", "void", "while", "window"
            };
            InsertWordArray(Candidates, JsWords);
        } else if (Ext == ".rs") {
            const char* RustWords[] = {
                "Arc", "as", "async", "await", "Box", "break", "const", "continue", "crate",
                "else", "enum", "Err", "expect", "extern", "false", "fn", "for", "format!", "if",
                "impl", "in", "let", "loop", "match", "mod", "move", "mut", "None", "Ok", "Option",
                "println!", "pub", "Rc", "ref", "Result", "return", "self", "Self", "Some",
                "static", "String", "struct", "super", "trait", "true", "type", "unsafe", "unwrap",
                "use", "Vec", "where", "while"
            };
            InsertWordArray(Candidates, RustWords);
        } else if (Ext == ".go") {
            const char* GoWords[] = {
                "append", "bool", "break", "byte", "cap", "case", "chan", "const", "continue",
                "default", "defer", "else", "error", "fallthrough", "false", "fmt", "for", "func",
                "go", "goto", "if", "import", "int", "interface", "len", "make", "map", "nil",
                "package", "Printf", "Println", "range", "return", "select", "Sprintf", "string",
                "struct", "switch", "true", "type", "var"
            };
            InsertWordArray(Candidates, GoWords);
        } else if (Ext == ".java" || Ext == ".cs") {
            const char* ManagedWords[] = {
                "abstract", "ArrayList", "async", "await", "base", "bool", "boolean", "break",
                "case", "catch", "class", "Console", "const", "continue", "default", "Dictionary",
                "double", "else", "enum", "extends", "false", "final", "finally", "float", "for",
                "foreach", "HashMap", "if", "implements", "import", "int", "interface", "List",
                "long", "Map", "namespace", "new", "null", "override", "package", "private",
                "println", "protected", "public", "readonly", "return", "static", "string",
                "String", "struct", "super", "switch", "System", "this", "throw", "throws", "true",
                "try", "using", "var", "virtual", "void", "while", "WriteLine"
            };
            InsertWordArray(Candidates, ManagedWords);
        } else if (Ext == ".sql") {
            const char* SqlWords[] = {
                "ALTER", "AND", "AS", "ASC", "AVG", "BY", "COUNT", "CREATE", "DELETE", "DESC",
                "DISTINCT", "DROP", "FOREIGN", "FROM", "GROUP", "HAVING", "INDEX", "INNER",
                "INSERT", "INTO", "JOIN", "KEY", "LEFT", "LIMIT", "MAX", "MIN", "NOT", "NULL",
                "OFFSET", "ON", "OR", "ORDER", "PRIMARY", "REFERENCES", "RIGHT", "SELECT", "SET",
                "SUM", "TABLE", "UPDATE", "VALUES", "WHERE"
            };
            InsertWordArray(Candidates, SqlWords);
        } else if (Ext == ".ps1" || Ext == ".sh" || Ext == ".bat" || Ext == ".cmd") {
            const char* ShellWords[] = {
                "break", "call", "continue", "echo", "elif", "else", "elseif", "export", "for",
                "foreach", "function", "Get-ChildItem", "Get-Content", "Get-Item", "goto", "if",
                "local", "New-Item", "param", "Remove-Item", "return", "Select-Object",
                "Select-String", "set", "Set-Content", "switch", "while", "Write-Host",
                "Write-Output"
            };
            InsertWordArray(Candidates, ShellWords);
        } else if (Ext == ".c" || Ext == ".cpp" || Ext == ".cc" || Ext == ".cxx" || Ext == ".h" || Ext == ".hpp") {
            const char* CppWords[] = {
                "auto", "begin", "bool", "break", "case", "catch", "cerr", "char", "class",
                "clear", "const", "constexpr", "continue", "cout", "default", "delete", "double",
                "dynamic_cast", "else", "emplace_back", "empty", "end", "endl", "enum", "explicit",
                "extern", "false", "filesystem", "find", "float", "for", "function", "if",
                "ifstream", "include", "inline", "int", "lock_guard", "long", "make_shared",
                "make_unique", "map", "mutex", "namespace", "new", "nullptr", "ofstream",
                "override", "private", "protected", "public", "push_back", "reinterpret_cast",
                "return", "set", "shared_ptr", "size", "size_t", "sizeof", "sort", "static",
                "static_cast", "std", "string", "stringstream", "struct", "switch", "template",
                "this", "thread", "throw", "true", "try", "typedef", "typename", "uint32_t",
                "unique_ptr", "unordered_map", "unsigned", "using", "vector", "virtual", "void",
                "while"
            };
            InsertWordArray(Candidates, CppWords);
        }
    }

    // Harvests identifier tokens from a source string into the candidate set.
    static void HarvestTokens(std::set<std::string>& Candidates, const std::string& Text, const std::string& SkipWord, bool AllowHyphen) {
        const size_t ZeroIdx = 0;
        const size_t MinTokenLen = 1 + 1;
        const size_t MaxScanBytes = 250000;

        size_t Limit = (Text.size() < MaxScanBytes) ? Text.size() : MaxScanBytes;
        size_t Idx = ZeroIdx;

        while (Idx < Limit) {
            if (std::isalpha(static_cast<unsigned char>(Text[Idx])) != static_cast<int>(ZeroIdx) || Text[Idx] == '_') {
                size_t Start = Idx;

                while (Idx < Limit && IsIdentChar(Text[Idx], AllowHyphen)) {
                    Idx++;
                }

                size_t TokenLen = Idx - Start;

                if (TokenLen >= MinTokenLen) {
                    std::string Token = Text.substr(Start, TokenLen);

                    if (Token != SkipWord) {
                        Candidates.insert(Token);
                    }
                }
            } else {
                Idx++;
            }
        }
    }

public:
    // Triggers local IntelliSense autocomplete popup from document symbols, open tabs, and language dictionaries.
    static void TriggerAutoComplete(ScintillaHost& Host, const std::string& FileName, bool ForceShow, const std::vector<std::string>& ExtraSymbols = {}) {
        const int ZeroInt = 0;
        const int OneInt = 1;
        const int MinAutoPrefixLen = 1 + 1;
        const size_t MaxPopupItems = 35;

        if (!Host.IsReady()) {
            return;
        }

        std::string Ext = ExtractLowerExt(FileName);
        bool AllowHyphen = (Ext == ".css" || Ext == ".scss" || Ext == ".less" || Ext == ".html" || Ext == ".htm" || Ext == ".xml" || Ext == ".svg");
        int Pos = Host.GetCurrentPos();
        int WordStart = Pos;

        while (WordStart > ZeroInt) {
            char Ch = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(WordStart - OneInt)));

            if (!IsIdentChar(Ch, AllowHyphen)) {
                break;
            }

            WordStart--;
        }

        int PrefixLen = Pos - WordStart;
        bool AfterHtmlTagOpen = (AllowHyphen && WordStart > ZeroInt && static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(WordStart - OneInt))) == '<');

        if (!ForceShow && !AfterHtmlTagOpen && PrefixLen < MinAutoPrefixLen) {
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
        HarvestTokens(Candidates, Host.GetText(), Prefix, AllowHyphen);

        for (const auto& Extra : ExtraSymbols) {
            if (!Extra.empty() && Extra != Prefix) {
                Candidates.insert(Extra);
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
