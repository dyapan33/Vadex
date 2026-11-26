#ifndef SCINTILLA_HOST_HPP
#define SCINTILLA_HOST_HPP
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "Scintilla/Include/Scintilla.h"
#include "Scintilla/Include/ILexer.h"
#include "Lexilla/Include/Lexilla.h"
#include "Lexilla/Include/SciLexer.h"



const uptr_t ZeroUptr = 0;
const sptr_t ZeroSptr = 0;
const int IndicatorMatchHighlight = 9;
const int IndicatorOccurrence = 10;
const int IndicatorBracketGold = 11;
const int IndicatorBracketViolet = 13;
const int IndicatorBracketCyan = 15;
const int IndicatorBracketError = 17;
const int IndicatorSnippetStop = 19;
const int IndicatorDiagError = 21;
const int IndicatorDiagWarning = 23;
const int MarginLineNumbers = 0;
const int MarginBreakpoints = 1;
const int MarginFolding = 1 + 1;
const int MarkerBreakpoint = 1;
const int MarkerBreakpointLine = 1 + 1;
const int MarkerDebugLine = 3;

// Encapsulates a native Scintilla 5 + Lexilla 5 editor control and document lifecycle.
class ScintillaHost {
private:
    HMODULE ScintillaModule;
    HMODULE LexillaModule;
    Lexilla::CreateLexerFn CreateLexerFunc;
    HWND ParentWindow;
    HWND EditorWindow;
    SciFnDirect DirectFunc;
    sptr_t DirectPtr;
    WNDPROC OriginalEditorProc;
    bool IsVisibleState;
    RECT LastBounds;
    bool HasClipExclude;
    std::vector<RECT> LastClipExcludes;
    std::function<bool(UINT, WPARAM, LPARAM)> KeyDownHandler;
    std::function<void()> CtrlClickHandler;

    static LRESULT CALLBACK EditorSubclassProc(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
    void ApplyBaseDarkTheme();
    void ApplyCppStyles();
    void ApplyPythonStyles();
    void ApplyMarkupStyles();
    void ApplyMarkdownStyles();
    void ApplyJsonStyles();
    void ApplyIndicators();

public:
    ScintillaHost();
    ~ScintillaHost();

    bool Initialize(HWND ParentHwnd);
    void Shutdown();
    bool IsReady() const { return (EditorWindow != nullptr && DirectFunc != nullptr); }
    HWND GetHwnd() const { return EditorWindow; }
    sptr_t Call(unsigned int Message, uptr_t WParam = ZeroUptr, sptr_t LParam = ZeroSptr) const;

    void SetKeyDownHandler(std::function<bool(UINT, WPARAM, LPARAM)> Handler);
    void SetCtrlClickHandler(std::function<void()> Handler);
    void SetBounds(int Left, int Top, int Width, int Height);
    void SetClipExcludeRects(const std::vector<RECT>& ExcludeRects);
    void SetVisible(bool Visible);
    void Focus();

    sptr_t CreateDocument(const std::string& InitialText);
    void ReleaseDocument(sptr_t DocPtr);
    void SetActiveDocument(sptr_t DocPtr, const std::string& FileName, int TabSize);
    void ApplyLanguageForFile(const std::string& FileName);
    std::string GetText() const;
    std::string GetDocumentText(sptr_t DocPtr);
    void SetText(const std::string& Text);
    void SetSavePoint();
    void SetReadOnly(bool ReadOnly);
    void SetTabWidth(int TabSize);
    void ToggleWordWrap();
    bool IsWordWrapEnabled() const;

    int GetLineCount() const;
    int GetFirstVisibleLine() const;
    void SetFirstVisibleLine(int LineIndex);
    int GetLinesOnScreen() const;
    int GetCurrentPos() const;
    int GetCurrentLine() const;
    int GetCurrentColumn() const;
    std::string GetLineText(int LineIndex) const;
    int GetFoldParent(int LineIndex) const;
    void GoToLine(int OneBasedLine);
    void GoToLineColumn(int OneBasedLine, int OneBasedCol);
    std::string GetCommentPrefixForFile(const std::string& FileName) const;

    void ClearIndicatorRange(int IndicatorId, int StartPos, int Length);
    void FillIndicatorRange(int IndicatorId, int StartPos, int Length);
    void ShowAutoComplete(int EnteredLength, const std::string& ItemList);
    void ShowCallTip(int Position, const std::string& TipText);
    void CancelCallTip();
    void ToggleBreakpoint(int ZeroBasedLine);
    void ClearAllBreakpoints();
    bool HasBreakpoint(int ZeroBasedLine) const;
    std::vector<int> GetBreakpointLines() const;
    void SetDebugExecutionLine(int OneBasedLine);
    void ClearDebugExecutionLine();
    void SetZoomLevel(int ZoomSteps);
    int GetZoomLevel() const;
    void ZoomIn();
    void ZoomOut();
    void ResetZoom();
};

#endif
