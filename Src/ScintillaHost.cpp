#include "Include/ScintillaHost.hpp"
#include <algorithm>
#include <cctype>



const int ZeroIntVal = 0;
const int OneIntVal = 1;
const int TwoIntVal = 1 + 1;
const int ThreeIntVal = 3;
const int DefaultScintillaFontSize = 11;
const int LineNumberMarginWidth = 51;
const int BreakpointMarginWidth = 17;
const int FoldMarginWidth = 15;
const int CaretWidthPixels = 1 + 1;
const int MouseDwellTimeMs = 500;
const int CaretLineAlphaVal = 25;
const int MatchIndicatorAlpha = 75;
const int OccurrenceIndicatorAlpha = 55;
const int SnippetIndicatorAlpha = 65;
const int AutoCompleteMaxHeight = 10;
const UINT MenuCmdUndo = 1001;
const UINT MenuCmdRedo = 1003;
const UINT MenuCmdCut = 1005;
const UINT MenuCmdCopy = 1007;
const UINT MenuCmdPaste = 1009;
const UINT MenuCmdDelete = 1011;
const UINT MenuCmdSelectAll = 1013;
const UINT MenuCmdSeparator = 1015;
const int DarkMenuItemWidth = 175;
const int DarkMenuItemHeight = 29;
const int DarkMenuSepHeight = 9;
const int DarkMenuTextPaddingX = 15;

// Describes a single entry in the custom dark Scintilla right-click menu.
struct DarkMenuEntry {
    UINT CommandId;
    const wchar_t* Label;
    bool Enabled;
};

// Applies Windows 11 DarkMode_Explorer theme to a Win32 control for dark scrollbars.
static void ApplyDarkScrollbars(HWND ControlHwnd) {
    HMODULE UxThemeModule = LoadLibraryA("uxtheme.dll");

    if (UxThemeModule != nullptr) {
        typedef HRESULT(WINAPI* PfnSetWindowTheme)(HWND, LPCWSTR, LPCWSTR);
        PfnSetWindowTheme SetThemeFunc = reinterpret_cast<PfnSetWindowTheme>(GetProcAddress(UxThemeModule, "SetWindowTheme"));

        if (SetThemeFunc != nullptr) {
            SetThemeFunc(ControlHwnd, L"DarkMode_Explorer", nullptr);
        }

        FreeLibrary(UxThemeModule);
    }
}

// Converts red, green, and blue byte values into an opaque Scintilla ABGR colour with alpha 255.
static sptr_t MakeRgba(int Red, int Green, int Blue) {
    const unsigned int OpaqueAlphaShift = 25 - 1;
    const unsigned int FullAlphaByte = 255;

    return static_cast<sptr_t>((FullAlphaByte << OpaqueAlphaShift) | static_cast<unsigned int>(RGB(Red, Green, Blue)));
}

// Converts red, green, and blue byte values into a Win32 COLORREF.
static COLORREF MakeRgb(int Red, int Green, int Blue) {
    return RGB(Red, Green, Blue);
}

// Extracts the lowercase file extension including the leading dot.
static std::string ExtractLowerExtension(const std::string& FileName) {
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

// Initializes all host handles and state to safe defaults.
ScintillaHost::ScintillaHost() {
    ScintillaModule = nullptr;
    LexillaModule = nullptr;
    CreateLexerFunc = nullptr;
    ParentWindow = nullptr;
    EditorWindow = nullptr;
    DirectFunc = nullptr;
    DirectPtr = ZeroSptr;
    OriginalEditorProc = nullptr;
    IsVisibleState = false;
    LastBounds = {};
    HasClipExclude = false;
    LastClipExcludes.clear();
}

// Releases the Scintilla window and loaded DLL modules.
ScintillaHost::~ScintillaHost() {
    Shutdown();
}

// Subclassed window procedure forwarding Monaco keybindings and rendering a dark context menu.
LRESULT CALLBACK ScintillaHost::EditorSubclassProc(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam) {
    const LRESULT HandledResult = 0;
    const LRESULT TrueResult = 1;
    const int HalfDivisor = 1 + 1;
    const int ZeroCoord = 0;

    ScintillaHost* Host = reinterpret_cast<ScintillaHost*>(GetPropW(Hwnd, L"VadexScintillaHost"));

    if (Host != nullptr && Msg == WM_MEASUREITEM) {
        MEASUREITEMSTRUCT* Measure = reinterpret_cast<MEASUREITEMSTRUCT*>(LParam);

        if (Measure->CtlType == ODT_MENU) {
            const DarkMenuEntry* Entry = reinterpret_cast<const DarkMenuEntry*>(Measure->itemData);
            Measure->itemWidth = DarkMenuItemWidth;
            Measure->itemHeight = (Entry != nullptr && Entry->CommandId == MenuCmdSeparator) ? DarkMenuSepHeight : DarkMenuItemHeight;

            return TrueResult;
        }
    }

    if (Host != nullptr && Msg == WM_DRAWITEM) {
        DRAWITEMSTRUCT* Draw = reinterpret_cast<DRAWITEMSTRUCT*>(LParam);

        if (Draw->CtlType == ODT_MENU) {
            const DarkMenuEntry* Entry = reinterpret_cast<const DarkMenuEntry*>(Draw->itemData);
            const COLORREF MenuBgColor = MakeRgb(37, 37, 45);
            const COLORREF MenuHoverColor = MakeRgb(69, 69, 79);
            const COLORREF MenuTextColor = MakeRgb(241, 241, 241);
            const COLORREF MenuDisabledColor = MakeRgb(125, 125, 135);
            const COLORREF MenuSepColor = MakeRgb(65, 65, 75);

            bool IsSelected = ((Draw->itemState & ODS_SELECTED) != ZeroIntVal);
            bool IsEnabled = (Entry != nullptr && Entry->Enabled);
            HBRUSH BgBrush = CreateSolidBrush((IsSelected && IsEnabled) ? MenuHoverColor : MenuBgColor);
            FillRect(Draw->hDC, &Draw->rcItem, BgBrush);
            DeleteObject(BgBrush);

            if (Entry != nullptr && Entry->CommandId == MenuCmdSeparator) {
                int MidY = (Draw->rcItem.top + Draw->rcItem.bottom) / HalfDivisor;
                RECT SepRect = { Draw->rcItem.left + DarkMenuTextPaddingX, MidY, Draw->rcItem.right - DarkMenuTextPaddingX, MidY + OneIntVal };
                HBRUSH SepBrush = CreateSolidBrush(MenuSepColor);
                FillRect(Draw->hDC, &SepRect, SepBrush);
                DeleteObject(SepBrush);
            } else if (Entry != nullptr && Entry->Label != nullptr) {
                SetBkMode(Draw->hDC, TRANSPARENT);
                SetTextColor(Draw->hDC, IsEnabled ? MenuTextColor : MenuDisabledColor);
                RECT TextRect = Draw->rcItem;
                TextRect.left += DarkMenuTextPaddingX;
                HFONT MenuFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
                HGDIOBJ OldFont = SelectObject(Draw->hDC, MenuFont);
                DrawTextW(Draw->hDC, Entry->Label, -OneIntVal, &TextRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                SelectObject(Draw->hDC, OldFont);
            }

            return TrueResult;
        }
    }

    if (Host != nullptr && Msg == WM_CONTEXTMENU) {
        POINT ScreenPos = {};
        GetCursorPos(&ScreenPos);

        bool CanUndo = (Host->Call(SCI_CANUNDO) != ZeroSptr);
        bool CanRedo = (Host->Call(SCI_CANREDO) != ZeroSptr);
        bool HasSelection = (Host->Call(SCI_GETSELECTIONSTART) != Host->Call(SCI_GETSELECTIONEND));
        bool CanPaste = (Host->Call(SCI_CANPASTE) != ZeroSptr);
        bool IsReadOnly = (Host->Call(SCI_GETREADONLY) != ZeroSptr);

        DarkMenuEntry Entries[] = {
            { MenuCmdUndo, L"Undo", CanUndo },
            { MenuCmdRedo, L"Redo", CanRedo },
            { MenuCmdSeparator, nullptr, false },
            { MenuCmdCut, L"Cut", HasSelection && !IsReadOnly },
            { MenuCmdCopy, L"Copy", HasSelection },
            { MenuCmdPaste, L"Paste", CanPaste && !IsReadOnly },
            { MenuCmdDelete, L"Delete", !IsReadOnly },
            { MenuCmdSeparator, nullptr, false },
            { MenuCmdSelectAll, L"Select All", true }
        };

        HMENU PopupMenu = CreatePopupMenu();

        if (PopupMenu != nullptr) {
            const COLORREF MenuBgColor = MakeRgb(37, 37, 45);
            MENUINFO MenuInfo = {};
            MenuInfo.cbSize = sizeof(MENUINFO);
            MenuInfo.fMask = MIM_BACKGROUND;
            HBRUSH MenuBgBrush = CreateSolidBrush(MenuBgColor);
            MenuInfo.hbrBack = MenuBgBrush;
            SetMenuInfo(PopupMenu, &MenuInfo);

            for (const auto& Entry : Entries) {
                UINT Flags = MF_OWNERDRAW;

                if (Entry.CommandId == MenuCmdSeparator || !Entry.Enabled) {
                    Flags |= MF_DISABLED | MF_GRAYED;
                }

                AppendMenuW(PopupMenu, Flags, Entry.CommandId, reinterpret_cast<LPCWSTR>(&Entry));
            }

            UINT Chosen = static_cast<UINT>(TrackPopupMenu(PopupMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, ScreenPos.x, ScreenPos.y, ZeroCoord, Hwnd, nullptr));

            if (Chosen == MenuCmdUndo) {
                Host->Call(SCI_UNDO);
            } else if (Chosen == MenuCmdRedo) {
                Host->Call(SCI_REDO);
            } else if (Chosen == MenuCmdCut) {
                Host->Call(SCI_CUT);
            } else if (Chosen == MenuCmdCopy) {
                Host->Call(SCI_COPY);
            } else if (Chosen == MenuCmdPaste) {
                Host->Call(SCI_PASTE);
            } else if (Chosen == MenuCmdDelete) {
                Host->Call(SCI_CLEAR);
            } else if (Chosen == MenuCmdSelectAll) {
                Host->Call(SCI_SELECTALL);
            }

            DestroyMenu(PopupMenu);
            DeleteObject(MenuBgBrush);
        }

        return HandledResult;
    }

    if (Host != nullptr && (Msg == WM_KEYDOWN || Msg == WM_SYSKEYDOWN || Msg == WM_CHAR)) {
        if (Host->KeyDownHandler && Host->KeyDownHandler(Msg, WParam, LParam)) {
            return HandledResult;
        }
    }

    if (Host != nullptr && Host->OriginalEditorProc != nullptr) {
        return CallWindowProcW(Host->OriginalEditorProc, Hwnd, Msg, WParam, LParam);
    }

    return DefWindowProcW(Hwnd, Msg, WParam, LParam);
}

// Loads Scintilla and Lexilla DLLs and creates the native child Scintilla window.
bool ScintillaHost::Initialize(HWND ParentHwnd) {
    const int ZeroPos = 0;
    const int InitialSize = 100;

    if (ParentHwnd == nullptr) {
        return false;
    }

    ParentWindow = ParentHwnd;

    ScintillaModule = LoadLibraryA("Scintilla.dll");

    if (ScintillaModule == nullptr) {
        ScintillaModule = LoadLibraryA("Build/Scintilla.dll");
    }

    if (ScintillaModule == nullptr) {
        return false;
    }

    LexillaModule = LoadLibraryA("Lexilla.dll");

    if (LexillaModule == nullptr) {
        LexillaModule = LoadLibraryA("Build/Lexilla.dll");
    }

    if (LexillaModule != nullptr) {
        CreateLexerFunc = reinterpret_cast<Lexilla::CreateLexerFn>(GetProcAddress(LexillaModule, LEXILLA_CREATELEXER));
    }

    EditorWindow = CreateWindowExW(
        ZeroPos,
        L"Scintilla",
        L"",
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        ZeroPos,
        ZeroPos,
        InitialSize,
        InitialSize,
        ParentWindow,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    if (EditorWindow == nullptr) {
        Shutdown();

        return false;
    }

    DirectFunc = reinterpret_cast<SciFnDirect>(SendMessageW(EditorWindow, SCI_GETDIRECTFUNCTION, ZeroUptr, ZeroSptr));
    DirectPtr = static_cast<sptr_t>(SendMessageW(EditorWindow, SCI_GETDIRECTPOINTER, ZeroUptr, ZeroSptr));

    SetPropW(EditorWindow, L"VadexScintillaHost", reinterpret_cast<HANDLE>(this));
    OriginalEditorProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(EditorWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditorSubclassProc)));

    // Configures UTF-8, DirectWrite hardware rendering, dark scrollbars, custom dark popup, and multi-cursor.
    ApplyDarkScrollbars(EditorWindow);
    Call(SCI_USEPOPUP, SC_POPUP_NEVER);
    Call(SCI_SETCODEPAGE, SC_CP_UTF8);
    Call(SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWRITE);
    Call(SCI_SETBUFFEREDDRAW, ZeroUptr);
    Call(SCI_SETMULTIPLESELECTION, OneIntVal);
    Call(SCI_SETADDITIONALSELECTIONTYPING, OneIntVal);
    Call(SCI_SETMULTIPASTE, SC_MULTIPASTE_EACH);
    Call(SCI_SETADDITIONALCARETSBLINK, OneIntVal);
    Call(SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION);
    Call(SCI_SETUSETABS, ZeroUptr);
    Call(SCI_SETINDENTATIONGUIDES, SC_IV_LOOKBOTH);
    Call(SCI_SETMOUSEDWELLTIME, MouseDwellTimeMs);
    Call(SCI_SETENDATLASTLINE, ZeroUptr);
    Call(SCI_SETHSCROLLBAR, OneIntVal);
    Call(SCI_SETVSCROLLBAR, OneIntVal);
    Call(SCI_SETSCROLLWIDTHTRACKING, OneIntVal);
    Call(SCI_AUTOCSETIGNORECASE, OneIntVal);
    Call(SCI_AUTOCSETMAXHEIGHT, AutoCompleteMaxHeight);

    ApplyBaseDarkTheme();
    ApplyIndicators();

    return true;
}

// Destroys the Scintilla child window and unloads DLL modules.
void ScintillaHost::Shutdown() {
    if (EditorWindow != nullptr) {
        if (OriginalEditorProc != nullptr) {
            SetWindowLongPtrW(EditorWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(OriginalEditorProc));
            OriginalEditorProc = nullptr;
        }

        RemovePropW(EditorWindow, L"VadexScintillaHost");
        DestroyWindow(EditorWindow);
        EditorWindow = nullptr;
    }

    DirectFunc = nullptr;
    DirectPtr = ZeroSptr;
    CreateLexerFunc = nullptr;

    if (LexillaModule != nullptr) {
        FreeLibrary(LexillaModule);
        LexillaModule = nullptr;
    }

    if (ScintillaModule != nullptr) {
        FreeLibrary(ScintillaModule);
        ScintillaModule = nullptr;
    }

    IsVisibleState = false;
}

// Sends a direct message to Scintilla without Win32 message queue overhead.
sptr_t ScintillaHost::Call(unsigned int Message, uptr_t WParam, sptr_t LParam) const {
    if (DirectFunc == nullptr || DirectPtr == ZeroSptr) {
        return ZeroSptr;
    }

    return DirectFunc(DirectPtr, Message, WParam, LParam);
}

// Registers the keyboard shortcut interception handler for the Scintilla HWND.
void ScintillaHost::SetKeyDownHandler(std::function<bool(UINT, WPARAM, LPARAM)> Handler) {
    KeyDownHandler = std::move(Handler);
}

// Repositions and resizes the Scintilla child window when bounds change.
void ScintillaHost::SetBounds(int Left, int Top, int Width, int Height) {
    const int MinDimension = 1;

    if (EditorWindow == nullptr) {
        return;
    }

    int SafeWidth = (Width < MinDimension) ? MinDimension : Width;
    int SafeHeight = (Height < MinDimension) ? MinDimension : Height;

    if (LastBounds.left == Left && LastBounds.top == Top && LastBounds.right == SafeWidth && LastBounds.bottom == SafeHeight) {
        return;
    }

    LastBounds.left = Left;
    LastBounds.top = Top;
    LastBounds.right = SafeWidth;
    LastBounds.bottom = SafeHeight;

    SetWindowPos(EditorWindow, nullptr, Left, Top, SafeWidth, SafeHeight, SWP_NOZORDER | SWP_NOACTIVATE);

    if (HasClipExclude) {
        std::vector<RECT> PrevExcludes = LastClipExcludes;
        HasClipExclude = false;
        LastClipExcludes.clear();
        SetClipExcludeRects(PrevExcludes);
    }
}

// Punches pixel-exact cutout regions in the Scintilla HWND for each overlapping ImGui menu or submenu popup.
void ScintillaHost::SetClipExcludeRects(const std::vector<RECT>& ExcludeRects) {
    const int ZeroCoord = 0;
    const size_t ZeroIdx = 0;

    if (EditorWindow == nullptr) {
        return;
    }

    if (ExcludeRects.empty()) {
        if (HasClipExclude) {
            HasClipExclude = false;
            LastClipExcludes.clear();
            SetWindowRgn(EditorWindow, nullptr, TRUE);
        }

        return;
    }

    bool IsSame = (HasClipExclude && LastClipExcludes.size() == ExcludeRects.size());

    if (IsSame) {
        for (size_t Idx = ZeroIdx; Idx < ExcludeRects.size(); ++Idx) {
            const RECT& OldRect = LastClipExcludes[Idx];
            const RECT& NewRect = ExcludeRects[Idx];

            if (OldRect.left != NewRect.left || OldRect.top != NewRect.top || OldRect.right != NewRect.right || OldRect.bottom != NewRect.bottom) {
                IsSame = false;
                break;
            }
        }
    }

    if (IsSame) {
        return;
    }

    HasClipExclude = true;
    LastClipExcludes = ExcludeRects;

    int EditorWidth = LastBounds.right;
    int EditorHeight = LastBounds.bottom;
    HRGN FullRegion = CreateRectRgn(ZeroCoord, ZeroCoord, EditorWidth, EditorHeight);
    bool AnyIntersect = false;

    for (const RECT& Rect : ExcludeRects) {
        int LocalLeft = Rect.left - LastBounds.left;
        int LocalTop = Rect.top - LastBounds.top;
        int LocalRight = Rect.right - LastBounds.left;
        int LocalBottom = Rect.bottom - LastBounds.top;

        if (LocalRight <= ZeroCoord || LocalBottom <= ZeroCoord || LocalLeft >= EditorWidth || LocalTop >= EditorHeight) {
            continue;
        }

        HRGN CutoutRegion = CreateRectRgn(LocalLeft, LocalTop, LocalRight, LocalBottom);
        CombineRgn(FullRegion, FullRegion, CutoutRegion, RGN_DIFF);
        DeleteObject(CutoutRegion);
        AnyIntersect = true;
    }

    if (!AnyIntersect) {
        DeleteObject(FullRegion);
        SetWindowRgn(EditorWindow, nullptr, TRUE);

        return;
    }

    SetWindowRgn(EditorWindow, FullRegion, TRUE);
}

// Shows or hides the Scintilla child window cleanly.
void ScintillaHost::SetVisible(bool Visible) {
    if (EditorWindow == nullptr || IsVisibleState == Visible) {
        return;
    }

    IsVisibleState = Visible;
    ShowWindow(EditorWindow, Visible ? SW_SHOWNA : SW_HIDE);
}

// Transfers keyboard focus to the Scintilla editor control.
void ScintillaHost::Focus() {
    if (EditorWindow != nullptr && IsVisibleState) {
        SetFocus(EditorWindow);
    }
}

// Applies Vadex dark palette, margins, fold markers, and selection styling.
void ScintillaHost::ApplyBaseDarkTheme() {
    const COLORREF EditorBg = MakeRgb(25, 25, 29);
    const COLORREF EditorFg = MakeRgb(217, 217, 221);
    const COLORREF GutterBg = MakeRgb(25, 25, 29);
    const COLORREF GutterFg = MakeRgb(115, 115, 130);
    const COLORREF FoldFg = MakeRgb(135, 135, 150);
    const COLORREF FoldBg = MakeRgb(35, 35, 41);
    const COLORREF IndentGuideFg = MakeRgb(55, 55, 65);
    const COLORREF SelectionBg = MakeRgb(55, 95, 145);
    const COLORREF CaretColor = MakeRgb(89, 165, 241);
    const COLORREF CaretLineBg = MakeRgb(255, 255, 255);
    const COLORREF BraceMatchFg = MakeRgb(255, 215, 0);
    const COLORREF BraceMatchBg = MakeRgb(65, 65, 85);
    const COLORREF BraceBadFg = MakeRgb(245, 85, 85);
    const COLORREF CallTipBg = MakeRgb(37, 37, 45);
    const COLORREF CallTipFg = MakeRgb(230, 230, 235);

    Call(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>("Cascadia Mono"));
    Call(SCI_STYLESETSIZE, STYLE_DEFAULT, DefaultScintillaFontSize);
    Call(SCI_STYLESETBACK, STYLE_DEFAULT, EditorBg);
    Call(SCI_STYLESETFORE, STYLE_DEFAULT, EditorFg);
    Call(SCI_STYLECLEARALL);

    // Configures line number, breakpoint, and fold margins.
    Call(SCI_SETMARGINTYPEN, MarginLineNumbers, SC_MARGIN_NUMBER);
    Call(SCI_SETMARGINWIDTHN, MarginLineNumbers, LineNumberMarginWidth);
    Call(SCI_STYLESETBACK, STYLE_LINENUMBER, GutterBg);
    Call(SCI_STYLESETFORE, STYLE_LINENUMBER, GutterFg);

    const COLORREF BreakpointRed = MakeRgb(235, 65, 65);
    const COLORREF BreakpointLineBg = MakeRgb(65, 25, 29);
    const int BreakpointLineAlpha = 95;
    const uptr_t BreakpointMask = (static_cast<uptr_t>(OneIntVal) << MarkerBreakpoint);

    Call(SCI_SETMARGINTYPEN, MarginBreakpoints, SC_MARGIN_SYMBOL);
    Call(SCI_SETMARGINMASKN, MarginBreakpoints, static_cast<sptr_t>(BreakpointMask));
    Call(SCI_SETMARGINWIDTHN, MarginBreakpoints, BreakpointMarginWidth);
    Call(SCI_SETMARGINSENSITIVEN, MarginBreakpoints, OneIntVal);
    Call(SCI_MARKERDEFINE, MarkerBreakpoint, SC_MARK_CIRCLE);
    Call(SCI_MARKERSETFORE, MarkerBreakpoint, BreakpointRed);
    Call(SCI_MARKERSETBACK, MarkerBreakpoint, BreakpointRed);
    Call(SCI_MARKERDEFINE, MarkerBreakpointLine, SC_MARK_BACKGROUND);
    Call(SCI_MARKERSETBACK, MarkerBreakpointLine, BreakpointLineBg);
    Call(SCI_MARKERSETALPHA, MarkerBreakpointLine, BreakpointLineAlpha);

    const COLORREF DebugLineBg = MakeRgb(95, 85, 25);
    const int DebugLineAlpha = 115;

    Call(SCI_MARKERDEFINE, MarkerDebugLine, SC_MARK_BACKGROUND);
    Call(SCI_MARKERSETBACK, MarkerDebugLine, DebugLineBg);
    Call(SCI_MARKERSETALPHA, MarkerDebugLine, DebugLineAlpha);

    Call(SCI_SETMARGINTYPEN, MarginFolding, SC_MARGIN_SYMBOL);
    Call(SCI_SETMARGINMASKN, MarginFolding, SC_MASK_FOLDERS);
    Call(SCI_SETMARGINWIDTHN, MarginFolding, FoldMarginWidth);
    Call(SCI_SETMARGINSENSITIVEN, MarginFolding, OneIntVal);
    Call(SCI_SETFOLDMARGINCOLOUR, OneIntVal, GutterBg);
    Call(SCI_SETFOLDMARGINHICOLOUR, OneIntVal, GutterBg);
    Call(SCI_SETAUTOMATICFOLD, SC_AUTOMATICFOLD_SHOW | SC_AUTOMATICFOLD_CLICK | SC_AUTOMATICFOLD_CHANGE);

    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);

    const int FoldMarkers[] = {
        SC_MARKNUM_FOLDEROPEN, SC_MARKNUM_FOLDER, SC_MARKNUM_FOLDERSUB,
        SC_MARKNUM_FOLDERTAIL, SC_MARKNUM_FOLDEREND, SC_MARKNUM_FOLDEROPENMID,
        SC_MARKNUM_FOLDERMIDTAIL
    };

    for (int MarkerId : FoldMarkers) {
        Call(SCI_MARKERSETFORE, static_cast<uptr_t>(MarkerId), FoldBg);
        Call(SCI_MARKERSETBACK, static_cast<uptr_t>(MarkerId), FoldFg);
    }

    // Configures indentation guides, selection, caret, brace match, and calltips.
    Call(SCI_STYLESETFORE, STYLE_INDENTGUIDE, IndentGuideFg);
    Call(SCI_STYLESETBACK, STYLE_INDENTGUIDE, EditorBg);
    Call(SCI_SETSELBACK, OneIntVal, SelectionBg);
    Call(SCI_SETCARETFORE, CaretColor);
    Call(SCI_SETCARETWIDTH, CaretWidthPixels);
    Call(SCI_SETCARETLINEVISIBLE, OneIntVal);
    Call(SCI_SETCARETLINEBACK, CaretLineBg);
    Call(SCI_SETCARETLINEBACKALPHA, CaretLineAlphaVal);
    Call(SCI_STYLESETFORE, STYLE_BRACELIGHT, BraceMatchFg);
    Call(SCI_STYLESETBACK, STYLE_BRACELIGHT, BraceMatchBg);
    Call(SCI_STYLESETBOLD, STYLE_BRACELIGHT, OneIntVal);
    Call(SCI_STYLESETFORE, STYLE_BRACEBAD, BraceBadFg);
    Call(SCI_CALLTIPSETBACK, CallTipBg);
    Call(SCI_CALLTIPSETFORE, CallTipFg);

    // Themes Scintilla's native autocomplete dropdown list in dark mode.
    const sptr_t AutoListText = MakeRgba(241, 241, 241);
    const sptr_t AutoListBg = MakeRgba(37, 37, 45);
    const sptr_t AutoListSelText = MakeRgba(255, 255, 255);
    const sptr_t AutoListSelBg = MakeRgba(69, 69, 79);

    Call(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST, AutoListText);
    Call(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST_BACK, AutoListBg);
    Call(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST_SELECTED, AutoListSelText);
    Call(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST_SELECTED_BACK, AutoListSelBg);
}

// Configures all Scintilla indicators used for search, occurrences, rainbow brackets, snippets, and LSP diagnostics.
void ScintillaHost::ApplyIndicators() {
    const COLORREF MatchColor = MakeRgb(235, 185, 65);
    const COLORREF OccurrenceColor = MakeRgb(130, 175, 235);
    const COLORREF GoldColor = MakeRgb(245, 205, 75);
    const COLORREF VioletColor = MakeRgb(215, 115, 215);
    const COLORREF CyanColor = MakeRgb(75, 195, 245);
    const COLORREF ErrorColor = MakeRgb(245, 75, 75);
    const COLORREF WarningColor = MakeRgb(235, 195, 65);
    const COLORREF SnippetColor = MakeRgb(89, 165, 241);

    Call(SCI_INDICSETSTYLE, IndicatorMatchHighlight, INDIC_ROUNDBOX);
    Call(SCI_INDICSETFORE, IndicatorMatchHighlight, MatchColor);
    Call(SCI_INDICSETALPHA, IndicatorMatchHighlight, MatchIndicatorAlpha);

    Call(SCI_INDICSETSTYLE, IndicatorOccurrence, INDIC_ROUNDBOX);
    Call(SCI_INDICSETFORE, IndicatorOccurrence, OccurrenceColor);
    Call(SCI_INDICSETALPHA, IndicatorOccurrence, OccurrenceIndicatorAlpha);

    Call(SCI_INDICSETSTYLE, IndicatorBracketGold, INDIC_TEXTFORE);
    Call(SCI_INDICSETFORE, IndicatorBracketGold, GoldColor);

    Call(SCI_INDICSETSTYLE, IndicatorBracketViolet, INDIC_TEXTFORE);
    Call(SCI_INDICSETFORE, IndicatorBracketViolet, VioletColor);

    Call(SCI_INDICSETSTYLE, IndicatorBracketCyan, INDIC_TEXTFORE);
    Call(SCI_INDICSETFORE, IndicatorBracketCyan, CyanColor);

    Call(SCI_INDICSETSTYLE, IndicatorBracketError, INDIC_TEXTFORE);
    Call(SCI_INDICSETFORE, IndicatorBracketError, ErrorColor);

    Call(SCI_INDICSETSTYLE, IndicatorSnippetStop, INDIC_ROUNDBOX);
    Call(SCI_INDICSETFORE, IndicatorSnippetStop, SnippetColor);
    Call(SCI_INDICSETALPHA, IndicatorSnippetStop, SnippetIndicatorAlpha);

    Call(SCI_INDICSETSTYLE, IndicatorDiagError, INDIC_SQUIGGLE);
    Call(SCI_INDICSETFORE, IndicatorDiagError, ErrorColor);

    Call(SCI_INDICSETSTYLE, IndicatorDiagWarning, INDIC_SQUIGGLE);
    Call(SCI_INDICSETFORE, IndicatorDiagWarning, WarningColor);
}

// Applies One Dark / VS Code Dark+ styles for C-family and JS/TS lexers.
void ScintillaHost::ApplyCppStyles() {
    const COLORREF CommentColor = MakeRgb(105, 155, 85);
    const COLORREF NumberColor = MakeRgb(181, 205, 169);
    const COLORREF KeywordColor = MakeRgb(197, 135, 193);
    const COLORREF TypeColor = MakeRgb(79, 201, 175);
    const COLORREF StringColor = MakeRgb(205, 145, 121);
    const COLORREF PreprocColor = MakeRgb(85, 155, 215);
    const COLORREF OperatorColor = MakeRgb(215, 215, 221);

    Call(SCI_STYLESETFORE, SCE_C_COMMENT, CommentColor);
    Call(SCI_STYLESETFORE, SCE_C_COMMENTLINE, CommentColor);
    Call(SCI_STYLESETFORE, SCE_C_COMMENTDOC, CommentColor);
    Call(SCI_STYLESETFORE, SCE_C_NUMBER, NumberColor);
    Call(SCI_STYLESETFORE, SCE_C_WORD, KeywordColor);
    Call(SCI_STYLESETFORE, SCE_C_WORD2, TypeColor);
    Call(SCI_STYLESETFORE, SCE_C_STRING, StringColor);
    Call(SCI_STYLESETFORE, SCE_C_CHARACTER, StringColor);
    Call(SCI_STYLESETFORE, SCE_C_STRINGRAW, StringColor);
    Call(SCI_STYLESETFORE, SCE_C_PREPROCESSOR, PreprocColor);
    Call(SCI_STYLESETFORE, SCE_C_OPERATOR, OperatorColor);

    const char* PrimaryKeywords =
        "alignas alignof auto break case catch class const constexpr const_cast continue "
        "default delete do dynamic_cast else enum explicit export extern false final for "
        "friend goto if inline mutable namespace new noexcept nullptr operator override "
        "private protected public reinterpret_cast return sizeof static static_assert "
        "static_cast struct switch template this throw true try typedef typeid typename "
        "union using virtual volatile while async await function let var import from extends "
        "super typeof instanceof yield undefined null of in";
    const char* TypeKeywords =
        "bool char char16_t char32_t double float int long short signed unsigned void wchar_t "
        "size_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t string "
        "vector map unordered_map set mutex thread HWND DWORD HANDLE LRESULT WPARAM LPARAM "
        "UINT BOOL LONG POINT RECT ImVec2 ImVec4 ImU32 ImFont ImDrawList GLFWwindow";

    Call(SCI_SETKEYWORDS, ZeroUptr, reinterpret_cast<sptr_t>(PrimaryKeywords));
    Call(SCI_SETKEYWORDS, OneIntVal, reinterpret_cast<sptr_t>(TypeKeywords));
}

// Applies dark theme styles for the Python lexer.
void ScintillaHost::ApplyPythonStyles() {
    const COLORREF CommentColor = MakeRgb(105, 155, 85);
    const COLORREF NumberColor = MakeRgb(181, 205, 169);
    const COLORREF KeywordColor = MakeRgb(197, 135, 193);
    const COLORREF StringColor = MakeRgb(205, 145, 121);
    const COLORREF FunctionColor = MakeRgb(221, 221, 171);
    const COLORREF ClassColor = MakeRgb(79, 201, 175);

    Call(SCI_STYLESETFORE, SCE_P_COMMENTLINE, CommentColor);
    Call(SCI_STYLESETFORE, SCE_P_COMMENTBLOCK, CommentColor);
    Call(SCI_STYLESETFORE, SCE_P_NUMBER, NumberColor);
    Call(SCI_STYLESETFORE, SCE_P_WORD, KeywordColor);
    Call(SCI_STYLESETFORE, SCE_P_STRING, StringColor);
    Call(SCI_STYLESETFORE, SCE_P_CHARACTER, StringColor);
    Call(SCI_STYLESETFORE, SCE_P_TRIPLE, StringColor);
    Call(SCI_STYLESETFORE, SCE_P_TRIPLEDOUBLE, StringColor);
    Call(SCI_STYLESETFORE, SCE_P_CLASSNAME, ClassColor);
    Call(SCI_STYLESETFORE, SCE_P_DEFNAME, FunctionColor);

    const char* PyKeywords =
        "and as assert async await break class continue def del elif else except False "
        "finally for from global if import in is lambda None nonlocal not or pass raise "
        "return True try while with yield";

    Call(SCI_SETKEYWORDS, ZeroUptr, reinterpret_cast<sptr_t>(PyKeywords));
}

// Applies dark theme styles for HTML and XML lexers.
void ScintillaHost::ApplyMarkupStyles() {
    const COLORREF TagColor = MakeRgb(85, 155, 215);
    const COLORREF AttrColor = MakeRgb(155, 220, 255);
    const COLORREF StringColor = MakeRgb(205, 145, 121);
    const COLORREF CommentColor = MakeRgb(105, 155, 85);

    Call(SCI_STYLESETFORE, SCE_H_TAG, TagColor);
    Call(SCI_STYLESETFORE, SCE_H_TAGUNKNOWN, TagColor);
    Call(SCI_STYLESETFORE, SCE_H_ATTRIBUTE, AttrColor);
    Call(SCI_STYLESETFORE, SCE_H_ATTRIBUTEUNKNOWN, AttrColor);
    Call(SCI_STYLESETFORE, SCE_H_DOUBLESTRING, StringColor);
    Call(SCI_STYLESETFORE, SCE_H_SINGLESTRING, StringColor);
    Call(SCI_STYLESETFORE, SCE_H_COMMENT, CommentColor);
}

// Applies dark theme styles for Markdown lexers.
void ScintillaHost::ApplyMarkdownStyles() {
    const COLORREF HeaderColor = MakeRgb(85, 155, 215);
    const COLORREF CodeColor = MakeRgb(205, 145, 121);
    const COLORREF StrongColor = MakeRgb(221, 221, 171);
    const COLORREF QuoteColor = MakeRgb(105, 155, 85);
    const COLORREF LinkColor = MakeRgb(79, 201, 175);

    Call(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER1, HeaderColor);
    Call(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER2, HeaderColor);
    Call(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER3, HeaderColor);
    Call(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER4, HeaderColor);
    Call(SCI_STYLESETFORE, SCE_MARKDOWN_CODE, CodeColor);
    Call(SCI_STYLESETFORE, SCE_MARKDOWN_CODE2, CodeColor);
    Call(SCI_STYLESETFORE, SCE_MARKDOWN_CODEBK, CodeColor);
    Call(SCI_STYLESETFORE, SCE_MARKDOWN_STRONG1, StrongColor);
    Call(SCI_STYLESETFORE, SCE_MARKDOWN_STRONG2, StrongColor);
    Call(SCI_STYLESETFORE, SCE_MARKDOWN_BLOCKQUOTE, QuoteColor);
    Call(SCI_STYLESETFORE, SCE_MARKDOWN_LINK, LinkColor);
}

// Applies dark theme styles for JSON lexers.
void ScintillaHost::ApplyJsonStyles() {
    const COLORREF PropertyColor = MakeRgb(155, 220, 255);
    const COLORREF StringColor = MakeRgb(205, 145, 121);
    const COLORREF NumberColor = MakeRgb(181, 205, 169);
    const COLORREF KeywordColor = MakeRgb(85, 155, 215);

    Call(SCI_STYLESETFORE, SCE_JSON_PROPERTYNAME, PropertyColor);
    Call(SCI_STYLESETFORE, SCE_JSON_STRING, StringColor);
    Call(SCI_STYLESETFORE, SCE_JSON_NUMBER, NumberColor);
    Call(SCI_STYLESETFORE, SCE_JSON_KEYWORD, KeywordColor);
}

// Creates a new Scintilla native document buffer populated with initial text.
sptr_t ScintillaHost::CreateDocument(const std::string& InitialText) {
    if (!IsReady()) {
        return ZeroSptr;
    }

    sptr_t PreviousDoc = Call(SCI_GETDOCPOINTER);

    if (PreviousDoc != ZeroSptr) {
        Call(SCI_ADDREFDOCUMENT, ZeroUptr, PreviousDoc);
    }

    sptr_t NewDoc = Call(SCI_CREATEDOCUMENT, ZeroUptr, SC_DOCUMENTOPTION_DEFAULT);

    if (NewDoc == ZeroSptr) {
        if (PreviousDoc != ZeroSptr) {
            Call(SCI_RELEASEDOCUMENT, ZeroUptr, PreviousDoc);
        }

        return ZeroSptr;
    }

    Call(SCI_SETDOCPOINTER, ZeroUptr, NewDoc);
    Call(SCI_SETTEXT, ZeroUptr, reinterpret_cast<sptr_t>(InitialText.c_str()));
    Call(SCI_EMPTYUNDOBUFFER);
    Call(SCI_SETSAVEPOINT);

    if (PreviousDoc != ZeroSptr) {
        Call(SCI_SETDOCPOINTER, ZeroUptr, PreviousDoc);
        Call(SCI_RELEASEDOCUMENT, ZeroUptr, PreviousDoc);
    }

    return NewDoc;
}

// Decrements the reference count of a Scintilla document pointer.
void ScintillaHost::ReleaseDocument(sptr_t DocPtr) {
    if (IsReady() && DocPtr != ZeroSptr) {
        Call(SCI_RELEASEDOCUMENT, ZeroUptr, DocPtr);
    }
}

// Switches the active view to a tab's Scintilla document and applies its language lexer.
void ScintillaHost::SetActiveDocument(sptr_t DocPtr, const std::string& FileName, int TabSize) {
    if (!IsReady() || DocPtr == ZeroSptr) {
        return;
    }

    if (Call(SCI_GETDOCPOINTER) != DocPtr) {
        Call(SCI_SETDOCPOINTER, ZeroUptr, DocPtr);
    }

    SetTabWidth(TabSize);
    ApplyLanguageForFile(FileName);
}

// Selects and configures the matching Lexilla lexer for a filename.
void ScintillaHost::ApplyLanguageForFile(const std::string& FileName) {
    if (!IsReady()) {
        return;
    }

    ApplyBaseDarkTheme();

    std::string Ext = ExtractLowerExtension(FileName);
    const char* LexerName = "null";

    if (Ext == ".cpp" || Ext == ".hpp" || Ext == ".h" || Ext == ".c" || Ext == ".cc" || Ext == ".cxx" || Ext == ".cs" || Ext == ".java" || Ext == ".js" || Ext == ".jsx" || Ext == ".ts" || Ext == ".tsx") {
        LexerName = "cpp";
    } else if (Ext == ".py") {
        LexerName = "python";
    } else if (Ext == ".html" || Ext == ".htm" || Ext == ".svg") {
        LexerName = "hypertext";
    } else if (Ext == ".xml") {
        LexerName = "xml";
    } else if (Ext == ".css") {
        LexerName = "css";
    } else if (Ext == ".md") {
        LexerName = "markdown";
    } else if (Ext == ".json") {
        LexerName = "json";
    } else if (Ext == ".ps1") {
        LexerName = "powershell";
    } else if (Ext == ".bat" || Ext == ".cmd") {
        LexerName = "batch";
    } else if (Ext == ".rs") {
        LexerName = "rust";
    } else if (Ext == ".sql") {
        LexerName = "sql";
    } else if (Ext == ".yaml" || Ext == ".yml") {
        LexerName = "yaml";
    } else if (FileName == "Makefile" || FileName == "makefile") {
        LexerName = "makefile";
    }

    if (CreateLexerFunc != nullptr) {
        Scintilla::ILexer5* LexerObj = CreateLexerFunc(LexerName);
        Call(SCI_SETILEXER, ZeroUptr, reinterpret_cast<sptr_t>(LexerObj));
    }

    Call(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("fold"), reinterpret_cast<sptr_t>("1"));
    Call(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("fold.compact"), reinterpret_cast<sptr_t>("0"));
    Call(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("fold.comment"), reinterpret_cast<sptr_t>("1"));
    Call(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("fold.preprocessor"), reinterpret_cast<sptr_t>("1"));

    std::string LexerStr(LexerName);

    if (LexerStr == "cpp") {
        ApplyCppStyles();
    } else if (LexerStr == "python") {
        ApplyPythonStyles();
    } else if (LexerStr == "hypertext" || LexerStr == "xml") {
        ApplyMarkupStyles();
    } else if (LexerStr == "markdown") {
        ApplyMarkdownStyles();
    } else if (LexerStr == "json") {
        ApplyJsonStyles();
    }

    ApplyIndicators();
    Call(SCI_COLOURISE, ZeroUptr, static_cast<sptr_t>(-OneIntVal));
}

// Retrieves the full UTF-8 text of the currently active document.
std::string ScintillaHost::GetText() const {
    if (!IsReady()) {
        return "";
    }

    sptr_t TextLength = Call(SCI_GETLENGTH);

    if (TextLength <= ZeroSptr) {
        return "";
    }

    std::string Buffer(static_cast<size_t>(TextLength) + OneIntVal, '\0');
    Call(SCI_GETTEXT, static_cast<uptr_t>(TextLength + OneIntVal), reinterpret_cast<sptr_t>(Buffer.data()));
    Buffer.resize(static_cast<size_t>(TextLength));

    return Buffer;
}

// Reads the UTF-8 text of any Scintilla document pointer without keeping a duplicate copy in RAM.
std::string ScintillaHost::GetDocumentText(sptr_t DocPtr) {
    if (!IsReady() || DocPtr == ZeroSptr) {
        return "";
    }

    sptr_t CurrentDoc = Call(SCI_GETDOCPOINTER);

    if (CurrentDoc == DocPtr) {
        return GetText();
    }

    Call(SCI_ADDREFDOCUMENT, ZeroUptr, CurrentDoc);
    Call(SCI_SETDOCPOINTER, ZeroUptr, DocPtr);
    std::string Result = GetText();
    Call(SCI_SETDOCPOINTER, ZeroUptr, CurrentDoc);
    Call(SCI_RELEASEDOCUMENT, ZeroUptr, CurrentDoc);

    return Result;
}

// Replaces the text of the currently active document.
void ScintillaHost::SetText(const std::string& Text) {
    if (IsReady()) {
        Call(SCI_SETTEXT, ZeroUptr, reinterpret_cast<sptr_t>(Text.c_str()));
    }
}

// Marks the current undo state as clean/saved.
void ScintillaHost::SetSavePoint() {
    if (IsReady()) {
        Call(SCI_SETSAVEPOINT);
    }
}

// Sets whether the active document is read-only.
void ScintillaHost::SetReadOnly(bool ReadOnly) {
    if (IsReady()) {
        Call(SCI_SETREADONLY, ReadOnly ? OneIntVal : ZeroIntVal);
    }
}

// Configures the tab and indent width for the active document.
void ScintillaHost::SetTabWidth(int TabSize) {
    if (IsReady()) {
        Call(SCI_SETTABWIDTH, static_cast<uptr_t>(TabSize));
        Call(SCI_SETINDENT, static_cast<uptr_t>(TabSize));
    }
}

// Toggles soft word wrap in the editor view.
void ScintillaHost::ToggleWordWrap() {
    if (!IsReady()) {
        return;
    }

    bool Enabled = IsWordWrapEnabled();
    Call(SCI_SETWRAPMODE, Enabled ? SC_WRAP_NONE : SC_WRAP_WORD);
}

// Returns whether soft word wrap is currently active.
bool ScintillaHost::IsWordWrapEnabled() const {
    if (!IsReady()) {
        return false;
    }

    return (Call(SCI_GETWRAPMODE) != SC_WRAP_NONE);
}

// Returns the total number of lines in the active document.
int ScintillaHost::GetLineCount() const {
    return static_cast<int>(Call(SCI_GETLINECOUNT));
}

// Returns the zero-based first visible line index.
int ScintillaHost::GetFirstVisibleLine() const {
    return static_cast<int>(Call(SCI_GETFIRSTVISIBLELINE));
}

// Scrolls the viewport to place the given zero-based line at the top.
void ScintillaHost::SetFirstVisibleLine(int LineIndex) {
    Call(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(LineIndex));
}

// Returns the number of complete lines currently visible on screen.
int ScintillaHost::GetLinesOnScreen() const {
    return static_cast<int>(Call(SCI_LINESONSCREEN));
}

// Returns the current caret byte position.
int ScintillaHost::GetCurrentPos() const {
    return static_cast<int>(Call(SCI_GETCURRENTPOS));
}

// Returns the one-based line number of the caret.
int ScintillaHost::GetCurrentLine() const {
    int Pos = GetCurrentPos();

    return static_cast<int>(Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(Pos))) + OneIntVal;
}

// Returns the one-based column number of the caret.
int ScintillaHost::GetCurrentColumn() const {
    int Pos = GetCurrentPos();

    return static_cast<int>(Call(SCI_GETCOLUMN, static_cast<uptr_t>(Pos))) + OneIntVal;
}

// Returns the text of a specific zero-based line without trailing CR/LF characters.
std::string ScintillaHost::GetLineText(int LineIndex) const {
    if (!IsReady() || LineIndex < ZeroIntVal || LineIndex >= GetLineCount()) {
        return "";
    }

    sptr_t LineLen = Call(SCI_LINELENGTH, static_cast<uptr_t>(LineIndex));

    if (LineLen <= ZeroSptr) {
        return "";
    }

    std::string LineStr(static_cast<size_t>(LineLen) + OneIntVal, '\0');
    Call(SCI_GETLINE, static_cast<uptr_t>(LineIndex), reinterpret_cast<sptr_t>(LineStr.data()));
    LineStr.resize(static_cast<size_t>(LineLen));

    while (!LineStr.empty() && (LineStr.back() == '\n' || LineStr.back() == '\r')) {
        LineStr.pop_back();
    }

    return LineStr;
}

// Returns the fold parent line index for a zero-based line, or -1 if none.
int ScintillaHost::GetFoldParent(int LineIndex) const {
    return static_cast<int>(Call(SCI_GETFOLDPARENT, static_cast<uptr_t>(LineIndex)));
}

// Moves the caret and scrolls to a one-based line number.
void ScintillaHost::GoToLine(int OneBasedLine) {
    int ZeroLine = OneBasedLine - OneIntVal;

    if (ZeroLine < ZeroIntVal) {
        ZeroLine = ZeroIntVal;
    }

    Call(SCI_GOTOLINE, static_cast<uptr_t>(ZeroLine));
    Call(SCI_SCROLLCARET);
}

// Moves the caret and scrolls to a one-based line and column.
void ScintillaHost::GoToLineColumn(int OneBasedLine, int OneBasedCol) {
    int ZeroLine = (OneBasedLine > ZeroIntVal) ? (OneBasedLine - OneIntVal) : ZeroIntVal;
    int ZeroCol = (OneBasedCol > ZeroIntVal) ? (OneBasedCol - OneIntVal) : ZeroIntVal;
    sptr_t LineStart = Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(ZeroLine));
    sptr_t LineEnd = Call(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(ZeroLine));
    sptr_t TargetPos = LineStart + ZeroCol;

    if (TargetPos > LineEnd) {
        TargetPos = LineEnd;
    }

    Call(SCI_GOTOPOS, static_cast<uptr_t>(TargetPos));
    Call(SCI_SCROLLCARET);
}

// Returns the single-line comment prefix for a given filename.
std::string ScintillaHost::GetCommentPrefixForFile(const std::string& FileName) const {
    std::string Ext = ExtractLowerExtension(FileName);

    if (Ext == ".py" || Ext == ".sh" || Ext == ".yaml" || Ext == ".yml" || Ext == ".ps1" || FileName == "Makefile" || FileName == "makefile") {
        return "#";
    }
    if (Ext == ".sql" || Ext == ".lua") {
        return "--";
    }

    return "//";
}

// Clears an indicator over the specified byte range.
void ScintillaHost::ClearIndicatorRange(int IndicatorId, int StartPos, int Length) {
    if (!IsReady() || Length <= ZeroIntVal) {
        return;
    }

    Call(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(IndicatorId));
    Call(SCI_INDICATORCLEARRANGE, static_cast<uptr_t>(StartPos), static_cast<sptr_t>(Length));
}

// Applies an indicator over the specified byte range.
void ScintillaHost::FillIndicatorRange(int IndicatorId, int StartPos, int Length) {
    if (!IsReady() || Length <= ZeroIntVal) {
        return;
    }

    Call(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(IndicatorId));
    Call(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(StartPos), static_cast<sptr_t>(Length));
}

// Displays Scintilla's native autocomplete popup list.
void ScintillaHost::ShowAutoComplete(int EnteredLength, const std::string& ItemList) {
    if (IsReady() && !ItemList.empty()) {
        Call(SCI_AUTOCSHOW, static_cast<uptr_t>(EnteredLength), reinterpret_cast<sptr_t>(ItemList.c_str()));
    }
}

// Displays Scintilla's native calltip popup at a byte position.
void ScintillaHost::ShowCallTip(int Position, const std::string& TipText) {
    if (IsReady() && !TipText.empty()) {
        Call(SCI_CALLTIPSHOW, static_cast<uptr_t>(Position), reinterpret_cast<sptr_t>(TipText.c_str()));
    }
}

// Dismisses any visible Scintilla calltip popup.
void ScintillaHost::CancelCallTip() {
    if (IsReady() && Call(SCI_CALLTIPACTIVE) != ZeroSptr) {
        Call(SCI_CALLTIPCANCEL);
    }
}

// Toggles a breakpoint marker and line tint on a zero-based line.
void ScintillaHost::ToggleBreakpoint(int ZeroBasedLine) {
    if (!IsReady() || ZeroBasedLine < ZeroIntVal) {
        return;
    }

    if (HasBreakpoint(ZeroBasedLine)) {
        Call(SCI_MARKERDELETE, static_cast<uptr_t>(ZeroBasedLine), MarkerBreakpoint);
        Call(SCI_MARKERDELETE, static_cast<uptr_t>(ZeroBasedLine), MarkerBreakpointLine);
    } else {
        Call(SCI_MARKERADD, static_cast<uptr_t>(ZeroBasedLine), MarkerBreakpoint);
        Call(SCI_MARKERADD, static_cast<uptr_t>(ZeroBasedLine), MarkerBreakpointLine);
    }
}

// Removes all breakpoint markers from the active document.
void ScintillaHost::ClearAllBreakpoints() {
    if (!IsReady()) {
        return;
    }

    Call(SCI_MARKERDELETEALL, MarkerBreakpoint);
    Call(SCI_MARKERDELETEALL, MarkerBreakpointLine);
}

// Returns whether a breakpoint is currently set on the given zero-based line.
bool ScintillaHost::HasBreakpoint(int ZeroBasedLine) const {
    if (!IsReady() || ZeroBasedLine < ZeroIntVal) {
        return false;
    }

    sptr_t Mask = Call(SCI_MARKERGET, static_cast<uptr_t>(ZeroBasedLine));

    return ((Mask & (static_cast<sptr_t>(OneIntVal) << MarkerBreakpoint)) != ZeroSptr);
}

// Returns all one-based line numbers that currently have breakpoints in the active document.
std::vector<int> ScintillaHost::GetBreakpointLines() const {
    std::vector<int> Lines;

    if (!IsReady()) {
        return Lines;
    }

    const int NegativeOne = -1;
    const uptr_t Mask = (static_cast<uptr_t>(OneIntVal) << MarkerBreakpoint);
    int SearchLine = ZeroIntVal;

    while (true) {
        int FoundLine = static_cast<int>(Call(SCI_MARKERNEXT, static_cast<uptr_t>(SearchLine), static_cast<sptr_t>(Mask)));

        if (FoundLine == NegativeOne) {
            break;
        }

        Lines.push_back(FoundLine + OneIntVal);
        SearchLine = FoundLine + OneIntVal;
    }

    return Lines;
}

// Highlights the currently paused debugger execution line.
void ScintillaHost::SetDebugExecutionLine(int OneBasedLine) {
    if (!IsReady()) {
        return;
    }

    ClearDebugExecutionLine();

    if (OneBasedLine > ZeroIntVal) {
        Call(SCI_MARKERADD, static_cast<uptr_t>(OneBasedLine - OneIntVal), MarkerDebugLine);
        GoToLine(OneBasedLine);
    }
}

// Clears the paused debugger execution line highlight.
void ScintillaHost::ClearDebugExecutionLine() {
    if (IsReady()) {
        Call(SCI_MARKERDELETEALL, MarkerDebugLine);
    }
}

// Sets the Scintilla font zoom level in steps.
void ScintillaHost::SetZoomLevel(int ZoomSteps) {
    if (IsReady()) {
        Call(SCI_SETZOOM, static_cast<uptr_t>(ZoomSteps));
    }
}

// Returns the current Scintilla font zoom level.
int ScintillaHost::GetZoomLevel() const {
    if (!IsReady()) {
        return ZeroIntVal;
    }

    return static_cast<int>(Call(SCI_GETZOOM));
}

// Increases the Scintilla editor font zoom by one step.
void ScintillaHost::ZoomIn() {
    if (IsReady()) {
        Call(SCI_ZOOMIN);
    }
}

// Decreases the Scintilla editor font zoom by one step.
void ScintillaHost::ZoomOut() {
    if (IsReady()) {
        Call(SCI_ZOOMOUT);
    }
}

// Resets the Scintilla editor font zoom to default.
void ScintillaHost::ResetZoom() {
    SetZoomLevel(ZeroIntVal);
}
