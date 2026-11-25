#define GLFW_EXPOSE_NATIVE_WIN32
#include "Include/Editor.hpp"
#include "ImGUI/imgui_internal.h"
#include "GLFW/Include/glfw3native.h"
#include <windows.h>
#include <ole2.h>
#include <shobjidl.h>
#include <commdlg.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <string>
#include <algorithm>
#include <cstring>



using namespace std::filesystem;


// Holds callback context for terminal command history navigation.
struct TerminalHistoryCallbackData {
    std::vector<std::string>* History;
    int* HistoryIndex;
};

// Handles Up/Down arrow command history cycling in the terminal input field.
static int TerminalHistoryCallback(ImGuiInputTextCallbackData* Data) {
    const int KeepCharacter = 0;
    const int ZeroIndex = 0;

    TerminalHistoryCallbackData* Context = static_cast<TerminalHistoryCallbackData*>(Data->UserData);

    if (Context == nullptr || Context->History == nullptr || Context->History->empty()) {
        return KeepCharacter;
    }

    if (Data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        int HistoryCount = static_cast<int>(Context->History->size());
        int CurrentIndex = *(Context->HistoryIndex);

        if (Data->EventKey == ImGuiKey_UpArrow) {
            if (CurrentIndex > ZeroIndex) {
                CurrentIndex--;
            }
        } else if (Data->EventKey == ImGuiKey_DownArrow) {
            if (CurrentIndex < HistoryCount) {
                CurrentIndex++;
            }
        }

        *(Context->HistoryIndex) = CurrentIndex;
        Data->DeleteChars(ZeroIndex, Data->BufTextLen);

        if (CurrentIndex >= ZeroIndex && CurrentIndex < HistoryCount) {
            const std::string& Entry = (*Context->History)[static_cast<size_t>(CurrentIndex)];
            Data->InsertChars(ZeroIndex, Entry.c_str());
        }
    }

    return KeepCharacter;
}

// Transfers Win32 keyboard focus back to the main GLFW/ImGui window.
void CodeEditor::FocusMainImGuiWindow() {
    if (AppWindow != nullptr) {
        HWND MainHwnd = glfwGetWin32Window(AppWindow);

        if (MainHwnd != nullptr) {
            SetFocus(MainHwnd);
        }
    }
}

// Saves current user preferences and recent workspaces to Config.json.
void CodeEditor::SaveUserConfig() {
    UserConfig.TabSize = TabSize;
    UserConfig.ZoomLevel = EditorHost.GetZoomLevel();
    UserConfig.ShowMinimap = ShowMinimap;
    UserConfig.ShowStickyScroll = ShowStickyScroll;
    UserConfig.WordWrap = EditorHost.IsWordWrapEnabled();
    UserConfig.AutoSave = AutoSaveEnabled;
    UserConfig.SidebarWidth = SidebarWidth;
    UserConfig.ConsoleHeight = ConsoleHeight;
    ConfigManager::Save(AppDataDir + "\\Config.json", UserConfig);
    ConfigManager::SaveRecentWorkspacesFile(AppDataDir + "\\RecentWorkspaces.txt", UserConfig.RecentWorkspaces);
}

// Reveals a file or folder in Windows Explorer.
void CodeEditor::RevealPathInExplorer(const std::string& TargetPath) const {
    if (TargetPath.empty()) {
        return;
    }

    std::string Params = "/select,\"" + TargetPath + "\"";
    ShellExecuteA(nullptr, "open", "explorer.exe", Params.c_str(), nullptr, SW_SHOWNORMAL);
}

// Copies a UTF-8 string to the system clipboard.
void CodeEditor::CopyTextToClipboard(const std::string& Text) const {
    if (!Text.empty()) {
        ImGui::SetClipboardText(Text.c_str());
    }
}

// Closes all open tabs except KeepIndex.
void CodeEditor::CloseOtherTabs(int KeepIndex) {
    const int ZeroInt = 0;
    const int OneOffset = 1;

    for (int Idx = static_cast<int>(OpenTabs.size()) - OneOffset; Idx >= ZeroInt; --Idx) {
        if (Idx != KeepIndex && !OpenTabs[static_cast<size_t>(Idx)].IsModified) {
            CloseTabImmediate(Idx);

            if (Idx < KeepIndex) {
                KeepIndex--;
            }
        }
    }
}

// Closes all unmodified open tabs.
void CodeEditor::CloseAllTabs() {
    const int ZeroInt = 0;
    const int OneOffset = 1;

    for (int Idx = static_cast<int>(OpenTabs.size()) - OneOffset; Idx >= ZeroInt; --Idx) {
        if (!OpenTabs[static_cast<size_t>(Idx)].IsModified) {
            CloseTabImmediate(Idx);
        }
    }
}

// Detects the LSP languageId string from a filename extension.
std::string CodeEditor::DetectLspLanguageId(const std::string& FileName) {
    size_t DotPos = FileName.find_last_of('.');

    if (DotPos == std::string::npos) {
        return "plaintext";
    }

    std::string Ext = FileName.substr(DotPos);
    std::transform(Ext.begin(), Ext.end(), Ext.begin(), [](unsigned char Ch) {
        return static_cast<char>(std::tolower(Ch));
    });

    if (Ext == ".cpp" || Ext == ".hpp" || Ext == ".cc" || Ext == ".cxx" || Ext == ".h") {
        return "cpp";
    }
    if (Ext == ".c") {
        return "c";
    }
    if (Ext == ".py") {
        return "python";
    }
    if (Ext == ".js" || Ext == ".jsx") {
        return "javascript";
    }
    if (Ext == ".ts" || Ext == ".tsx") {
        return "typescript";
    }

    return "plaintext";
}

// Builds the centered breadcrumb displayed in the custom titlebar.
std::string CodeEditor::BuildTitleBreadcrumb() const {
    const int ZeroIndex = 0;

    std::string WorkspaceLabel = "";

    if (!CurrentFolderPath.empty()) {
        path FolderPathObj(CurrentFolderPath);
        WorkspaceLabel = FolderPathObj.filename().string();

        if (WorkspaceLabel.empty()) {
            WorkspaceLabel = CurrentFolderPath;
        }
    }

    std::string FileLabel = "";

    if (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
        FileLabel = OpenTabs[ActiveTabIndex].FileName;

        if (OpenTabs[ActiveTabIndex].IsModified) {
            FileLabel += " *";
        }
    }

    if (!WorkspaceLabel.empty() && !FileLabel.empty()) {
        return WorkspaceLabel + "  -  " + FileLabel;
    }
    if (!FileLabel.empty()) {
        return FileLabel;
    }
    if (!WorkspaceLabel.empty()) {
        return WorkspaceLabel;
    }

    return "Vadex";
}

// Builds the list of executable commands for the Command Palette.
std::vector<PaletteCommand> CodeEditor::BuildPaletteCommands() {
    std::vector<PaletteCommand> Commands;

    // File commands.
    Commands.push_back({ "File: New Text File", "Ctrl+N", [this]() { CreateNewTextFile(); } });
    Commands.push_back({ "File: New Code File", "", [this]() { CreateNewTab("Untitled.cpp", "// New Code File\n"); } });
    Commands.push_back({ "File: New Window", "Ctrl+Shift+N", [this]() { LaunchNewWindow(); } });
    Commands.push_back({ "File: Open File", "Ctrl+O", [this]() { OpenFileWithDialog(); } });
    Commands.push_back({ "File: Open Folder", "Ctrl+Shift+O", [this]() { OpenFolderWithDialog(); } });
    Commands.push_back({ "File: Quick Open", "Ctrl+P", [this]() { Palette.Open(PaletteModeQuickOpen); } });
    Commands.push_back({ "File: Save", "Ctrl+S", [this]() { SaveActiveTab(); } });
    Commands.push_back({ "File: Save As", "Ctrl+Shift+S", [this]() { SaveActiveTabAs(); } });
    Commands.push_back({ "File: Toggle Auto Save", "", [this]() {
        AutoSaveEnabled = !AutoSaveEnabled;
        SaveUserConfig();
    } });
    Commands.push_back({ "File: Close Workspace", "", [this]() { CloseWorkspace(); } });
    Commands.push_back({ "File: Compare Files Side by Side", "", [this]() { OpenCompareSideBySideDialog(); } });
    Commands.push_back({ "File: Git Diff Viewer", "", [this]() { OpenGitDiffFileDialog(); } });
    Commands.push_back({ "File: Copy Active File Path", "", [this]() {
        const int ZeroIdx = 0;

        if (ActiveTabIndex >= ZeroIdx && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            CopyTextToClipboard(OpenTabs[ActiveTabIndex].FilePath);
        }
    } });
    Commands.push_back({ "File: Reveal Active File in Explorer", "", [this]() {
        const int ZeroIdx = 0;

        if (ActiveTabIndex >= ZeroIdx && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            RevealPathInExplorer(OpenTabs[ActiveTabIndex].FilePath);
        }
    } });
    Commands.push_back({ "File: Close Active Tab", "", [this]() {
        const int ZeroIdx = 0;

        if (ActiveTabIndex >= ZeroIdx && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            if (OpenTabs[ActiveTabIndex].IsModified) {
                PendingCloseTabIndex = ActiveTabIndex;
                PendingExitAfterSave = false;
                ShowUnsavedModal = true;
            } else {
                CloseTabImmediate(ActiveTabIndex);
            }
        }
    } });
    Commands.push_back({ "File: Close Other Tabs", "", [this]() { CloseOtherTabs(ActiveTabIndex); } });
    Commands.push_back({ "File: Close All Tabs", "", [this]() { CloseAllTabs(); } });
    Commands.push_back({ "File: Exit", "", [this]() { RequestExit(); } });

    // Edit commands.
    Commands.push_back({ "Edit: Undo", "Ctrl+Z", [this]() { EditorHost.Call(SCI_UNDO); } });
    Commands.push_back({ "Edit: Redo", "Ctrl+Y", [this]() { EditorHost.Call(SCI_REDO); } });
    Commands.push_back({ "Edit: Cut", "Ctrl+X", [this]() { EditorHost.Call(SCI_CUT); } });
    Commands.push_back({ "Edit: Copy", "Ctrl+C", [this]() { EditorHost.Call(SCI_COPY); } });
    Commands.push_back({ "Edit: Paste", "Ctrl+V", [this]() { EditorHost.Call(SCI_PASTE); } });
    Commands.push_back({ "Edit: Delete", "Del", [this]() { EditorHost.Call(SCI_CLEAR); } });
    Commands.push_back({ "Edit: Find & Replace", "Ctrl+F", [this]() {
        ShowSearchPanel = !ShowSearchPanel;
        FocusSearchNextFrame = ShowSearchPanel;
    } });
    Commands.push_back({ "Edit: Find / Replace in Files", "Ctrl+Shift+F", [this]() {
        const float ZeroFloat = 0;
        IsZenMode = false;
        ShowSidebar = true;
        ActiveSidebarPanel = SidebarPanelSearch;
        SidebarSlideProgress = ZeroFloat;
    } });
    Commands.push_back({ "Edit: Toggle Line Comment", "Ctrl+/", [this]() {
        const int ZeroIdx = 0;

        if (ActiveTabIndex >= ZeroIdx && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            FeatureLayer.ToggleLineComment(EditorHost, OpenTabs[ActiveTabIndex].FileName);
        }
    } });
    Commands.push_back({ "Edit: Toggle Block Comment", "Shift+Alt+A", [this]() {
        const int ZeroIdx = 0;

        if (ActiveTabIndex >= ZeroIdx && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            FeatureLayer.ToggleBlockComment(EditorHost, OpenTabs[ActiveTabIndex].FileName);
        }
    } });
    Commands.push_back({ "Edit: Delete Line", "Ctrl+Shift+K", [this]() { FeatureLayer.DeleteCurrentLines(EditorHost); } });
    Commands.push_back({ "Edit: Trigger IntelliSense", "Ctrl+Space", [this]() {
        const int ZeroIdx = 0;

        if (ActiveTabIndex >= ZeroIdx && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            IntelliSenseEngine::TriggerAutoComplete(EditorHost, OpenTabs[ActiveTabIndex].FileName, true);
        }
    } });
    Commands.push_back({ "Edit: Change Tab Size", "", [this]() { ShowTabSizeEditor = true; } });

    // Selection commands.
    Commands.push_back({ "Selection: Select All", "Ctrl+A", [this]() { EditorHost.Call(SCI_SELECTALL); } });
    Commands.push_back({ "Selection: Expand Selection", "Shift+Alt+Right", [this]() { FeatureLayer.ExpandSelection(EditorHost); } });
    Commands.push_back({ "Selection: Shrink Selection", "Shift+Alt+Left", [this]() { FeatureLayer.ShrinkSelection(EditorHost); } });
    Commands.push_back({ "Selection: Copy Line Up", "Shift+Alt+Up", [this]() { FeatureLayer.DuplicateLines(EditorHost, true); } });
    Commands.push_back({ "Selection: Copy Line Down", "Shift+Alt+Down", [this]() { FeatureLayer.DuplicateLines(EditorHost, false); } });
    Commands.push_back({ "Selection: Move Line Up", "Alt+Up", [this]() { FeatureLayer.MoveLinesUp(EditorHost); } });
    Commands.push_back({ "Selection: Move Line Down", "Alt+Down", [this]() { FeatureLayer.MoveLinesDown(EditorHost); } });
    Commands.push_back({ "Selection: Duplicate Selection", "", [this]() { FeatureLayer.DuplicateSelection(EditorHost); } });
    Commands.push_back({ "Selection: Add Cursor Above", "Ctrl+Alt+Up", [this]() { FeatureLayer.AddCursorAbove(EditorHost); } });
    Commands.push_back({ "Selection: Add Cursor Below", "Ctrl+Alt+Down", [this]() { FeatureLayer.AddCursorBelow(EditorHost); } });
    Commands.push_back({ "Selection: Add Cursors to Line Ends", "Shift+Alt+I", [this]() { FeatureLayer.AddCursorsToLineEnds(EditorHost); } });
    Commands.push_back({ "Selection: Add Next Occurrence", "Ctrl+D", [this]() { FeatureLayer.AddNextOccurrence(EditorHost); } });
    Commands.push_back({ "Selection: Add Previous Occurrence", "", [this]() { FeatureLayer.AddPreviousOccurrence(EditorHost); } });
    Commands.push_back({ "Selection: Select All Occurrences", "Ctrl+Shift+L", [this]() { FeatureLayer.SelectAllOccurrences(EditorHost); } });
    Commands.push_back({ "Selection: Toggle Column Selection Mode", "", [this]() { FeatureLayer.ToggleColumnSelectionMode(EditorHost); } });

    // View & Editor Layout commands.
    Commands.push_back({ "View: Open Explorer", "", [this]() {
        const float ZeroFloat = 0;
        IsZenMode = false;
        ShowSidebar = true;
        ActiveSidebarPanel = SidebarPanelExplorer;
        SidebarSlideProgress = ZeroFloat;
    } });
    Commands.push_back({ "View: Open Search", "Ctrl+Shift+F", [this]() {
        const float ZeroFloat = 0;
        IsZenMode = false;
        ShowSidebar = true;
        ActiveSidebarPanel = SidebarPanelSearch;
        SidebarSlideProgress = ZeroFloat;
    } });
    Commands.push_back({ "View: Open Source Control (Git)", "", [this]() {
        const float ZeroFloat = 0;
        IsZenMode = false;
        ShowSidebar = true;
        ActiveSidebarPanel = SidebarPanelGit;
        SidebarSlideProgress = ZeroFloat;
        Git.Refresh(CurrentFolderPath);
    } });
    Commands.push_back({ "View: Open Run & Debug", "", [this]() {
        const float ZeroFloat = 0;
        IsZenMode = false;
        ShowSidebar = true;
        ActiveSidebarPanel = SidebarPanelDebug;
        SidebarSlideProgress = ZeroFloat;
    } });
    Commands.push_back({ "View: Open Symbol Outline", "", [this]() {
        const float ZeroFloat = 0;
        IsZenMode = false;
        ShowSidebar = true;
        ActiveSidebarPanel = SidebarPanelOutline;
        SidebarSlideProgress = ZeroFloat;
        RefreshOutlineSymbols();
    } });
    Commands.push_back({ "View: Open Settings", "", [this]() {
        ShowSettingsWindow = true;
    } });
    Commands.push_back({ "View: Toggle Sidebar", "", [this]() { ShowSidebar = !ShowSidebar; } });
    Commands.push_back({ "View: Toggle Terminal", "Ctrl+`", [this]() {
        if (!CurrentFolderPath.empty()) {
            ShowTerminal = !ShowTerminal;
        }
    } });
    Commands.push_back({ "View: Editor Layout - Single", "", [this]() { CloseSplitEditor(); } });
    Commands.push_back({ "View: Editor Layout - Two Columns (Split Right)", "", [this]() { SplitCurrentEditor(LayoutTwoColumns); } });
    Commands.push_back({ "View: Editor Layout - Two Rows (Split Down)", "", [this]() { SplitCurrentEditor(LayoutTwoRows); } });
    Commands.push_back({ "View: Editor Layout - Flip Split Layout", "", [this]() { FlipEditorLayout(); } });
    Commands.push_back({ "View: Close Split Editor", "", [this]() { CloseSplitEditor(); } });
    Commands.push_back({ "View: Toggle Markdown Split Preview", "", [this]() { ShowMarkdownPreview = !ShowMarkdownPreview; } });
    Commands.push_back({ "View: Toggle Minimap", "", [this]() { ShowMinimap = !ShowMinimap; } });
    Commands.push_back({ "View: Toggle Sticky Scroll", "", [this]() { ShowStickyScroll = !ShowStickyScroll; } });
    Commands.push_back({ "View: Toggle Word Wrap", "Alt+Z", [this]() { EditorHost.ToggleWordWrap(); } });
    Commands.push_back({ "View: Toggle Zen Mode", "Ctrl+K", [this]() { IsZenMode = !IsZenMode; } });
    Commands.push_back({ "View: Zoom In", "Ctrl++", [this]() { EditorHost.ZoomIn(); } });
    Commands.push_back({ "View: Zoom Out", "Ctrl+-", [this]() { EditorHost.ZoomOut(); } });
    Commands.push_back({ "View: Reset Zoom", "Ctrl+0", [this]() { EditorHost.ResetZoom(); } });
    Commands.push_back({ "View: Git Diff Active File", "", [this]() {
        const int ZeroIdx = 0;

        if (ActiveTabIndex >= ZeroIdx && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            OpenGitDiffForPath(OpenTabs[ActiveTabIndex].FilePath);
        }
    } });

    // Go navigation commands.
    Commands.push_back({ "Go: Back", "Alt+Left", [this]() { NavigateBack(); } });
    Commands.push_back({ "Go: Forward", "Alt+Right", [this]() { NavigateForward(); } });
    Commands.push_back({ "Go: Last Edit Location", "", [this]() { NavigateToLastEdit(); } });
    Commands.push_back({ "Go: Go to File", "Ctrl+P", [this]() { Palette.Open(PaletteModeQuickOpen); } });
    Commands.push_back({ "Go: Go to Symbol", "", [this]() { GoToSymbolInFile(); } });
    Commands.push_back({ "Go: Go to Definition", "F12", [this]() { GoToDefinitionOrFallback(); } });
    Commands.push_back({ "Go: Go to Declaration", "", [this]() { GoToDeclarationOrFallback(); } });
    Commands.push_back({ "Go: Go to Type Definition", "", [this]() { GoToTypeDefinitionOrFallback(); } });
    Commands.push_back({ "Go: Go to Implementations", "", [this]() { GoToImplementationOrFallback(); } });
    Commands.push_back({ "Go: Go to References", "Shift+F12", [this]() { GoToReferencesOrFallback(); } });
    Commands.push_back({ "Go: Go to Line / Column", "Ctrl+G", [this]() { Palette.Open(PaletteModeGoToLine); } });
    Commands.push_back({ "Go: Go to Bracket", "Ctrl+Shift+\\", [this]() { FeatureLayer.GoToBracket(EditorHost); } });
    Commands.push_back({ "Go: Go to Next Problem", "F8", [this]() { GoToNextProblem(true); } });
    Commands.push_back({ "Go: Go to Previous Problem", "Shift+F8", [this]() { GoToNextProblem(false); } });
    Commands.push_back({ "Go: Go to Next Change", "Alt+F5", [this]() { GoToNextChange(true); } });
    Commands.push_back({ "Go: Go to Previous Change", "Shift+Alt+F5", [this]() { GoToNextChange(false); } });

    // Run & Debug commands.
    Commands.push_back({ "Run: Start / Continue Debugging", "F5", [this]() { StartDebuggingSession(); } });
    Commands.push_back({ "Run: Run Without Debugging", "Ctrl+F5", [this]() { RunWithoutDebuggingSession(); } });
    Commands.push_back({ "Run: Stop Debugging", "Shift+F5", [this]() {
        EditorHost.ClearDebugExecutionLine();
        Debugger.Stop();
    } });
    Commands.push_back({ "Run: Restart Debugging", "Ctrl+Shift+F5", [this]() { RestartDebuggingSession(); } });
    Commands.push_back({ "Run: Step Over", "F10", [this]() {
        EditorHost.ClearDebugExecutionLine();
        Debugger.StepOver();
    } });
    Commands.push_back({ "Run: Step Into", "F11", [this]() {
        EditorHost.ClearDebugExecutionLine();
        Debugger.StepInto();
    } });
    Commands.push_back({ "Run: Step Out", "Shift+F11", [this]() {
        EditorHost.ClearDebugExecutionLine();
        Debugger.StepOut();
    } });
    Commands.push_back({ "Run: Toggle Breakpoint", "F9", [this]() {
        const int OneVal = 1;
        EditorHost.ToggleBreakpoint(EditorHost.GetCurrentLine() - OneVal);
    } });
    Commands.push_back({ "Run: Clear All Breakpoints", "", [this]() { EditorHost.ClearAllBreakpoints(); } });
    Commands.push_back({ "Run: Select Executable Target", "", [this]() { BrowseDebugExecutable(); } });

    // Terminal, Workspace, Window, and Help commands.
    Commands.push_back({ "Terminal: New Terminal Instance", "", [this]() {
        if (!CurrentFolderPath.empty()) {
            ShowTerminal = true;
            CreateTerminalInstance();
        }
    } });
    Commands.push_back({ "Workspace: Refresh Explorer", "", [this]() { RefreshWorkspace(); } });
    Commands.push_back({ "Window: Maximize", "Win+Up", [this]() { glfwMaximizeWindow(AppWindow); } });
    Commands.push_back({ "Window: Restore", "Win+Down", [this]() { glfwRestoreWindow(AppWindow); } });
    Commands.push_back({ "Window: Tile Left Half", "Win+Left", [this]() { TileWindow(TileLeftHalf); } });
    Commands.push_back({ "Window: Tile Right Half", "Win+Right", [this]() { TileWindow(TileRightHalf); } });
    Commands.push_back({ "Window: Tile Top-Left Quarter", "", [this]() { TileWindow(TileTopLeftQuarter); } });
    Commands.push_back({ "Window: Tile Top-Right Quarter", "", [this]() { TileWindow(TileTopRightQuarter); } });
    Commands.push_back({ "Window: Tile Bottom-Left Quarter", "", [this]() { TileWindow(TileBottomLeftQuarter); } });
    Commands.push_back({ "Window: Tile Bottom-Right Quarter", "", [this]() { TileWindow(TileBottomRightQuarter); } });
    Commands.push_back({ "Help: Welcome (Welcome.md)", "", [this]() { OpenWelcomeTab(); } });
    Commands.push_back({ "Help: About Vadex", "", [this]() { ShowAboutModal = true; } });

    return Commands;
}

// Checks whether any open tab has unsaved modifications.
bool CodeEditor::HasUnsavedChanges() const {
    for (const auto& Tab : OpenTabs) {
        if (Tab.IsModified) {
            return true;
        }
    }

    return false;
}

// Requests application exit, prompting if unsaved changes exist.
void CodeEditor::RequestExit() {
    const int NegativeOneInt = -1;

    if (HasUnsavedChanges()) {
        PendingExitAfterSave = true;
        PendingCloseTabIndex = NegativeOneInt;
        ShowUnsavedModal = true;
    } else {
        ShouldExit = true;
    }
}

// Immediately removes a tab, releases its Scintilla document, and adjusts ActiveTabIndex.
void CodeEditor::CloseTabImmediate(int TabIndex) {
    const int ZeroIndex = 0;
    const int OneOffset = 1;
    const int NegativeOneInt = -1;

    if (TabIndex < ZeroIndex || TabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    EditorHost.ReleaseDocument(OpenTabs[TabIndex].ScintillaDoc);
    OpenTabs.erase(OpenTabs.begin() + TabIndex);

    if (ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        ActiveTabIndex = static_cast<int>(OpenTabs.size()) - OneOffset;
    }

    LastSyncedTabIndex = NegativeOneInt;
    SyncActiveTabDocument();
}

// Switches ScintillaHost to display the active tab's native document pointer.
void CodeEditor::SyncActiveTabDocument() {
    const int ZeroIndex = 0;
    const int NegativeOneInt = -1;

    if (!EditorHost.IsReady()) {
        return;
    }

    if (ActiveTabIndex < ZeroIndex || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        LastSyncedTabIndex = NegativeOneInt;

        return;
    }

    if (ActiveTabIndex == LastSyncedTabIndex) {
        return;
    }

    FileTab& ActiveTab = OpenTabs[ActiveTabIndex];

    if (ActiveTab.ScintillaDoc == ZeroSptr) {
        ActiveTab.ScintillaDoc = EditorHost.CreateDocument(ActiveTab.Content);
    }

    EditorHost.SetActiveDocument(ActiveTab.ScintillaDoc, ActiveTab.FileName, TabSize);
    EditorHost.SetReadOnly(ActiveTab.IsReadOnly);
    LastSyncedTabIndex = ActiveTabIndex;
    ActiveTab.CursorLine = EditorHost.GetCurrentLine();
    ActiveTab.CursorColumn = EditorHost.GetCurrentColumn();
    FeatureLayer.OnUpdateUi(EditorHost);
    ApplyDiagnosticsToActiveFile();
    UpdateSearchHighlights();
}

// Intercepts Monaco shortcuts while Scintilla has Win32 keyboard focus.
bool CodeEditor::HandleEditorKeyDown(UINT Msg, WPARAM WParam, LPARAM LParam) {
    const int ZeroIndex = 0;
    const int OneOffset = 1;
    const WPARAM MaxControlChar = 31;
    const SHORT HighBitMask = static_cast<SHORT>(0x8000);

    (void)LParam;

    bool IsCtrl = ((GetKeyState(VK_CONTROL) & HighBitMask) != ZeroIndex);
    bool IsShift = ((GetKeyState(VK_SHIFT) & HighBitMask) != ZeroIndex);
    bool IsAlt = ((GetKeyState(VK_MENU) & HighBitMask) != ZeroIndex);

    // Suppresses stray control-character WM_CHAR messages generated by Ctrl shortcuts.
    if (Msg == WM_CHAR) {
        if (IsCtrl && !IsAlt && WParam <= MaxControlChar && WParam != VK_RETURN && WParam != VK_BACK && WParam != VK_TAB) {
            return true;
        }

        return false;
    }

    if (Msg != WM_KEYDOWN && Msg != WM_SYSKEYDOWN) {
        return false;
    }

    if (IsCtrl && IsShift && WParam == 'P') {
        Palette.Open(PaletteModeCommands);
        FocusMainImGuiWindow();

        return true;
    }
    if (IsCtrl && !IsShift && WParam == 'P') {
        if (!CurrentFolderPath.empty()) {
            Palette.Open(PaletteModeQuickOpen);
            FocusMainImGuiWindow();
        }

        return true;
    }
    if (IsCtrl && !IsShift && WParam == 'G') {
        Palette.Open(PaletteModeGoToLine);
        FocusMainImGuiWindow();

        return true;
    }
    if (IsCtrl && IsShift && WParam == 'S') {
        SaveActiveTabAs();

        return true;
    }
    if (IsCtrl && !IsShift && WParam == 'S') {
        SaveActiveTab();

        return true;
    }
    if (IsCtrl && IsShift && WParam == 'O') {
        OpenFolderWithDialog();

        return true;
    }
    if (IsCtrl && !IsShift && WParam == 'O') {
        OpenFileWithDialog();

        return true;
    }
    if (IsCtrl && IsShift && WParam == 'N') {
        LaunchNewWindow();

        return true;
    }
    if (IsCtrl && !IsShift && WParam == 'N') {
        CreateNewTextFile();

        return true;
    }
    if (IsCtrl && IsShift && WParam == 'F') {
        IsZenMode = false;
        ShowSidebar = true;
        ActiveSidebarPanel = SidebarPanelSearch;
        FocusMainImGuiWindow();

        return true;
    }
    if (IsCtrl && !IsShift && WParam == 'F') {
        ShowSearchPanel = !ShowSearchPanel;
        FocusSearchNextFrame = ShowSearchPanel;

        if (ShowSearchPanel) {
            FocusMainImGuiWindow();
        }

        return true;
    }
    if (IsCtrl && !IsShift && WParam == 'K') {
        IsZenMode = !IsZenMode;

        return true;
    }
    if (IsCtrl && (WParam == VK_OEM_PLUS || WParam == VK_ADD)) {
        EditorHost.ZoomIn();

        return true;
    }
    if (IsCtrl && (WParam == VK_OEM_MINUS || WParam == VK_SUBTRACT)) {
        EditorHost.ZoomOut();

        return true;
    }
    if (IsCtrl && WParam == '0') {
        EditorHost.ResetZoom();

        return true;
    }
    if (WParam == VK_F5 && IsCtrl && IsShift) {
        RestartDebuggingSession();

        return true;
    }
    if (WParam == VK_F5 && IsCtrl && !IsShift) {
        RunWithoutDebuggingSession();

        return true;
    }
    if (WParam == VK_F5 && IsAlt) {
        GoToNextChange(!IsShift);

        return true;
    }
    if (WParam == VK_F5 && IsShift) {
        EditorHost.ClearDebugExecutionLine();
        Debugger.Stop();

        return true;
    }
    if (WParam == VK_F5) {
        StartDebuggingSession();

        return true;
    }
    if (WParam == VK_F8) {
        GoToNextProblem(!IsShift);

        return true;
    }
    if (WParam == VK_F10 && Debugger.GetIsRunning()) {
        EditorHost.ClearDebugExecutionLine();
        Debugger.StepOver();

        return true;
    }
    if (WParam == VK_F11 && Debugger.GetIsRunning()) {
        EditorHost.ClearDebugExecutionLine();

        if (IsShift) {
            Debugger.StepOut();
        } else {
            Debugger.StepInto();
        }

        return true;
    }
    if (IsCtrl && !IsShift && WParam == 'D') {
        FeatureLayer.AddNextOccurrence(EditorHost);

        return true;
    }
    if (IsCtrl && !IsShift && WParam == VK_OEM_2) {
        if (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            FeatureLayer.ToggleLineComment(EditorHost, OpenTabs[ActiveTabIndex].FileName);
        }

        return true;
    }
    if (IsCtrl && IsShift && WParam == 'K') {
        FeatureLayer.DeleteCurrentLines(EditorHost);

        return true;
    }
    if (IsAlt && IsShift && WParam == VK_UP) {
        FeatureLayer.DuplicateLines(EditorHost, true);

        return true;
    }
    if (IsAlt && IsShift && WParam == VK_DOWN) {
        FeatureLayer.DuplicateLines(EditorHost, false);

        return true;
    }
    if (IsAlt && !IsShift && WParam == VK_UP) {
        FeatureLayer.MoveLinesUp(EditorHost);

        return true;
    }
    if (IsAlt && !IsShift && WParam == VK_DOWN) {
        FeatureLayer.MoveLinesDown(EditorHost);

        return true;
    }
    if (IsAlt && !IsCtrl && WParam == 'Z') {
        EditorHost.ToggleWordWrap();

        return true;
    }
    if (IsCtrl && WParam == VK_OEM_3) {
        if (!CurrentFolderPath.empty()) {
            ShowTerminal = !ShowTerminal;
            FocusMainImGuiWindow();
        }

        return true;
    }
    if (WParam == VK_F9) {
        EditorHost.ToggleBreakpoint(EditorHost.GetCurrentLine() - OneOffset);

        return true;
    }
    if (IsCtrl && WParam == VK_SPACE) {
        if (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            IntelliSenseEngine::TriggerAutoComplete(EditorHost, OpenTabs[ActiveTabIndex].FileName, true);
            Lsp.RequestCompletion(OpenTabs[ActiveTabIndex].FilePath, EditorHost.GetCurrentLine() - OneOffset, EditorHost.GetCurrentColumn() - OneOffset);
        }

        return true;
    }
    if (IsCtrl && IsShift && WParam == 'L') {
        FeatureLayer.SelectAllOccurrences(EditorHost);

        return true;
    }
    if (IsCtrl && IsShift && WParam == VK_OEM_5) {
        FeatureLayer.GoToBracket(EditorHost);

        return true;
    }
    if (IsAlt && IsShift && WParam == 'A') {
        if (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            FeatureLayer.ToggleBlockComment(EditorHost, OpenTabs[ActiveTabIndex].FileName);
        }

        return true;
    }
    if (IsAlt && IsShift && WParam == 'I') {
        FeatureLayer.AddCursorsToLineEnds(EditorHost);

        return true;
    }
    if (IsAlt && IsShift && WParam == VK_RIGHT) {
        FeatureLayer.ExpandSelection(EditorHost);

        return true;
    }
    if (IsAlt && IsShift && WParam == VK_LEFT) {
        FeatureLayer.ShrinkSelection(EditorHost);

        return true;
    }
    if (IsCtrl && IsAlt && WParam == VK_UP) {
        FeatureLayer.AddCursorAbove(EditorHost);

        return true;
    }
    if (IsCtrl && IsAlt && WParam == VK_DOWN) {
        FeatureLayer.AddCursorBelow(EditorHost);

        return true;
    }
    if (IsAlt && !IsCtrl && !IsShift && WParam == VK_LEFT) {
        NavigateBack();

        return true;
    }
    if (IsAlt && !IsCtrl && !IsShift && WParam == VK_RIGHT) {
        NavigateForward();

        return true;
    }
    if (WParam == VK_F12 && IsShift) {
        GoToReferencesOrFallback();

        return true;
    }
    if (WParam == VK_F12) {
        GoToDefinitionOrFallback();

        return true;
    }
    if (!IsCtrl && !IsAlt && WParam == VK_BACK) {
        return FeatureLayer.HandleBackspacePair(EditorHost);
    }
    if (!IsAlt && !IsShift && WParam == VK_DELETE) {
        if (IsCtrl) {
            EditorHost.Call(SCI_DELWORDRIGHT);
        } else {
            EditorHost.Call(SCI_CLEAR);
        }

        return true;
    }
    if (!IsCtrl && !IsAlt && WParam == VK_TAB) {
        if (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            return FeatureLayer.HandleTabSnippet(EditorHost, OpenTabs[ActiveTabIndex].FileName);
        }
    }
    if (WParam == VK_ESCAPE) {
        if (FeatureLayer.HandleEscapeSnippet(EditorHost)) {
            return true;
        }
        if (ShowSearchPanel) {
            ShowSearchPanel = false;

            return true;
        }
    }

    return false;
}

// Processes WM_NOTIFY messages emitted by the primary and secondary Scintilla child windows.
void CodeEditor::HandleNativeNotify(WPARAM WParam, LPARAM LParam) {
    const int ZeroIndex = 0;

    (void)WParam;

    if (LParam == ZeroSptr) {
        return;
    }

    SCNotification* Notify = reinterpret_cast<SCNotification*>(LParam);

    // Handles breakpoint margin clicks on the secondary split editor.
    if (SecondaryEditorHost.IsReady() && Notify->nmhdr.hwndFrom == SecondaryEditorHost.GetHwnd()) {
        if (Notify->nmhdr.code == SCN_MARGINCLICK && Notify->margin == MarginBreakpoints) {
            int ClickedLine = static_cast<int>(SecondaryEditorHost.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(Notify->position)));
            SecondaryEditorHost.ToggleBreakpoint(ClickedLine);
        }

        return;
    }

    if (!EditorHost.IsReady() || Notify->nmhdr.hwndFrom != EditorHost.GetHwnd()) {
        return;
    }

    if (Notify->nmhdr.code == SCN_MARGINCLICK && Notify->margin == MarginBreakpoints) {
        int ClickedLine = static_cast<int>(EditorHost.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(Notify->position)));
        EditorHost.ToggleBreakpoint(ClickedLine);
    } else if (Notify->nmhdr.code == SCN_CHARADDED) {
        FeatureLayer.OnCharAdded(EditorHost, Notify->ch, TabSize);

        if (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            char Typed = static_cast<char>(Notify->ch);

            if (std::isalnum(static_cast<unsigned char>(Typed)) != ZeroIndex || Typed == '_') {
                IntelliSenseEngine::TriggerAutoComplete(EditorHost, OpenTabs[ActiveTabIndex].FileName, false);
            } else if (Typed == '(') {
                IntelliSenseEngine::TriggerSignatureCallTip(EditorHost);
            } else if (Typed == '.' || Typed == ':') {
                const int OneOffset = 1;
                Lsp.RequestCompletion(OpenTabs[ActiveTabIndex].FilePath, EditorHost.GetCurrentLine() - OneOffset, EditorHost.GetCurrentColumn() - OneOffset);
            }
        }
    } else if (Notify->nmhdr.code == SCN_UPDATEUI) {
        if (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            OpenTabs[ActiveTabIndex].CursorLine = EditorHost.GetCurrentLine();
            OpenTabs[ActiveTabIndex].CursorColumn = EditorHost.GetCurrentColumn();
            RecordNavigationPoint();
        }

        FeatureLayer.OnUpdateUi(EditorHost);
    } else if (Notify->nmhdr.code == SCN_SAVEPOINTLEFT) {
        if (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            OpenTabs[ActiveTabIndex].IsModified = true;
            Lsp.NotifyDidChange(OpenTabs[ActiveTabIndex].FilePath, EditorHost.GetText());
        }
    } else if (Notify->nmhdr.code == SCN_SAVEPOINTREACHED) {
        if (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
            OpenTabs[ActiveTabIndex].IsModified = false;
        }
    } else if (Notify->nmhdr.code == SCN_DWELLSTART) {
        if (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size()) && Notify->position >= ZeroIndex) {
            int HoverLine = static_cast<int>(EditorHost.Call(SCI_LINEFROMPOSITION, static_cast<uptr_t>(Notify->position)));
            int HoverCol = static_cast<int>(EditorHost.Call(SCI_GETCOLUMN, static_cast<uptr_t>(Notify->position)));
            std::vector<LspDiagnostic> Diags = Lsp.GetDiagnosticsForFile(OpenTabs[ActiveTabIndex].FilePath);

            for (const auto& Diag : Diags) {
                if (Diag.Line == HoverLine && !Diag.Message.empty()) {
                    EditorHost.ShowCallTip(static_cast<int>(Notify->position), Diag.Message);

                    return;
                }
            }

            Lsp.RequestHover(OpenTabs[ActiveTabIndex].FilePath, HoverLine, HoverCol, static_cast<int>(Notify->position));
        }
    } else if (Notify->nmhdr.code == SCN_DWELLEND) {
        EditorHost.CancelCallTip();
    } else if (Notify->nmhdr.code == SCN_MODIFIED) {
        if ((Notify->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) != ZeroIndex) {
            MarkdownDirty = true;

            if (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
                Lsp.NotifyDidChange(OpenTabs[ActiveTabIndex].FilePath, EditorHost.GetText());
                LastEditLocation.FilePath = OpenTabs[ActiveTabIndex].FilePath;
                LastEditLocation.Line = EditorHost.GetCurrentLine();
                LastEditLocation.Column = EditorHost.GetCurrentColumn();

                if (AutoSaveEnabled) {
                    PendingAutoSave = true;
                    LastEditTimestamp = glfwGetTime();
                }
            }

            if (ShowSearchPanel && SearchText[ZeroIndex] != '\0') {
                UpdateSearchHighlights();
            }
        }
    }
}

// Initializes fonts, icons, primary Scintilla host, welcome tab, and initial logs from the Build directory.
void CodeEditor::Initialize(GLFWwindow* WindowPointer) {
    AppWindow = WindowPointer;

    // Resolves the executable directory (Build/) so Vadex relies only on its own Assets folder.
    const DWORD ZeroDword = 0;
    char ExePathBuf[MaxFileSize] = {};

    if (GetModuleFileNameA(nullptr, ExePathBuf, MaxFileSize) > ZeroDword) {
        AppDataDir = path(ExePathBuf).parent_path().string();
    } else {
        AppDataDir = ".";
    }

    std::string AssetsDir = AppDataDir + "\\Assets";
    const int SingleOversample = 1;
    static const ImWchar AsciiRanges[] = { 0x0020, 0x007E, 0 };
    ImFontConfig CompactFontCfg;
    CompactFontCfg.OversampleH = SingleOversample;
    CompactFontCfg.OversampleV = SingleOversample;

    ImGuiIO& Io = ImGui::GetIO();
    Io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines;
    MenuBarFont = Io.Fonts->AddFontFromFileTTF((AssetsDir + "\\Fonts\\Poppins-Bold.ttf").c_str(), static_cast<float>(FontSize), &CompactFontCfg, AsciiRanges);
    RegularFont = Io.Fonts->AddFontFromFileTTF((AssetsDir + "\\Fonts\\Poppins-Regular.ttf").c_str(), static_cast<float>(FontSize), &CompactFontCfg, AsciiRanges);

    // Loads the monospace code font from Build/Assets with system fallback.
    std::string LocalMonoPath = AssetsDir + "\\Fonts\\CascadiaMono.ttf";

    if (exists(LocalMonoPath)) {
        CodeFont = Io.Fonts->AddFontFromFileTTF(LocalMonoPath.c_str(), static_cast<float>(CodeFontSize), &CompactFontCfg, AsciiRanges);
    } else if (exists("C:/Windows/Fonts/CascadiaMono.ttf")) {
        CodeFont = Io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/CascadiaMono.ttf", static_cast<float>(CodeFontSize), &CompactFontCfg, AsciiRanges);
    } else if (exists("C:/Windows/Fonts/consola.ttf")) {
        CodeFont = Io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf", static_cast<float>(CodeFontSize), &CompactFontCfg, AsciiRanges);
    } else {
        CodeFont = RegularFont;
    }

    // Loads Segoe Fluent Icons / Segoe MDL2 Assets for native Windows caption icons.
    static const ImWchar CaptionRanges[] = { 0xE8BB, 0xE8BB, 0xE921, 0xE923, 0 };

    if (exists("C:/Windows/Fonts/SegoeIcons.ttf")) {
        CaptionIconFont = Io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/SegoeIcons.ttf", static_cast<float>(CaptionIconFontSize), &CompactFontCfg, CaptionRanges);
    } else if (exists("C:/Windows/Fonts/segmdl2.ttf")) {
        CaptionIconFont = Io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segmdl2.ttf", static_cast<float>(CaptionIconFontSize), &CompactFontCfg, CaptionRanges);
    }

    // Loads persisted user settings from the app directory.
    std::string ConfigPath = AppDataDir + "\\Config.json";
    std::string WorkspacesPath = AppDataDir + "\\RecentWorkspaces.txt";
    UserConfig = ConfigManager::Load(ConfigPath);
    std::vector<std::string> FileWorkspaces = ConfigManager::LoadRecentWorkspacesFile(WorkspacesPath);

    if (!FileWorkspaces.empty()) {
        UserConfig.RecentWorkspaces = FileWorkspaces;
    } else if (!UserConfig.RecentWorkspaces.empty()) {
        ConfigManager::SaveRecentWorkspacesFile(WorkspacesPath, UserConfig.RecentWorkspaces);
    }

    TabSize = UserConfig.TabSize;
    ShowMinimap = UserConfig.ShowMinimap;
    ShowStickyScroll = UserConfig.ShowStickyScroll;
    AutoSaveEnabled = UserConfig.AutoSave;
    SidebarWidth = UserConfig.SidebarWidth;
    ConsoleHeight = UserConfig.ConsoleHeight;

    // Loads fallback caption icons and Activity Bar icons from Build/Assets/Icons.
    std::string IconsDir = AssetsDir + "\\Icons";
    MinimizeIcon.LoadFromFile(IconsDir + "\\Minimize.svg");
    MaximizeIcon.LoadFromFile(IconsDir + "\\Maximize.svg");
    RestoreIcon.LoadFromFile(IconsDir + "\\Restore.svg");
    CloseIcon.LoadFromFile(IconsDir + "\\Close.svg");
    ExplorerIcon.LoadFromLucideApi("files", IconsDir + "\\Explorer.svg");
    SearchIcon.LoadFromLucideApi("search", IconsDir + "\\Search.svg");
    GitIcon.LoadFromLucideApi("git-branch", IconsDir + "\\Git.svg");
    DebugIcon.LoadFromLucideApi("bug", IconsDir + "\\Debug.svg");
    OutlineIcon.LoadFromLucideApi("list-tree", IconsDir + "\\Outline.svg");
    SettingsIcon.LoadFromLucideApi("settings", IconsDir + "\\Settings.svg");

    // Initializes the primary and secondary native Scintilla 5 + Lexilla 5 hosts.
    HWND OwnerHwnd = (AppWindow != nullptr) ? glfwGetWin32Window(AppWindow) : nullptr;

    if (EditorHost.Initialize(OwnerHwnd)) {
        EditorHost.SetKeyDownHandler([this](UINT Msg, WPARAM WParam, LPARAM LParam) {
            return HandleEditorKeyDown(Msg, WParam, LParam);
        });
        EditorHost.SetZoomLevel(UserConfig.ZoomLevel);

        if (UserConfig.WordWrap) {
            EditorHost.ToggleWordWrap();
        }

        AddLog("Scintilla 5 + Lexilla 5 initialized successfully.");
    } else {
        AddLog("[Error] Failed to initialize Scintilla.dll.");
    }

    // SecondaryEditorHost is initialized lazily only when a split editor is opened to save RAM and VRAM.

    // Reads the entire welcome markdown file from the Build directory into a string buffer.
    std::string WelcomeContent;
    std::ifstream WelcomeFile(AppDataDir + "\\Welcome.md");

    if (WelcomeFile.is_open()) {
        std::stringstream WelcomeStream;
        WelcomeStream << WelcomeFile.rdbuf();
        WelcomeContent = WelcomeStream.str();
        WelcomeFile.close();
    }

    CreateNewTab("Welcome.md", WelcomeContent, true);
    AddLog("Editor Initialized Successfully.");

    // Trims temporary startup working set pages to keep memory usage minimal.
    const SIZE_T TrimWorkingSetFlag = static_cast<SIZE_T>(-1);
    SetProcessWorkingSetSize(GetCurrentProcess(), TrimWorkingSetFlag, TrimWorkingSetFlag);
}

// Creates a new editor tab backed by its own Scintilla document pointer and stable TabId.
void CodeEditor::CreateNewTab(const std::string& Name, const std::string& Content, bool ReadOnly) {
    const int OneOffset = 1;
    const int NegativeOneInt = -1;

    FileTab Tab;
    Tab.TabId = NextTabId++;
    Tab.FileName = Name;
    Tab.FilePath = Name;
    Tab.ScintillaDoc = EditorHost.CreateDocument(Content);
    Tab.Content = (Tab.ScintillaDoc == ZeroSptr) ? Content : "";
    Tab.IsModified = false;
    Tab.IsReadOnly = ReadOnly;
    Tab.CursorLine = OneOffset;
    Tab.CursorColumn = OneOffset;

    OpenTabs.push_back(Tab);
    ActiveTabIndex = static_cast<int>(OpenTabs.size()) - OneOffset;
    LastSyncedTabIndex = NegativeOneInt;
    SyncActiveTabDocument();
}

// Attempts to start a matching language server when opening a source file in a workspace.
void CodeEditor::TryStartLspForFile(const std::string& FilePath) {
    if (Lsp.GetIsRunning() || CurrentFolderPath.empty()) {
        return;
    }

    std::string LangId = DetectLspLanguageId(FilePath);
    std::string ServerCmd = "";

    if (LangId == "cpp" || LangId == "c") {
        ServerCmd = "clangd";
    } else if (LangId == "python") {
        ServerCmd = "pyright-langserver --stdio";
    } else if (LangId == "javascript" || LangId == "typescript") {
        ServerCmd = "typescript-language-server --stdio";
    }

    if (!ServerCmd.empty() && Lsp.StartForWorkspace(ServerCmd, CurrentFolderPath)) {
        AddLog("LSP connected: " + ServerCmd);
    }
}

// Opens a file from disk into a tab or focuses it if already open.
void CodeEditor::OpenFileFromDisk(const std::string& Path) {
    const size_t StartIndex = 0;
    const int OneOffset = 1;
    const int NegativeOneInt = -1;

    std::ifstream FileStream(Path);

    if (FileStream.is_open()) {
        std::stringstream StringStream;
        StringStream << FileStream.rdbuf();
        std::string FileContents = StringStream.str();
        FileStream.close();

        path NormalizedTarget = path(Path).lexically_normal();

        for (size_t Index = StartIndex; Index < OpenTabs.size(); ++Index) {
            if (path(OpenTabs[Index].FilePath).lexically_normal() == NormalizedTarget) {
                ActiveTabIndex = static_cast<int>(Index);
                PendingSelectTabId = OpenTabs[Index].TabId;
                SyncActiveTabDocument();
                AddLog("Switched to existing tab: " + Path);

                return;
            }
        }

        FileTab Tab;
        Tab.TabId = NextTabId++;
        Tab.FilePath = Path;
        path PathObj(Path);
        Tab.FileName = PathObj.filename().string();
        Tab.ScintillaDoc = EditorHost.CreateDocument(FileContents);
        Tab.Content = (Tab.ScintillaDoc == ZeroSptr) ? FileContents : "";
        Tab.IsModified = false;
        Tab.IsReadOnly = false;
        Tab.CursorLine = OneOffset;
        Tab.CursorColumn = OneOffset;

        OpenTabs.push_back(Tab);
        ActiveTabIndex = static_cast<int>(OpenTabs.size()) - OneOffset;
        LastSyncedTabIndex = NegativeOneInt;
        SyncActiveTabDocument();

        TryStartLspForFile(Path);
        Lsp.NotifyDidOpen(Path, DetectLspLanguageId(Tab.FileName), FileContents);
        AddLog("Opened file: " + Path);
    } else {
        AddLog("Failed to open file: " + Path);
    }
}

// Populates the immediate sorted children of a cached workspace directory node.
void CodeEditor::LoadWorkspaceNodeChildren(WorkspaceNode& Node) {
    Node.Children.clear();
    Node.IsLoaded = true;

    std::error_code ErrorCode;
    path DirPath(Node.FullPath);
    std::vector<WorkspaceNode> SubDirs;
    std::vector<WorkspaceNode> Files;

    for (const auto& Entry : directory_iterator(DirPath, directory_options::skip_permission_denied, ErrorCode)) {
        if (ErrorCode) {
            break;
        }

        WorkspaceNode Child;
        Child.Name = Entry.path().filename().string();
        Child.FullPath = Entry.path().string();
        Child.IsDirectory = Entry.is_directory();
        Child.IsLoaded = false;

        if (Child.IsDirectory) {
            SubDirs.push_back(Child);
        } else if (Entry.is_regular_file()) {
            Files.push_back(Child);
        }
    }

    auto NameSort = [](const WorkspaceNode& Left, const WorkspaceNode& Right) {
        return Left.Name < Right.Name;
    };

    std::sort(SubDirs.begin(), SubDirs.end(), NameSort);
    std::sort(Files.begin(), Files.end(), NameSort);

    Node.Children.reserve(SubDirs.size() + Files.size());

    for (auto& DirItem : SubDirs) {
        Node.Children.push_back(std::move(DirItem));
    }

    for (auto& FileItem : Files) {
        Node.Children.push_back(std::move(FileItem));
    }
}

// Collects cached workspace files recursively for the Quick Open fuzzy finder.
void CodeEditor::CollectQuickOpenFiles(const WorkspaceNode& Node, const std::string& Prefix, std::vector<QuickOpenEntry>& OutFiles) const {
    const size_t MaxQuickOpenEntries = 500;

    for (const auto& Child : Node.Children) {
        if (OutFiles.size() >= MaxQuickOpenEntries) {
            return;
        }

        std::string RelPath = Prefix.empty() ? Child.Name : (Prefix + "/" + Child.Name);

        if (Child.IsDirectory) {
            if (Child.Name == ".git" || Child.Name == "node_modules" || Child.Name == "ObjFiles") {
                continue;
            }
            if (Child.IsLoaded) {
                CollectQuickOpenFiles(Child, RelPath, OutFiles);
            }
        } else {
            OutFiles.push_back({ RelPath, Child.FullPath });
        }
    }
}

// Reloads the cached workspace tree from disk.
void CodeEditor::RefreshWorkspace() {
    if (CurrentFolderPath.empty()) {
        return;
    }

    path RootPath(CurrentFolderPath);
    WorkspaceRoot.Name = RootPath.filename().string();

    if (WorkspaceRoot.Name.empty()) {
        WorkspaceRoot.Name = CurrentFolderPath;
    }

    WorkspaceRoot.FullPath = CurrentFolderPath;
    WorkspaceRoot.IsDirectory = true;
    LoadWorkspaceNodeChildren(WorkspaceRoot);
}

// Opens a workspace folder, caches its root tree, and creates the initial terminal instance.
void CodeEditor::OpenFolderFromDisk(const std::string& Path) {
    try {
        path FolderPathObj(Path);

        if (exists(FolderPathObj) && is_directory(FolderPathObj)) {
            CurrentFolderPath = Path;
            RefreshWorkspace();
            Git.Refresh(Path);
            ConfigManager::RecordRecentWorkspace(UserConfig, Path);
            SaveUserConfig();
            AddLog("Opened folder: " + Path);

            StopAllTerminals();
            ShowTerminal = true;
            CreateTerminalInstance();
        } else {
            AddLog("Failed to open folder: " + Path);
        }
    } catch (const std::exception& ExceptionObj) {
        (void)ExceptionObj;
        AddLog("Exception opening folder: " + Path);
    }
}

// Closes the currently open workspace and stops all terminal and LSP sessions.
void CodeEditor::CloseWorkspace() {
    if (CurrentFolderPath.empty()) {
        return;
    }

    StopAllTerminals();
    Lsp.Stop();

    CurrentFolderPath.clear();
    WorkspaceRoot = {};
    ShowTerminal = false;

    AddLog("Closed workspace.");
}

// Saves a specific tab by index to disk using its Scintilla document text.
void CodeEditor::SaveTabByIndex(int TabIndex) {
    const int ZeroIndex = 0;

    if (TabIndex < ZeroIndex || TabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    FileTab& CurrentTab = OpenTabs[TabIndex];
    std::string SavePayload = CurrentTab.Content;

    if (EditorHost.IsReady() && CurrentTab.ScintillaDoc != ZeroSptr) {
        SavePayload = (TabIndex == ActiveTabIndex) ? EditorHost.GetText() : EditorHost.GetDocumentText(CurrentTab.ScintillaDoc);
    }

    std::ofstream OutputFile(CurrentTab.FilePath);

    if (OutputFile.is_open()) {
        OutputFile << SavePayload;
        OutputFile.close();
        CurrentTab.IsModified = false;

        if (TabIndex == ActiveTabIndex) {
            EditorHost.SetSavePoint();
        }

        Lsp.NotifyDidSave(CurrentTab.FilePath);
        Git.Refresh(CurrentFolderPath);
        AddLog("Saved file: " + CurrentTab.FilePath);
    } else {
        AddLog("Error saving file: " + CurrentTab.FilePath);
    }
}

// Saves the currently active tab to disk.
void CodeEditor::SaveActiveTab() {
    SaveTabByIndex(ActiveTabIndex);
}

// Appends a formatted log entry to the bounded console log buffer.
void CodeEditor::AddLog(const std::string& LogText) {
    const size_t MaxLogEntries = 250;

    LogMessages.push_back("[LOG] " + LogText);

    if (LogMessages.size() > MaxLogEntries) {
        LogMessages.erase(LogMessages.begin());
    }
}

// Shows the native Windows file picker dialog and opens the selected file.
void CodeEditor::OpenFileWithDialog() {
    const DWORD DefaultFilterIndex = 1;

    HWND OwnerHwnd = (AppWindow != nullptr) ? glfwGetWin32Window(AppWindow) : nullptr;
    char FilenameBuf[MaxFileSize] = {};
    OPENFILENAMEA OpenFileNameStructure = {};
    OpenFileNameStructure.lStructSize = sizeof(OpenFileNameStructure);
    OpenFileNameStructure.hwndOwner = OwnerHwnd;
    OpenFileNameStructure.lpstrFile = FilenameBuf;
    OpenFileNameStructure.nMaxFile = MaxFileSize;
    OpenFileNameStructure.lpstrFilter = "All Files\0*.*\0Text Files\0*.txt\0Code Files\0*.cpp;*.hpp;*.h;*.c\0Markdown Files\0*.md\0\0";
    OpenFileNameStructure.nFilterIndex = DefaultFilterIndex;
    OpenFileNameStructure.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&OpenFileNameStructure)) {
        OpenFileFromDisk(FilenameBuf);
    }
}

// Shows the modern Explorer-style Windows folder picker dialog and opens the selected folder.
void CodeEditor::OpenFolderWithDialog() {
    const DWORD ZeroOptions = 0;
    const int ZeroVal = 0;
    const int NullTerminatorSize = 1;
    const int UnknownLength = -1;

    HRESULT ComResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool ShouldUninitCom = SUCCEEDED(ComResult);

    IFileOpenDialog* FolderDialog = nullptr;
    HRESULT CreateResult = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&FolderDialog));

    if (SUCCEEDED(CreateResult) && FolderDialog != nullptr) {
        DWORD DialogOptions = ZeroOptions;

        if (SUCCEEDED(FolderDialog->GetOptions(&DialogOptions))) {
            FolderDialog->SetOptions(DialogOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        }

        FolderDialog->SetTitle(L"Select Folder");

        HWND OwnerHwnd = (AppWindow != nullptr) ? glfwGetWin32Window(AppWindow) : nullptr;

        if (SUCCEEDED(FolderDialog->Show(OwnerHwnd))) {
            IShellItem* SelectedItem = nullptr;

            if (SUCCEEDED(FolderDialog->GetResult(&SelectedItem)) && SelectedItem != nullptr) {
                PWSTR WideFolderPath = nullptr;

                if (SUCCEEDED(SelectedItem->GetDisplayName(SIGDN_FILESYSPATH, &WideFolderPath)) && WideFolderPath != nullptr) {
                    int RequiredBytes = WideCharToMultiByte(CP_UTF8, ZeroVal, WideFolderPath, UnknownLength, nullptr, ZeroVal, nullptr, nullptr);

                    if (RequiredBytes > NullTerminatorSize) {
                        std::string FolderPath(static_cast<size_t>(RequiredBytes - NullTerminatorSize), '\0');
                        WideCharToMultiByte(CP_UTF8, ZeroVal, WideFolderPath, UnknownLength, &FolderPath[ZeroVal], RequiredBytes, nullptr, nullptr);
                        OpenFolderFromDisk(FolderPath);
                    }

                    CoTaskMemFree(WideFolderPath);
                }

                SelectedItem->Release();
            }
        }

        FolderDialog->Release();
    }

    if (ShouldUninitCom) {
        CoUninitialize();
    }
}

// Shows the native Windows save-as dialog and saves the active tab.
void CodeEditor::SaveActiveTabAs() {
    const int ZeroIndex = 0;
    const DWORD DefaultFilterIndex = 1;

    if (ActiveTabIndex < ZeroIndex || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    HWND OwnerHwnd = (AppWindow != nullptr) ? glfwGetWin32Window(AppWindow) : nullptr;
    char SaveFilenameBuf[MaxFileSize] = {};
    OPENFILENAMEA SaveDialogStructure = {};
    SaveDialogStructure.lStructSize = sizeof(SaveDialogStructure);
    SaveDialogStructure.hwndOwner = OwnerHwnd;
    SaveDialogStructure.lpstrFile = SaveFilenameBuf;
    SaveDialogStructure.nMaxFile = MaxFileSize;
    SaveDialogStructure.lpstrFilter = "All Files\0*.*\0Text Files\0*.txt\0Code Files\0*.cpp;*.hpp;*.h;*.c\0Markdown Files\0*.md\0\0";
    SaveDialogStructure.nFilterIndex = DefaultFilterIndex;
    SaveDialogStructure.lpstrDefExt = "cpp";
    SaveDialogStructure.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&SaveDialogStructure)) {
        FileTab& CurrentTab = OpenTabs[ActiveTabIndex];
        std::string SavePayload = EditorHost.GetText();
        std::ofstream OutputFile(SaveFilenameBuf);

        if (OutputFile.is_open()) {
            OutputFile << SavePayload;
            OutputFile.close();
            CurrentTab.FilePath = SaveFilenameBuf;
            path PathObj(SaveFilenameBuf);
            CurrentTab.FileName = PathObj.filename().string();
            CurrentTab.IsModified = false;
            EditorHost.SetSavePoint();
            EditorHost.ApplyLanguageForFile(CurrentTab.FileName);
            AddLog("Saved file as: " + CurrentTab.FilePath);
            RefreshWorkspace();
        } else {
            AddLog("Error saving file: " + std::string(SaveFilenameBuf));
        }
    }
}

// Updates in-editor search match highlights and live match count using Scintilla's search engine.
void CodeEditor::UpdateSearchHighlights() {
    const int ZeroInt = 0;
    const int OneInt = 1;
    const int NegativeOneInt = -1;

    if (!EditorHost.IsReady()) {
        return;
    }

    int DocLen = static_cast<int>(EditorHost.Call(SCI_GETLENGTH));
    EditorHost.ClearIndicatorRange(IndicatorMatchHighlight, ZeroInt, DocLen);
    SearchMatchCount = ZeroInt;
    SearchCurrentMatch = ZeroInt;

    if (!ShowSearchPanel || SearchText[ZeroInt] == '\0' || DocLen <= ZeroInt) {
        return;
    }

    int SearchFlags = ZeroInt;

    if (SearchMatchCase) {
        SearchFlags |= SCFIND_MATCHCASE;
    }
    if (SearchWholeWord) {
        SearchFlags |= SCFIND_WHOLEWORD;
    }
    if (SearchUseRegex) {
        SearchFlags |= SCFIND_REGEXP | SCFIND_CXX11REGEX;
    }

    std::string Query(SearchText);
    EditorHost.Call(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(SearchFlags));

    int CaretPos = EditorHost.GetCurrentPos();
    int ScanPos = ZeroInt;

    while (ScanPos < DocLen) {
        EditorHost.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(ScanPos));
        EditorHost.Call(SCI_SETTARGETEND, static_cast<uptr_t>(DocLen));

        int FoundPos = static_cast<int>(EditorHost.Call(SCI_SEARCHINTARGET, static_cast<uptr_t>(Query.size()), reinterpret_cast<sptr_t>(Query.c_str())));

        if (FoundPos == NegativeOneInt) {
            break;
        }

        int FoundEnd = static_cast<int>(EditorHost.Call(SCI_GETTARGETEND));
        int MatchLen = FoundEnd - FoundPos;

        if (MatchLen <= ZeroInt) {
            ScanPos = FoundPos + OneInt;
            continue;
        }

        SearchMatchCount++;

        if (FoundPos <= CaretPos) {
            SearchCurrentMatch = SearchMatchCount;
        }

        EditorHost.FillIndicatorRange(IndicatorMatchHighlight, FoundPos, MatchLen);
        ScanPos = FoundEnd;
    }
}

// Navigates to the next or previous search match in the active Scintilla document.
void CodeEditor::FindNextMatch(bool Forward) {
    const int ZeroInt = 0;
    const int NegativeOneInt = -1;

    if (!EditorHost.IsReady() || SearchText[ZeroInt] == '\0') {
        return;
    }

    int SearchFlags = ZeroInt;

    if (SearchMatchCase) {
        SearchFlags |= SCFIND_MATCHCASE;
    }
    if (SearchWholeWord) {
        SearchFlags |= SCFIND_WHOLEWORD;
    }
    if (SearchUseRegex) {
        SearchFlags |= SCFIND_REGEXP | SCFIND_CXX11REGEX;
    }

    std::string Query(SearchText);
    int DocLen = static_cast<int>(EditorHost.Call(SCI_GETLENGTH));
    int SelStart = static_cast<int>(EditorHost.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(EditorHost.Call(SCI_GETSELECTIONEND));

    EditorHost.Call(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(SearchFlags));

    if (Forward) {
        EditorHost.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(SelEnd));
        EditorHost.Call(SCI_SETTARGETEND, static_cast<uptr_t>(DocLen));
    } else {
        EditorHost.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(SelStart));
        EditorHost.Call(SCI_SETTARGETEND, ZeroSptr);
    }

    int FoundPos = static_cast<int>(EditorHost.Call(SCI_SEARCHINTARGET, static_cast<uptr_t>(Query.size()), reinterpret_cast<sptr_t>(Query.c_str())));

    if (FoundPos == NegativeOneInt) {
        if (Forward) {
            EditorHost.Call(SCI_SETTARGETSTART, ZeroUptr);
            EditorHost.Call(SCI_SETTARGETEND, static_cast<uptr_t>(DocLen));
        } else {
            EditorHost.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(DocLen));
            EditorHost.Call(SCI_SETTARGETEND, ZeroSptr);
        }

        FoundPos = static_cast<int>(EditorHost.Call(SCI_SEARCHINTARGET, static_cast<uptr_t>(Query.size()), reinterpret_cast<sptr_t>(Query.c_str())));
    }

    if (FoundPos != NegativeOneInt) {
        int FoundEnd = static_cast<int>(EditorHost.Call(SCI_GETTARGETEND));
        EditorHost.Call(SCI_SETSEL, static_cast<uptr_t>(FoundPos), static_cast<sptr_t>(FoundEnd));
        EditorHost.Call(SCI_SCROLLCARET);
        UpdateSearchHighlights();
    }
}

// Replaces the currently selected match and advances to the next occurrence.
void CodeEditor::ReplaceCurrentMatch() {
    const int ZeroInt = 0;

    if (!EditorHost.IsReady() || SearchText[ZeroInt] == '\0') {
        return;
    }

    int SelStart = static_cast<int>(EditorHost.Call(SCI_GETSELECTIONSTART));
    int SelEnd = static_cast<int>(EditorHost.Call(SCI_GETSELECTIONEND));

    if (SelEnd > SelStart) {
        EditorHost.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(SelStart));
        EditorHost.Call(SCI_SETTARGETEND, static_cast<uptr_t>(SelEnd));
        EditorHost.Call(SCI_REPLACETARGET, static_cast<uptr_t>(strlen(ReplaceText)), reinterpret_cast<sptr_t>(ReplaceText));
    }

    FindNextMatch(true);
}

// Replaces all occurrences of SearchText in the active Scintilla document.
void CodeEditor::ReplaceAllMatches() {
    const int ZeroInt = 0;
    const int OneInt = 1;
    const int NegativeOneInt = -1;

    if (!EditorHost.IsReady() || SearchText[ZeroInt] == '\0') {
        return;
    }

    int SearchFlags = ZeroInt;

    if (SearchMatchCase) {
        SearchFlags |= SCFIND_MATCHCASE;
    }
    if (SearchWholeWord) {
        SearchFlags |= SCFIND_WHOLEWORD;
    }
    if (SearchUseRegex) {
        SearchFlags |= SCFIND_REGEXP | SCFIND_CXX11REGEX;
    }

    std::string Query(SearchText);
    std::string Replacement(ReplaceText);
    int Replacements = ZeroInt;
    int ScanPos = ZeroInt;

    EditorHost.Call(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(SearchFlags));
    EditorHost.Call(SCI_BEGINUNDOACTION);

    while (true) {
        int DocLen = static_cast<int>(EditorHost.Call(SCI_GETLENGTH));

        if (ScanPos >= DocLen) {
            break;
        }

        EditorHost.Call(SCI_SETTARGETSTART, static_cast<uptr_t>(ScanPos));
        EditorHost.Call(SCI_SETTARGETEND, static_cast<uptr_t>(DocLen));

        int FoundPos = static_cast<int>(EditorHost.Call(SCI_SEARCHINTARGET, static_cast<uptr_t>(Query.size()), reinterpret_cast<sptr_t>(Query.c_str())));

        if (FoundPos == NegativeOneInt) {
            break;
        }

        int FoundEnd = static_cast<int>(EditorHost.Call(SCI_GETTARGETEND));

        if (FoundEnd <= FoundPos) {
            ScanPos = FoundPos + OneInt;
            continue;
        }

        EditorHost.Call(SCI_REPLACETARGET, static_cast<uptr_t>(Replacement.size()), reinterpret_cast<sptr_t>(Replacement.c_str()));
        ScanPos = FoundPos + static_cast<int>(Replacement.size());
        Replacements++;
    }

    EditorHost.Call(SCI_ENDUNDOACTION);
    UpdateSearchHighlights();
    AddLog("Replaced instances: " + std::to_string(Replacements));
}

// Prompts for two files and opens them side-by-side in split Scintilla editors.
void CodeEditor::OpenCompareSideBySideDialog() {
    const DWORD DefaultFilterIndex = 1;

    HWND OwnerHwnd = (AppWindow != nullptr) ? glfwGetWin32Window(AppWindow) : nullptr;
    char LeftFileBuf[MaxFileSize] = {};
    OPENFILENAMEA LeftDlg = {};
    LeftDlg.lStructSize = sizeof(LeftDlg);
    LeftDlg.hwndOwner = OwnerHwnd;
    LeftDlg.lpstrFile = LeftFileBuf;
    LeftDlg.nMaxFile = MaxFileSize;
    LeftDlg.lpstrFilter = "All Files\0*.*\0\0";
    LeftDlg.nFilterIndex = DefaultFilterIndex;
    LeftDlg.lpstrTitle = "Select Left File";
    LeftDlg.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&LeftDlg)) {
        return;
    }

    char RightFileBuf[MaxFileSize] = {};
    OPENFILENAMEA RightDlg = {};
    RightDlg.lStructSize = sizeof(RightDlg);
    RightDlg.hwndOwner = OwnerHwnd;
    RightDlg.lpstrFile = RightFileBuf;
    RightDlg.nMaxFile = MaxFileSize;
    RightDlg.lpstrFilter = "All Files\0*.*\0\0";
    RightDlg.nFilterIndex = DefaultFilterIndex;
    RightDlg.lpstrTitle = "Select Right File";
    RightDlg.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&RightDlg)) {
        return;
    }

    OpenFileFromDisk(LeftFileBuf);

    std::ifstream RightStream(RightFileBuf);
    std::string RightContent = "";

    if (RightStream.is_open()) {
        std::stringstream Stream;
        Stream << RightStream.rdbuf();
        RightContent = Stream.str();
        RightStream.close();
    }

    EnsureSecondaryHostInitialized();

    if (SecondaryDoc != ZeroSptr) {
        SecondaryEditorHost.ReleaseDocument(SecondaryDoc);
        SecondaryDoc = ZeroSptr;
    }

    SecondaryFilePath = RightFileBuf;
    SecondaryFileName = path(RightFileBuf).filename().string();
    SecondaryDoc = SecondaryEditorHost.CreateDocument(RightContent);
    SecondaryEditorHost.SetActiveDocument(SecondaryDoc, SecondaryFileName, TabSize);
    CurrentLayoutMode = LayoutTwoColumns;
    IsSplitEditorOpen = true;
    AddLog("Opened side-by-side comparison: " + path(LeftFileBuf).filename().string() + " | " + SecondaryFileName);
}

// Initializes the secondary Scintilla host on demand to conserve memory when split view is closed.
void CodeEditor::EnsureSecondaryHostInitialized() {
    if (SecondaryEditorHost.IsReady()) {
        return;
    }

    HWND OwnerHwnd = (AppWindow != nullptr) ? glfwGetWin32Window(AppWindow) : nullptr;
    SecondaryEditorHost.Initialize(OwnerHwnd);
    SecondaryEditorHost.SetVisible(false);
}

// Closes the secondary side-by-side split editor pane and releases its native resources.
void CodeEditor::CloseSplitEditor() {
    if (!IsSplitEditorOpen) {
        return;
    }

    IsSplitEditorOpen = false;
    CurrentLayoutMode = LayoutSingle;
    SecondaryEditorHost.SetVisible(false);

    if (SecondaryDoc != ZeroSptr) {
        SecondaryEditorHost.ReleaseDocument(SecondaryDoc);
        SecondaryDoc = ZeroSptr;
    }

    SecondaryEditorHost.Shutdown();
    SecondaryFilePath.clear();
    SecondaryFileName.clear();
}

// Splits the current active editor pane into two columns or two rows.
void CodeEditor::SplitCurrentEditor(EditorLayoutMode Mode) {
    const int ZeroInt = 0;

    if (Mode == LayoutSingle) {
        CloseSplitEditor();

        return;
    }

    if (ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    EnsureSecondaryHostInitialized();

    const FileTab& ActiveTab = OpenTabs[ActiveTabIndex];
    std::string ActiveText = EditorHost.GetText();

    if (SecondaryDoc != ZeroSptr) {
        SecondaryEditorHost.ReleaseDocument(SecondaryDoc);
        SecondaryDoc = ZeroSptr;
    }

    SecondaryFilePath = ActiveTab.FilePath;
    SecondaryFileName = ActiveTab.FileName;
    SecondaryDoc = SecondaryEditorHost.CreateDocument(ActiveText);
    SecondaryEditorHost.SetActiveDocument(SecondaryDoc, SecondaryFileName, TabSize);
    CurrentLayoutMode = Mode;
    IsSplitEditorOpen = true;
}

// Toggles the active split editor layout between columns (side by side) and rows (top/bottom).
void CodeEditor::FlipEditorLayout() {
    if (!IsSplitEditorOpen) {
        return;
    }

    CurrentLayoutMode = (CurrentLayoutMode == LayoutTwoRows) ? LayoutTwoColumns : LayoutTwoRows;
}

// Launches a new independent instance of the Vadex editor executable.
void CodeEditor::LaunchNewWindow() {
    const DWORD ZeroDword = 0;
    char ExePath[MaxFileSize] = {};

    if (GetModuleFileNameA(nullptr, ExePath, MaxFileSize) == ZeroDword) {
        AddLog("Failed to resolve executable path for new window.");

        return;
    }

    STARTUPINFOA StartInfo = {};
    StartInfo.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION ProcInfo = {};
    std::string CmdLine = "\"" + std::string(ExePath) + "\"";
    std::vector<char> CmdBuf(CmdLine.begin(), CmdLine.end());
    CmdBuf.push_back('\0');

    if (CreateProcessA(nullptr, CmdBuf.data(), nullptr, nullptr, FALSE, ZeroDword, nullptr, nullptr, &StartInfo, &ProcInfo)) {
        CloseHandle(ProcInfo.hProcess);
        CloseHandle(ProcInfo.hThread);
        AddLog("Launched new Vadex window.");
    } else {
        AddLog("Failed to launch new Vadex window.");
    }
}

// Creates a new untitled plain-text file tab.
void CodeEditor::CreateNewTextFile() {
    std::string Name = "Untitled-" + std::to_string(NextUntitledIndex++) + ".txt";
    CreateNewTab(Name, "", false);
}

// Opens Welcome.md in a tab (or focuses it if already open) with Markdown split preview enabled.
void CodeEditor::OpenWelcomeTab() {
    const int ZeroInt = 0;

    ShowMarkdownPreview = true;

    for (int Idx = ZeroInt; Idx < static_cast<int>(OpenTabs.size()); ++Idx) {
        if (OpenTabs[Idx].FileName == "Welcome.md") {
            ActiveTabIndex = Idx;
            PendingSelectTabId = OpenTabs[Idx].TabId;
            SyncActiveTabDocument();

            return;
        }
    }

    std::string WelcomeContent;
    std::ifstream WelcomeFile(AppDataDir + "\\Welcome.md");

    if (WelcomeFile.is_open()) {
        std::stringstream WelcomeStream;
        WelcomeStream << WelcomeFile.rdbuf();
        WelcomeContent = WelcomeStream.str();
        WelcomeFile.close();
    }

    CreateNewTab("Welcome.md", WelcomeContent, true);
}

// Renders the About Vadex modal dialog.
void CodeEditor::RenderAboutModal() {
    const float HalfFloat = 0.5f;
    const float AccentRed = 0.35f;
    const float AccentGreen = 0.65f;
    const float AccentBlue = 0.95f;
    const float FullAlpha = 1;

    if (!ShowAboutModal) {
        return;
    }

    ImGui::OpenPopup("About Vadex");

    ImGuiIO& IoRef = ImGui::GetIO();
    ImVec2 CenterPosition = ImVec2(IoRef.DisplaySize.x * HalfFloat, IoRef.DisplaySize.y * HalfFloat);
    ImGui::SetNextWindowPos(CenterPosition, ImGuiCond_Appearing, ImVec2(HalfFloat, HalfFloat));

    if (ImGui::BeginPopupModal("About Vadex", &ShowAboutModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(AccentRed, AccentGreen, AccentBlue, FullAlpha), "Vadex v1");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("A fast, native C++ code editor built with system resources in mind:");
        ImGui::BulletText("Powered by Scintilla, Lexilla, GDB, GLFW, Vulkan and ImGUI.");
        ImGui::BulletText("Lucide SVG vector icons via Lucide API");
        ImGui::BulletText("Open-Source project.");
        ImGui::BulletText("Made by D Letten.");
        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Close")) {
            ShowAboutModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// Automatically saves the active file after a short idle debounce when Auto Save is enabled.
void CodeEditor::UpdateAutoSave() {
    const double AutoSaveDelaySec = 1;
    const int ZeroInt = 0;

    if (!AutoSaveEnabled || !PendingAutoSave) {
        return;
    }

    if ((glfwGetTime() - LastEditTimestamp) < AutoSaveDelaySec) {
        return;
    }

    PendingAutoSave = false;

    if (ActiveTabIndex >= ZeroInt && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
        const FileTab& ActiveTab = OpenTabs[ActiveTabIndex];

        if (ActiveTab.IsModified && !ActiveTab.IsReadOnly && !ActiveTab.FilePath.empty() && exists(ActiveTab.FilePath)) {
            SaveActiveTab();
        }
    }
}

// Records the current cursor location into the Back/Forward navigation history stack.
void CodeEditor::RecordNavigationPoint() {
    const int ZeroInt = 0;
    const int OneInt = 1;
    const int MinLineDelta = 5;
    const size_t MaxHistorySize = 50;

    if (IsNavigatingHistory || ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    NavigationLocation Loc;
    Loc.FilePath = OpenTabs[ActiveTabIndex].FilePath;
    Loc.Line = EditorHost.GetCurrentLine();
    Loc.Column = EditorHost.GetCurrentColumn();

    if (NavHistoryIndex >= ZeroInt && NavHistoryIndex < static_cast<int>(NavHistory.size())) {
        const NavigationLocation& Cur = NavHistory[static_cast<size_t>(NavHistoryIndex)];

        if (Cur.FilePath == Loc.FilePath && std::abs(Cur.Line - Loc.Line) < MinLineDelta) {
            return;
        }

        NavHistory.resize(static_cast<size_t>(NavHistoryIndex + OneInt));
    }

    NavHistory.push_back(Loc);

    if (NavHistory.size() > MaxHistorySize) {
        NavHistory.erase(NavHistory.begin());
    }

    NavHistoryIndex = static_cast<int>(NavHistory.size()) - OneInt;
}

// Navigates backward to the previous recorded cursor location.
void CodeEditor::NavigateBack() {
    const int ZeroInt = 0;
    const int OneInt = 1;

    if (NavHistoryIndex <= ZeroInt || NavHistory.empty()) {
        return;
    }

    NavHistoryIndex -= OneInt;
    const NavigationLocation& Target = NavHistory[static_cast<size_t>(NavHistoryIndex)];
    IsNavigatingHistory = true;

    if (!Target.FilePath.empty() && exists(Target.FilePath)) {
        OpenFileFromDisk(Target.FilePath);
    }

    EditorHost.GoToLineColumn(Target.Line, Target.Column);
    EditorHost.Focus();
    IsNavigatingHistory = false;
}

// Navigates forward to the next recorded cursor location.
void CodeEditor::NavigateForward() {
    const int ZeroInt = 0;
    const int OneInt = 1;

    if (NavHistoryIndex < ZeroInt || NavHistoryIndex + OneInt >= static_cast<int>(NavHistory.size())) {
        return;
    }

    NavHistoryIndex += OneInt;
    const NavigationLocation& Target = NavHistory[static_cast<size_t>(NavHistoryIndex)];
    IsNavigatingHistory = true;

    if (!Target.FilePath.empty() && exists(Target.FilePath)) {
        OpenFileFromDisk(Target.FilePath);
    }

    EditorHost.GoToLineColumn(Target.Line, Target.Column);
    EditorHost.Focus();
    IsNavigatingHistory = false;
}

// Navigates to the last recorded text edit location.
void CodeEditor::NavigateToLastEdit() {
    if (LastEditLocation.FilePath.empty()) {
        return;
    }

    if (exists(LastEditLocation.FilePath)) {
        OpenFileFromDisk(LastEditLocation.FilePath);
    }

    EditorHost.GoToLineColumn(LastEditLocation.Line, LastEditLocation.Column);
    EditorHost.Focus();
}

// Opens the fuzzy Go to Symbol palette for symbols in the active document.
void CodeEditor::GoToSymbolInFile() {
    RefreshOutlineSymbols();
    Palette.Open(PaletteModeGoToSymbol);
    FocusMainImGuiWindow();
}

// Extracts the identifier word currently under the caret in the primary Scintilla editor.
std::string CodeEditor::GetWordUnderCaret() const {
    const int ZeroInt = 0;
    const int OneInt = 1;

    if (!EditorHost.IsReady()) {
        return "";
    }

    int Pos = EditorHost.GetCurrentPos();
    int WordStart = static_cast<int>(EditorHost.Call(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(Pos), OneInt));
    int WordEnd = static_cast<int>(EditorHost.Call(SCI_WORDENDPOSITION, static_cast<uptr_t>(Pos), OneInt));
    int WordLen = WordEnd - WordStart;

    if (WordLen <= ZeroInt) {
        return "";
    }

    std::string Word(static_cast<size_t>(WordLen), '\0');

    for (int Idx = ZeroInt; Idx < WordLen; ++Idx) {
        Word[static_cast<size_t>(Idx)] = static_cast<char>(EditorHost.Call(SCI_GETCHARAT, static_cast<uptr_t>(WordStart + Idx)));
    }

    return Word;
}

// Searches local outline symbols and workspace files as a fallback when LSP is unavailable.
void CodeEditor::JumpToWorkspaceSymbol(const std::string& SymbolName, const std::string& PreferKind) {
    if (SymbolName.empty()) {
        return;
    }

    RefreshOutlineSymbols();

    for (const auto& Sym : CachedOutlineSymbols) {
        if (Sym.Name == SymbolName || Sym.Name.find("::" + SymbolName) != std::string::npos) {
            if (PreferKind.empty() || Sym.Kind == PreferKind) {
                EditorHost.GoToLine(Sym.LineNumber);
                EditorHost.Focus();

                return;
            }
        }
    }

    if (!CurrentFolderPath.empty()) {
        std::vector<WorkspaceSearchMatch> Matches = WorkspaceSearch::Search(CurrentFolderPath, SymbolName, true);

        for (const auto& Match : Matches) {
            bool IsTypeMatch = (PreferKind == "type" && (Match.LinePreview.find("class " + SymbolName) != std::string::npos || Match.LinePreview.find("struct " + SymbolName) != std::string::npos || Match.LinePreview.find("enum " + SymbolName) != std::string::npos));
            bool IsDeclMatch = (PreferKind == "decl" && (Match.RelativePath.find(".hpp") != std::string::npos || Match.RelativePath.find(".h") != std::string::npos));
            bool IsImplMatch = (PreferKind == "impl" && (Match.LinePreview.find("::" + SymbolName) != std::string::npos || Match.LinePreview.find('{') != std::string::npos));

            if (IsTypeMatch || IsDeclMatch || IsImplMatch || PreferKind.empty()) {
                OpenFileFromDisk(Match.FilePath);
                EditorHost.GoToLine(Match.LineNumber);
                EditorHost.Focus();

                return;
            }
        }
    }

    AddLog("Symbol not found: " + SymbolName);
}

// Requests Go to Definition via LSP or falls back to local symbol lookup.
void CodeEditor::GoToDefinitionOrFallback() {
    const int ZeroInt = 0;
    const int OneInt = 1;

    if (ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    if (Lsp.GetIsRunning()) {
        Lsp.RequestDefinition(OpenTabs[ActiveTabIndex].FilePath, EditorHost.GetCurrentLine() - OneInt, EditorHost.GetCurrentColumn() - OneInt);
    } else {
        JumpToWorkspaceSymbol(GetWordUnderCaret(), "");
    }
}

// Requests Go to Declaration via LSP or falls back to header symbol lookup.
void CodeEditor::GoToDeclarationOrFallback() {
    const int ZeroInt = 0;
    const int OneInt = 1;

    if (ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    if (Lsp.GetIsRunning()) {
        Lsp.RequestDeclaration(OpenTabs[ActiveTabIndex].FilePath, EditorHost.GetCurrentLine() - OneInt, EditorHost.GetCurrentColumn() - OneInt);
    } else {
        JumpToWorkspaceSymbol(GetWordUnderCaret(), "decl");
    }
}

// Requests Go to Type Definition via LSP or falls back to class/struct/enum lookup.
void CodeEditor::GoToTypeDefinitionOrFallback() {
    const int ZeroInt = 0;
    const int OneInt = 1;

    if (ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    if (Lsp.GetIsRunning()) {
        Lsp.RequestTypeDefinition(OpenTabs[ActiveTabIndex].FilePath, EditorHost.GetCurrentLine() - OneInt, EditorHost.GetCurrentColumn() - OneInt);
    } else {
        JumpToWorkspaceSymbol(GetWordUnderCaret(), "type");
    }
}

// Requests Go to Implementations via LSP or falls back to implementation lookup.
void CodeEditor::GoToImplementationOrFallback() {
    const int ZeroInt = 0;
    const int OneInt = 1;

    if (ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    if (Lsp.GetIsRunning()) {
        Lsp.RequestImplementation(OpenTabs[ActiveTabIndex].FilePath, EditorHost.GetCurrentLine() - OneInt, EditorHost.GetCurrentColumn() - OneInt);
    } else {
        JumpToWorkspaceSymbol(GetWordUnderCaret(), "impl");
    }
}

// Requests Go to References via LSP and opens Workspace Search populated with the symbol under caret.
void CodeEditor::GoToReferencesOrFallback() {
    const int ZeroInt = 0;
    const int OneInt = 1;
    const float ZeroFloat = 0;

    if (ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    std::string Word = GetWordUnderCaret();

    if (Lsp.GetIsRunning()) {
        Lsp.RequestReferences(OpenTabs[ActiveTabIndex].FilePath, EditorHost.GetCurrentLine() - OneInt, EditorHost.GetCurrentColumn() - OneInt);
    }

    if (!Word.empty() && !CurrentFolderPath.empty()) {
        strncpy(WorkspaceSearchBuffer, Word.c_str(), DefaultBufferCapacity - OneInt);
        WorkspaceSearchBuffer[DefaultBufferCapacity - OneInt] = '\0';
        WorkspaceSearchCase = true;
        WorkspaceSearchResults = WorkspaceSearch::Search(CurrentFolderPath, Word, true);
        IsZenMode = false;
        ShowSidebar = true;
        ActiveSidebarPanel = SidebarPanelSearch;
        SidebarSlideProgress = ZeroFloat;
    }
}

// Navigates to the next or previous LSP diagnostic problem in the active file.
void CodeEditor::GoToNextProblem(bool Forward) {
    const int ZeroInt = 0;
    const int OneInt = 1;
    const int NegativeOneInt = -1;

    if (ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    std::vector<LspDiagnostic> Diags = Lsp.GetDiagnosticsForFile(OpenTabs[ActiveTabIndex].FilePath);

    if (Diags.empty()) {
        AddLog("No problems reported in active file.");

        return;
    }

    std::sort(Diags.begin(), Diags.end(), [](const LspDiagnostic& Left, const LspDiagnostic& Right) {
        return (Left.Line != Right.Line) ? (Left.Line < Right.Line) : (Left.StartChar < Right.StartChar);
    });

    int CurZeroLine = EditorHost.GetCurrentLine() - OneInt;
    int TargetIdx = NegativeOneInt;

    if (Forward) {
        for (int Idx = ZeroInt; Idx < static_cast<int>(Diags.size()); ++Idx) {
            if (Diags[static_cast<size_t>(Idx)].Line > CurZeroLine) {
                TargetIdx = Idx;
                break;
            }
        }

        if (TargetIdx == NegativeOneInt) {
            TargetIdx = ZeroInt;
        }
    } else {
        for (int Idx = static_cast<int>(Diags.size()) - OneInt; Idx >= ZeroInt; --Idx) {
            if (Diags[static_cast<size_t>(Idx)].Line < CurZeroLine) {
                TargetIdx = Idx;
                break;
            }
        }

        if (TargetIdx == NegativeOneInt) {
            TargetIdx = static_cast<int>(Diags.size()) - OneInt;
        }
    }

    const LspDiagnostic& Target = Diags[static_cast<size_t>(TargetIdx)];
    EditorHost.GoToLineColumn(Target.Line + OneInt, Target.StartChar + OneInt);
    EditorHost.Focus();

    if (!Target.Message.empty()) {
        EditorHost.ShowCallTip(EditorHost.GetCurrentPos(), Target.Message);
    }
}

// Navigates to the next or previous Git diff hunk in the active file.
void CodeEditor::GoToNextChange(bool Forward) {
    const int ZeroInt = 0;
    const int OneInt = 1;
    const int NegativeOneInt = -1;

    if (ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    std::string FilePath = OpenTabs[ActiveTabIndex].FilePath;

    if (FilePath.empty() || !exists(FilePath)) {
        return;
    }

    path TargetPath(FilePath);
    std::string WorkDir = CurrentFolderPath.empty() ? TargetPath.parent_path().string() : CurrentFolderPath;
    std::string DiffOut = RunGitCommandCapture(WorkDir, "diff -U0 -- \"" + FilePath + "\"");
    std::vector<int> ChangedLines;
    std::istringstream Stream(DiffOut);
    std::string Line;

    while (std::getline(Stream, Line)) {
        if (Line.rfind("@@ ", ZeroInt) == ZeroInt) {
            size_t PlusPos = Line.find('+');

            if (PlusPos != std::string::npos) {
                try {
                    int HunkLine = std::stoi(Line.substr(PlusPos + OneInt));

                    if (HunkLine < OneInt) {
                        HunkLine = OneInt;
                    }

                    ChangedLines.push_back(HunkLine);
                } catch (...) {
                }
            }
        }
    }

    if (ChangedLines.empty()) {
        AddLog("No Git changes found in active file.");

        return;
    }

    int CurLine = EditorHost.GetCurrentLine();
    int TargetLine = NegativeOneInt;

    if (Forward) {
        for (int HunkLine : ChangedLines) {
            if (HunkLine > CurLine) {
                TargetLine = HunkLine;
                break;
            }
        }

        if (TargetLine == NegativeOneInt) {
            TargetLine = ChangedLines.front();
        }
    } else {
        for (int Idx = static_cast<int>(ChangedLines.size()) - OneInt; Idx >= ZeroInt; --Idx) {
            if (ChangedLines[static_cast<size_t>(Idx)] < CurLine) {
                TargetLine = ChangedLines[static_cast<size_t>(Idx)];
                break;
            }
        }

        if (TargetLine == NegativeOneInt) {
            TargetLine = ChangedLines.back();
        }
    }

    EditorHost.GoToLine(TargetLine);
    EditorHost.Focus();
}

// Saves the file currently open in the secondary side-by-side split editor.
void CodeEditor::SaveSecondaryFile() {
    if (!IsSplitEditorOpen || SecondaryFilePath.empty()) {
        return;
    }

    std::ofstream OutputFile(SecondaryFilePath);

    if (OutputFile.is_open()) {
        OutputFile << SecondaryEditorHost.GetText();
        OutputFile.close();
        SecondaryEditorHost.SetSavePoint();
        AddLog("Saved split file: " + SecondaryFilePath);
    }
}

// Runs a git command synchronously in WorkingDir and returns its stdout output.
std::string CodeEditor::RunGitCommandCapture(const std::string& WorkingDir, const std::string& GitArgs) const {
    const DWORD ZeroDword = 0;
    const int OneOffset = 1;
    constexpr int ReadBufSize = 4095;

    SECURITY_ATTRIBUTES SecAttr = {};
    SecAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    SecAttr.bInheritHandle = TRUE;
    SecAttr.lpSecurityDescriptor = nullptr;

    HANDLE ReadPipe = nullptr;
    HANDLE WritePipe = nullptr;

    if (!CreatePipe(&ReadPipe, &WritePipe, &SecAttr, ZeroDword)) {
        return "";
    }

    SetHandleInformation(ReadPipe, HANDLE_FLAG_INHERIT, ZeroDword);

    STARTUPINFOA StartInfo = {};
    StartInfo.cb = sizeof(STARTUPINFOA);
    StartInfo.hStdOutput = WritePipe;
    StartInfo.hStdError = WritePipe;
    StartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    StartInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION ProcInfo = {};
    std::string FullCmd = "git " + GitArgs;
    std::vector<char> CmdBuf(FullCmd.begin(), FullCmd.end());
    CmdBuf.push_back('\0');

    BOOL Created = CreateProcessA(
        nullptr,
        CmdBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        WorkingDir.c_str(),
        &StartInfo,
        &ProcInfo
    );

    CloseHandle(WritePipe);

    if (!Created) {
        CloseHandle(ReadPipe);

        return "";
    }

    std::string Output = "";
    char Buffer[ReadBufSize];
    DWORD BytesRead = ZeroDword;

    while (ReadFile(ReadPipe, Buffer, ReadBufSize - OneOffset, &BytesRead, nullptr) && BytesRead > ZeroDword) {
        Output.append(Buffer, static_cast<size_t>(BytesRead));
    }

    WaitForSingleObject(ProcInfo.hProcess, INFINITE);
    CloseHandle(ReadPipe);
    CloseHandle(ProcInfo.hProcess);
    CloseHandle(ProcInfo.hThread);

    return Output;
}

// Prompts for a file on disk and opens the Git HEAD vs. Local diff viewer for it.
void CodeEditor::OpenGitDiffFileDialog() {
    const DWORD DefaultFilterIndex = 1;

    HWND OwnerHwnd = (AppWindow != nullptr) ? glfwGetWin32Window(AppWindow) : nullptr;
    char TargetFileBuf[MaxFileSize] = {};
    OPENFILENAMEA Dialog = {};
    Dialog.lStructSize = sizeof(Dialog);
    Dialog.hwndOwner = OwnerHwnd;
    Dialog.lpstrFile = TargetFileBuf;
    Dialog.nMaxFile = MaxFileSize;
    Dialog.lpstrFilter = "All Files\0*.*\0\0";
    Dialog.nFilterIndex = DefaultFilterIndex;
    Dialog.lpstrTitle = "Select File for Git Diff";
    Dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&Dialog)) {
        OpenGitDiffForPath(TargetFileBuf);
    }
}

// Fetches the Git HEAD version of a file and opens the DiffViewer against the local file on disk.
void CodeEditor::OpenGitDiffForPath(const std::string& FilePath) {
    if (FilePath.empty()) {
        return;
    }

    path TargetPath(FilePath);
    std::string FileDir = TargetPath.parent_path().string();

    if (FileDir.empty()) {
        FileDir = CurrentFolderPath.empty() ? "." : CurrentFolderPath;
    }

    // Resolves the git repository root and relative path for git show HEAD:<rel>.
    std::string RepoRoot = RunGitCommandCapture(FileDir, "rev-parse --show-toplevel");

    while (!RepoRoot.empty() && (RepoRoot.back() == '\n' || RepoRoot.back() == '\r')) {
        RepoRoot.pop_back();
    }

    std::string GitContent = "";
    std::string LeftLabel = TargetPath.filename().string() + " (Git HEAD)";

    if (!RepoRoot.empty() && RepoRoot.find("fatal:") == std::string::npos) {
        std::error_code Err;
        path RelPath = relative(TargetPath, path(RepoRoot), Err);

        if (!Err) {
            std::string RelStr = RelPath.generic_string();
            std::string GitOut = RunGitCommandCapture(RepoRoot, "show \"HEAD:" + RelStr + "\"");

            if (GitOut.find("fatal:") == std::string::npos) {
                GitContent = GitOut;
            } else {
                LeftLabel = TargetPath.filename().string() + " (Untracked in Git)";
            }
        }
    } else {
        LeftLabel = TargetPath.filename().string() + " (Not in Git Repo)";
    }

    // Reads the current version from disk (or live buffer if it is the active tab).
    std::string LocalContent = "";
    const int ZeroInt = 0;

    if (ActiveTabIndex >= ZeroInt && ActiveTabIndex < static_cast<int>(OpenTabs.size()) && OpenTabs[ActiveTabIndex].FilePath == FilePath) {
        LocalContent = EditorHost.GetText();
    } else {
        std::ifstream LocalStream(FilePath);

        if (LocalStream.is_open()) {
            std::stringstream Stream;
            Stream << LocalStream.rdbuf();
            LocalContent = Stream.str();
            LocalStream.close();
        }
    }

    DiffView.OpenDiff(LeftLabel, GitContent, TargetPath.filename().string() + " (Local)", LocalContent);
}

// Applies LSP diagnostic squiggle indicators to the currently active file.
void CodeEditor::ApplyDiagnosticsToActiveFile() {
    const int ZeroInt = 0;
    const int OneInt = 1;
    const int ErrorSeverity = 1;

    if (!EditorHost.IsReady() || ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    int DocLen = static_cast<int>(EditorHost.Call(SCI_GETLENGTH));
    EditorHost.ClearIndicatorRange(IndicatorDiagError, ZeroInt, DocLen);
    EditorHost.ClearIndicatorRange(IndicatorDiagWarning, ZeroInt, DocLen);

    std::vector<LspDiagnostic> Diags = Lsp.GetDiagnosticsForFile(OpenTabs[ActiveTabIndex].FilePath);

    for (const auto& Diag : Diags) {
        int LineStart = static_cast<int>(EditorHost.Call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(Diag.Line)));
        int StartPos = LineStart + Diag.StartChar;
        int Length = Diag.EndChar - Diag.StartChar;

        if (Length < OneInt) {
            Length = OneInt;
        }

        int IndicatorId = (Diag.Severity == ErrorSeverity) ? IndicatorDiagError : IndicatorDiagWarning;
        EditorHost.FillIndicatorRange(IndicatorId, StartPos, Length);
    }
}

// Consumes pending asynchronous LSP completion, hover, and definition responses.
void CodeEditor::PollLspResponses() {
    const int ZeroInt = 0;

    if (!Lsp.GetIsRunning()) {
        return;
    }

    std::string Completions;

    if (Lsp.PopCompletions(Completions)) {
        EditorHost.ShowAutoComplete(ZeroInt, Completions);
    }

    std::string HoverText;
    int HoverPos = ZeroInt;

    if (Lsp.PopHover(HoverText, HoverPos)) {
        EditorHost.ShowCallTip(HoverPos, HoverText);
    }

    LspDefinitionTarget DefTarget;

    if (Lsp.PopDefinition(DefTarget)) {
        OpenFileFromDisk(DefTarget.FilePath);
        EditorHost.GoToLineColumn(DefTarget.Line, DefTarget.Column);
        EditorHost.Focus();
    }

    ApplyDiagnosticsToActiveFile();
}

// Renders the modal dialog for New File, New Folder, Rename, and Delete explorer actions.
void CodeEditor::RenderExplorerActionModal() {
    const int ZeroIndex = 0;
    const float HalfFloat = 0.5f;
    const float InputWidth = 270;

    if (ActiveExplorerAction == ExplorerActionNone) {
        return;
    }

    const char* ModalTitle = "Workspace Action";

    if (ActiveExplorerAction == ExplorerActionNewFile) {
        ModalTitle = "New File";
    } else if (ActiveExplorerAction == ExplorerActionNewFolder) {
        ModalTitle = "New Folder";
    } else if (ActiveExplorerAction == ExplorerActionRename) {
        ModalTitle = "Rename";
    } else if (ActiveExplorerAction == ExplorerActionDelete) {
        ModalTitle = "Delete Item";
    }

    ImGui::OpenPopup(ModalTitle);

    ImGuiIO& IoRef = ImGui::GetIO();
    ImVec2 CenterPosition = ImVec2(IoRef.DisplaySize.x * HalfFloat, IoRef.DisplaySize.y * HalfFloat);
    ImGui::SetNextWindowPos(CenterPosition, ImGuiCond_Appearing, ImVec2(HalfFloat, HalfFloat));

    if (ImGui::BeginPopupModal(ModalTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ExplorerInputBuffer[ZeroIndex] = '\0';
            ActiveExplorerAction = ExplorerActionNone;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();

            return;
        }

        if (ActiveExplorerAction == ExplorerActionDelete) {
            path TargetObj(ExplorerActionTargetPath);
            ImGui::Text("Delete \"%s\"?", TargetObj.filename().string().c_str());

            if (ImGui::Button("Delete")) {
                const int OneOffset = 1;
                std::string NormalizedTarget = TargetObj.lexically_normal().string();

                // Closes any open tabs matching or inside the deleted path before removing from disk.
                for (int TabIdx = static_cast<int>(OpenTabs.size()) - OneOffset; TabIdx >= ZeroIndex; --TabIdx) {
                    std::string TabNorm = path(OpenTabs[static_cast<size_t>(TabIdx)].FilePath).lexically_normal().string();

                    if (TabNorm == NormalizedTarget || (TabNorm.size() > NormalizedTarget.size() && TabNorm.rfind(NormalizedTarget, ZeroIndex) == ZeroIndex && (TabNorm[NormalizedTarget.size()] == '\\' || TabNorm[NormalizedTarget.size()] == '/'))) {
                        CloseTabImmediate(TabIdx);
                    }
                }

                if (IsSplitEditorOpen && !SecondaryFilePath.empty()) {
                    std::string SecNorm = path(SecondaryFilePath).lexically_normal().string();

                    if (SecNorm == NormalizedTarget || (SecNorm.size() > NormalizedTarget.size() && SecNorm.rfind(NormalizedTarget, ZeroIndex) == ZeroIndex)) {
                        CloseSplitEditor();
                    }
                }

                // Clears Windows read-only attributes recursively so remove_all succeeds on protected files.
                std::error_code AttrErr;

                if (is_directory(TargetObj, AttrErr)) {
                    for (auto Iter = recursive_directory_iterator(TargetObj, directory_options::skip_permission_denied, AttrErr);
                         Iter != recursive_directory_iterator();
                         Iter.increment(AttrErr)) {
                        if (AttrErr) {
                            break;
                        }

                        SetFileAttributesA(Iter->path().string().c_str(), FILE_ATTRIBUTE_NORMAL);
                    }
                }

                SetFileAttributesA(TargetObj.string().c_str(), FILE_ATTRIBUTE_NORMAL);

                std::error_code ErrorCode;
                remove_all(TargetObj, ErrorCode);

                if (!ErrorCode) {
                    AddLog("Deleted: " + ExplorerActionTargetPath);
                    RefreshWorkspace();
                    Git.Refresh(CurrentFolderPath);
                } else {
                    AddLog("Failed to delete: " + ExplorerActionTargetPath + " (" + ErrorCode.message() + ")");
                }

                ActiveExplorerAction = ExplorerActionNone;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                ActiveExplorerAction = ExplorerActionNone;
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::Text("Enter name:");
            ImGui::SetNextItemWidth(InputWidth);
            bool Submitted = ImGui::InputText("##ExplorerActionInput", ExplorerInputBuffer, DefaultBufferCapacity, ImGuiInputTextFlags_EnterReturnsTrue);

            if (ImGui::Button("Confirm") || Submitted) {
                std::string NewName(ExplorerInputBuffer);

                if (!NewName.empty()) {
                    std::error_code ErrorCode;

                    if (ActiveExplorerAction == ExplorerActionNewFile) {
                        path NewFilePath = path(ExplorerActionTargetPath) / NewName;
                        std::ofstream CreatedFile(NewFilePath);

                        if (CreatedFile.is_open()) {
                            CreatedFile.close();
                            RefreshWorkspace();
                            OpenFileFromDisk(NewFilePath.string());
                        }
                    } else if (ActiveExplorerAction == ExplorerActionNewFolder) {
                        path NewFolderPath = path(ExplorerActionTargetPath) / NewName;
                        create_directories(NewFolderPath, ErrorCode);
                        RefreshWorkspace();
                    } else if (ActiveExplorerAction == ExplorerActionRename) {
                        path OldPath(ExplorerActionTargetPath);
                        path NewPath = OldPath.parent_path() / NewName;
                        rename(OldPath, NewPath, ErrorCode);

                        if (!ErrorCode) {
                            for (auto& Tab : OpenTabs) {
                                if (Tab.FilePath == OldPath.string()) {
                                    Tab.FilePath = NewPath.string();
                                    Tab.FileName = NewName;
                                }
                            }

                            RefreshWorkspace();
                        }
                    }
                }

                ExplorerInputBuffer[ZeroIndex] = '\0';
                ActiveExplorerAction = ExplorerActionNone;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                ExplorerInputBuffer[ZeroIndex] = '\0';
                ActiveExplorerAction = ExplorerActionNone;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

// Renders the unsaved changes confirmation modal when closing modified tabs or exiting.
void CodeEditor::RenderUnsavedChangesModal() {
    const int ZeroIndex = 0;
    const int NegativeOneInt = -1;
    const float HalfFloat = 0.5f;

    if (!ShowUnsavedModal) {
        return;
    }

    ImGui::OpenPopup("Unsaved Changes");

    ImGuiIO& IoRef = ImGui::GetIO();
    ImVec2 CenterPosition = ImVec2(IoRef.DisplaySize.x * HalfFloat, IoRef.DisplaySize.y * HalfFloat);
    ImGui::SetNextWindowPos(CenterPosition, ImGuiCond_Appearing, ImVec2(HalfFloat, HalfFloat));

    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (PendingExitAfterSave) {
            ImGui::Text("You have unsaved changes. Save before exiting?");
        } else if (PendingCloseTabIndex >= ZeroIndex && PendingCloseTabIndex < static_cast<int>(OpenTabs.size())) {
            ImGui::Text("Save changes to \"%s\" before closing?", OpenTabs[PendingCloseTabIndex].FileName.c_str());
        }

        if (ImGui::Button("Save")) {
            if (PendingExitAfterSave) {
                for (int Idx = ZeroIndex; Idx < static_cast<int>(OpenTabs.size()); ++Idx) {
                    if (OpenTabs[Idx].IsModified) {
                        SaveTabByIndex(Idx);
                    }
                }

                ShouldExit = true;
            } else {
                SaveTabByIndex(PendingCloseTabIndex);
                CloseTabImmediate(PendingCloseTabIndex);
            }

            ShowUnsavedModal = false;
            PendingExitAfterSave = false;
            PendingCloseTabIndex = NegativeOneInt;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Don't Save")) {
            if (PendingExitAfterSave) {
                ShouldExit = true;
            } else {
                CloseTabImmediate(PendingCloseTabIndex);
            }

            ShowUnsavedModal = false;
            PendingExitAfterSave = false;
            PendingCloseTabIndex = NegativeOneInt;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            ShowUnsavedModal = false;
            PendingExitAfterSave = false;
            PendingCloseTabIndex = NegativeOneInt;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// Renders the complete editor layout, primary/secondary Scintilla bounds, minimap, sticky scroll, multi-terminals, and modals.
void CodeEditor::Render() {
    const int ZeroInt = 0;
    const int OneInt = 1;
    const int NegativeOneInt = -1;
    const int MainStyleVarCount = 3;
    const int MinTabSize = 1;
    const int MaxTabSize = 9;
    const float ZeroFloat = 0;
    const float FullFloat = 1;
    const float NegativeOneFloat = -1;
    const float HalfFloat = 0.5f;
    const float DoubleMultiplier = 1 + 1;
    const float MenuBarPaddingX = 10;
    const float MenuBarPaddingY = 7;
    const float OuterMargin = 7;
    const float StatusBarHeight = 31;
    const float StatusBarPaddingX = 11;
    const float StatusBarPaddingY = 5;
    const float ConsoleSpacing = 5;
    const float NormalItemSpacingX = 9;
    const float ConsoleResizeHeight = 5;
    const float ResizeHandleWidth = 5;
    const float SplitDividerWidth = 5;
    const float MinSidebarWidth = 100;
    const float MaxSidebarWidth = 500;
    const float MinCenterHeight = 100;
    const float MinConsoleHeight = 120;
    const float ConsoleMaxOffset = 100;
    const float SearchInputWidth = 150;
    const float MinimapWidth = 85;
    const float ReservedPromptHeight = 45;
    const float MinScrollHeight = 30;
    const float PromptRed = 0.20f;
    const float PromptGreen = 0.85f;
    const float PromptBlue = 0.40f;

    PollLspResponses();
    PollDebuggerEvents();
    UpdateAutoSave();

    if (RegularFont != nullptr) {
        ImGui::PushFont(RegularFont);
    }

    ImGuiViewport* Viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(Viewport->WorkPos);
    ImGui::SetNextWindowSize(Viewport->WorkSize);

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(MenuBarPaddingX, MenuBarPaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ZeroFloat);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, ZeroFloat);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ZeroFloat, ZeroFloat));
    ImGui::Begin("MainLayoutWindow", nullptr, WindowFlags);
    ImGui::PopStyleVar(MainStyleVarCount);

    DrawMenuBar();

    ImGui::PopStyleVar();

    // Handles global keyboard shortcuts when ImGui has focus.
    ImGuiIO& KeyIo = ImGui::GetIO();
    bool IsCtrlHeld = KeyIo.KeyCtrl;
    bool IsShiftHeld = KeyIo.KeyShift;

    if (IsCtrlHeld && IsShiftHeld && ImGui::IsKeyPressed(ImGuiKey_P, false)) {
        Palette.Open(PaletteModeCommands);
    } else if (IsCtrlHeld && !IsShiftHeld && ImGui::IsKeyPressed(ImGuiKey_P, false)) {
        if (!CurrentFolderPath.empty()) {
            Palette.Open(PaletteModeQuickOpen);
        }
    } else if (IsCtrlHeld && !IsShiftHeld && ImGui::IsKeyPressed(ImGuiKey_G, false)) {
        Palette.Open(PaletteModeGoToLine);
    } else if (IsCtrlHeld && IsShiftHeld && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        SaveActiveTabAs();
    } else if (IsCtrlHeld && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        SaveActiveTab();
    } else if (IsCtrlHeld && IsShiftHeld && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        OpenFolderWithDialog();
    } else if (IsCtrlHeld && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        OpenFileWithDialog();
    } else if (IsCtrlHeld && IsShiftHeld && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        LaunchNewWindow();
    } else if (IsCtrlHeld && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        CreateNewTextFile();
    } else if (IsCtrlHeld && IsShiftHeld && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        IsZenMode = false;
        ShowSidebar = true;
        ActiveSidebarPanel = SidebarPanelSearch;
    } else if (IsCtrlHeld && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        ShowSearchPanel = !ShowSearchPanel;
        FocusSearchNextFrame = ShowSearchPanel;
        UpdateSearchHighlights();
    } else if (IsCtrlHeld && ImGui::IsKeyPressed(ImGuiKey_K, false)) {
        IsZenMode = !IsZenMode;
    } else if (ShowSearchPanel && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        ShowSearchPanel = false;
        UpdateSearchHighlights();
    } else if (ImGui::IsKeyPressed(ImGuiKey_F9, false)) {
        EditorHost.ToggleBreakpoint(EditorHost.GetCurrentLine() - OneInt);
    } else if (IsCtrlHeld && !CurrentFolderPath.empty() && ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false)) {
        ShowTerminal = !ShowTerminal;
    }

    float AvailableHeight = ImGui::GetContentRegionAvail().y;
    float WorkAreaHeight = AvailableHeight - StatusBarHeight - OuterMargin;

    ImGui::SetCursorPos(ImVec2(OuterMargin, ImGui::GetCursorPosY() + OuterMargin));

    if (!IsZenMode) {
        RenderActivityBar(WorkAreaHeight);
        ImGui::SameLine(ZeroFloat, ConsoleSpacing);

        if (ShowSidebar) {
            RenderSidebarPanel(WorkAreaHeight);
            ImGui::SameLine();

            ImGui::InvisibleButton("##SidebarResize", ImVec2(ResizeHandleWidth, WorkAreaHeight));

            if (ImGui::IsItemActive()) {
                SidebarWidth += ImGui::GetIO().MouseDelta.x;

                if (SidebarWidth < MinSidebarWidth) {
                    SidebarWidth = MinSidebarWidth;
                }
                if (SidebarWidth > MaxSidebarWidth) {
                    SidebarWidth = MaxSidebarWidth;
                }
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }

            ImGui::SameLine();
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(NormalItemSpacingX, ConsoleSpacing));
    ImGui::BeginGroup();

    float MainCenterHeight = IsZenMode ? WorkAreaHeight : (WorkAreaHeight - ConsoleHeight - ConsoleResizeHeight - (ConsoleSpacing * DoubleMultiplier));

    if (MainCenterHeight < MinCenterHeight) {
        MainCenterHeight = MinCenterHeight;
    }

    ImGui::BeginChild("EditorCentralPanel", ImVec2(ImGui::GetContentRegionAvail().x - OuterMargin, MainCenterHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Renders the Monaco-style Find & Replace bar with Case, Whole Word, Regex, and match counter.
    if (ShowSearchPanel) {
        bool SearchChanged = false;

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Find:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(SearchInputWidth);

        if (FocusSearchNextFrame) {
            ImGui::SetKeyboardFocusHere();
            FocusSearchNextFrame = false;
        }

        if (ImGui::InputText("##SearchField", SearchText, DefaultBufferCapacity)) {
            SearchChanged = true;
        }

        ImGui::SameLine();

        if (ImGui::Checkbox("Aa", &SearchMatchCase)) {
            SearchChanged = true;
        }

        ImGui::SameLine();

        if (ImGui::Checkbox("ab", &SearchWholeWord)) {
            SearchChanged = true;
        }

        ImGui::SameLine();

        if (ImGui::Checkbox(".*", &SearchUseRegex)) {
            SearchChanged = true;
        }

        if (SearchChanged) {
            UpdateSearchHighlights();
        }

        ImGui::SameLine();

        if (ImGui::Button("Prev")) {
            FindNextMatch(false);
        }

        ImGui::SameLine();

        if (ImGui::Button("Next")) {
            FindNextMatch(true);
        }

        ImGui::SameLine();

        std::string MatchLabel = std::to_string(SearchCurrentMatch) + " of " + std::to_string(SearchMatchCount);
        ImGui::Text("%s", MatchLabel.c_str());

        ImGui::SameLine();
        ImGui::Text("Replace:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(SearchInputWidth);
        ImGui::InputText("##ReplaceField", ReplaceText, DefaultBufferCapacity);
        ImGui::SameLine();

        if (ImGui::Button("Replace")) {
            ReplaceCurrentMatch();
        }

        ImGui::SameLine();

        if (ImGui::Button("All")) {
            ReplaceAllMatches();
        }

        ImGui::SameLine();

        if (ImGui::Button("X")) {
            ShowSearchPanel = false;
            UpdateSearchHighlights();
        }

        ImGui::Separator();
    }

    bool HasVisibleEditorBounds = false;
    ImVec2 ScintillaPos(ZeroFloat, ZeroFloat);
    ImVec2 ScintillaSize(ZeroFloat, ZeroFloat);
    bool HasVisibleSecondaryBounds = false;
    ImVec2 SecondaryPos(ZeroFloat, ZeroFloat);
    ImVec2 SecondarySize(ZeroFloat, ZeroFloat);

    if (ImGui::BeginTabBar("EditorTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
        for (int Index = ZeroInt; Index < static_cast<int>(OpenTabs.size()); ++Index) {
            ImGui::PushID(OpenTabs[Index].TabId);
            FileTab& Tab = OpenTabs[Index];
            std::string TabTitle = Tab.FileName + (Tab.IsModified ? " *" : "") + "###Tab_" + std::to_string(Tab.TabId);
            bool KeepOpen = true;

            ImGuiTabItemFlags TabFlags = (Tab.TabId == PendingSelectTabId) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;

            if (Tab.TabId == PendingSelectTabId) {
                PendingSelectTabId = NegativeOneInt;
            }

            bool TabOpened = ImGui::BeginTabItem(TabTitle.c_str(), &KeepOpen, TabFlags);

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                KeepOpen = false;
            }

            if (ImGui::BeginPopupContextItem("EditorTabContextMenu")) {
                if (ImGui::MenuItem("Close")) {
                    KeepOpen = false;
                }
                if (ImGui::MenuItem("Close Others")) {
                    CloseOtherTabs(Index);
                    ImGui::EndPopup();

                    if (TabOpened) {
                        ImGui::EndTabItem();
                    }

                    ImGui::PopID();
                    break;
                }
                if (ImGui::MenuItem("Close All")) {
                    CloseAllTabs();
                    ImGui::EndPopup();

                    if (TabOpened) {
                        ImGui::EndTabItem();
                    }

                    ImGui::PopID();
                    break;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Copy Path")) {
                    CopyTextToClipboard(Tab.FilePath);
                }
                if (ImGui::MenuItem("Reveal in Explorer")) {
                    RevealPathInExplorer(Tab.FilePath);
                }
                ImGui::EndPopup();
            }

            if (TabOpened) {
                ActiveTabIndex = Index;
                SyncActiveTabDocument();

                ImVec2 AvailEditorRegion = ImGui::GetContentRegionAvail();
                bool IsMarkdownSplit = (!IsSplitEditorOpen && ShowMarkdownPreview && MarkdownPreview::IsMarkdownFile(Tab.FileName));

                if (IsSplitEditorOpen) {
                    if (CurrentLayoutMode == LayoutTwoRows) {
                        float HalfPaneHeight = (AvailEditorRegion.y - SplitDividerWidth) * HalfFloat;

                        // Renders top primary Scintilla pane.
                        ImGui::BeginChild("PrimarySplitPane", ImVec2(AvailEditorRegion.x, HalfPaneHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                        if (ShowStickyScroll) {
                            StickyScrollBar::Render(EditorHost, CodeFont, AvailEditorRegion.x);
                        }

                        ImVec2 TopAvail = ImGui::GetContentRegionAvail();
                        ScintillaPos = ImGui::GetCursorScreenPos();
                        ScintillaSize = ImVec2(AvailEditorRegion.x, TopAvail.y);
                        HasVisibleEditorBounds = (ScintillaSize.x > ZeroFloat && ScintillaSize.y > ZeroFloat);
                        ImGui::Dummy(ScintillaSize);
                        ImGui::EndChild();

                        // Renders bottom secondary split Scintilla pane with Save and Close Split controls.
                        ImGui::BeginChild("SecondarySplitPane", ImVec2(AvailEditorRegion.x, HalfPaneHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(SecondaryFileName.c_str());
                        ImGui::SameLine();

                        if (ImGui::SmallButton("Save")) {
                            SaveSecondaryFile();
                        }

                        ImGui::SameLine();

                        if (ImGui::SmallButton("Flip")) {
                            FlipEditorLayout();
                        }

                        ImGui::SameLine();

                        if (ImGui::SmallButton("Close Split")) {
                            CloseSplitEditor();
                        }

                        ImGui::Separator();

                        ImVec2 BottomAvail = ImGui::GetContentRegionAvail();
                        SecondaryPos = ImGui::GetCursorScreenPos();
                        SecondarySize = ImVec2(AvailEditorRegion.x, BottomAvail.y);
                        HasVisibleSecondaryBounds = (IsSplitEditorOpen && SecondarySize.x > ZeroFloat && SecondarySize.y > ZeroFloat);
                        ImGui::Dummy(SecondarySize);
                        ImGui::EndChild();
                    } else {
                        float HalfPaneWidth = (AvailEditorRegion.x - SplitDividerWidth) * HalfFloat;

                        // Renders left primary Scintilla pane.
                        ImGui::BeginChild("PrimarySplitPane", ImVec2(HalfPaneWidth, AvailEditorRegion.y), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                        if (ShowStickyScroll) {
                            StickyScrollBar::Render(EditorHost, CodeFont, HalfPaneWidth);
                        }

                        ImVec2 LeftAvail = ImGui::GetContentRegionAvail();
                        ScintillaPos = ImGui::GetCursorScreenPos();
                        ScintillaSize = ImVec2(HalfPaneWidth, LeftAvail.y);
                        HasVisibleEditorBounds = (ScintillaSize.x > ZeroFloat && ScintillaSize.y > ZeroFloat);
                        ImGui::Dummy(ScintillaSize);
                        ImGui::EndChild();

                        ImGui::SameLine(ZeroFloat, SplitDividerWidth);

                        // Renders right secondary split Scintilla pane with Save and Close Split controls.
                        ImGui::BeginChild("SecondarySplitPane", ImVec2(HalfPaneWidth, AvailEditorRegion.y), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(SecondaryFileName.c_str());
                        ImGui::SameLine();

                        if (ImGui::SmallButton("Save")) {
                            SaveSecondaryFile();
                        }

                        ImGui::SameLine();

                        if (ImGui::SmallButton("Flip")) {
                            FlipEditorLayout();
                        }

                        ImGui::SameLine();

                        if (ImGui::SmallButton("Close Split")) {
                            CloseSplitEditor();
                        }

                        ImGui::Separator();

                        ImVec2 RightAvail = ImGui::GetContentRegionAvail();
                        SecondaryPos = ImGui::GetCursorScreenPos();
                        SecondarySize = ImVec2(HalfPaneWidth, RightAvail.y);
                        HasVisibleSecondaryBounds = (IsSplitEditorOpen && SecondarySize.x > ZeroFloat && SecondarySize.y > ZeroFloat);
                        ImGui::Dummy(SecondarySize);
                        ImGui::EndChild();
                    }
                } else if (IsMarkdownSplit) {
                    float HalfPaneWidth = (AvailEditorRegion.x - SplitDividerWidth) * HalfFloat;

                    // Renders left raw Markdown source code pane.
                    ImGui::BeginChild("MarkdownCodePane", ImVec2(HalfPaneWidth, AvailEditorRegion.y), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    ImVec2 LeftAvail = ImGui::GetContentRegionAvail();
                    ScintillaPos = ImGui::GetCursorScreenPos();
                    ScintillaSize = ImVec2(HalfPaneWidth, LeftAvail.y);
                    HasVisibleEditorBounds = (ScintillaSize.x > ZeroFloat && ScintillaSize.y > ZeroFloat);
                    ImGui::Dummy(ScintillaSize);
                    ImGui::EndChild();

                    ImGui::SameLine(ZeroFloat, SplitDividerWidth);

                    // Renders right live Markdown preview pane using cached document text.
                    if (MarkdownDirty || CachedMarkdownTabId != Tab.TabId) {
                        CachedMarkdownText = EditorHost.GetText();
                        CachedMarkdownTabId = Tab.TabId;
                        MarkdownDirty = false;
                    }

                    MarkdownPreview::Render(CachedMarkdownText, Tab.FileName, MenuBarFont, CodeFont, HalfPaneWidth, AvailEditorRegion.y);
                } else {
                    float ActiveMinimapWidth = (ShowMinimap && AvailEditorRegion.x > MinimapWidth * DoubleMultiplier) ? MinimapWidth : ZeroFloat;
                    float CodeAreaWidth = AvailEditorRegion.x - ActiveMinimapWidth;

                    if (ShowStickyScroll) {
                        StickyScrollBar::Render(EditorHost, CodeFont, CodeAreaWidth);
                    }

                    ImVec2 PostStickyAvail = ImGui::GetContentRegionAvail();
                    ScintillaPos = ImGui::GetCursorScreenPos();
                    ScintillaSize = ImVec2(CodeAreaWidth, PostStickyAvail.y);
                    HasVisibleEditorBounds = (ScintillaSize.x > ZeroFloat && ScintillaSize.y > ZeroFloat);

                    ImGui::Dummy(ScintillaSize);

                    if (ActiveMinimapWidth > ZeroFloat) {
                        ImGui::SameLine(ZeroFloat, ZeroFloat);
                        MinimapPanel::Render(EditorHost, ActiveMinimapWidth, ScintillaSize.y);
                    }
                }

                ImGui::EndTabItem();
            }

            ImGui::PopID();

            if (!KeepOpen) {
                if (Tab.IsModified) {
                    PendingCloseTabIndex = Index;
                    PendingExitAfterSave = false;
                    ShowUnsavedModal = true;
                } else {
                    CloseTabImmediate(Index);
                }
                break;
            }
        }

        ImGui::EndTabBar();
    }

    ImGui::EndChild();

    if (!IsZenMode) {
        ImGui::InvisibleButton("##ConsoleResize", ImVec2(NegativeOneFloat, ConsoleResizeHeight));

    if (ImGui::IsItemActive()) {
        ConsoleHeight -= ImGui::GetIO().MouseDelta.y;

        if (ConsoleHeight < MinConsoleHeight) {
            ConsoleHeight = MinConsoleHeight;
        }
        if (ConsoleHeight > AvailableHeight - ConsoleMaxOffset) {
            ConsoleHeight = AvailableHeight - ConsoleMaxOffset;
        }
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    ImGui::BeginChild("ConsolePanel", ImVec2(ImGui::GetContentRegionAvail().x - OuterMargin, ConsoleHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::Text("OUTPUT CONSOLE & LOGS");

    if (ImGui::BeginTabBar("ConsoleTabBar")) {
        if (ImGui::BeginTabItem("Logs")) {
            if (ImGui::Button("Clear Logs")) {
                LogMessages.clear();
            }

            ImGui::Separator();

            ImGui::BeginChild("LogScrollRegion", ImVec2(ZeroFloat, ZeroFloat), false, ImGuiWindowFlags_HorizontalScrollbar);

            for (const auto& Message : LogMessages) {
                ImGui::TextUnformatted(Message.c_str());
            }

            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(FullFloat);
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (!CurrentFolderPath.empty() && ShowTerminal) {
            if (ImGui::BeginTabItem("Terminal", &ShowTerminal)) {
                if (Terminals.empty()) {
                    CreateTerminalInstance();
                }

                if (ImGui::Button("+ New Terminal")) {
                    CreateTerminalInstance();
                }

                ImGui::SameLine();

                if (ActiveTerminalIndex >= ZeroInt && ActiveTerminalIndex < static_cast<int>(Terminals.size())) {
                    TerminalInstance& ActiveTerm = *Terminals[static_cast<size_t>(ActiveTerminalIndex)];

                    if (ImGui::Button("Clear")) {
                        ActiveTerm.Output.clear();
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Restart Shell")) {
                        StartTerminalSession(ActiveTerm);
                    }
                }

                // Renders the multi-terminal instance switcher tab bar.
                if (ImGui::BeginTabBar("TerminalInstanceTabs")) {
                    for (int TermIdx = ZeroInt; TermIdx < static_cast<int>(Terminals.size()); ++TermIdx) {
                        TerminalInstance& Term = *Terminals[static_cast<size_t>(TermIdx)];
                        ImGui::PushID(Term.InstanceId);
                        bool KeepTermOpen = true;
                        std::string TermLabel = Term.Title + "###Term_" + std::to_string(Term.InstanceId);

                        if (ImGui::BeginTabItem(TermLabel.c_str(), &KeepTermOpen)) {
                            ActiveTerminalIndex = TermIdx;
                            UpdateTerminalOutput(Term);

                            if (CodeFont != nullptr) {
                                ImGui::PushFont(CodeFont);
                            }

                            float AvailableScrollHeight = ImGui::GetContentRegionAvail().y - ReservedPromptHeight;

                            if (AvailableScrollHeight < MinScrollHeight) {
                                AvailableScrollHeight = MinScrollHeight;
                            }

                            ImGui::BeginChild("TerminalScrollRegion", ImVec2(ZeroFloat, AvailableScrollHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

                            for (const auto& OutputLine : Term.Output) {
                                RenderHighlightedTerminalLine(OutputLine);
                            }

                            if (Term.AutoScroll) {
                                ImGui::SetScrollHereY(FullFloat);
                                Term.AutoScroll = false;
                            }

                            ImGui::EndChild();
                            ImGui::Separator();

                            const ImVec4 PromptColor = ImVec4(PromptRed, PromptGreen, PromptBlue, FullFloat);
                            const std::string PromptLabel = "PS>";

                            ImGui::AlignTextToFramePadding();
                            ImGui::TextColored(PromptColor, "%s", PromptLabel.c_str());
                            ImGui::SameLine();

                            TerminalHistoryCallbackData HistoryData;
                            HistoryData.History = &Term.CommandHistory;
                            HistoryData.HistoryIndex = &Term.HistoryIndex;

                            ImGuiInputTextFlags InputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;

                            ImGui::PushItemWidth(NegativeOneFloat);

                            if (ImGui::InputText("##TerminalPromptInput", Term.InputBuffer, DefaultBufferCapacity, InputFlags, TerminalHistoryCallback, &HistoryData)) {
                                std::string InputCommand = Term.InputBuffer;
                                ExecutePowershellCommand(Term, InputCommand);
                                Term.InputBuffer[ZeroInt] = '\0';
                                ImGui::SetKeyboardFocusHere(NegativeOneInt);
                            }

                            ImGui::PopItemWidth();

                            if (CodeFont != nullptr) {
                                ImGui::PopFont();
                            }

                            ImGui::EndTabItem();
                        }

                        ImGui::PopID();

                        if (!KeepTermOpen) {
                            CloseTerminalInstance(TermIdx);
                            break;
                        }
                    }

                    ImGui::EndTabBar();
                }

                ImGui::EndTabItem();
            }
        }

        ImGui::EndTabBar();
    }

    ImGui::EndChild();
    }

    ImGui::EndGroup();
    ImGui::PopStyleVar();

    ImGui::SetCursorPos(ImVec2(ZeroFloat, ImGui::GetWindowHeight() - StatusBarHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(StatusBarPaddingX, StatusBarPaddingY));
    ImGui::BeginChild("StatusBar", ImVec2(ImGui::GetWindowWidth(), StatusBarHeight), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::Separator();
    ImGui::SetCursorPosX(StatusBarPaddingX);

    std::string StatusText = "";

    if (Git.GetIsGitRepo()) {
        StatusText += "Branch: " + Git.GetBranch() + "  |  ";
    }

    StatusText += "UTF-8  |  Spaces: " + std::to_string(TabSize);

    if (ActiveTabIndex >= ZeroInt && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
        const FileTab& ActiveTab = OpenTabs[ActiveTabIndex];
        StatusText += "  |  Ln " + std::to_string(ActiveTab.CursorLine) + ", Col " + std::to_string(ActiveTab.CursorColumn);
        StatusText += "  |  " + ActiveTab.FileName;

        if (ActiveTab.IsModified) {
            StatusText += " *";
        }
    }

    int DiagCount = Lsp.GetTotalDiagnosticCount();

    if (Lsp.GetIsRunning()) {
        StatusText += "  |  Problems: " + std::to_string(DiagCount);
    }

    ImGui::Text("%s", StatusText.c_str());
    ImGui::SameLine();
    ImGui::Text("| Tab Size: ");
    ImGui::SameLine();

    if (ImGui::SmallButton(std::to_string(TabSize).c_str())) {
        ShowTabSizeEditor = true;
    }

    if (ShowTabSizeEditor) {
        ImGuiIO& IoRef = ImGui::GetIO();
        ImVec2 CenterPosition = ImVec2(IoRef.DisplaySize.x * HalfFloat, IoRef.DisplaySize.y * HalfFloat);
        ImGui::SetNextWindowPos(CenterPosition, ImGuiCond_Appearing, ImVec2(HalfFloat, HalfFloat));

        if (ImGui::Begin("TabSizePopup", &ShowTabSizeEditor, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter tab size (spaces):");
            ImGui::InputText("##TabSizeInput", TabSizeBuffer, TabSizeBufferCapacity, ImGuiInputTextFlags_CharsDecimal);

            if (ImGui::Button("Set")) {
                if (TabSizeBuffer[ZeroInt] != '\0') {
                    TabSize = std::stoi(TabSizeBuffer);

                    if (TabSize < MinTabSize) {
                        TabSize = MinTabSize;
                    }
                    if (TabSize > MaxTabSize) {
                        TabSize = MaxTabSize;
                    }

                    EditorHost.SetTabWidth(TabSize);

                    if (IsSplitEditorOpen) {
                        SecondaryEditorHost.SetTabWidth(TabSize);
                    }

                    TabSizeBuffer[ZeroInt] = '\0';
                    ShowTabSizeEditor = false;
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                TabSizeBuffer[ZeroInt] = '\0';
                ShowTabSizeEditor = false;
            }

            ImGui::End();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Renders Command Palette (lazily populated), Diff Viewer, Explorer Action Modal, Unsaved Changes Modal, and About Modal.
    if (Palette.IsOpen()) {
        std::vector<QuickOpenEntry> QuickOpenFiles;
        std::vector<QuickSymbolEntry> QuickSymbols;
        CollectQuickOpenFiles(WorkspaceRoot, "", QuickOpenFiles);

        for (const auto& Sym : CachedOutlineSymbols) {
            QuickSymbols.push_back({ Sym.Kind, Sym.Name, Sym.LineNumber });
        }

        if (CachedPaletteCommands.empty()) {
            CachedPaletteCommands = BuildPaletteCommands();
        }

        Palette.Render(
            CachedPaletteCommands,
            QuickOpenFiles,
            QuickSymbols,
            [this](const std::string& FilePath) {
                OpenFileFromDisk(FilePath);
                EditorHost.Focus();
            },
            [this](int LineNum, int ColNum) {
                EditorHost.GoToLineColumn(LineNum, ColNum);
                EditorHost.Focus();
            },
            [this](int LineNum) {
                EditorHost.GoToLine(LineNum);
                EditorHost.Focus();
            }
        );
    }

    DiffView.Render(CodeFont);
    RenderExplorerActionModal();
    RenderUnsavedChangesModal();
    RenderAboutModal();
    RenderSettingsWindow();

    // Keeps Scintilla visible while punching exact cutout regions for ALL active ImGui popups, context menus, tooltips, and floating windows.
    bool AnyModalOpen = Palette.IsOpen() || DiffView.IsOpen() || ShowUnsavedModal || ShowAboutModal || (ActiveExplorerAction != ExplorerActionNone) || ShowTabSizeEditor;
    std::vector<RECT> EmptyCutouts;
    std::vector<RECT> MenuCutouts;
    ImGuiContext* GuiCtx = ImGui::GetCurrentContext();

    if (GuiCtx != nullptr) {
        const ImGuiWindowFlags OverlayMask = ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Modal;

        for (int WinIdx = ZeroInt; WinIdx < GuiCtx->Windows.Size; ++WinIdx) {
            ImGuiWindow* Win = GuiCtx->Windows[WinIdx];

            if (Win == nullptr || !Win->WasActive || Win->Hidden) {
                continue;
            }

            bool IsPopupOrTooltip = ((Win->Flags & OverlayMask) != ZeroInt);
            bool IsFloatingWindow = ((Win->Flags & ImGuiWindowFlags_ChildWindow) == ZeroInt && std::string(Win->Name) != "MainLayoutWindow");

            if (IsPopupOrTooltip || IsFloatingWindow) {
                RECT Cutout = {};
                Cutout.left = static_cast<LONG>(Win->Pos.x - Viewport->Pos.x);
                Cutout.top = static_cast<LONG>(Win->Pos.y - Viewport->Pos.y);
                Cutout.right = static_cast<LONG>((Win->Pos.x + Win->Size.x) - Viewport->Pos.x);
                Cutout.bottom = static_cast<LONG>((Win->Pos.y + Win->Size.y) - Viewport->Pos.y);
                MenuCutouts.push_back(Cutout);
            }
        }
    }

    if (HasVisibleEditorBounds && !AnyModalOpen) {
        int ClientLeft = static_cast<int>(ScintillaPos.x - Viewport->Pos.x);
        int ClientTop = static_cast<int>(ScintillaPos.y - Viewport->Pos.y);
        EditorHost.SetBounds(ClientLeft, ClientTop, static_cast<int>(ScintillaSize.x), static_cast<int>(ScintillaSize.y));
        EditorHost.SetClipExcludeRects(MenuCutouts);
        EditorHost.SetVisible(true);
    } else {
        EditorHost.SetClipExcludeRects(EmptyCutouts);
        EditorHost.SetVisible(false);
    }

    if (HasVisibleSecondaryBounds && !AnyModalOpen) {
        int SecLeft = static_cast<int>(SecondaryPos.x - Viewport->Pos.x);
        int SecTop = static_cast<int>(SecondaryPos.y - Viewport->Pos.y);
        SecondaryEditorHost.SetBounds(SecLeft, SecTop, static_cast<int>(SecondarySize.x), static_cast<int>(SecondarySize.y));
        SecondaryEditorHost.SetClipExcludeRects(MenuCutouts);
        SecondaryEditorHost.SetVisible(true);
    } else {
        SecondaryEditorHost.SetClipExcludeRects(EmptyCutouts);
        SecondaryEditorHost.SetVisible(false);
    }

    ImGui::End();

    if (RegularFont != nullptr) {
        ImGui::PopFont();
    }
}

// Creates and starts a new independent persistent PowerShell terminal instance.
void CodeEditor::CreateTerminalInstance() {
    const int OneOffset = 1;

    std::unique_ptr<TerminalInstance> Instance = std::make_unique<TerminalInstance>();
    Instance->InstanceId = NextTerminalId++;
    Instance->Title = "PowerShell " + std::to_string(Instance->InstanceId);
    Instance->Output.push_back("[Navigated to: " + CurrentFolderPath + "]");
    Instance->AutoScroll = true;

    StartTerminalSession(*Instance);
    Terminals.push_back(std::move(Instance));
    ActiveTerminalIndex = static_cast<int>(Terminals.size()) - OneOffset;
}

// Closes a specific terminal instance by index and cleans up its child process.
void CodeEditor::CloseTerminalInstance(int Index) {
    const int ZeroInt = 0;
    const int OneOffset = 1;

    if (Index < ZeroInt || Index >= static_cast<int>(Terminals.size())) {
        return;
    }

    StopTerminalSession(*Terminals[static_cast<size_t>(Index)]);
    Terminals.erase(Terminals.begin() + Index);

    if (ActiveTerminalIndex >= static_cast<int>(Terminals.size())) {
        ActiveTerminalIndex = static_cast<int>(Terminals.size()) - OneOffset;
    }

    if (ActiveTerminalIndex < ZeroInt) {
        ActiveTerminalIndex = ZeroInt;
    }
}

// Stops and removes all running terminal instances.
void CodeEditor::StopAllTerminals() {
    const int ZeroInt = 0;

    for (auto& TermPtr : Terminals) {
        if (TermPtr != nullptr) {
            StopTerminalSession(*TermPtr);
        }
    }

    Terminals.clear();
    ActiveTerminalIndex = ZeroInt;
}

// Terminates a specific persistent PowerShell session safely under its mutex.
void CodeEditor::StopTerminalSession(TerminalInstance& Instance) {
    const UINT ExitCodeZero = 0;

    std::lock_guard<std::mutex> Lock(Instance.Mutex);

    if (Instance.StdinWrite != nullptr) {
        CloseHandle(Instance.StdinWrite);
        Instance.StdinWrite = nullptr;
    }

    if (Instance.ProcessHandle != nullptr) {
        TerminateProcess(Instance.ProcessHandle, ExitCodeZero);
        Instance.ProcessHandle = nullptr;
    }

    Instance.IsRunning = false;
}

// Starts a persistent PowerShell child process for the specified terminal instance.
void CodeEditor::StartTerminalSession(TerminalInstance& Instance) {
    const DWORD ZeroDword = 0;
    const size_t StartPos = 0;
    const int OneOffset = 1;

    StopTerminalSession(Instance);

    SECURITY_ATTRIBUTES SecurityAttr = {};
    SecurityAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    SecurityAttr.bInheritHandle = TRUE;
    SecurityAttr.lpSecurityDescriptor = nullptr;

    HANDLE StdoutRead = nullptr;
    HANDLE StdoutWrite = nullptr;
    HANDLE StdinRead = nullptr;
    HANDLE StdinWrite = nullptr;

    if (!CreatePipe(&StdoutRead, &StdoutWrite, &SecurityAttr, ZeroDword)) {
        Instance.Output.push_back("[Error] Failed to create terminal output pipe.");

        return;
    }

    if (!CreatePipe(&StdinRead, &StdinWrite, &SecurityAttr, ZeroDword)) {
        CloseHandle(StdoutRead);
        CloseHandle(StdoutWrite);
        Instance.Output.push_back("[Error] Failed to create terminal input pipe.");

        return;
    }

    SetHandleInformation(StdoutRead, HANDLE_FLAG_INHERIT, ZeroDword);
    SetHandleInformation(StdinWrite, HANDLE_FLAG_INHERIT, ZeroDword);

    STARTUPINFOA StartupInfo = {};
    StartupInfo.cb = sizeof(STARTUPINFOA);
    StartupInfo.hStdOutput = StdoutWrite;
    StartupInfo.hStdError = StdoutWrite;
    StartupInfo.hStdInput = StdinRead;
    StartupInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    StartupInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION ProcessInfo = {};
    std::string LaunchCmd = "powershell.exe -NoLogo -NoExit -NoProfile -ExecutionPolicy Bypass -Command -";
    std::vector<char> CmdBuffer(LaunchCmd.begin(), LaunchCmd.end());
    CmdBuffer.push_back('\0');

    const char* WorkingDir = CurrentFolderPath.empty() ? nullptr : CurrentFolderPath.c_str();
    const DWORD ProcessFlags = CREATE_NO_WINDOW;

    BOOL Success = CreateProcessA(
        nullptr,
        CmdBuffer.data(),
        nullptr,
        nullptr,
        TRUE,
        ProcessFlags,
        nullptr,
        WorkingDir,
        &StartupInfo,
        &ProcessInfo
    );

    CloseHandle(StdoutWrite);
    CloseHandle(StdinRead);

    if (!Success) {
        CloseHandle(StdoutRead);
        CloseHandle(StdinWrite);
        Instance.Output.push_back("[Error] Failed to start persistent powershell.exe session.");

        return;
    }

    CloseHandle(ProcessInfo.hThread);

    {
        std::lock_guard<std::mutex> Lock(Instance.Mutex);
        Instance.ProcessHandle = ProcessInfo.hProcess;
        Instance.StdinWrite = StdinWrite;
        Instance.IsRunning = true;
    }

    TerminalInstance* InstancePtr = &Instance;

    // Spawns a background thread to stream stdout from this terminal instance.
    std::thread([InstancePtr, StdoutRead, ProcessInfo, ZeroDword, StartPos, OneOffset]() {
        constexpr int ThreadReadBufferLength = 4095;
        char ReadBuffer[ThreadReadBufferLength];
        DWORD BytesRead = ZeroDword;
        std::string AccumulatedText = "";

        while (ReadFile(StdoutRead, ReadBuffer, ThreadReadBufferLength - OneOffset, &BytesRead, nullptr) && BytesRead > ZeroDword) {
            ReadBuffer[BytesRead] = '\0';
            AccumulatedText += ReadBuffer;

            size_t NewlinePos = StartPos;

            while ((NewlinePos = AccumulatedText.find('\n')) != std::string::npos) {
                std::string Line = AccumulatedText.substr(StartPos, NewlinePos);

                if (!Line.empty() && Line.back() == '\r') {
                    Line.pop_back();
                }

                {
                    std::lock_guard<std::mutex> Lock(InstancePtr->Mutex);
                    InstancePtr->PendingOutput.push_back(Line);
                }

                AccumulatedText.erase(StartPos, NewlinePos + OneOffset);
            }
        }

        if (!AccumulatedText.empty()) {
            std::lock_guard<std::mutex> Lock(InstancePtr->Mutex);
            InstancePtr->PendingOutput.push_back(AccumulatedText);
        }

        {
            std::lock_guard<std::mutex> Lock(InstancePtr->Mutex);

            if (InstancePtr->ProcessHandle == ProcessInfo.hProcess) {
                InstancePtr->ProcessHandle = nullptr;
                InstancePtr->IsRunning = false;
            }
        }

        CloseHandle(StdoutRead);
        CloseHandle(ProcessInfo.hProcess);
    }).detach();
}

// Writes a command line into a specific terminal instance's stdin pipe.
void CodeEditor::ExecutePowershellCommand(TerminalInstance& Instance, const std::string& CommandLine) {
    const DWORD ZeroDword = 0;

    if (CommandLine.empty()) {
        Instance.Output.push_back("PS>");
        Instance.AutoScroll = true;

        return;
    }

    Instance.CommandHistory.push_back(CommandLine);
    Instance.HistoryIndex = static_cast<int>(Instance.CommandHistory.size());

    Instance.Output.push_back("PS> " + CommandLine);
    Instance.AutoScroll = true;

    if (!Instance.IsRunning) {
        StartTerminalSession(Instance);
    }

    std::string Payload = CommandLine + "\r\n";
    DWORD BytesWritten = ZeroDword;

    std::lock_guard<std::mutex> Lock(Instance.Mutex);

    if (Instance.StdinWrite != nullptr) {
        WriteFile(Instance.StdinWrite, Payload.c_str(), static_cast<DWORD>(Payload.size()), &BytesWritten, nullptr);
    } else {
        Instance.Output.push_back("[Error] Terminal session is not running.");
    }
}

// Transmits pending asynchronous output lines into a terminal instance's bounded UI buffer.
void CodeEditor::UpdateTerminalOutput(TerminalInstance& Instance) {
    const size_t MaxTerminalLines = 500;
    std::lock_guard<std::mutex> Lock(Instance.Mutex);

    if (!Instance.PendingOutput.empty()) {
        for (const auto& Line : Instance.PendingOutput) {
            Instance.Output.push_back(Line);
        }

        Instance.PendingOutput.clear();

        if (Instance.Output.size() > MaxTerminalLines) {
            size_t Excess = Instance.Output.size() - MaxTerminalLines;
            Instance.Output.erase(Instance.Output.begin(), Instance.Output.begin() + static_cast<std::ptrdiff_t>(Excess));
        }

        Instance.AutoScroll = true;
    }
}

// Renders an individual line of terminal output with syntax and status highlighting.
void CodeEditor::RenderHighlightedTerminalLine(const std::string& OutputLine) {
    const size_t PrefixPosition = 0;
    const float FullAlpha = 1;
    const float PromptRed = 0.20f;
    const float PromptGreen = 0.85f;
    const float PromptBlue = 0.40f;
    const float ErrorRed = 0.95f;
    const float ErrorGreenBlue = 0.3f;
    const float WarningRed = 0.95f;
    const float WarningGreen = 0.85f;
    const float WarningBlue = 0.20f;
    const float InfoRed = 0.35f;
    const float InfoGreen = 0.75f;
    const float TableRed = 0.60f;
    const float TableGreen = 0.75f;
    const float TableBlue = 0.95f;

    if (OutputLine.rfind("PS", PrefixPosition) == PrefixPosition) {
        const ImVec4 PromptHighlightColor = ImVec4(PromptRed, PromptGreen, PromptBlue, FullAlpha);
        ImGui::TextColored(PromptHighlightColor, "%s", OutputLine.c_str());
    } else if (OutputLine.find("[Error]") != std::string::npos || OutputLine.find("error") != std::string::npos || OutputLine.find("Error") != std::string::npos || OutputLine.find("Exception") != std::string::npos || OutputLine.find("CategoryInfo") != std::string::npos || OutputLine.rfind("+ ", PrefixPosition) == PrefixPosition) {
        const ImVec4 ErrorHighlightColor = ImVec4(ErrorRed, ErrorGreenBlue, ErrorGreenBlue, FullAlpha);
        ImGui::TextColored(ErrorHighlightColor, "%s", OutputLine.c_str());
    } else if (OutputLine.find("[Warning]") != std::string::npos || OutputLine.find("WARNING:") != std::string::npos || OutputLine.find("warning") != std::string::npos) {
        const ImVec4 WarningHighlightColor = ImVec4(WarningRed, WarningGreen, WarningBlue, FullAlpha);
        ImGui::TextColored(WarningHighlightColor, "%s", OutputLine.c_str());
    } else if (OutputLine.find("[Navigated to:") != std::string::npos || OutputLine.find("Directory:") != std::string::npos || OutputLine.find("[Vadex]") != std::string::npos) {
        const ImVec4 InfoHighlightColor = ImVec4(InfoRed, InfoGreen, FullAlpha, FullAlpha);
        ImGui::TextColored(InfoHighlightColor, "%s", OutputLine.c_str());
    } else if (OutputLine.rfind("Mode", PrefixPosition) == PrefixPosition || OutputLine.rfind("d----", PrefixPosition) == PrefixPosition || OutputLine.rfind("da---", PrefixPosition) == PrefixPosition) {
        const ImVec4 TableHighlightColor = ImVec4(TableRed, TableGreen, TableBlue, FullAlpha);
        ImGui::TextColored(TableHighlightColor, "%s", OutputLine.c_str());
    } else {
        ImGui::TextUnformatted(OutputLine.c_str());
    }
}

// Renders a cached workspace node recursively with rounded hover pills and context menus.
void CodeEditor::DrawWorkspaceNode(WorkspaceNode& Node) {
    const int ZeroIndex = 0;
    const int OneOffset = 1;
    const int StyleVarCount = 3;
    const float ZeroFloat = 0;
    const float RowRounding = 5;
    const float RowPaddingX = 5;
    const float RowPaddingY = 3;

    ImGui::PushID(Node.FullPath.c_str());
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(ZeroFloat, ZeroFloat, ZeroFloat, ZeroFloat));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, RowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(RowPaddingX, RowPaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, ZeroFloat);

    if (Node.IsDirectory) {
        ImGuiTreeNodeFlags FolderFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        bool FolderOpened = ImGui::TreeNodeEx(Node.Name.c_str(), FolderFlags);

        ImGui::PopStyleVar(StyleVarCount);
        ImGui::PopStyleColor();

        if (ImGui::BeginPopupContextItem("FolderContextMenu")) {
            if (ImGui::MenuItem("New File")) {
                ActiveExplorerAction = ExplorerActionNewFile;
                ExplorerActionTargetPath = Node.FullPath;
                ExplorerInputBuffer[ZeroIndex] = '\0';
            }
            if (ImGui::MenuItem("New Folder")) {
                ActiveExplorerAction = ExplorerActionNewFolder;
                ExplorerActionTargetPath = Node.FullPath;
                ExplorerInputBuffer[ZeroIndex] = '\0';
            }
            if (ImGui::MenuItem("Rename")) {
                ActiveExplorerAction = ExplorerActionRename;
                ExplorerActionTargetPath = Node.FullPath;
                strncpy(ExplorerInputBuffer, Node.Name.c_str(), DefaultBufferCapacity - OneOffset);
                ExplorerInputBuffer[DefaultBufferCapacity - OneOffset] = '\0';
            }
            if (ImGui::MenuItem("Delete")) {
                ActiveExplorerAction = ExplorerActionDelete;
                ExplorerActionTargetPath = Node.FullPath;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Refresh")) {
                LoadWorkspaceNodeChildren(Node);
            }
            ImGui::EndPopup();
        }

        if (FolderOpened) {
            if (!Node.IsLoaded) {
                LoadWorkspaceNodeChildren(Node);
            }

            for (auto& ChildNode : Node.Children) {
                DrawWorkspaceNode(ChildNode);
            }

            ImGui::TreePop();
        }
    } else {
        const float FullAlpha = 1;
        const float ModRed = 0.95f;
        const float ModGreen = 0.75f;
        const float ModBlue = 0.25f;
        const float NewRed = 0.35f;
        const float NewGreen = 0.85f;
        const float NewBlue = 0.45f;
        const float ActiveBgChannel = 0.21f;
        const float ActiveBgBlue = 0.25f;
        const float AccentStripeWidth = 3;
        const float AccentRed = 0.35f;
        const float AccentGreen = 0.65f;
        const float AccentBlue = 0.95f;

        bool IsActiveFile = (ActiveTabIndex >= ZeroIndex && ActiveTabIndex < static_cast<int>(OpenTabs.size()) && OpenTabs[ActiveTabIndex].FilePath == Node.FullPath);

        if (IsActiveFile) {
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(ActiveBgChannel, ActiveBgChannel, ActiveBgBlue, FullAlpha));
        }

        std::string BadgeTag;
        ImVec4 BadgeColor;
        GetFileBadge(Node.Name, BadgeTag, BadgeColor);

        char GitStat = Git.GetStatusForPath(Node.FullPath);
        bool HasGitTint = (GitStat != '\0');

        if (HasGitTint) {
            ImVec4 Tint = (GitStat == '?') ? ImVec4(NewRed, NewGreen, NewBlue, FullAlpha) : ImVec4(ModRed, ModGreen, ModBlue, FullAlpha);
            ImGui::PushStyleColor(ImGuiCol_Text, Tint);
        }

        std::string RowLabel = "[" + BadgeTag + "] " + Node.Name;

        if (GitStat == '?') {
            RowLabel += "  U";
        } else if (GitStat != '\0') {
            RowLabel += "  M";
        }

        ImGuiTreeNodeFlags FileFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        ImGui::TreeNodeEx(RowLabel.c_str(), FileFlags);

        if (IsActiveFile) {
            ImVec2 RowMin = ImGui::GetItemRectMin();
            ImVec2 RowMax = ImGui::GetItemRectMax();
            ImU32 AccentColor = ImGui::ColorConvertFloat4ToU32(ImVec4(AccentRed, AccentGreen, AccentBlue, FullAlpha));
            ImGui::GetWindowDrawList()->AddRectFilled(RowMin, ImVec2(RowMin.x + AccentStripeWidth, RowMax.y), AccentColor);
        }

        if (HasGitTint) {
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar(StyleVarCount);
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked()) {
            OpenFileFromDisk(Node.FullPath);
        }

        if (ImGui::BeginPopupContextItem("FileContextMenu")) {
            if (ImGui::MenuItem("Git Diff")) {
                OpenGitDiffForPath(Node.FullPath);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Rename")) {
                ActiveExplorerAction = ExplorerActionRename;
                ExplorerActionTargetPath = Node.FullPath;
                strncpy(ExplorerInputBuffer, Node.Name.c_str(), DefaultBufferCapacity - OneOffset);
                ExplorerInputBuffer[DefaultBufferCapacity - OneOffset] = '\0';
            }
            if (ImGui::MenuItem("Delete")) {
                ActiveExplorerAction = ExplorerActionDelete;
                ExplorerActionTargetPath = Node.FullPath;
            }
            ImGui::EndPopup();
        }
    }

    ImGui::PopID();
}

// Logs a placeholder message for unimplemented actions.
void CodeEditor::DoNothing() {
    AddLog("Working on feature...");
}
