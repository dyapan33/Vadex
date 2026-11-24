#ifndef EDITOR_FEATURES_HPP
#define EDITOR_FEATURES_HPP
#include <string>
#include <vector>
#include "Include/ScintillaHost.hpp"



// Represents a single editable tab stop inside an active snippet expansion.
struct SnippetTabStop {
    int StartPos;
    int EndPos;
};

// Represents a saved selection range for Expand/Shrink selection history.
struct SelectionRange {
    int StartPos;
    int EndPos;
};

// Implements Monaco Editor editing features on top of ScintillaHost.
class EditorFeatures {
private:
    std::vector<SnippetTabStop> ActiveSnippetStops;
    int ActiveSnippetIndex;
    bool IsSnippetActive;
    bool IsUpdatingUi;
    std::vector<SelectionRange> SelectionHistory;
    bool ColumnSelectionEnabled;

    bool IsWordChar(int Ch) const;
    bool IsOpenPair(char Ch, char& OutClose) const;
    bool IsClosePair(char Ch) const;
    void ClearSnippetState(ScintillaHost& Host);
    void SelectSnippetStop(ScintillaHost& Host, int StopIndex);

public:
    EditorFeatures();

    void OnCharAdded(ScintillaHost& Host, int Ch, int TabSize);
    void OnUpdateUi(ScintillaHost& Host);
    bool HandleBackspacePair(ScintillaHost& Host);
    bool HandleTabSnippet(ScintillaHost& Host, const std::string& FileName);
    bool HandleEscapeSnippet(ScintillaHost& Host);

    void AddNextOccurrence(ScintillaHost& Host);
    void AddPreviousOccurrence(ScintillaHost& Host);
    void SelectAllOccurrences(ScintillaHost& Host);
    void ExpandSelection(ScintillaHost& Host);
    void ShrinkSelection(ScintillaHost& Host);
    void DuplicateSelection(ScintillaHost& Host);
    void AddCursorAbove(ScintillaHost& Host);
    void AddCursorBelow(ScintillaHost& Host);
    void AddCursorsToLineEnds(ScintillaHost& Host);
    void ToggleColumnSelectionMode(ScintillaHost& Host);
    bool IsColumnSelectionMode() const { return ColumnSelectionEnabled; }
    void GoToBracket(ScintillaHost& Host);
    void MoveLinesUp(ScintillaHost& Host);
    void MoveLinesDown(ScintillaHost& Host);
    void DuplicateLines(ScintillaHost& Host, bool CopyUp);
    void DeleteCurrentLines(ScintillaHost& Host);
    void ToggleLineComment(ScintillaHost& Host, const std::string& FileName);
    void ToggleBlockComment(ScintillaHost& Host, const std::string& FileName);
    void UpdateRainbowBrackets(ScintillaHost& Host);
    void UpdateOccurrenceHighlights(ScintillaHost& Host);
    void UpdateBraceMatchHighlight(ScintillaHost& Host);
};

#endif
