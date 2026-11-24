#include "Include/EditorFeatures.hpp"
#include <cctype>
#include <algorithm>



const int ZeroVal = 0;
const int OneVal = 1;
const int TwoVal = 1 + 1;
const int ThreeVal = 3;
const int NegativeOneVal = -1;
const int MaxOccurrenceDocSize = 250000;
const int MaxBracketScanLines = 500;

// Initializes snippet tracking and selection state.
EditorFeatures::EditorFeatures() {
    ActiveSnippetIndex = NegativeOneVal;
    IsSnippetActive = false;
    IsUpdatingUi = false;
    ColumnSelectionEnabled = false;
}

// Checks whether a byte value is an identifier word character.
bool EditorFeatures::IsWordChar(int Ch) const {
    return (std::isalnum(static_cast<unsigned char>(Ch)) != ZeroVal || Ch == '_');
}

// Maps an opening bracket or quote character to its matching closing character.
bool EditorFeatures::IsOpenPair(char Ch, char& OutClose) const {
    if (Ch == '(') {
        OutClose = ')';

        return true;
    }
    if (Ch == '{') {
        OutClose = '}';

        return true;
    }
    if (Ch == '[') {
        OutClose = ']';

        return true;
    }
    if (Ch == '"') {
        OutClose = '"';

        return true;
    }
    if (Ch == '\'') {
        OutClose = '\'';

        return true;
    }
    if (Ch == '`') {
        OutClose = '`';

        return true;
    }

    return false;
}

// Checks whether a character is a closing bracket or quote.
bool EditorFeatures::IsClosePair(char Ch) const {
    return (Ch == ')' || Ch == '}' || Ch == ']' || Ch == '"' || Ch == '\'' || Ch == '`');
}

// Clears active snippet tab-stop indicators and resets state.
void EditorFeatures::ClearSnippetState(ScintillaHost& Host) {
    int DocLen = static_cast<int>(Host.Call(SCI_GETLENGTH));
    Host.ClearIndicatorRange(IndicatorSnippetStop, ZeroVal, DocLen);
    ActiveSnippetStops.clear();
    ActiveSnippetIndex = NegativeOneVal;
    IsSnippetActive = false;
}

// Selects the text range of the specified snippet tab stop.
void EditorFeatures::SelectSnippetStop(ScintillaHost& Host, int StopIndex) {
    if (StopIndex < ZeroVal || StopIndex >= static_cast<int>(ActiveSnippetStops.size())) {
        ClearSnippetState(Host);

        return;
    }

    const SnippetTabStop& Stop = ActiveSnippetStops[static_cast<size_t>(StopIndex)];
    Host.Call(SCI_SETSEL, static_cast<uptr_t>(Stop.StartPos), static_cast<sptr_t>(Stop.EndPos));
}

// Handles auto-closing brackets/quotes, closing skip-over, and smart auto-indent on newline.
void EditorFeatures::OnCharAdded(ScintillaHost& Host, int Ch, int TabSize) {
    if (!Host.IsReady()) {
        return;
    }

    int Pos = Host.GetCurrentPos();
    char TypedChar = static_cast<char>(Ch);

    // Handles smart auto-indentation when pressing Enter.
    if (TypedChar == '\n' || TypedChar == '\r') {
        if (TypedChar == '\r') {
            return;
        }

        int CurLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(Pos)));

        if (CurLine <= ZeroVal) {
            return;
        }

        int PrevLine = CurLine - OneVal;
        int PrevIndent = static_cast<int>(Host.Call(SCI_GETLINEINDENTATION, static_cast<uptr_t>(PrevLine)));
        std::string PrevText = Host.GetLineText(PrevLine);

        // Strips trailing whitespace from previous line to inspect its last significant character.
        while (!PrevText.empty() && (PrevText.back() == ' ' || PrevText.back() == '\t')) {
            PrevText.pop_back();
        }

        bool OpensBlock = (!PrevText.empty() && (PrevText.back() == '{' || PrevText.back() == ':' || PrevText.back() == '(' || PrevText.back() == '['));
        int NextChar = static_cast<int>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(Pos)));
        bool SplitsBracePair = (OpensBlock && !PrevText.empty() && PrevText.back() == '{' && NextChar == '}');

        if (SplitsBracePair) {
            std::string InnerIndent(static_cast<size_t>(PrevIndent + TabSize), ' ');
            std::string OuterIndent(static_cast<size_t>(PrevIndent), ' ');
            std::string Insertion = InnerIndent + "\n" + OuterIndent;

            Host.Call(SCI_BEGINUNDOACTION);
            Host.Call(SCI_INSERTTEXT, static_cast<uptr_t>(Pos), reinterpret_cast<sptr_t>(Insertion.c_str()));
            Host.Call(SCI_GOTOPOS, static_cast<uptr_t>(Pos + static_cast<int>(InnerIndent.size())));
            Host.Call(SCI_ENDUNDOACTION);

            return;
        }

        int TargetIndent = OpensBlock ? (PrevIndent + TabSize) : PrevIndent;

        if (TargetIndent > ZeroVal) {
            Host.Call(SCI_SETLINEINDENTATION, static_cast<uptr_t>(CurLine), static_cast<sptr_t>(TargetIndent));
            int NewPos = static_cast<int>(Host.Call(SCI_GETLINEINDENTPOSITION, static_cast<uptr_t>(CurLine)));
            Host.Call(SCI_GOTOPOS, static_cast<uptr_t>(NewPos));
        }

        return;
    }

    // Skips over an existing matching closing character instead of duplicating it.
    if (IsClosePair(TypedChar)) {
        int NextChar = static_cast<int>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(Pos)));

        if (NextChar == Ch) {
            Host.Call(SCI_DELETERANGE, static_cast<uptr_t>(Pos), OneVal);

            return;
        }
    }

    // Auto-inserts the matching closing bracket or quote.
    char CloseChar = '\0';

    if (IsOpenPair(TypedChar, CloseChar)) {
        int NextChar = static_cast<int>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(Pos)));

        if (IsWordChar(NextChar)) {
            return;
        }

        // Avoids doubling quotes when typing an apostrophe inside a word.
        if (TypedChar == '\'' && Pos >= TwoVal) {
            int PrevChar = static_cast<int>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(Pos - TwoVal)));

            if (IsWordChar(PrevChar)) {
                return;
            }
        }

        char CloseBuf[TwoVal] = { CloseChar, '\0' };
        Host.Call(SCI_INSERTTEXT, static_cast<uptr_t>(Pos), reinterpret_cast<sptr_t>(CloseBuf));
        Host.Call(SCI_GOTOPOS, static_cast<uptr_t>(Pos));
    }
}

// Deletes both characters when pressing Backspace inside an empty bracket or quote pair.
bool EditorFeatures::HandleBackspacePair(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return false;
    }

    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));

    if (SelStart != SelEnd || SelStart <= ZeroVal) {
        return false;
    }

    char PrevChar = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(SelStart - OneVal)));
    char NextChar = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(SelStart)));
    char ExpectedClose = '\0';

    if (IsOpenPair(PrevChar, ExpectedClose) && NextChar == ExpectedClose) {
        Host.Call(SCI_BEGINUNDOACTION);
        Host.Call(SCI_DELETERANGE, static_cast<uptr_t>(SelStart - OneVal), TwoVal);
        Host.Call(SCI_ENDUNDOACTION);

        return true;
    }

    return false;
}

// Expands built-in snippets or advances to the next active snippet tab stop on Tab.
bool EditorFeatures::HandleTabSnippet(ScintillaHost& Host, const std::string& FileName) {
    if (!Host.IsReady()) {
        return false;
    }

    // Advances to the next tab stop when a snippet is currently active.
    if (IsSnippetActive) {
        ActiveSnippetIndex++;

        if (ActiveSnippetIndex < static_cast<int>(ActiveSnippetStops.size())) {
            SelectSnippetStop(Host, ActiveSnippetIndex);

            return true;
        }

        ClearSnippetState(Host);

        return false;
    }

    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));

    if (SelStart != SelEnd) {
        return false;
    }

    int WordStart = static_cast<int>(Host.Call(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(SelStart), OneVal));
    int WordLen = SelStart - WordStart;

    const int MaxSnippetTriggerLength = 15;

    if (WordLen <= ZeroVal || WordLen > MaxSnippetTriggerLength) {
        return false;
    }

    std::string Trigger(static_cast<size_t>(WordLen), '\0');

    for (int Idx = ZeroVal; Idx < WordLen; ++Idx) {
        Trigger[static_cast<size_t>(Idx)] = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(WordStart + Idx)));
    }

    int CurLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelStart)));
    int IndentCount = static_cast<int>(Host.Call(SCI_GETLINEINDENTATION, static_cast<uptr_t>(CurLine)));
    std::string BaseIndent(static_cast<size_t>(IndentCount), ' ');
    std::string InnerIndent = BaseIndent + "    ";

    // Matches common C++/JS/Python snippet triggers with $1/$2/$0 tab stops.
    std::string Template = "";
    bool IsPython = (FileName.size() >= ThreeVal && FileName.substr(FileName.size() - ThreeVal) == ".py");

    if (Trigger == "for" && !IsPython) {
        Template = "for (int $1 = 0; $1 < $2; ++$1) {\n" + InnerIndent + "$0\n" + BaseIndent + "}";
    } else if (Trigger == "if" && !IsPython) {
        Template = "if ($1) {\n" + InnerIndent + "$0\n" + BaseIndent + "}";
    } else if (Trigger == "ife" && !IsPython) {
        Template = "if ($1) {\n" + InnerIndent + "$2\n" + BaseIndent + "} else {\n" + InnerIndent + "$0\n" + BaseIndent + "}";
    } else if (Trigger == "while" && !IsPython) {
        Template = "while ($1) {\n" + InnerIndent + "$0\n" + BaseIndent + "}";
    } else if (Trigger == "cls" && !IsPython) {
        Template = "class $1 {\n" + BaseIndent + "public:\n" + InnerIndent + "$0\n" + BaseIndent + "};";
    } else if (Trigger == "inc" && !IsPython) {
        Template = "#include \"$1\"$0";
    } else if (Trigger == "def" && IsPython) {
        Template = "def $1($2):\n" + InnerIndent + "$0";
    } else if (Trigger == "log") {
        Template = "console.log($1);$0";
    } else {
        return false;
    }

    // Parses $1, $2, $0 placeholders into concrete text and byte-offset stops.
    std::string Expanded = "";
    SnippetTabStop StopOne = { NegativeOneVal, NegativeOneVal };
    SnippetTabStop StopTwo = { NegativeOneVal, NegativeOneVal };
    SnippetTabStop StopZero = { NegativeOneVal, NegativeOneVal };

    for (size_t Idx = ZeroVal; Idx < Template.size(); ++Idx) {
        if (Template[Idx] == '$' && (Idx + OneVal) < Template.size()) {
            char Digit = Template[Idx + OneVal];
            int Offset = WordStart + static_cast<int>(Expanded.size());

            if (Digit == '1') {
                std::string Placeholder = "Index";

                if (StopOne.StartPos == NegativeOneVal) {
                    StopOne.StartPos = Offset;
                    StopOne.EndPos = Offset + static_cast<int>(Placeholder.size());
                }

                Expanded += Placeholder;
                Idx++;
                continue;
            }
            if (Digit == '2') {
                std::string Placeholder = "Count";

                if (StopTwo.StartPos == NegativeOneVal) {
                    StopTwo.StartPos = Offset;
                    StopTwo.EndPos = Offset + static_cast<int>(Placeholder.size());
                }

                Expanded += Placeholder;
                Idx++;
                continue;
            }
            if (Digit == '0') {
                StopZero.StartPos = Offset;
                StopZero.EndPos = Offset;
                Idx++;
                continue;
            }
        }

        Expanded.push_back(Template[Idx]);
    }

    Host.Call(SCI_BEGINUNDOACTION);
    Host.Call(SCI_DELETERANGE, static_cast<uptr_t>(WordStart), static_cast<sptr_t>(WordLen));
    Host.Call(SCI_INSERTTEXT, static_cast<uptr_t>(WordStart), reinterpret_cast<sptr_t>(Expanded.c_str()));
    Host.Call(SCI_ENDUNDOACTION);

    ActiveSnippetStops.clear();

    if (StopOne.StartPos != NegativeOneVal) {
        ActiveSnippetStops.push_back(StopOne);
        Host.FillIndicatorRange(IndicatorSnippetStop, StopOne.StartPos, StopOne.EndPos - StopOne.StartPos);
    }
    if (StopTwo.StartPos != NegativeOneVal) {
        ActiveSnippetStops.push_back(StopTwo);
        Host.FillIndicatorRange(IndicatorSnippetStop, StopTwo.StartPos, StopTwo.EndPos - StopTwo.StartPos);
    }
    if (StopZero.StartPos != NegativeOneVal) {
        ActiveSnippetStops.push_back(StopZero);
    }

    if (!ActiveSnippetStops.empty()) {
        IsSnippetActive = true;
        ActiveSnippetIndex = ZeroVal;
        SelectSnippetStop(Host, ActiveSnippetIndex);
    }

    return true;
}

// Exits active snippet tab-stop mode when Escape is pressed.
bool EditorFeatures::HandleEscapeSnippet(ScintillaHost& Host) {
    if (!IsSnippetActive) {
        return false;
    }

    ClearSnippetState(Host);

    return true;
}

// Selects the word under caret or adds a new cursor on the next matching occurrence (Ctrl+D).
void EditorFeatures::AddNextOccurrence(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int MainSel = static_cast<int>(Host.Call(SCI_GETMAINSELECTION));
    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONNSTART, static_cast<uptr_t>(MainSel)));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONNEND, static_cast<uptr_t>(MainSel)));

    // Selects the word under caret if there is no active selection yet.
    if (SelStart == SelEnd) {
        int WordStart = static_cast<int>(Host.Call(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(SelStart), OneVal));
        int WordEnd = static_cast<int>(Host.Call(SCI_WORDENDPOSITION, static_cast<uptr_t>(SelStart), OneVal));

        if (WordEnd > WordStart) {
            Host.Call(SCI_SETSEL, static_cast<uptr_t>(WordStart), static_cast<sptr_t>(WordEnd));
        }

        return;
    }

    int SelLen = SelEnd - SelStart;
    std::string Needle(static_cast<size_t>(SelLen), '\0');

    for (int Idx = ZeroVal; Idx < SelLen; ++Idx) {
        Needle[static_cast<size_t>(Idx)] = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(SelStart + Idx)));
    }

    int DocLen = static_cast<int>(Host.Call(SCI_GETLENGTH));
    Host.Call(SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE);
    Host.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(SelEnd));
    Host.Call(SCI_SETTARGETEND, static_cast<uptr_t>(DocLen));

    int FoundPos = static_cast<int>(Host.Call(SCI_SEARCHINTARGET, static_cast<uptr_t>(Needle.size()), reinterpret_cast<sptr_t>(Needle.c_str())));

    // Wraps search around to the start of the document if no match follows.
    if (FoundPos == NegativeOneVal) {
        Host.Call(SCI_SETTARGETSTART, ZeroUptr);
        Host.Call(SCI_SETTARGETEND, static_cast<uptr_t>(SelStart));
        FoundPos = static_cast<int>(Host.Call(SCI_SEARCHINTARGET, static_cast<uptr_t>(Needle.size()), reinterpret_cast<sptr_t>(Needle.c_str())));
    }

    if (FoundPos != NegativeOneVal) {
        int FoundEnd = static_cast<int>(Host.Call(SCI_GETTARGETEND));
        Host.Call(SCI_ADDSELECTION, static_cast<uptr_t>(FoundEnd), static_cast<sptr_t>(FoundPos));
        Host.Call(SCI_SCROLLRANGE, static_cast<uptr_t>(FoundPos), static_cast<sptr_t>(FoundEnd));
    }
}

// Moves the current or selected lines up by one line while preserving selection offsets (Alt+Up).
void EditorFeatures::MoveLinesUp(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));
    int AnchorPos = static_cast<int>(Host.Call(SCI_GETANCHOR));
    int CaretPos = static_cast<int>(Host.Call(SCI_GETCURRENTPOS));
    int StartLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelStart)));
    int EndLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelEnd)));

    if (EndLine > StartLine && SelEnd == static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(EndLine)))) {
        EndLine--;
    }

    if (StartLine <= ZeroVal) {
        return;
    }

    int EolMode = static_cast<int>(Host.Call(SCI_GETEOLMODE));
    std::string Eol = (EolMode == SC_EOL_CRLF) ? "\r\n" : ((EolMode == SC_EOL_CR) ? "\r" : "\n");

    int PrevLine = StartLine - OneVal;
    std::string PrevLineText = Host.GetLineText(PrevLine);
    std::string BlockText = "";

    for (int LineIdx = StartLine; LineIdx <= EndLine; ++LineIdx) {
        BlockText += Host.GetLineText(LineIdx);

        if (LineIdx < EndLine) {
            BlockText += Eol;
        }
    }

    int ReplaceStart = static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(PrevLine)));
    int ReplaceEnd = static_cast<int>(Host.Call(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(EndLine)));
    int AnchorOffset = AnchorPos - static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(StartLine)));
    int CaretOffset = CaretPos - static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(StartLine)));
    std::string Replacement = BlockText + Eol + PrevLineText;

    Host.Call(SCI_BEGINUNDOACTION);
    Host.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(ReplaceStart));
    Host.Call(SCI_SETTARGETEND, static_cast<uptr_t>(ReplaceEnd));
    Host.Call(SCI_REPLACETARGET, static_cast<uptr_t>(Replacement.size()), reinterpret_cast<sptr_t>(Replacement.c_str()));
    Host.Call(SCI_SETSEL, static_cast<uptr_t>(ReplaceStart + AnchorOffset), static_cast<sptr_t>(ReplaceStart + CaretOffset));
    Host.Call(SCI_SCROLLCARET);
    Host.Call(SCI_ENDUNDOACTION);
}

// Moves the current or selected lines down by one line while preserving selection offsets (Alt+Down).
void EditorFeatures::MoveLinesDown(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));
    int AnchorPos = static_cast<int>(Host.Call(SCI_GETANCHOR));
    int CaretPos = static_cast<int>(Host.Call(SCI_GETCURRENTPOS));
    int StartLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelStart)));
    int EndLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelEnd)));

    if (EndLine > StartLine && SelEnd == static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(EndLine)))) {
        EndLine--;
    }

    int TotalLines = Host.GetLineCount();

    if (EndLine + OneVal >= TotalLines) {
        return;
    }

    int EolMode = static_cast<int>(Host.Call(SCI_GETEOLMODE));
    std::string Eol = (EolMode == SC_EOL_CRLF) ? "\r\n" : ((EolMode == SC_EOL_CR) ? "\r" : "\n");

    int NextLine = EndLine + OneVal;
    std::string NextLineText = Host.GetLineText(NextLine);
    std::string BlockText = "";

    for (int LineIdx = StartLine; LineIdx <= EndLine; ++LineIdx) {
        BlockText += Host.GetLineText(LineIdx);

        if (LineIdx < EndLine) {
            BlockText += Eol;
        }
    }

    int ReplaceStart = static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(StartLine)));
    int ReplaceEnd = static_cast<int>(Host.Call(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(NextLine)));
    int AnchorOffset = AnchorPos - ReplaceStart;
    int CaretOffset = CaretPos - ReplaceStart;
    std::string Replacement = NextLineText + Eol + BlockText;
    int NewBlockStart = ReplaceStart + static_cast<int>(NextLineText.size()) + static_cast<int>(Eol.size());

    Host.Call(SCI_BEGINUNDOACTION);
    Host.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(ReplaceStart));
    Host.Call(SCI_SETTARGETEND, static_cast<uptr_t>(ReplaceEnd));
    Host.Call(SCI_REPLACETARGET, static_cast<uptr_t>(Replacement.size()), reinterpret_cast<sptr_t>(Replacement.c_str()));
    Host.Call(SCI_SETSEL, static_cast<uptr_t>(NewBlockStart + AnchorOffset), static_cast<sptr_t>(NewBlockStart + CaretOffset));
    Host.Call(SCI_SCROLLCARET);
    Host.Call(SCI_ENDUNDOACTION);
}

// Duplicates the currently selected line range above or below while preserving selection (Shift+Alt+Up/Down).
void EditorFeatures::DuplicateLines(ScintillaHost& Host, bool CopyUp) {
    if (!Host.IsReady()) {
        return;
    }

    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));
    int AnchorPos = static_cast<int>(Host.Call(SCI_GETANCHOR));
    int CaretPos = static_cast<int>(Host.Call(SCI_GETCURRENTPOS));
    int StartLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelStart)));
    int EndLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelEnd)));

    if (EndLine > StartLine && SelEnd == static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(EndLine)))) {
        EndLine--;
    }

    int EolMode = static_cast<int>(Host.Call(SCI_GETEOLMODE));
    std::string Eol = (EolMode == SC_EOL_CRLF) ? "\r\n" : ((EolMode == SC_EOL_CR) ? "\r" : "\n");

    std::string BlockText = "";

    for (int LineIdx = StartLine; LineIdx <= EndLine; ++LineIdx) {
        BlockText += Host.GetLineText(LineIdx);

        if (LineIdx < EndLine) {
            BlockText += Eol;
        }
    }

    int BlockStartPos = static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(StartLine)));
    int AnchorOffset = AnchorPos - BlockStartPos;
    int CaretOffset = CaretPos - BlockStartPos;

    Host.Call(SCI_BEGINUNDOACTION);

    if (CopyUp) {
        std::string Payload = BlockText + Eol;
        Host.Call(SCI_INSERTTEXT, static_cast<uptr_t>(BlockStartPos), reinterpret_cast<sptr_t>(Payload.c_str()));
        Host.Call(SCI_SETSEL, static_cast<uptr_t>(BlockStartPos + AnchorOffset), static_cast<sptr_t>(BlockStartPos + CaretOffset));
    } else {
        int InsertPos = static_cast<int>(Host.Call(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(EndLine)));
        std::string Payload = Eol + BlockText;
        int NewBlockStart = InsertPos + static_cast<int>(Eol.size());
        Host.Call(SCI_INSERTTEXT, static_cast<uptr_t>(InsertPos), reinterpret_cast<sptr_t>(Payload.c_str()));
        Host.Call(SCI_SETSEL, static_cast<uptr_t>(NewBlockStart + AnchorOffset), static_cast<sptr_t>(NewBlockStart + CaretOffset));
    }

    Host.Call(SCI_SCROLLCARET);
    Host.Call(SCI_ENDUNDOACTION);
}

// Deletes the current line or all lines spanned by the selection (Ctrl+Shift+K).
void EditorFeatures::DeleteCurrentLines(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));
    int StartLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelStart)));
    int EndLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelEnd)));

    Host.Call(SCI_BEGINUNDOACTION);

    for (int LineIdx = EndLine; LineIdx >= StartLine; --LineIdx) {
        int LineStart = static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(LineIdx)));
        int LineLen = static_cast<int>(Host.Call(SCI_LINELENGTH, static_cast<uptr_t>(LineIdx)));
        Host.Call(SCI_DELETERANGE, static_cast<uptr_t>(LineStart), static_cast<sptr_t>(LineLen));
    }

    Host.Call(SCI_ENDUNDOACTION);
}

// Toggles single-line comments across the active selection using the file's comment prefix (Ctrl+/).
void EditorFeatures::ToggleLineComment(ScintillaHost& Host, const std::string& FileName) {
    if (!Host.IsReady()) {
        return;
    }

    std::string Prefix = Host.GetCommentPrefixForFile(FileName);
    std::string PrefixWithSpace = Prefix + " ";
    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));
    int StartLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelStart)));
    int EndLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelEnd)));

    // Checks whether all non-empty lines in the selection are already commented.
    bool AllCommented = true;

    for (int LineIdx = StartLine; LineIdx <= EndLine; ++LineIdx) {
        std::string LineText = Host.GetLineText(LineIdx);
        size_t FirstNonSpace = LineText.find_first_not_of(" \t");

        if (FirstNonSpace == std::string::npos) {
            continue;
        }

        if (LineText.compare(FirstNonSpace, Prefix.size(), Prefix) != ZeroVal) {
            AllCommented = false;
            break;
        }
    }

    Host.Call(SCI_BEGINUNDOACTION);

    for (int LineIdx = StartLine; LineIdx <= EndLine; ++LineIdx) {
        std::string LineText = Host.GetLineText(LineIdx);
        size_t FirstNonSpace = LineText.find_first_not_of(" \t");

        if (FirstNonSpace == std::string::npos) {
            continue;
        }

        int LineStartPos = static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(LineIdx)));
        int TargetPos = LineStartPos + static_cast<int>(FirstNonSpace);

        if (AllCommented) {
            int RemoveLen = static_cast<int>(Prefix.size());

            if (LineText.compare(FirstNonSpace, PrefixWithSpace.size(), PrefixWithSpace) == ZeroVal) {
                RemoveLen = static_cast<int>(PrefixWithSpace.size());
            }

            Host.Call(SCI_DELETERANGE, static_cast<uptr_t>(TargetPos), static_cast<sptr_t>(RemoveLen));
        } else {
            Host.Call(SCI_INSERTTEXT, static_cast<uptr_t>(TargetPos), reinterpret_cast<sptr_t>(PrefixWithSpace.c_str()));
        }
    }

    Host.Call(SCI_ENDUNDOACTION);
}

// Toggles block comments around the active selection or current line (Shift+Alt+A).
void EditorFeatures::ToggleBlockComment(ScintillaHost& Host, const std::string& FileName) {
    if (!Host.IsReady()) {
        return;
    }

    std::string OpenDelim = "/*";
    std::string CloseDelim = "*/";
    size_t DotPos = FileName.find_last_of('.');
    std::string Ext = (DotPos == std::string::npos) ? "" : FileName.substr(DotPos);
    std::transform(Ext.begin(), Ext.end(), Ext.begin(), [](unsigned char Ch) {
        return static_cast<char>(std::tolower(Ch));
    });

    if (Ext == ".html" || Ext == ".htm" || Ext == ".xml" || Ext == ".svg" || Ext == ".md") {
        OpenDelim = "<!--";
        CloseDelim = "-->";
    } else if (Ext == ".py") {
        OpenDelim = "\"\"\"";
        CloseDelim = "\"\"\"";
    }

    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));

    if (SelStart == SelEnd) {
        int CurLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelStart)));
        SelStart = static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(CurLine)));
        SelEnd = static_cast<int>(Host.Call(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(CurLine)));
    }

    int SelLen = SelEnd - SelStart;
    std::string Selected(static_cast<size_t>(SelLen), '\0');

    for (int Idx = ZeroVal; Idx < SelLen; ++Idx) {
        Selected[static_cast<size_t>(Idx)] = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(SelStart + Idx)));
    }

    Host.Call(SCI_BEGINUNDOACTION);

    bool AlreadyWrapped = (Selected.size() >= OpenDelim.size() + CloseDelim.size() && Selected.rfind(OpenDelim, ZeroVal) == ZeroVal && Selected.compare(Selected.size() - CloseDelim.size(), CloseDelim.size(), CloseDelim) == ZeroVal);

    if (AlreadyWrapped) {
        std::string Unwrapped = Selected.substr(OpenDelim.size(), Selected.size() - OpenDelim.size() - CloseDelim.size());
        Host.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(SelStart));
        Host.Call(SCI_SETTARGETEND, static_cast<uptr_t>(SelEnd));
        Host.Call(SCI_REPLACETARGET, static_cast<uptr_t>(Unwrapped.size()), reinterpret_cast<sptr_t>(Unwrapped.c_str()));
        Host.Call(SCI_SETSEL, static_cast<uptr_t>(SelStart), static_cast<sptr_t>(SelStart + static_cast<int>(Unwrapped.size())));
    } else {
        std::string Wrapped = OpenDelim + Selected + CloseDelim;
        Host.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(SelStart));
        Host.Call(SCI_SETTARGETEND, static_cast<uptr_t>(SelEnd));
        Host.Call(SCI_REPLACETARGET, static_cast<uptr_t>(Wrapped.size()), reinterpret_cast<sptr_t>(Wrapped.c_str()));
        Host.Call(SCI_SETSEL, static_cast<uptr_t>(SelStart), static_cast<sptr_t>(SelStart + static_cast<int>(Wrapped.size())));
    }

    Host.Call(SCI_ENDUNDOACTION);
}

// Selects the word under caret or adds a new cursor on the previous matching occurrence.
void EditorFeatures::AddPreviousOccurrence(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int MainSel = static_cast<int>(Host.Call(SCI_GETMAINSELECTION));
    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONNSTART, static_cast<uptr_t>(MainSel)));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONNEND, static_cast<uptr_t>(MainSel)));

    if (SelStart == SelEnd) {
        int WordStart = static_cast<int>(Host.Call(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(SelStart), OneVal));
        int WordEnd = static_cast<int>(Host.Call(SCI_WORDENDPOSITION, static_cast<uptr_t>(SelStart), OneVal));

        if (WordEnd > WordStart) {
            Host.Call(SCI_SETSEL, static_cast<uptr_t>(WordStart), static_cast<sptr_t>(WordEnd));
        }

        return;
    }

    int SelLen = SelEnd - SelStart;
    std::string Needle(static_cast<size_t>(SelLen), '\0');

    for (int Idx = ZeroVal; Idx < SelLen; ++Idx) {
        Needle[static_cast<size_t>(Idx)] = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(SelStart + Idx)));
    }

    int DocLen = static_cast<int>(Host.Call(SCI_GETLENGTH));
    Host.Call(SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE);
    Host.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(SelStart));
    Host.Call(SCI_SETTARGETEND, ZeroSptr);

    int FoundPos = static_cast<int>(Host.Call(SCI_SEARCHINTARGET, static_cast<uptr_t>(Needle.size()), reinterpret_cast<sptr_t>(Needle.c_str())));

    if (FoundPos == NegativeOneVal) {
        Host.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(DocLen));
        Host.Call(SCI_SETTARGETEND, static_cast<uptr_t>(SelEnd));
        FoundPos = static_cast<int>(Host.Call(SCI_SEARCHINTARGET, static_cast<uptr_t>(Needle.size()), reinterpret_cast<sptr_t>(Needle.c_str())));
    }

    if (FoundPos != NegativeOneVal) {
        int FoundEnd = static_cast<int>(Host.Call(SCI_GETTARGETEND));
        Host.Call(SCI_ADDSELECTION, static_cast<uptr_t>(FoundEnd), static_cast<sptr_t>(FoundPos));
        Host.Call(SCI_SCROLLRANGE, static_cast<uptr_t>(FoundPos), static_cast<sptr_t>(FoundEnd));
    }
}

// Selects every occurrence of the current selection or word under caret in the document (Ctrl+Shift+L).
void EditorFeatures::SelectAllOccurrences(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));

    if (SelStart == SelEnd) {
        SelStart = static_cast<int>(Host.Call(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(SelStart), OneVal));
        SelEnd = static_cast<int>(Host.Call(SCI_WORDENDPOSITION, static_cast<uptr_t>(SelStart), OneVal));
    }

    int SelLen = SelEnd - SelStart;

    if (SelLen <= ZeroVal) {
        return;
    }

    std::string Needle(static_cast<size_t>(SelLen), '\0');

    for (int Idx = ZeroVal; Idx < SelLen; ++Idx) {
        Needle[static_cast<size_t>(Idx)] = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(SelStart + Idx)));
    }

    int DocLen = static_cast<int>(Host.Call(SCI_GETLENGTH));
    Host.Call(SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE);
    int ScanPos = ZeroVal;
    bool IsFirst = true;

    while (ScanPos < DocLen) {
        Host.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(ScanPos));
        Host.Call(SCI_SETTARGETEND, static_cast<uptr_t>(DocLen));

        int FoundPos = static_cast<int>(Host.Call(SCI_SEARCHINTARGET, static_cast<uptr_t>(Needle.size()), reinterpret_cast<sptr_t>(Needle.c_str())));

        if (FoundPos == NegativeOneVal) {
            break;
        }

        int FoundEnd = static_cast<int>(Host.Call(SCI_GETTARGETEND));

        if (IsFirst) {
            Host.Call(SCI_SETSELECTION, static_cast<uptr_t>(FoundEnd), static_cast<sptr_t>(FoundPos));
            IsFirst = false;
        } else {
            Host.Call(SCI_ADDSELECTION, static_cast<uptr_t>(FoundEnd), static_cast<sptr_t>(FoundPos));
        }

        ScanPos = FoundEnd;
    }
}

// Progressively expands the active selection from word to delimiters, line, block, and full document.
void EditorFeatures::ExpandSelection(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));
    int DocLen = static_cast<int>(Host.Call(SCI_GETLENGTH));

    const size_t MaxSelectionHistory = 35;
    SelectionHistory.push_back({ SelStart, SelEnd });

    if (SelectionHistory.size() > MaxSelectionHistory) {
        SelectionHistory.erase(SelectionHistory.begin());
    }

    // Step 1: Expand to word under caret.
    int WordStart = static_cast<int>(Host.Call(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(SelStart), OneVal));
    int WordEnd = static_cast<int>(Host.Call(SCI_WORDENDPOSITION, static_cast<uptr_t>(SelEnd), OneVal));

    if (WordStart < SelStart || WordEnd > SelEnd) {
        if (WordEnd > WordStart) {
            Host.Call(SCI_SETSEL, static_cast<uptr_t>(WordStart), static_cast<sptr_t>(WordEnd));

            return;
        }
    }

    // Step 2: Expand to enclosing line content.
    int StartLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelStart)));
    int EndLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelEnd)));
    int LineStart = static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(StartLine)));
    int LineEnd = static_cast<int>(Host.Call(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(EndLine)));

    if (LineStart < SelStart || LineEnd > SelEnd) {
        Host.Call(SCI_SETSEL, static_cast<uptr_t>(LineStart), static_cast<sptr_t>(LineEnd));

        return;
    }

    // Step 3: Expand to enclosing brace block if present.
    int Depth = ZeroVal;
    int OpenPos = NegativeOneVal;

    for (int Pos = SelStart - OneVal; Pos >= ZeroVal; --Pos) {
        char Ch = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(Pos)));

        if (Ch == '}' || Ch == ')' || Ch == ']') {
            Depth++;
        } else if (Ch == '{' || Ch == '(' || Ch == '[') {
            if (Depth == ZeroVal) {
                OpenPos = Pos;
                break;
            }
            Depth--;
        }
    }

    if (OpenPos != NegativeOneVal) {
        int MatchClose = static_cast<int>(Host.Call(SCI_BRACEMATCH, static_cast<uptr_t>(OpenPos), ZeroSptr));

        if (MatchClose != NegativeOneVal && MatchClose >= SelEnd) {
            int BlockStart = OpenPos;
            int BlockEnd = MatchClose + OneVal;

            if (BlockStart < SelStart || BlockEnd > SelEnd) {
                Host.Call(SCI_SETSEL, static_cast<uptr_t>(BlockStart), static_cast<sptr_t>(BlockEnd));

                return;
            }
        }
    }

    // Step 4: Expand to full document.
    if (SelStart > ZeroVal || SelEnd < DocLen) {
        Host.Call(SCI_SETSEL, ZeroUptr, static_cast<sptr_t>(DocLen));
    }
}

// Restores the previous narrower selection from the expand-selection history stack.
void EditorFeatures::ShrinkSelection(ScintillaHost& Host) {
    if (!Host.IsReady() || SelectionHistory.empty()) {
        return;
    }

    SelectionRange Prev = SelectionHistory.back();
    SelectionHistory.pop_back();
    Host.Call(SCI_SETSEL, static_cast<uptr_t>(Prev.StartPos), static_cast<sptr_t>(Prev.EndPos));
}

// Duplicates the currently selected text inline, or duplicates the current line if empty.
void EditorFeatures::DuplicateSelection(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));

    if (SelStart == SelEnd) {
        Host.Call(SCI_LINEDUPLICATE);

        return;
    }

    int SelLen = SelEnd - SelStart;
    std::string Selected(static_cast<size_t>(SelLen), '\0');

    for (int Idx = ZeroVal; Idx < SelLen; ++Idx) {
        Selected[static_cast<size_t>(Idx)] = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(SelStart + Idx)));
    }

    Host.Call(SCI_BEGINUNDOACTION);
    Host.Call(SCI_INSERTTEXT, static_cast<uptr_t>(SelEnd), reinterpret_cast<sptr_t>(Selected.c_str()));
    Host.Call(SCI_SETSEL, static_cast<uptr_t>(SelEnd), static_cast<sptr_t>(SelEnd + SelLen));
    Host.Call(SCI_ENDUNDOACTION);
}

// Adds an additional caret one line above the topmost active caret at the same column.
void EditorFeatures::AddCursorAbove(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int SelCount = static_cast<int>(Host.Call(SCI_GETSELECTIONS));
    int TopPos = static_cast<int>(Host.Call(SCI_GETSELECTIONNCARET, ZeroUptr));

    for (int Idx = OneVal; Idx < SelCount; ++Idx) {
        int CaretPos = static_cast<int>(Host.Call(SCI_GETSELECTIONNCARET, static_cast<uptr_t>(Idx)));

        if (CaretPos < TopPos) {
            TopPos = CaretPos;
        }
    }

    int TopLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(TopPos)));

    if (TopLine <= ZeroVal) {
        return;
    }

    int Col = static_cast<int>(Host.Call(SCI_GETCOLUMN, static_cast<uptr_t>(TopPos)));
    int TargetLine = TopLine - OneVal;
    int NewPos = static_cast<int>(Host.Call(SCI_FINDCOLUMN, static_cast<uptr_t>(TargetLine), static_cast<sptr_t>(Col)));
    Host.Call(SCI_ADDSELECTION, static_cast<uptr_t>(NewPos), static_cast<sptr_t>(NewPos));
}

// Adds an additional caret one line below the bottommost active caret at the same column.
void EditorFeatures::AddCursorBelow(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int SelCount = static_cast<int>(Host.Call(SCI_GETSELECTIONS));
    int BottomPos = static_cast<int>(Host.Call(SCI_GETSELECTIONNCARET, ZeroUptr));

    for (int Idx = OneVal; Idx < SelCount; ++Idx) {
        int CaretPos = static_cast<int>(Host.Call(SCI_GETSELECTIONNCARET, static_cast<uptr_t>(Idx)));

        if (CaretPos > BottomPos) {
            BottomPos = CaretPos;
        }
    }

    int BottomLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(BottomPos)));
    int TotalLines = Host.GetLineCount();

    if (BottomLine + OneVal >= TotalLines) {
        return;
    }

    int Col = static_cast<int>(Host.Call(SCI_GETCOLUMN, static_cast<uptr_t>(BottomPos)));
    int TargetLine = BottomLine + OneVal;
    int NewPos = static_cast<int>(Host.Call(SCI_FINDCOLUMN, static_cast<uptr_t>(TargetLine), static_cast<sptr_t>(Col)));
    Host.Call(SCI_ADDSELECTION, static_cast<uptr_t>(NewPos), static_cast<sptr_t>(NewPos));
}

// Places a multi-cursor caret at the end of every line spanned by the active selection.
void EditorFeatures::AddCursorsToLineEnds(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int SelStart = static_cast<int>(Host.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(Host.Call(SCI_GETSELECTIONEND));
    int StartLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelStart)));
    int EndLine = static_cast<int>(Host.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(SelEnd)));

    for (int LineIdx = StartLine; LineIdx <= EndLine; ++LineIdx) {
        int EndPos = static_cast<int>(Host.Call(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(LineIdx)));

        if (LineIdx == StartLine) {
            Host.Call(SCI_SETSELECTION, static_cast<uptr_t>(EndPos), static_cast<sptr_t>(EndPos));
        } else {
            Host.Call(SCI_ADDSELECTION, static_cast<uptr_t>(EndPos), static_cast<sptr_t>(EndPos));
        }
    }
}

// Toggles rectangular column selection mode on the Scintilla host.
void EditorFeatures::ToggleColumnSelectionMode(ScintillaHost& Host) {
    ColumnSelectionEnabled = !ColumnSelectionEnabled;

    if (Host.IsReady()) {
        Host.Call(SCI_SETSELECTIONMODE, ColumnSelectionEnabled ? SC_SEL_RECTANGLE : SC_SEL_STREAM);
    }
}

// Moves the caret to the matching bracket if adjacent to one, or to the nearest enclosing bracket.
void EditorFeatures::GoToBracket(ScintillaHost& Host) {
    if (!Host.IsReady()) {
        return;
    }

    int Pos = Host.GetCurrentPos();
    int BracePos = NegativeOneVal;

    auto IsBracket = [](int Ch) {
        return (Ch == '(' || Ch == ')' || Ch == '{' || Ch == '}' || Ch == '[' || Ch == ']');
    };

    if (IsBracket(static_cast<int>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(Pos))))) {
        BracePos = Pos;
    } else if (Pos > ZeroVal && IsBracket(static_cast<int>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(Pos - OneVal))))) {
        BracePos = Pos - OneVal;
    }

    if (BracePos != NegativeOneVal) {
        int MatchPos = static_cast<int>(Host.Call(SCI_BRACEMATCH, static_cast<uptr_t>(BracePos), ZeroSptr));

        if (MatchPos != NegativeOneVal) {
            Host.Call(SCI_GOTOPOS, static_cast<uptr_t>(MatchPos));
            Host.Call(SCI_SCROLLCARET);

            return;
        }
    }

    // Finds the nearest enclosing opening bracket if caret is not directly on one.
    int Depth = ZeroVal;

    for (int ScanPos = Pos - OneVal; ScanPos >= ZeroVal; --ScanPos) {
        char Ch = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(ScanPos)));

        if (Ch == ')' || Ch == '}' || Ch == ']') {
            Depth++;
        } else if (Ch == '(' || Ch == '{' || Ch == '[') {
            if (Depth == ZeroVal) {
                Host.Call(SCI_GOTOPOS, static_cast<uptr_t>(ScanPos));
                Host.Call(SCI_SCROLLCARET);

                return;
            }
            Depth--;
        }
    }
}

// Updates matching brace highlighting when the caret is adjacent to a bracket.
void EditorFeatures::UpdateBraceMatchHighlight(ScintillaHost& Host) {
    int Pos = Host.GetCurrentPos();
    int BracePos = NegativeOneVal;

    auto IsBracket = [](int Ch) {
        return (Ch == '(' || Ch == ')' || Ch == '{' || Ch == '}' || Ch == '[' || Ch == ']');
    };

    if (Pos > ZeroVal && IsBracket(static_cast<int>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(Pos - OneVal))))) {
        BracePos = Pos - OneVal;
    } else if (IsBracket(static_cast<int>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(Pos))))) {
        BracePos = Pos;
    }

    if (BracePos != NegativeOneVal) {
        int MatchPos = static_cast<int>(Host.Call(SCI_BRACEMATCH, static_cast<uptr_t>(BracePos), ZeroSptr));

        if (MatchPos != NegativeOneVal) {
            Host.Call(SCI_BRACEHIGHLIGHT, static_cast<uptr_t>(BracePos), static_cast<sptr_t>(MatchPos));
        } else {
            Host.Call(SCI_BRACEBADLIGHT, static_cast<uptr_t>(BracePos));
        }
    } else {
        Host.Call(SCI_BRACEHIGHLIGHT, static_cast<uptr_t>(NegativeOneVal), static_cast<sptr_t>(NegativeOneVal));
    }
}

// Colors nested brackets on visible lines using cycling rainbow indicator colors.
void EditorFeatures::UpdateRainbowBrackets(ScintillaHost& Host) {
    int FirstVis = Host.GetFirstVisibleLine();
    int VisCount = Host.GetLinesOnScreen();
    int TotalLines = Host.GetLineCount();
    int LastVis = FirstVis + VisCount + OneVal;

    if (LastVis >= TotalLines) {
        LastVis = TotalLines - OneVal;
    }

    int RangeStart = static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(FirstVis)));
    int RangeEnd = static_cast<int>(Host.Call(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(LastVis)));
    int RangeLen = RangeEnd - RangeStart;

    if (RangeLen <= ZeroVal) {
        return;
    }

    Host.ClearIndicatorRange(IndicatorBracketGold, RangeStart, RangeLen);
    Host.ClearIndicatorRange(IndicatorBracketViolet, RangeStart, RangeLen);
    Host.ClearIndicatorRange(IndicatorBracketCyan, RangeStart, RangeLen);
    Host.ClearIndicatorRange(IndicatorBracketError, RangeStart, RangeLen);

    // Estimates nesting depth by scanning a bounded window preceding the visible range.
    int ScanStartLine = (FirstVis > MaxBracketScanLines) ? (FirstVis - MaxBracketScanLines) : ZeroVal;
    int ScanStartPos = static_cast<int>(Host.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(ScanStartLine)));
    int Depth = ZeroVal;
    bool InDoubleQuote = false;
    bool InSingleQuote = false;

    const int RainbowIndicators[ThreeVal] = {
        IndicatorBracketGold,
        IndicatorBracketViolet,
        IndicatorBracketCyan
    };

    for (int Pos = ScanStartPos; Pos < RangeEnd; ++Pos) {
        char Ch = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(Pos)));

        if (Ch == '\n') {
            InDoubleQuote = false;
            InSingleQuote = false;
            continue;
        }

        if (Ch == '"' && !InSingleQuote) {
            InDoubleQuote = !InDoubleQuote;
            continue;
        }
        if (Ch == '\'' && !InDoubleQuote) {
            InSingleQuote = !InSingleQuote;
            continue;
        }
        if (InDoubleQuote || InSingleQuote) {
            continue;
        }

        if (Ch == '(' || Ch == '{' || Ch == '[') {
            if (Pos >= RangeStart) {
                Host.FillIndicatorRange(RainbowIndicators[Depth % ThreeVal], Pos, OneVal);
            }
            Depth++;
        } else if (Ch == ')' || Ch == '}' || Ch == ']') {
            if (Depth > ZeroVal) {
                Depth--;

                if (Pos >= RangeStart) {
                    Host.FillIndicatorRange(RainbowIndicators[Depth % ThreeVal], Pos, OneVal);
                }
            } else if (Pos >= RangeStart) {
                Host.FillIndicatorRange(IndicatorBracketError, Pos, OneVal);
            }
        }
    }
}

// Highlights all whole-word matches of the identifier currently under the caret.
void EditorFeatures::UpdateOccurrenceHighlights(ScintillaHost& Host) {
    int DocLen = static_cast<int>(Host.Call(SCI_GETLENGTH));

    if (DocLen <= ZeroVal || DocLen > MaxOccurrenceDocSize) {
        return;
    }

    Host.ClearIndicatorRange(IndicatorOccurrence, ZeroVal, DocLen);

    int Pos = Host.GetCurrentPos();
    int WordStart = static_cast<int>(Host.Call(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(Pos), OneVal));
    int WordEnd = static_cast<int>(Host.Call(SCI_WORDENDPOSITION, static_cast<uptr_t>(Pos), OneVal));
    int WordLen = WordEnd - WordStart;

    if (WordLen < TwoVal) {
        return;
    }

    std::string Word(static_cast<size_t>(WordLen), '\0');

    for (int Idx = ZeroVal; Idx < WordLen; ++Idx) {
        Word[static_cast<size_t>(Idx)] = static_cast<char>(Host.Call(SCI_GETCHARAT, static_cast<uptr_t>(WordStart + Idx)));
    }

    if (!IsWordChar(static_cast<unsigned char>(Word[ZeroVal]))) {
        return;
    }

    Host.Call(SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE | SCFIND_WHOLEWORD);
    int SearchPos = ZeroVal;
    int MatchCount = ZeroVal;
    std::vector<int> MatchPositions;

    while (SearchPos < DocLen) {
        Host.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(SearchPos));
        Host.Call(SCI_SETTARGETEND, static_cast<uptr_t>(DocLen));

        int FoundPos = static_cast<int>(Host.Call(SCI_SEARCHINTARGET, static_cast<uptr_t>(Word.size()), reinterpret_cast<sptr_t>(Word.c_str())));

        if (FoundPos == NegativeOneVal) {
            break;
        }

        MatchPositions.push_back(FoundPos);
        MatchCount++;
        SearchPos = FoundPos + WordLen;
    }

    // Only renders occurrence boxes when at least two occurrences exist.
    if (MatchCount >= TwoVal) {
        for (int MatchPos : MatchPositions) {
            Host.FillIndicatorRange(IndicatorOccurrence, MatchPos, WordLen);
        }
    }
}

// Runs periodic UI visual updates on caret movement or scroll with re-entrancy protection.
void EditorFeatures::OnUpdateUi(ScintillaHost& Host) {
    if (!Host.IsReady() || IsUpdatingUi) {
        return;
    }

    IsUpdatingUi = true;
    UpdateBraceMatchHighlight(Host);
    UpdateRainbowBrackets(Host);
    UpdateOccurrenceHighlights(Host);
    IsUpdatingUi = false;
}
