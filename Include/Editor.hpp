#ifndef EDITOR_HPP
#define EDITOR_HPP
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include "ImGUI/imgui.h"
#include "GLFW/Include/glfw3.h"
#include "Include/Icon.hpp"
#include "Include/ScintillaHost.hpp"
#include "Include/EditorFeatures.hpp"
#include "Include/Minimap.hpp"
#include "Include/StickyScroll.hpp"
#include "Include/CommandPalette.hpp"
#include "Include/DiffViewer.hpp"
#include "Include/LspClient.hpp"
#include "Include/ConfigManager.hpp"
#include "Include/WorkspaceSearch.hpp"
#include "Include/GitManager.hpp"
#include "Include/SymbolOutline.hpp"
#include "Include/IntelliSense.hpp"
#include "Include/DebuggerClient.hpp"
#include "Include/MarkdownPreview.hpp"



const int MaxFileSize = 260;
const int DefaultBufferCapacity = 255;
const int TabSizeBufferCapacity = 10;
const int TileLeftHalf = 1;
const int TileRightHalf = 1 + 1;
const int TileTopLeftQuarter = 3;
const int TileTopRightQuarter = 5 - 1;
const int TileBottomLeftQuarter = 5;
const int TileBottomRightQuarter = 5 + 1;

// Identifies which sidebar view is currently selected in the Activity Bar.
enum SidebarPanelMode {
    SidebarPanelExplorer,
    SidebarPanelSearch,
    SidebarPanelGit,
    SidebarPanelDebug,
    SidebarPanelOutline,
    SidebarPanelSettings
};

// Identifies how split editor panes are arranged.
enum EditorLayoutMode {
    LayoutSingle,
    LayoutTwoColumns,
    LayoutTwoRows
};

// Represents a cursor position in a file for Back/Forward and Last Edit navigation.
struct NavigationLocation {
    std::string FilePath;
    int Line;
    int Column;
};

// Represents an open file tab inside the editor backed by a native Scintilla document.
struct FileTab {
    int TabId;
    std::string FilePath;
    std::string FileName;
    std::string Content;
    sptr_t ScintillaDoc;
    bool IsModified;
    bool IsReadOnly;
    int CursorLine;
    int CursorColumn;
};

// Represents one independent persistent PowerShell terminal instance.
struct TerminalInstance {
    int InstanceId;
    std::string Title;
    std::vector<std::string> Output;
    std::vector<std::string> CommandHistory;
    int HistoryIndex;
    char InputBuffer[DefaultBufferCapacity];
    std::mutex Mutex;
    std::vector<std::string> PendingOutput;
    bool AutoScroll;
    HANDLE ProcessHandle;
    HANDLE StdinWrite;
    bool IsRunning;

    // Initializes a terminal instance with safe empty handles.
    TerminalInstance() {
        const int ZeroInt = 0;

        InstanceId = ZeroInt;
        HistoryIndex = ZeroInt;
        InputBuffer[ZeroInt] = '\0';
        AutoScroll = false;
        ProcessHandle = nullptr;
        StdinWrite = nullptr;
        IsRunning = false;
    }
};

// Represents a cached file or directory entry in the workspace tree.
struct WorkspaceNode {
    std::string Name;
    std::string FullPath;
    bool IsDirectory;
    bool IsLoaded;
    std::vector<WorkspaceNode> Children;
};

// Identifies an active workspace explorer modal action.
enum ExplorerActionType {
    ExplorerActionNone,
    ExplorerActionNewFile,
    ExplorerActionNewFolder,
    ExplorerActionRename,
    ExplorerActionDelete
};

// Represents a 2D rectangle in window client coordinates.
struct TitleBarRect {
    float Left;
    float Top;
    float Right;
    float Bottom;
};

// Stores layout geometry of the custom titlebar for native hit testing.
struct TitleBarLayout {
    float TitleBarHeight;
    float MenuItemsEndX;
    TitleBarRect MinimizeRect;
    TitleBarRect MaximizeRect;
    TitleBarRect CloseRect;
    bool IsPopupOpen;
};

// Manages the code editor state, Activity Bar, split Scintilla hosts, IntelliSense, Git, Debugger, and terminals.
class CodeEditor {
private:
    std::vector<FileTab> OpenTabs;
    int ActiveTabIndex;
    int LastSyncedTabIndex;
    int PendingSelectTabId;
    int NextTabId;
    char SearchText[DefaultBufferCapacity];
    char ReplaceText[DefaultBufferCapacity];
    bool SearchMatchCase;
    bool SearchWholeWord;
    bool SearchUseRegex;
    int SearchMatchCount;
    int SearchCurrentMatch;
    std::vector<std::string> LogMessages;
    WorkspaceNode WorkspaceRoot;
    std::string CurrentFolderPath;
    bool ShowSearchPanel;
    bool FocusSearchNextFrame;
    bool ShowMinimap;
    bool ShowStickyScroll;
    bool ShowSidebar;
    bool IsZenMode;
    SidebarPanelMode ActiveSidebarPanel;
    bool ShouldExit;
    ImFont* MenuBarFont;
    ImFont* RegularFont;
    ImFont* CodeFont;
    ImFont* CaptionIconFont;
    const int FontSize = 20;
    const int CodeFontSize = 19;
    const int CaptionIconFontSize = 11;
    std::string AppDataDir;
    int TabSize;
    bool ShowTabSizeEditor;
    char TabSizeBuffer[TabSizeBufferCapacity];
    float SidebarWidth;
    float ConsoleHeight;
    bool ShowTerminal;
    std::vector<std::unique_ptr<TerminalInstance>> Terminals;
    int ActiveTerminalIndex;
    int NextTerminalId;
    GLFWwindow* AppWindow;
    Icon MinimizeIcon;
    Icon MaximizeIcon;
    Icon RestoreIcon;
    Icon CloseIcon;
    Icon ExplorerIcon;
    Icon SearchIcon;
    Icon GitIcon;
    Icon DebugIcon;
    Icon OutlineIcon;
    Icon SettingsIcon;
    std::vector<PaletteCommand> CachedPaletteCommands;
    TitleBarLayout CurrentTitleBarLayout;
    bool HasOpenMenuPopup;
    std::vector<TitleBarRect> OpenMenuPopupRects;
    bool IsMaximizeButtonHovered;
    bool IsMaximizeButtonPressed;
    bool ShowUnsavedModal;
    bool PendingExitAfterSave;
    int PendingCloseTabIndex;
    ExplorerActionType ActiveExplorerAction;
    std::string ExplorerActionTargetPath;
    char ExplorerInputBuffer[DefaultBufferCapacity];
    char WorkspaceSearchBuffer[DefaultBufferCapacity];
    char WorkspaceReplaceBuffer[DefaultBufferCapacity];
    bool WorkspaceSearchCase;
    std::vector<WorkspaceSearchMatch> WorkspaceSearchResults;
    char GitCommitMessageBuffer[DefaultBufferCapacity];
    char DebugExePathBuffer[DefaultBufferCapacity];
    std::vector<OutlineSymbol> CachedOutlineSymbols;
    int CachedOutlineTabId;
    bool AutoSaveEnabled;
    double LastEditTimestamp;
    bool PendingAutoSave;
    float ActivityIndicatorY;
    float SidebarSlideProgress;
    EditorLayoutMode CurrentLayoutMode;
    bool ShowMarkdownPreview;
    std::string CachedMarkdownText;
    int CachedMarkdownTabId;
    bool MarkdownDirty;
    bool ShowAboutModal;
    std::vector<NavigationLocation> NavHistory;
    int NavHistoryIndex;
    bool IsNavigatingHistory;
    NavigationLocation LastEditLocation;
    int NextUntitledIndex;
    EditorConfig UserConfig;
    ScintillaHost EditorHost;
    ScintillaHost SecondaryEditorHost;
    bool IsSplitEditorOpen;
    std::string SecondaryFilePath;
    std::string SecondaryFileName;
    sptr_t SecondaryDoc;
    EditorFeatures FeatureLayer;
    CommandPalette Palette;
    DiffViewer DiffView;
    LspClient Lsp;
    GitManager Git;
    DebuggerClient Debugger;

    void SaveUserConfig();
    void CreateTerminalInstance();
    void StartTerminalSession(TerminalInstance& Instance);
    void StopTerminalSession(TerminalInstance& Instance);
    void StopAllTerminals();
    void CloseTerminalInstance(int Index);
    void ExecutePowershellCommand(TerminalInstance& Instance, const std::string& CommandLine);
    void UpdateTerminalOutput(TerminalInstance& Instance);
    void RenderHighlightedTerminalLine(const std::string& OutputLine);
    void LoadWorkspaceNodeChildren(WorkspaceNode& Node);
    void DrawWorkspaceNode(WorkspaceNode& Node);
    void RenderActivityBar(float WorkAreaHeight);
    void RenderSidebarPanel(float WorkAreaHeight);
    void RenderExplorerSidebar();
    void RenderSearchSidebar();
    void RenderGitSidebar();
    void RenderDebugSidebar();
    void RenderOutlineSidebar();
    void RenderSettingsSidebar();
    void RefreshOutlineSymbols();
    void BrowseDebugExecutable();
    void StartDebuggingSession();
    void RunWithoutDebuggingSession();
    void RestartDebuggingSession();
    void PollDebuggerEvents();
    void LaunchNewWindow();
    void CreateNewTextFile();
    void OpenWelcomeTab();
    void RenderAboutModal();
    void RecordNavigationPoint();
    void NavigateBack();
    void NavigateForward();
    void NavigateToLastEdit();
    void GoToSymbolInFile();
    void GoToDefinitionOrFallback();
    void GoToDeclarationOrFallback();
    void GoToTypeDefinitionOrFallback();
    void GoToImplementationOrFallback();
    void GoToReferencesOrFallback();
    void GoToNextProblem(bool Forward);
    void GoToNextChange(bool Forward);
    void SplitCurrentEditor(EditorLayoutMode Mode);
    void FlipEditorLayout();
    void UpdateAutoSave();
    std::string GetWordUnderCaret() const;
    void JumpToWorkspaceSymbol(const std::string& SymbolName, const std::string& PreferKind);
    void CollectQuickOpenFiles(const WorkspaceNode& Node, const std::string& Prefix, std::vector<QuickOpenEntry>& OutFiles) const;
    void RenderExplorerActionModal();
    void RenderUnsavedChangesModal();
    void CloseTabImmediate(int TabIndex);
    void CloseOtherTabs(int KeepIndex);
    void CloseAllTabs();
    void SyncActiveTabDocument();
    void UpdateSearchHighlights();
    void FindNextMatch(bool Forward);
    void ReplaceCurrentMatch();
    void ReplaceAllMatches();
    void OpenCompareSideBySideDialog();
    void EnsureSecondaryHostInitialized();
    void CloseSplitEditor();
    void SaveSecondaryFile();
    void OpenGitDiffFileDialog();
    void OpenGitDiffForPath(const std::string& FilePath);
    std::string RunGitCommandCapture(const std::string& WorkingDir, const std::string& GitArgs) const;
    void PollLspResponses();
    void ApplyDiagnosticsToActiveFile();
    void TryStartLspForFile(const std::string& FilePath);
    void FocusMainImGuiWindow();
    void RevealPathInExplorer(const std::string& TargetPath) const;
    void CopyTextToClipboard(const std::string& Text) const;
    bool HandleEditorKeyDown(UINT Msg, WPARAM WParam, LPARAM LParam);
    std::string BuildTitleBreadcrumb() const;
    std::vector<PaletteCommand> BuildPaletteCommands();
    static std::string DetectLspLanguageId(const std::string& FileName);
    static void GetFileBadge(const std::string& FileName, std::string& OutTag, ImVec4& OutColor);

    // Pushes the regular non-bold font and comfortable spacing for dropdown menu contents.
    void PushDropdownFont() {
        const float DropdownItemSpacingX = 15;
        const float DropdownItemSpacingY = 7;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(DropdownItemSpacingX, DropdownItemSpacingY));

        if (RegularFont != nullptr) {
            ImGui::PushFont(RegularFont);
        }
    }

    // Pops the regular non-bold font and spacing after dropdown menu contents.
    void PopDropdownFont() {
        if (RegularFont != nullptr) {
            ImGui::PopFont();
        }

        ImGui::PopStyleVar();
    }

    // Draws the custom titlebar, menu bar, centered breadcrumb, and Chrome-style window controls.
    void DrawMenuBar() {
        const float ZeroVal = 0;
        const float FullVal = 1;
        const float HalfVal = 0.5f;
        const float MenuBarLeftMargin = 7;
        const float MenuSpacingX = 7;
        const float BreadcrumbPaddingX = 15;
        const float ControlButtonWidth = 45;
        const float ControlButtonCount = 3;
        const float HoverOverlayAlpha = 0.1f;
        const float ActiveOverlayAlpha = 0.15f;
        const float CloseHoverRed = 0.77f;
        const float CloseHoverGreen = 0.17f;
        const float CloseHoverBlue = 0.11f;
        const float CloseActiveRed = 0.69f;
        const float CloseActiveGreen = 0.15f;
        const float CloseActiveBlue = 0.1f;
        const float IconStrokeThickness = 1;
        const int ControlColorCount = 3;
        const int CloseColorCount = 1 + 1;
        const int ControlStyleVarCount = 3;
        const int MaximizedZero = 0;
        const int OneOffset = 1;

        if (MenuBarFont != nullptr) {
            ImGui::PushFont(MenuBarFont);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(MenuSpacingX, ZeroVal));

        bool AnyTopMenuOpen = false;
        HasOpenMenuPopup = false;
        OpenMenuPopupRects.clear();

        // Records each active ImGui menu or submenu popup window rect individually.
        auto RecordActivePopupRect = [&]() {
            ImVec2 PopPos = ImGui::GetWindowPos();
            ImVec2 PopSize = ImGui::GetWindowSize();
            float PopRight = PopPos.x + PopSize.x;
            float PopBottom = PopPos.y + PopSize.y;

            HasOpenMenuPopup = true;
            OpenMenuPopupRects.push_back({ PopPos.x, PopPos.y, PopRight, PopBottom });
        };

        if (ImGui::BeginMenuBar()) {
            bool HasActiveFolder = !CurrentFolderPath.empty();
            ImGui::SetCursorPosX(MenuBarLeftMargin);

            if (ImGui::BeginMenu("File")) {
                AnyTopMenuOpen = true;
                RecordActivePopupRect();
                PushDropdownFont();

                if (ImGui::MenuItem("New Text File", "Ctrl+N")) {
                    CreateNewTextFile();
                }
                if (ImGui::MenuItem("New Code File")) {
                    CreateNewTab("Untitled.cpp", "// New Code File\n");
                }
                if (ImGui::MenuItem("New Window", "Ctrl+Shift+N")) {
                    LaunchNewWindow();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Open File", "Ctrl+O")) {
                    OpenFileWithDialog();
                }
                if (ImGui::MenuItem("Open Folder", "Ctrl+Shift+O")) {
                    OpenFolderWithDialog();
                }

                if (ImGui::BeginMenu("Recent Workspaces", !UserConfig.RecentWorkspaces.empty())) {
                    RecordActivePopupRect();

                    for (int RecentIdx = MaximizedZero; RecentIdx < static_cast<int>(UserConfig.RecentWorkspaces.size()); ++RecentIdx) {
                        const std::string& RecentPath = UserConfig.RecentWorkspaces[static_cast<size_t>(RecentIdx)];
                        ImGui::PushID(RecentIdx);

                        if (ImGui::MenuItem(RecentPath.c_str())) {
                            OpenFolderFromDisk(RecentPath);
                        }

                        ImGui::PopID();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Quick Open", "Ctrl+P", false, HasActiveFolder)) {
                    Palette.Open(PaletteModeQuickOpen);
                }
                if (ImGui::MenuItem("Close Workspace", nullptr, false, HasActiveFolder)) {
                    CloseWorkspace();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Save File", "Ctrl+S")) {
                    SaveActiveTab();
                }
                if (ImGui::MenuItem("Save As", "Ctrl+Shift+S")) {
                    SaveActiveTabAs();
                }
                if (ImGui::MenuItem("Auto Save", nullptr, &AutoSaveEnabled)) {
                    SaveUserConfig();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Compare Files (Side by Side)...")) {
                    OpenCompareSideBySideDialog();
                }
                if (ImGui::MenuItem("Git Diff Viewer...")) {
                    OpenGitDiffFileDialog();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Exit")) {
                    RequestExit();
                }

                PopDropdownFont();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                AnyTopMenuOpen = true;
                RecordActivePopupRect();
                PushDropdownFont();

                bool CanUndo = EditorHost.IsReady() && (EditorHost.Call(SCI_CANUNDO) != ZeroSptr);
                bool CanRedo = EditorHost.IsReady() && (EditorHost.Call(SCI_CANREDO) != ZeroSptr);
                bool HasSel = EditorHost.IsReady() && (EditorHost.Call(SCI_GETSELECTIONSTART) != EditorHost.Call(SCI_GETSELECTIONEND));
                bool CanPaste = EditorHost.IsReady() && (EditorHost.Call(SCI_CANPASTE) != ZeroSptr);

                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo)) {
                    EditorHost.Call(SCI_UNDO);
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo)) {
                    EditorHost.Call(SCI_REDO);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Cut", "Ctrl+X", false, HasSel)) {
                    EditorHost.Call(SCI_CUT);
                }
                if (ImGui::MenuItem("Copy", "Ctrl+C", false, HasSel)) {
                    EditorHost.Call(SCI_COPY);
                }
                if (ImGui::MenuItem("Paste", "Ctrl+V", false, CanPaste)) {
                    EditorHost.Call(SCI_PASTE);
                    EditorHost.Focus();
                }
                if (ImGui::MenuItem("Delete", "Del", false, EditorHost.IsReady())) {
                    EditorHost.Call(SCI_CLEAR);
                    EditorHost.Focus();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Find & Replace", "Ctrl+F")) {
                    ShowSearchPanel = !ShowSearchPanel;
                    FocusSearchNextFrame = ShowSearchPanel;
                }
                if (ImGui::MenuItem("Find / Replace in Files", "Ctrl+Shift+F", false, HasActiveFolder)) {
                    IsZenMode = false;
                    ShowSidebar = true;
                    ActiveSidebarPanel = SidebarPanelSearch;
                    SidebarSlideProgress = ZeroVal;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Toggle Line Comment", "Ctrl+/")) {
                    if (ActiveTabIndex >= MaximizedZero && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
                        FeatureLayer.ToggleLineComment(EditorHost, OpenTabs[ActiveTabIndex].FileName);
                    }
                }
                if (ImGui::MenuItem("Toggle Block Comment", "Shift+Alt+A")) {
                    if (ActiveTabIndex >= MaximizedZero && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
                        FeatureLayer.ToggleBlockComment(EditorHost, OpenTabs[ActiveTabIndex].FileName);
                    }
                }
                if (ImGui::MenuItem("Trigger IntelliSense", "Ctrl+Space")) {
                    if (ActiveTabIndex >= MaximizedZero && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
                        IntelliSenseEngine::TriggerAutoComplete(EditorHost, OpenTabs[ActiveTabIndex].FileName, true);
                    }
                }

                PopDropdownFont();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Selection")) {
                AnyTopMenuOpen = true;
                RecordActivePopupRect();
                PushDropdownFont();

                if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                    EditorHost.Call(SCI_SELECTALL);
                }
                if (ImGui::MenuItem("Expand Selection", "Shift+Alt+Right")) {
                    FeatureLayer.ExpandSelection(EditorHost);
                }
                if (ImGui::MenuItem("Shrink Selection", "Shift+Alt+Left")) {
                    FeatureLayer.ShrinkSelection(EditorHost);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Copy Line Up", "Shift+Alt+Up")) {
                    FeatureLayer.DuplicateLines(EditorHost, true);
                    EditorHost.Focus();
                }
                if (ImGui::MenuItem("Copy Line Down", "Shift+Alt+Down")) {
                    FeatureLayer.DuplicateLines(EditorHost, false);
                    EditorHost.Focus();
                }
                if (ImGui::MenuItem("Move Line Up", "Alt+Up")) {
                    FeatureLayer.MoveLinesUp(EditorHost);
                    EditorHost.Focus();
                }
                if (ImGui::MenuItem("Move Line Down", "Alt+Down")) {
                    FeatureLayer.MoveLinesDown(EditorHost);
                    EditorHost.Focus();
                }
                if (ImGui::MenuItem("Duplicate Selection")) {
                    FeatureLayer.DuplicateSelection(EditorHost);
                    EditorHost.Focus();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Add Cursor Above", "Ctrl+Alt+Up")) {
                    FeatureLayer.AddCursorAbove(EditorHost);
                }
                if (ImGui::MenuItem("Add Cursor Below", "Ctrl+Alt+Down")) {
                    FeatureLayer.AddCursorBelow(EditorHost);
                }
                if (ImGui::MenuItem("Add Cursors to Line Ends", "Shift+Alt+I")) {
                    FeatureLayer.AddCursorsToLineEnds(EditorHost);
                }
                if (ImGui::MenuItem("Add Next Occurrence", "Ctrl+D")) {
                    FeatureLayer.AddNextOccurrence(EditorHost);
                }
                if (ImGui::MenuItem("Add Previous Occurrence")) {
                    FeatureLayer.AddPreviousOccurrence(EditorHost);
                }
                if (ImGui::MenuItem("Select All Occurrences", "Ctrl+Shift+L")) {
                    FeatureLayer.SelectAllOccurrences(EditorHost);
                }

                ImGui::Separator();

                bool ColSel = FeatureLayer.IsColumnSelectionMode();

                if (ImGui::MenuItem("Column Selection Mode", nullptr, &ColSel)) {
                    FeatureLayer.ToggleColumnSelectionMode(EditorHost);
                }

                PopDropdownFont();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                AnyTopMenuOpen = true;
                RecordActivePopupRect();
                PushDropdownFont();

                if (ImGui::MenuItem("Command Palette...", "Ctrl+Shift+P")) {
                    Palette.Open(PaletteModeCommands);
                }

                if (ImGui::BeginMenu("Open View")) {
                    RecordActivePopupRect();

                    if (ImGui::MenuItem("Explorer")) {
                        IsZenMode = false;
                        ShowSidebar = true;
                        ActiveSidebarPanel = SidebarPanelExplorer;
                        SidebarSlideProgress = ZeroVal;
                    }
                    if (ImGui::MenuItem("Search", "Ctrl+Shift+F")) {
                        IsZenMode = false;
                        ShowSidebar = true;
                        ActiveSidebarPanel = SidebarPanelSearch;
                        SidebarSlideProgress = ZeroVal;
                    }
                    if (ImGui::MenuItem("Source Control")) {
                        IsZenMode = false;
                        ShowSidebar = true;
                        ActiveSidebarPanel = SidebarPanelGit;
                        SidebarSlideProgress = ZeroVal;
                        Git.Refresh(CurrentFolderPath);
                    }
                    if (ImGui::MenuItem("Run & Debug")) {
                        IsZenMode = false;
                        ShowSidebar = true;
                        ActiveSidebarPanel = SidebarPanelDebug;
                        SidebarSlideProgress = ZeroVal;
                    }
                    if (ImGui::MenuItem("Symbol Outline")) {
                        IsZenMode = false;
                        ShowSidebar = true;
                        ActiveSidebarPanel = SidebarPanelOutline;
                        SidebarSlideProgress = ZeroVal;
                        RefreshOutlineSymbols();
                    }
                    if (ImGui::MenuItem("Settings")) {
                        IsZenMode = false;
                        ShowSidebar = true;
                        ActiveSidebarPanel = SidebarPanelSettings;
                        SidebarSlideProgress = ZeroVal;
                    }

                    ImGui::Separator();

                    ImGui::MenuItem("Terminal", "Ctrl+`", &ShowTerminal, HasActiveFolder);

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Editor Layout")) {
                    RecordActivePopupRect();

                    if (ImGui::MenuItem("Single", nullptr, CurrentLayoutMode == LayoutSingle && !IsSplitEditorOpen)) {
                        CloseSplitEditor();
                    }
                    if (ImGui::MenuItem("Two Columns (Left / Right)", nullptr, IsSplitEditorOpen && CurrentLayoutMode == LayoutTwoColumns)) {
                        SplitCurrentEditor(LayoutTwoColumns);
                    }
                    if (ImGui::MenuItem("Two Rows (Top / Bottom)", nullptr, IsSplitEditorOpen && CurrentLayoutMode == LayoutTwoRows)) {
                        SplitCurrentEditor(LayoutTwoRows);
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("Split Right (Current File)")) {
                        SplitCurrentEditor(LayoutTwoColumns);
                    }
                    if (ImGui::MenuItem("Split Down (Current File)")) {
                        SplitCurrentEditor(LayoutTwoRows);
                    }
                    if (ImGui::MenuItem("Compare Files Side by Side...")) {
                        OpenCompareSideBySideDialog();
                    }
                    if (ImGui::MenuItem("Flip Layout", nullptr, false, IsSplitEditorOpen)) {
                        FlipEditorLayout();
                    }
                    if (ImGui::MenuItem("Close Split Editor", nullptr, false, IsSplitEditorOpen)) {
                        CloseSplitEditor();
                    }

                    ImGui::EndMenu();
                }

                ImGui::Separator();

                ImGui::MenuItem("Markdown Split Preview", nullptr, &ShowMarkdownPreview);
                ImGui::MenuItem("Minimap", nullptr, &ShowMinimap);
                ImGui::MenuItem("Sticky Scroll", nullptr, &ShowStickyScroll);
                ImGui::MenuItem("Zen Mode", "Ctrl+K", &IsZenMode);

                bool WordWrap = EditorHost.IsWordWrapEnabled();

                if (ImGui::MenuItem("Word Wrap", "Alt+Z", &WordWrap)) {
                    EditorHost.ToggleWordWrap();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Zoom In", "Ctrl++")) {
                    EditorHost.ZoomIn();
                }
                if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) {
                    EditorHost.ZoomOut();
                }
                if (ImGui::MenuItem("Reset Zoom", "Ctrl+0")) {
                    EditorHost.ResetZoom();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Git Diff Active File")) {
                    if (ActiveTabIndex >= MaximizedZero && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
                        OpenGitDiffForPath(OpenTabs[ActiveTabIndex].FilePath);
                    }
                }

                PopDropdownFont();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Go")) {
                AnyTopMenuOpen = true;
                RecordActivePopupRect();
                PushDropdownFont();

                if (ImGui::MenuItem("Back", "Alt+Left", false, NavHistoryIndex > MaximizedZero)) {
                    NavigateBack();
                }
                if (ImGui::MenuItem("Forward", "Alt+Right", false, NavHistoryIndex >= MaximizedZero && NavHistoryIndex + OneOffset < static_cast<int>(NavHistory.size()))) {
                    NavigateForward();
                }
                if (ImGui::MenuItem("Last Edit Location", nullptr, false, !LastEditLocation.FilePath.empty())) {
                    NavigateToLastEdit();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Go to File...", "Ctrl+P", false, HasActiveFolder)) {
                    Palette.Open(PaletteModeQuickOpen);
                }
                if (ImGui::MenuItem("Go to Symbol...", "Ctrl+Shift+O")) {
                    GoToSymbolInFile();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Go to Definition", "F12")) {
                    GoToDefinitionOrFallback();
                }
                if (ImGui::MenuItem("Go to Declaration")) {
                    GoToDeclarationOrFallback();
                }
                if (ImGui::MenuItem("Go to Type Definition")) {
                    GoToTypeDefinitionOrFallback();
                }
                if (ImGui::MenuItem("Go to Implementations")) {
                    GoToImplementationOrFallback();
                }
                if (ImGui::MenuItem("Go to References", "Shift+F12")) {
                    GoToReferencesOrFallback();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Go to Line/Column...", "Ctrl+G")) {
                    Palette.Open(PaletteModeGoToLine);
                }
                if (ImGui::MenuItem("Go to Bracket", "Ctrl+Shift+\\")) {
                    FeatureLayer.GoToBracket(EditorHost);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Go to Next Problem", "F8")) {
                    GoToNextProblem(true);
                }
                if (ImGui::MenuItem("Go to Previous Problem", "Shift+F8")) {
                    GoToNextProblem(false);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Go to Next Change", "Alt+F5")) {
                    GoToNextChange(true);
                }
                if (ImGui::MenuItem("Go to Previous Change", "Shift+Alt+F5")) {
                    GoToNextChange(false);
                }

                PopDropdownFont();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Run")) {
                AnyTopMenuOpen = true;
                RecordActivePopupRect();
                PushDropdownFont();

                if (ImGui::MenuItem("Start Debugging", "F5")) {
                    StartDebuggingSession();
                }
                if (ImGui::MenuItem("Run Without Debugging", "Ctrl+F5")) {
                    RunWithoutDebuggingSession();
                }
                if (ImGui::MenuItem("Stop Debugging", "Shift+F5", false, Debugger.GetIsRunning())) {
                    EditorHost.ClearDebugExecutionLine();
                    Debugger.Stop();
                }
                if (ImGui::MenuItem("Restart Debugging", "Ctrl+Shift+F5", false, Debugger.GetIsRunning())) {
                    RestartDebuggingSession();
                }

                ImGui::Separator();

                bool IsDebugging = Debugger.GetIsRunning() && Debugger.GetIsDebugMode();

                if (ImGui::MenuItem("Continue", "F5", false, IsDebugging)) {
                    EditorHost.ClearDebugExecutionLine();
                    Debugger.Continue();
                }
                if (ImGui::MenuItem("Step Over", "F10", false, IsDebugging)) {
                    EditorHost.ClearDebugExecutionLine();
                    Debugger.StepOver();
                }
                if (ImGui::MenuItem("Step Into", "F11", false, IsDebugging)) {
                    EditorHost.ClearDebugExecutionLine();
                    Debugger.StepInto();
                }
                if (ImGui::MenuItem("Step Out", "Shift+F11", false, IsDebugging)) {
                    EditorHost.ClearDebugExecutionLine();
                    Debugger.StepOut();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Toggle Breakpoint", "F9")) {
                    EditorHost.ToggleBreakpoint(EditorHost.GetCurrentLine() - OneOffset);
                }
                if (ImGui::MenuItem("Clear All Breakpoints")) {
                    EditorHost.ClearAllBreakpoints();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Select Executable Target...")) {
                    BrowseDebugExecutable();
                }
                if (ImGui::MenuItem("Open Run & Debug View")) {
                    IsZenMode = false;
                    ShowSidebar = true;
                    ActiveSidebarPanel = SidebarPanelDebug;
                    SidebarSlideProgress = ZeroVal;
                }

                PopDropdownFont();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window")) {
                AnyTopMenuOpen = true;
                RecordActivePopupRect();
                PushDropdownFont();

                if (ImGui::MenuItem("Maximize", "Win+Up")) {
                    glfwMaximizeWindow(AppWindow);
                }
                if (ImGui::MenuItem("Restore", "Win+Down")) {
                    glfwRestoreWindow(AppWindow);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Tile Left Half", "Win+Left")) {
                    TileWindow(TileLeftHalf);
                }
                if (ImGui::MenuItem("Tile Right Half", "Win+Right")) {
                    TileWindow(TileRightHalf);
                }
                if (ImGui::MenuItem("Tile Top-Left Quarter")) {
                    TileWindow(TileTopLeftQuarter);
                }
                if (ImGui::MenuItem("Tile Top-Right Quarter")) {
                    TileWindow(TileTopRightQuarter);
                }
                if (ImGui::MenuItem("Tile Bottom-Left Quarter")) {
                    TileWindow(TileBottomLeftQuarter);
                }
                if (ImGui::MenuItem("Tile Bottom-Right Quarter")) {
                    TileWindow(TileBottomRightQuarter);
                }

                PopDropdownFont();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                AnyTopMenuOpen = true;
                RecordActivePopupRect();
                PushDropdownFont();

                if (ImGui::MenuItem("Welcome (Welcome.md)")) {
                    OpenWelcomeTab();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("About Vadex")) {
                    ShowAboutModal = true;
                }

                PopDropdownFont();
                ImGui::EndMenu();
            }

            // Records menu bar geometry for native Windows 11 hit testing.
            float TitleBarHeight = ImGui::GetFrameHeight();
            CurrentTitleBarLayout.TitleBarHeight = TitleBarHeight;
            CurrentTitleBarLayout.MenuItemsEndX = ImGui::GetCursorPosX();
            CurrentTitleBarLayout.IsPopupOpen = AnyTopMenuOpen;

            // Draws the centered white workspace and file breadcrumb in the draggable titlebar area.
            float WindowWidth = ImGui::GetWindowWidth();
            float ControlsTotalWidth = ControlButtonWidth * ControlButtonCount;
            float ControlsStartX = WindowWidth - ControlsTotalWidth;
            ImDrawList* DrawList = ImGui::GetWindowDrawList();

            std::string Breadcrumb = BuildTitleBreadcrumb();

            if (!Breadcrumb.empty() && RegularFont != nullptr) {
                ImVec2 TextSize = RegularFont->CalcTextSizeA(static_cast<float>(FontSize), FLT_MAX, ZeroVal, Breadcrumb.c_str());
                float CenterX = (WindowWidth - TextSize.x) * HalfVal;
                float MinAllowedX = CurrentTitleBarLayout.MenuItemsEndX + BreadcrumbPaddingX;
                float MaxAllowedX = ControlsStartX - BreadcrumbPaddingX;

                if (CenterX < MinAllowedX) {
                    CenterX = MinAllowedX;
                }

                if (CenterX + TextSize.x <= MaxAllowedX) {
                    float TextPosY = (TitleBarHeight - TextSize.y) * HalfVal;
                    ImU32 BreadcrumbColor = ImGui::GetColorU32(ImGuiCol_Text);
                    DrawList->AddText(RegularFont, static_cast<float>(FontSize), ImVec2(CenterX, TextPosY), BreadcrumbColor, Breadcrumb.c_str());
                }
            }

            // Positions Chrome-style window controls flush with the top-right edge.
            ImGui::SetCursorPos(ImVec2(ControlsStartX, ZeroVal));

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ZeroVal, ZeroVal));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ZeroVal);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ZeroVal, ZeroVal));

            ImVec4 TransparentColor = ImVec4(ZeroVal, ZeroVal, ZeroVal, ZeroVal);
            ImVec4 ControlHoverColor = ImVec4(FullVal, FullVal, FullVal, HoverOverlayAlpha);
            ImVec4 ControlActiveColor = ImVec4(FullVal, FullVal, FullVal, ActiveOverlayAlpha);

            ImGui::PushStyleColor(ImGuiCol_Button, TransparentColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ControlHoverColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ControlActiveColor);

            ImU32 IconColor = ImGui::GetColorU32(ImGuiCol_Text);
            const char* GlyphMinimize = "\xEE\xA4\xA1";
            const char* GlyphMaximize = "\xEE\xA4\xA2";
            const char* GlyphRestore = "\xEE\xA4\xA3";
            const char* GlyphClose = "\xEE\xA2\xBB";

            auto DrawCaptionGlyph = [&](const ImVec2& RectMin, const ImVec2& RectMax, const char* GlyphUtf8, const Icon& FallbackIcon) {
                if (CaptionIconFont != nullptr) {
                    float GlyphSize = static_cast<float>(CaptionIconFontSize);
                    ImVec2 TextSize = CaptionIconFont->CalcTextSizeA(GlyphSize, FLT_MAX, ZeroVal, GlyphUtf8);
                    float PosX = RectMin.x + ((RectMax.x - RectMin.x - TextSize.x) * HalfVal);
                    float PosY = RectMin.y + ((RectMax.y - RectMin.y - TextSize.y) * HalfVal);
                    DrawList->AddText(CaptionIconFont, GlyphSize, ImVec2(PosX, PosY), IconColor, GlyphUtf8);
                } else {
                    FallbackIcon.Draw(DrawList, RectMin, RectMax, IconColor, IconStrokeThickness);
                }
            };

            if (ImGui::Button("##MinimizeButton", ImVec2(ControlButtonWidth, TitleBarHeight))) {
                glfwIconifyWindow(AppWindow);
            }

            ImVec2 MinRectMin = ImGui::GetItemRectMin();
            ImVec2 MinRectMax = ImGui::GetItemRectMax();
            CurrentTitleBarLayout.MinimizeRect = { MinRectMin.x, MinRectMin.y, MinRectMax.x, MinRectMax.y };
            DrawCaptionGlyph(MinRectMin, MinRectMax, GlyphMinimize, MinimizeIcon);

            ImGui::SameLine(ZeroVal, ZeroVal);

            bool IsMaximized = (glfwGetWindowAttrib(AppWindow, GLFW_MAXIMIZED) != MaximizedZero);
            ImVec4 MaximizeBgColor = TransparentColor;

            if (IsMaximizeButtonPressed) {
                MaximizeBgColor = ControlActiveColor;
            } else if (IsMaximizeButtonHovered) {
                MaximizeBgColor = ControlHoverColor;
            }

            ImGui::PushStyleColor(ImGuiCol_Button, MaximizeBgColor);

            if (ImGui::Button("##MaximizeButton", ImVec2(ControlButtonWidth, TitleBarHeight))) {
                if (IsMaximized) {
                    glfwRestoreWindow(AppWindow);
                } else {
                    glfwMaximizeWindow(AppWindow);
                }
            }

            ImGui::PopStyleColor();

            ImVec2 MaxRectMin = ImGui::GetItemRectMin();
            ImVec2 MaxRectMax = ImGui::GetItemRectMax();
            CurrentTitleBarLayout.MaximizeRect = { MaxRectMin.x, MaxRectMin.y, MaxRectMax.x, MaxRectMax.y };

            if (IsMaximized) {
                DrawCaptionGlyph(MaxRectMin, MaxRectMax, GlyphRestore, RestoreIcon);
            } else {
                DrawCaptionGlyph(MaxRectMin, MaxRectMax, GlyphMaximize, MaximizeIcon);
            }

            ImGui::SameLine(ZeroVal, ZeroVal);

            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(CloseHoverRed, CloseHoverGreen, CloseHoverBlue, FullVal));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(CloseActiveRed, CloseActiveGreen, CloseActiveBlue, FullVal));

            if (ImGui::Button("##CloseButton", ImVec2(ControlButtonWidth, TitleBarHeight))) {
                RequestExit();
            }

            ImVec2 CloseRectMin = ImGui::GetItemRectMin();
            ImVec2 CloseRectMax = ImGui::GetItemRectMax();
            CurrentTitleBarLayout.CloseRect = { CloseRectMin.x, CloseRectMin.y, CloseRectMax.x, CloseRectMax.y };
            DrawCaptionGlyph(CloseRectMin, CloseRectMax, GlyphClose, CloseIcon);

            ImGui::PopStyleColor(CloseColorCount);
            ImGui::PopStyleColor(ControlColorCount);
            ImGui::PopStyleVar(ControlStyleVarCount);

            ImGui::EndMenuBar();
        }

        ImGui::PopStyleVar();

        if (MenuBarFont != nullptr) {
            ImGui::PopFont();
        }
    }

public:
    // Initializes all editor members with default values.
    CodeEditor() {
        const int ZeroInt = 0;
        const int NegativeOneInt = -1;
        const int DefaultTabBase = 5;
        const int OneInt = 1;
        const float ZeroFloat = 0;
        const float DefaultSidebarWidth = 250;
        const float DefaultConsoleHeight = 170;
        const float DefaultTitleBarHeight = 35;

        ActiveTabIndex = ZeroInt;
        LastSyncedTabIndex = NegativeOneInt;
        PendingSelectTabId = NegativeOneInt;
        NextTabId = OneInt;
        SearchMatchCase = false;
        SearchWholeWord = false;
        SearchUseRegex = false;
        SearchMatchCount = ZeroInt;
        SearchCurrentMatch = ZeroInt;
        ShowSearchPanel = false;
        FocusSearchNextFrame = false;
        ShowMinimap = true;
        ShowStickyScroll = true;
        ShowSidebar = true;
        IsZenMode = false;
        ActiveSidebarPanel = SidebarPanelExplorer;
        ShouldExit = false;
        MenuBarFont = nullptr;
        RegularFont = nullptr;
        CodeFont = nullptr;
        CaptionIconFont = nullptr;
        AppDataDir = ".";
        TabSize = DefaultTabBase - OneInt;
        ShowTabSizeEditor = false;
        SidebarWidth = DefaultSidebarWidth;
        ConsoleHeight = DefaultConsoleHeight;
        ShowTerminal = false;
        ActiveTerminalIndex = ZeroInt;
        NextTerminalId = OneInt;
        SearchText[ZeroInt] = '\0';
        ReplaceText[ZeroInt] = '\0';
        TabSizeBuffer[ZeroInt] = '\0';
        ExplorerInputBuffer[ZeroInt] = '\0';
        WorkspaceSearchBuffer[ZeroInt] = '\0';
        WorkspaceReplaceBuffer[ZeroInt] = '\0';
        WorkspaceSearchCase = false;
        GitCommitMessageBuffer[ZeroInt] = '\0';
        DebugExePathBuffer[ZeroInt] = '\0';
        CachedOutlineTabId = NegativeOneInt;
        AutoSaveEnabled = false;
        LastEditTimestamp = static_cast<double>(ZeroFloat);
        PendingAutoSave = false;
        ActivityIndicatorY = ZeroFloat;
        SidebarSlideProgress = static_cast<float>(OneInt);
        CurrentLayoutMode = LayoutSingle;
        ShowMarkdownPreview = true;
        CachedMarkdownTabId = NegativeOneInt;
        MarkdownDirty = true;
        ShowAboutModal = false;
        NavHistoryIndex = NegativeOneInt;
        IsNavigatingHistory = false;
        LastEditLocation = { "", OneInt, OneInt };
        NextUntitledIndex = OneInt;
        AppWindow = nullptr;
        IsMaximizeButtonHovered = false;
        IsMaximizeButtonPressed = false;
        ShowUnsavedModal = false;
        PendingExitAfterSave = false;
        PendingCloseTabIndex = NegativeOneInt;
        ActiveExplorerAction = ExplorerActionNone;
        IsSplitEditorOpen = false;
        SecondaryDoc = ZeroSptr;
        WorkspaceRoot = {};
        CurrentTitleBarLayout = {};
        CurrentTitleBarLayout.TitleBarHeight = DefaultTitleBarHeight;
        CurrentTitleBarLayout.MenuItemsEndX = ZeroFloat;
        HasOpenMenuPopup = false;
        OpenMenuPopupRects.clear();
    }

    // Saves user configuration and cleans up terminals, debugger, LSP, and Scintilla resources.
    ~CodeEditor() {
        SaveUserConfig();
        StopAllTerminals();
        Debugger.Stop();
        Lsp.Stop();

        for (auto& Tab : OpenTabs) {
            EditorHost.ReleaseDocument(Tab.ScintillaDoc);
            Tab.ScintillaDoc = ZeroSptr;
        }

        if (SecondaryDoc != ZeroSptr) {
            SecondaryEditorHost.ReleaseDocument(SecondaryDoc);
            SecondaryDoc = ZeroSptr;
        }

        SecondaryEditorHost.Shutdown();
        EditorHost.Shutdown();
    }

    void Initialize(GLFWwindow* WindowPointer);
    void CreateNewTab(const std::string& Name, const std::string& Content, bool ReadOnly = false);
    void OpenFileFromDisk(const std::string& Path);
    void OpenFolderFromDisk(const std::string& Path);
    void RefreshWorkspace();
    void CloseWorkspace();
    void SaveActiveTab();
    void SaveTabByIndex(int TabIndex);
    void Render();
    void AddLog(const std::string& LogText);
    bool GetShouldExit() const { return ShouldExit; }
    bool HasUnsavedChanges() const;
    void RequestExit();
    void HandleNativeNotify(WPARAM WParam, LPARAM LParam);
    const TitleBarLayout& GetTitleBarLayout() const { return CurrentTitleBarLayout; }
    void SetMaximizeButtonState(bool IsHovered, bool IsPressed) {
        IsMaximizeButtonHovered = IsHovered;
        IsMaximizeButtonPressed = IsPressed;
    }
    void OpenFileWithDialog();
    void OpenFolderWithDialog();
    void SaveActiveTabAs();

    // Tiles the application window to a specific screen section.
    void TileWindow(int TileType) {
        const int ZeroVal = 0;
        const int HalfDivisor = 1 + 1;

        if (AppWindow == nullptr) {
            return;
        }

        GLFWmonitor* TargetMonitor = glfwGetPrimaryMonitor();

        if (TargetMonitor == nullptr) {
            return;
        }

        int MonX = ZeroVal;
        int MonY = ZeroVal;
        int MonWidth = ZeroVal;
        int MonHeight = ZeroVal;

        glfwGetMonitorWorkarea(TargetMonitor, &MonX, &MonY, &MonWidth, &MonHeight);

        int HalfWidth = MonWidth / HalfDivisor;
        int HalfHeight = MonHeight / HalfDivisor;

        glfwRestoreWindow(AppWindow);

        if (TileType == TileLeftHalf) {
            glfwSetWindowPos(AppWindow, MonX, MonY);
            glfwSetWindowSize(AppWindow, HalfWidth, MonHeight);
        } else if (TileType == TileRightHalf) {
            glfwSetWindowPos(AppWindow, MonX + HalfWidth, MonY);
            glfwSetWindowSize(AppWindow, HalfWidth, MonHeight);
        } else if (TileType == TileTopLeftQuarter) {
            glfwSetWindowPos(AppWindow, MonX, MonY);
            glfwSetWindowSize(AppWindow, HalfWidth, HalfHeight);
        } else if (TileType == TileTopRightQuarter) {
            glfwSetWindowPos(AppWindow, MonX + HalfWidth, MonY);
            glfwSetWindowSize(AppWindow, HalfWidth, HalfHeight);
        } else if (TileType == TileBottomLeftQuarter) {
            glfwSetWindowPos(AppWindow, MonX, MonY + HalfHeight);
            glfwSetWindowSize(AppWindow, HalfWidth, HalfHeight);
        } else if (TileType == TileBottomRightQuarter) {
            glfwSetWindowPos(AppWindow, MonX + HalfWidth, MonY + HalfHeight);
            glfwSetWindowSize(AppWindow, HalfWidth, HalfHeight);
        }
    }

    // Logs a placeholder message for unimplemented actions.
    void DoNothing();
};

#endif
