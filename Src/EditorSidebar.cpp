#define GLFW_EXPOSE_NATIVE_WIN32
#include "Include/Editor.hpp"
#include "GLFW/Include/glfw3native.h"
#include <commdlg.h>
#include <filesystem>
#include <algorithm>
#include <cstring>



using namespace std::filesystem;


// Returns a short color-coded file-type badge tag and tint for the Explorer.
void CodeEditor::GetFileBadge(const std::string& FileName, std::string& OutTag, ImVec4& OutColor) {
    const float FullAlpha = 1;
    const float CppRed = 0.35f;
    const float CppGreen = 0.65f;
    const float CppBlue = 0.95f;
    const float HeaderRed = 0.75f;
    const float HeaderGreen = 0.55f;
    const float HeaderBlue = 0.95f;
    const float PyRed = 0.95f;
    const float PyGreen = 0.80f;
    const float PyBlue = 0.35f;
    const float JsRed = 0.95f;
    const float JsGreen = 0.85f;
    const float JsBlue = 0.25f;
    const float WebRed = 0.90f;
    const float WebGreen = 0.45f;
    const float WebBlue = 0.30f;
    const float MdRed = 0.45f;
    const float MdGreen = 0.85f;
    const float MdBlue = 0.75f;
    const float DefaultRgb = 0.55f;

    size_t DotPos = FileName.find_last_of('.');
    std::string Ext = (DotPos == std::string::npos) ? "" : FileName.substr(DotPos);
    std::transform(Ext.begin(), Ext.end(), Ext.begin(), [](unsigned char Ch) {
        return static_cast<char>(std::tolower(Ch));
    });

    if (Ext == ".cpp" || Ext == ".cc" || Ext == ".cxx" || Ext == ".c") {
        OutTag = "C++";
        OutColor = ImVec4(CppRed, CppGreen, CppBlue, FullAlpha);
    } else if (Ext == ".hpp" || Ext == ".h") {
        OutTag = "H";
        OutColor = ImVec4(HeaderRed, HeaderGreen, HeaderBlue, FullAlpha);
    } else if (Ext == ".py") {
        OutTag = "PY";
        OutColor = ImVec4(PyRed, PyGreen, PyBlue, FullAlpha);
    } else if (Ext == ".js" || Ext == ".jsx") {
        OutTag = "JS";
        OutColor = ImVec4(JsRed, JsGreen, JsBlue, FullAlpha);
    } else if (Ext == ".ts" || Ext == ".tsx") {
        OutTag = "TS";
        OutColor = ImVec4(CppRed, CppGreen, CppBlue, FullAlpha);
    } else if (Ext == ".json") {
        OutTag = "{}";
        OutColor = ImVec4(PyRed, PyGreen, PyBlue, FullAlpha);
    } else if (Ext == ".html" || Ext == ".htm" || Ext == ".css" || Ext == ".svg") {
        OutTag = "<>";
        OutColor = ImVec4(WebRed, WebGreen, WebBlue, FullAlpha);
    } else if (Ext == ".md") {
        OutTag = "MD";
        OutColor = ImVec4(MdRed, MdGreen, MdBlue, FullAlpha);
    } else if (ImageViewer::IsImageFile(FileName)) {
        OutTag = "IMG";
        OutColor = ImVec4(HeaderRed, HeaderGreen, HeaderBlue, FullAlpha);
    } else if (FileName == "Makefile" || FileName == "makefile") {
        OutTag = "MK";
        OutColor = ImVec4(WebRed, WebGreen, WebBlue, FullAlpha);
    } else {
        OutTag = "TX";
        OutColor = ImVec4(DefaultRgb, DefaultRgb, DefaultRgb, FullAlpha);
    }
}

// Renders the slim vertical Activity Bar beside the sidebar with a smooth sliding indicator animation.
void CodeEditor::RenderActivityBar(float WorkAreaHeight) {
    const float ZeroFloat = 0;
    const float FullFloat = 1;
    const float ActivityBarWidth = 45;
    const float ButtonSize = 45;
    const float IconDrawSize = 19;
    const float IconStroke = 1.5f;
    const float AccentStripeWidth = 3;
    const float AccentRed = 0.35f;
    const float AccentGreen = 0.65f;
    const float AccentBlue = 0.95f;
    const float ActivePillAlpha = 0.13f;
    const float InactiveAlpha = 0.55f;
    const float HoverAlpha = 0.1f;
    const float SlideLerpSpeed = 15;
    const float MaxFrameStep = 0.1f;
    const int ColorPushCount = 3;
    const int StylePushCount = 3;
    const int IndexExplorer = 0;
    const int IndexSearch = 1;
    const int IndexGit = 1 + 1;
    const int IndexDebug = 3;
    const int IndexOutline = 5 - 1;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ZeroFloat, ZeroFloat));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ZeroFloat, ZeroFloat));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ZeroFloat);
    ImGui::BeginChild("ActivityBarPanel", ImVec2(ActivityBarWidth, WorkAreaHeight), true, ImGuiWindowFlags_NoScrollbar);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 PanelOrigin = ImGui::GetWindowPos();
    ImU32 ActiveIconColor = ImGui::GetColorU32(ImGuiCol_Text);
    ImU32 InactiveIconColor = ImGui::ColorConvertFloat4ToU32(ImVec4(FullFloat, FullFloat, FullFloat, InactiveAlpha));
    ImU32 AccentStripeColor = ImGui::ColorConvertFloat4ToU32(ImVec4(AccentRed, AccentGreen, AccentBlue, FullFloat));
    ImU32 ActivePillColor = ImGui::ColorConvertFloat4ToU32(ImVec4(AccentRed, AccentGreen, AccentBlue, ActivePillAlpha));

    int TargetButtonIndex = IndexExplorer;

    if (ActiveSidebarPanel == SidebarPanelSearch) {
        TargetButtonIndex = IndexSearch;
    } else if (ActiveSidebarPanel == SidebarPanelGit) {
        TargetButtonIndex = IndexGit;
    } else if (ActiveSidebarPanel == SidebarPanelDebug) {
        TargetButtonIndex = IndexDebug;
    } else if (ActiveSidebarPanel == SidebarPanelOutline) {
        TargetButtonIndex = IndexOutline;
    }

    float TargetY = static_cast<float>(TargetButtonIndex) * ButtonSize;
    float DeltaTime = ImGui::GetIO().DeltaTime;

    if (DeltaTime > MaxFrameStep) {
        DeltaTime = MaxFrameStep;
    }

    float BlendFactor = DeltaTime * SlideLerpSpeed;

    if (BlendFactor > FullFloat) {
        BlendFactor = FullFloat;
    }

    ActivityIndicatorY += (TargetY - ActivityIndicatorY) * BlendFactor;

    if (ShowSidebar) {
        ImVec2 PillMin(PanelOrigin.x, PanelOrigin.y + ActivityIndicatorY);
        ImVec2 PillMax(PanelOrigin.x + ActivityBarWidth, PanelOrigin.y + ActivityIndicatorY + ButtonSize);
        DrawList->AddRectFilled(PillMin, PillMax, ActivePillColor);
        DrawList->AddRectFilled(PillMin, ImVec2(PillMin.x + AccentStripeWidth, PillMax.y), AccentStripeColor);
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ZeroFloat, ZeroFloat, ZeroFloat, ZeroFloat));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(FullFloat, FullFloat, FullFloat, HoverAlpha));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(FullFloat, FullFloat, FullFloat, HoverAlpha));

    auto DrawActivityButton = [&](const char* Id, const char* Tooltip, SidebarPanelMode Mode, const Icon& Glyph) {
        bool IsSelected = (ShowSidebar && ActiveSidebarPanel == Mode);

        if (ImGui::Button(Id, ImVec2(ButtonSize, ButtonSize))) {
            if (ShowSidebar && ActiveSidebarPanel == Mode) {
                ShowSidebar = false;
            } else {
                ShowSidebar = true;
                ActiveSidebarPanel = Mode;
                SidebarSlideProgress = ZeroFloat;

                if (Mode == SidebarPanelGit) {
                    Git.Refresh(CurrentFolderPath);
                } else if (Mode == SidebarPanelOutline) {
                    RefreshOutlineSymbols();
                }
            }
        }

        ImVec2 BtnMin = ImGui::GetItemRectMin();
        ImVec2 BtnMax = ImGui::GetItemRectMax();

        Glyph.Draw(DrawList, BtnMin, BtnMax, IsSelected ? ActiveIconColor : InactiveIconColor, IconStroke, IconDrawSize);

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", Tooltip);
        }
    };

    DrawActivityButton("##ActExplorer", "Explorer", SidebarPanelExplorer, ExplorerIcon);
    DrawActivityButton("##ActSearch", "Code Search (Ctrl+Shift+F)", SidebarPanelSearch, SearchIcon);
    DrawActivityButton("##ActGit", "Source Control (Git)", SidebarPanelGit, GitIcon);
    DrawActivityButton("##ActDebug", "Run & Debug (F5)", SidebarPanelDebug, DebugIcon);
    DrawActivityButton("##ActOutline", "Symbol Outline", SidebarPanelOutline, OutlineIcon);

    float SettingsPosY = WorkAreaHeight - ButtonSize;

    if (ImGui::GetCursorPosY() < SettingsPosY) {
        ImGui::SetCursorPosY(SettingsPosY);
    }

    if (ImGui::Button("##ActSettings", ImVec2(ButtonSize, ButtonSize))) {
        ShowSettingsWindow = !ShowSettingsWindow;
    }

    ImVec2 SettingsMin = ImGui::GetItemRectMin();
    ImVec2 SettingsMax = ImGui::GetItemRectMax();

    if (ShowSettingsWindow) {
        DrawList->AddRectFilled(SettingsMin, SettingsMax, ActivePillColor);
        DrawList->AddRectFilled(SettingsMin, ImVec2(SettingsMin.x + AccentStripeWidth, SettingsMax.y), AccentStripeColor);
    }

    SettingsIcon.Draw(DrawList, SettingsMin, SettingsMax, ShowSettingsWindow ? ActiveIconColor : InactiveIconColor, IconStroke, IconDrawSize);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Settings");
    }

    ImGui::PopStyleColor(ColorPushCount);
    ImGui::EndChild();
    ImGui::PopStyleVar(StylePushCount);
}

// Dispatches rendering of the active sidebar panel view with a smooth horizontal slide-in transition.
void CodeEditor::RenderSidebarPanel(float WorkAreaHeight) {
    const float FullFloat = 1;
    const float SlideSpeed = 11;
    const float MaxStep = 0.1f;
    const float SlideDistance = 25;
    const float SnapThreshold = 0.01f;
    const float MinSlideOffset = 0.1f;

    float DeltaTime = ImGui::GetIO().DeltaTime;

    if (DeltaTime > MaxStep) {
        DeltaTime = MaxStep;
    }

    if (SidebarSlideProgress < FullFloat) {
        SidebarSlideProgress += (FullFloat - SidebarSlideProgress) * (DeltaTime * SlideSpeed);

        if (FullFloat - SidebarSlideProgress < SnapThreshold) {
            SidebarSlideProgress = FullFloat;
        }
    }

    ImGui::BeginChild("SidebarPanel", ImVec2(SidebarWidth, WorkAreaHeight), true);

    float SlideOffset = (FullFloat - SidebarSlideProgress) * SlideDistance;

    if (SlideOffset > MinSlideOffset) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SlideOffset);
    }

    ImGui::BeginGroup();

    if (ActiveSidebarPanel == SidebarPanelExplorer) {
        RenderExplorerSidebar();
    } else if (ActiveSidebarPanel == SidebarPanelSearch) {
        RenderSearchSidebar();
    } else if (ActiveSidebarPanel == SidebarPanelGit) {
        RenderGitSidebar();
    } else if (ActiveSidebarPanel == SidebarPanelDebug) {
        RenderDebugSidebar();
    } else if (ActiveSidebarPanel == SidebarPanelOutline) {
        RenderOutlineSidebar();
    }

    ImGui::EndGroup();
    ImGui::EndChild();
}

// Renders the Workspace Explorer tree panel.
void CodeEditor::RenderExplorerSidebar() {
    const int ZeroInt = 0;

    ImGui::Text("WORKSPACE EXPLORER");
    ImGui::Separator();

    if (CurrentFolderPath.empty()) {
        ImGui::Text("No folder open.");

        return;
    }

    ImGui::TextUnformatted(WorkspaceRoot.Name.c_str());
    ImGui::SameLine();

    if (ImGui::SmallButton("+##RootAddBtn")) {
        ImGui::OpenPopup("RootAddMenu");
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("New File or Folder");
    }

    if (ImGui::BeginPopup("RootAddMenu")) {
        if (ImGui::MenuItem("New File")) {
            ActiveExplorerAction = ExplorerActionNewFile;
            ExplorerActionTargetPath = CurrentFolderPath;
            ExplorerInputBuffer[ZeroInt] = '\0';
        }
        if (ImGui::MenuItem("New Folder")) {
            ActiveExplorerAction = ExplorerActionNewFolder;
            ExplorerActionTargetPath = CurrentFolderPath;
            ExplorerInputBuffer[ZeroInt] = '\0';
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    if (WorkspaceRoot.Children.empty()) {
        ImGui::Text("Folder is empty.");
    } else {
        for (auto& ChildNode : WorkspaceRoot.Children) {
            DrawWorkspaceNode(ChildNode);
        }
    }

    if (ImGui::BeginPopupContextWindow("SidebarEmptyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("New File")) {
            ActiveExplorerAction = ExplorerActionNewFile;
            ExplorerActionTargetPath = CurrentFolderPath;
            ExplorerInputBuffer[ZeroInt] = '\0';
        }
        if (ImGui::MenuItem("New Folder")) {
            ActiveExplorerAction = ExplorerActionNewFolder;
            ExplorerActionTargetPath = CurrentFolderPath;
            ExplorerInputBuffer[ZeroInt] = '\0';
        }
        if (ImGui::MenuItem("Refresh")) {
            RefreshWorkspace();
        }
        ImGui::EndPopup();
    }
}

// Renders the Workspace-Wide Code Search sidebar panel (Ctrl+Shift+F).
void CodeEditor::RenderSearchSidebar() {
    const float FullWidth = -1;
    const float ZeroFloat = 0;
    const float LineNumChannel = 0.55f;
    const float FullAlpha = 1;
    const int ZeroInt = 0;

    ImGui::Text("CODE SEARCH");
    ImGui::Separator();

    if (CurrentFolderPath.empty()) {
        ImGui::Text("Open a folder to search.");

        return;
    }

    ImGui::SetNextItemWidth(FullWidth);
    bool SubmitSearch = ImGui::InputTextWithHint("##WorkspaceSearchInput", "Search across files...", WorkspaceSearchBuffer, DefaultBufferCapacity, ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::SetNextItemWidth(FullWidth);
    ImGui::InputTextWithHint("##WorkspaceReplaceInput", "Replace across files...", WorkspaceReplaceBuffer, DefaultBufferCapacity);

    ImGui::Checkbox("Match Case (Aa)", &WorkspaceSearchCase);

    if (ImGui::Button("Search", ImVec2(FullWidth, ZeroFloat)) || SubmitSearch) {
        WorkspaceSearchResults = WorkspaceSearch::Search(CurrentFolderPath, WorkspaceSearchBuffer, WorkspaceSearchCase);
    }

    if (ImGui::Button("Replace All in Files", ImVec2(FullWidth, ZeroFloat))) {
        int FilesChanged = ZeroInt;
        int ReplacedCount = WorkspaceSearch::ReplaceAll(CurrentFolderPath, WorkspaceSearchBuffer, WorkspaceReplaceBuffer, WorkspaceSearchCase, FilesChanged);
        AddLog("Replaced " + std::to_string(ReplacedCount) + " occurrence(s) across " + std::to_string(FilesChanged) + " file(s).");
        WorkspaceSearchResults = WorkspaceSearch::Search(CurrentFolderPath, WorkspaceSearchBuffer, WorkspaceSearchCase);
    }

    ImGui::Text("Results: %d", static_cast<int>(WorkspaceSearchResults.size()));
    ImGui::Separator();

    ImGui::BeginChild("SearchResultsScroll", ImVec2(ZeroFloat, ZeroFloat), false);

    for (int Idx = ZeroInt; Idx < static_cast<int>(WorkspaceSearchResults.size()); ++Idx) {
        const WorkspaceSearchMatch& Match = WorkspaceSearchResults[static_cast<size_t>(Idx)];
        ImGui::PushID(Idx);
        std::string Label = Match.RelativePath + ":" + std::to_string(Match.LineNumber);

        if (ImGui::Selectable(Label.c_str())) {
            OpenFileFromDisk(Match.FilePath);
            EditorHost.GoToLine(Match.LineNumber);
            EditorHost.Focus();
        }

        ImGui::TextColored(ImVec4(LineNumChannel, LineNumChannel, LineNumChannel, FullAlpha), "  %s", Match.LinePreview.c_str());
        ImGui::PopID();
    }

    ImGui::EndChild();
}

// Renders the Source Control (Git) sidebar panel with Stage, Unstage, Commit, and Diff actions.
void CodeEditor::RenderGitSidebar() {
    const float FullWidth = -1;
    const float ZeroFloat = 0;
    const float FullAlpha = 1;
    const float ModifiedRed = 0.95f;
    const float ModifiedGreen = 0.75f;
    const float ModifiedBlue = 0.25f;
    const float UntrackedRed = 0.35f;
    const float UntrackedGreen = 0.85f;
    const float UntrackedBlue = 0.45f;
    const int ZeroInt = 0;

    ImGui::Text("SOURCE CONTROL");
    ImGui::Separator();

    if (CurrentFolderPath.empty()) {
        ImGui::Text("No folder open.");

        return;
    }

    if (ImGui::Button("Refresh")) {
        Git.Refresh(CurrentFolderPath);
    }

    if (!Git.GetIsGitRepo()) {
        ImGui::Text("Not a Git repository.");

        return;
    }

    ImGui::SameLine();
    ImGui::Text("Branch: %s", Git.GetBranch().c_str());

    ImGui::SetNextItemWidth(FullWidth);
    bool SubmitCommit = ImGui::InputTextWithHint("##GitCommitMsg", "Commit message...", GitCommitMessageBuffer, DefaultBufferCapacity, ImGuiInputTextFlags_EnterReturnsTrue);

    const float HalfFactor = 0.5f;
    const float DoubleFactor = 1 + 1;
    const float ThirdDivisor = 3;
    float AvailW = ImGui::GetContentRegionAvail().x;
    float SpacingX = ImGui::GetStyle().ItemSpacing.x;
    float TwoBtnW = (AvailW - SpacingX) * HalfFactor;
    float ThreeBtnW = (AvailW - (SpacingX * DoubleFactor)) / ThirdDivisor;

    if (ImGui::Button("Stage All", ImVec2(TwoBtnW, ZeroFloat))) {
        Git.StageAll(CurrentFolderPath);
    }

    ImGui::SameLine();

    if (ImGui::Button("Commit", ImVec2(TwoBtnW, ZeroFloat)) || SubmitCommit) {
        std::string CommitOut = Git.Commit(CurrentFolderPath, GitCommitMessageBuffer);
        AddLog("Git Commit: " + CommitOut);

        if (CommitOut.find("Enter a commit message") == std::string::npos) {
            GitCommitMessageBuffer[ZeroInt] = '\0';
        }
    }

    if (ImGui::Button("Fetch", ImVec2(ThreeBtnW, ZeroFloat))) {
        std::string Out = Git.Fetch(CurrentFolderPath);
        AddLog("Git Fetch: " + Out);
    }

    ImGui::SameLine();

    if (ImGui::Button("Pull", ImVec2(ThreeBtnW, ZeroFloat))) {
        std::string Out = Git.Pull(CurrentFolderPath);
        AddLog("Git Pull: " + Out);
    }

    ImGui::SameLine();

    if (ImGui::Button("Push", ImVec2(ThreeBtnW, ZeroFloat))) {
        std::string Out = Git.Push(CurrentFolderPath);
        AddLog("Git Push: " + Out);
    }

    if (!Git.GetChangedFiles().empty()) {
        if (ImGui::Button("Discard All Changes", ImVec2(FullWidth, ZeroFloat))) {
            ShowDiscardAllGitModal = true;
        }
    }

    if (ShowDiscardAllGitModal) {
        const float HalfFloat = 0.5f;

        ImGui::OpenPopup("Discard All Git Changes");

        ImGuiIO& IoRef = ImGui::GetIO();
        ImVec2 CenterPos(IoRef.DisplaySize.x * HalfFloat, IoRef.DisplaySize.y * HalfFloat);
        ImGui::SetNextWindowPos(CenterPos, ImGuiCond_Appearing, ImVec2(HalfFloat, HalfFloat));

        if (ImGui::BeginPopupModal("Discard All Git Changes", &ShowDiscardAllGitModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Discard all uncommitted changes in this repository?");
            ImGui::TextDisabled("This will revert modified files and delete untracked files.");
            ImGui::Separator();

            if (ImGui::Button("Discard All")) {
                std::vector<GitFileStatus> PrevChanges = Git.GetChangedFiles();
                std::string Out = Git.DiscardAllChanges(CurrentFolderPath);
                AddLog("Git Discard All: " + Out);
                RefreshWorkspace();

                for (const auto& Entry : PrevChanges) {
                    ReloadOpenTabFromDisk(Entry.FullPath);
                }

                ShowDiscardAllGitModal = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                ShowDiscardAllGitModal = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    if (!Git.GetLastStatusMessage().empty()) {
        ImGui::TextWrapped("%s", Git.GetLastStatusMessage().c_str());
    }

    ImGui::Separator();

    const std::vector<GitFileStatus>& Changes = Git.GetChangedFiles();

    if (Changes.empty()) {
        ImGui::Text("Working tree clean.");

        return;
    }

    ImGui::BeginChild("GitChangesScroll", ImVec2(ZeroFloat, ZeroFloat), false);

    for (int Idx = ZeroInt; Idx < static_cast<int>(Changes.size()); ++Idx) {
        const GitFileStatus& Item = Changes[static_cast<size_t>(Idx)];
        ImGui::PushID(Idx);

        bool IsUntracked = (Item.IndexStatus == '?');
        ImVec4 StatusColor = IsUntracked ? ImVec4(UntrackedRed, UntrackedGreen, UntrackedBlue, FullAlpha) : ImVec4(ModifiedRed, ModifiedGreen, ModifiedBlue, FullAlpha);
        std::string Tag = IsUntracked ? "[U]" : (Item.IsStaged ? "[S]" : "[M]");

        ImGui::TextColored(StatusColor, "%s", Tag.c_str());
        ImGui::SameLine();

        if (ImGui::Selectable(Item.RelativePath.c_str(), false)) {
            OpenGitDiffForPath(Item.FullPath);
        }

        if (ImGui::BeginPopupContextItem("GitItemContext")) {
            if (ImGui::MenuItem("Open Git Diff")) {
                OpenGitDiffForPath(Item.FullPath);
            }
            if (ImGui::MenuItem("Open File")) {
                OpenFileFromDisk(Item.FullPath);
            }
            if (!Item.IsStaged && ImGui::MenuItem("Stage File")) {
                Git.StageFile(CurrentFolderPath, Item.RelativePath);
            }
            if (Item.IsStaged && ImGui::MenuItem("Unstage File")) {
                Git.UnstageFile(CurrentFolderPath, Item.RelativePath);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Discard Changes")) {
                std::string TargetFullPath = Item.FullPath;
                std::string Out = Git.DiscardFile(CurrentFolderPath, Item.RelativePath, IsUntracked);
                AddLog("Git Discard: " + Out);
                RefreshWorkspace();
                ReloadOpenTabFromDisk(TargetFullPath);
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
}

// Opens a file picker to select an executable target for the GDB debugger.
void CodeEditor::BrowseDebugExecutable() {
    const DWORD DefaultFilterIndex = 1;
    const int OneOffset = 1;

    HWND OwnerHwnd = (AppWindow != nullptr) ? glfwGetWin32Window(AppWindow) : nullptr;
    char ExeFileBuf[MaxFileSize] = {};
    OPENFILENAMEA Dlg = {};
    Dlg.lStructSize = sizeof(Dlg);
    Dlg.hwndOwner = OwnerHwnd;
    Dlg.lpstrFile = ExeFileBuf;
    Dlg.nMaxFile = MaxFileSize;
    Dlg.lpstrFilter = "Executables\0*.exe\0All Files\0*.*\0\0";
    Dlg.nFilterIndex = DefaultFilterIndex;
    Dlg.lpstrTitle = "Select Executable to Debug";
    Dlg.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&Dlg)) {
        strncpy(DebugExePathBuffer, ExeFileBuf, DefaultBufferCapacity - OneOffset);
        DebugExePathBuffer[DefaultBufferCapacity - OneOffset] = '\0';
    }
}

// Collects breakpoints from the active editor and starts a GDB MI2 debugging session.
void CodeEditor::StartDebuggingSession() {
    const int ZeroInt = 0;

    if (Debugger.GetIsRunning() && Debugger.GetIsPaused()) {
        EditorHost.ClearDebugExecutionLine();
        Debugger.Continue();

        return;
    }

    if (DebugExePathBuffer[ZeroInt] == '\0') {
        BrowseDebugExecutable();

        if (DebugExePathBuffer[ZeroInt] == '\0') {
            return;
        }
    }

    std::vector<DebugBreakpoint> Breakpoints;

    if (ActiveTabIndex >= ZeroInt && ActiveTabIndex < static_cast<int>(OpenTabs.size())) {
        std::vector<int> BpLines = EditorHost.GetBreakpointLines();

        for (int LineNum : BpLines) {
            Breakpoints.push_back({ OpenTabs[ActiveTabIndex].FilePath, LineNum });
        }
    }

    if (Debugger.Start(DebugExePathBuffer, CurrentFolderPath, Breakpoints)) {
        AddLog("Started GDB session: " + std::string(DebugExePathBuffer));
    }
}

// Runs the selected executable target without attaching the GDB debugger.
void CodeEditor::RunWithoutDebuggingSession() {
    const int ZeroInt = 0;

    if (DebugExePathBuffer[ZeroInt] == '\0') {
        BrowseDebugExecutable();
    }

    if (DebugExePathBuffer[ZeroInt] != '\0') {
        Debugger.RunWithoutDebugging(DebugExePathBuffer, CurrentFolderPath);
        AddLog("Running target: " + std::string(DebugExePathBuffer));
    }
}

// Restarts the active debugging session from the beginning.
void CodeEditor::RestartDebuggingSession() {
    EditorHost.ClearDebugExecutionLine();
    Debugger.Stop();
    StartDebuggingSession();
}

// Polls GDB stop notifications to jump Scintilla to the paused execution line.
void CodeEditor::PollDebuggerEvents() {
    const int ZeroInt = 0;

    std::string StopFile;
    int StopLine = ZeroInt;

    if (Debugger.PopStopLocation(StopFile, StopLine)) {
        if (exists(StopFile)) {
            OpenFileFromDisk(StopFile);
            EditorHost.SetDebugExecutionLine(StopLine);
        }
    }
}

// Renders the integrated GDB Run & Debug sidebar panel with responsive non-overflowing buttons.
void CodeEditor::RenderDebugSidebar() {
    const float FullWidth = -1;
    const float ZeroFloat = 0;
    const float HalfFactor = 0.5f;
    const float ThirdDivisor = 3;
    const float StackHeight = 100;
    const float LocalsHeight = 100;
    const int ZeroInt = 0;

    ImGui::Text("RUN & DEBUG");
    ImGui::Separator();

    ImGui::SetNextItemWidth(FullWidth);
    ImGui::InputTextWithHint("##DebugExeInput", "Path to .exe target...", DebugExePathBuffer, DefaultBufferCapacity);

    const float DoubleFactor = 1 + 1;
    float AvailW = ImGui::GetContentRegionAvail().x;
    float SpacingX = ImGui::GetStyle().ItemSpacing.x;
    float HalfBtnW = (AvailW - SpacingX) * HalfFactor;
    float ThirdBtnW = (AvailW - (SpacingX * DoubleFactor)) / ThirdDivisor;

    if (ImGui::Button("Browse...", ImVec2(HalfBtnW, ZeroFloat))) {
        BrowseDebugExecutable();
    }

    ImGui::SameLine();

    if (ImGui::Button("Run", ImVec2(HalfBtnW, ZeroFloat))) {
        if (DebugExePathBuffer[ZeroInt] == '\0') {
            BrowseDebugExecutable();
        }
        if (DebugExePathBuffer[ZeroInt] != '\0') {
            Debugger.RunWithoutDebugging(DebugExePathBuffer, CurrentFolderPath);
        }
    }

    if (ImGui::Button(Debugger.GetIsRunning() ? "Continue" : "Debug (F5)", ImVec2(HalfBtnW, ZeroFloat))) {
        StartDebuggingSession();
    }

    ImGui::SameLine();

    if (ImGui::Button("Stop", ImVec2(HalfBtnW, ZeroFloat))) {
        EditorHost.ClearDebugExecutionLine();
        Debugger.Stop();
    }

    if (Debugger.GetIsRunning() && Debugger.GetIsDebugMode()) {
        if (ImGui::Button("Over", ImVec2(ThirdBtnW, ZeroFloat))) {
            EditorHost.ClearDebugExecutionLine();
            Debugger.StepOver();
        }

        ImGui::SameLine();

        if (ImGui::Button("Into", ImVec2(ThirdBtnW, ZeroFloat))) {
            EditorHost.ClearDebugExecutionLine();
            Debugger.StepInto();
        }

        ImGui::SameLine();

        if (ImGui::Button("Out", ImVec2(ThirdBtnW, ZeroFloat))) {
            EditorHost.ClearDebugExecutionLine();
            Debugger.StepOut();
        }
    }

    ImGui::Separator();
    ImGui::Text("CALL STACK");
    ImGui::BeginChild("DebugCallStack", ImVec2(ZeroFloat, StackHeight), true);

    std::vector<DebugStackFrame> Frames = Debugger.GetCallStack();

    for (int Idx = ZeroInt; Idx < static_cast<int>(Frames.size()); ++Idx) {
        const DebugStackFrame& Frame = Frames[static_cast<size_t>(Idx)];
        ImGui::PushID(Idx);
        std::string FrameLabel = Frame.FunctionName + " :" + std::to_string(Frame.LineNumber);

        if (ImGui::Selectable(FrameLabel.c_str()) && !Frame.FilePath.empty() && exists(Frame.FilePath)) {
            OpenFileFromDisk(Frame.FilePath);
            EditorHost.GoToLine(Frame.LineNumber);
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::Text("LOCALS");
    ImGui::BeginChild("DebugLocals", ImVec2(ZeroFloat, LocalsHeight), true);

    std::vector<DebugVariable> Locals = Debugger.GetLocals();

    for (const auto& Var : Locals) {
        ImGui::Text("%s = %s", Var.Name.c_str(), Var.Value.c_str());
    }

    ImGui::EndChild();

    ImGui::Text("DEBUG LOG");
    ImGui::BeginChild("DebugLogScroll", ImVec2(ZeroFloat, ZeroFloat), true, ImGuiWindowFlags_HorizontalScrollbar);

    std::vector<std::string> LogLines = Debugger.GetLogLines();

    for (const auto& Line : LogLines) {
        ImGui::TextUnformatted(Line.c_str());
    }

    ImGui::EndChild();
}

// Re-parses the active document to populate the cached Symbol Outline list.
void CodeEditor::RefreshOutlineSymbols() {
    const int ZeroInt = 0;

    CachedOutlineSymbols.clear();

    if (ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        return;
    }

    const FileTab& Tab = OpenTabs[ActiveTabIndex];
    CachedOutlineTabId = Tab.TabId;
    CachedOutlineSymbols = SymbolOutline::Extract(EditorHost.GetText(), Tab.FileName);
}

// Renders the Symbol Outline sidebar panel for jumping to classes, structs, and functions.
void CodeEditor::RenderOutlineSidebar() {
    const int ZeroInt = 0;
    const float ZeroFloat = 0;
    const float FullAlpha = 1;
    const float KindRed = 0.35f;
    const float KindGreen = 0.75f;
    const float KindBlue = 0.95f;

    ImGui::Text("SYMBOL OUTLINE");
    ImGui::SameLine();

    if (ImGui::SmallButton("Refresh")) {
        RefreshOutlineSymbols();
    }

    ImGui::Separator();

    if (ActiveTabIndex < ZeroInt || ActiveTabIndex >= static_cast<int>(OpenTabs.size())) {
        ImGui::Text("No active file.");

        return;
    }

    if (CachedOutlineTabId != OpenTabs[ActiveTabIndex].TabId) {
        RefreshOutlineSymbols();
    }

    if (CachedOutlineSymbols.empty()) {
        ImGui::Text("No symbols found.");

        return;
    }

    ImGui::BeginChild("OutlineSymbolsScroll", ImVec2(ZeroFloat, ZeroFloat), false);

    for (int Idx = ZeroInt; Idx < static_cast<int>(CachedOutlineSymbols.size()); ++Idx) {
        const OutlineSymbol& Sym = CachedOutlineSymbols[static_cast<size_t>(Idx)];
        ImGui::PushID(Idx);
        ImGui::TextColored(ImVec4(KindRed, KindGreen, KindBlue, FullAlpha), "[%s]", Sym.Kind.c_str());
        ImGui::SameLine();

        std::string Label = Sym.Name + " :" + std::to_string(Sym.LineNumber);

        if (ImGui::Selectable(Label.c_str())) {
            EditorHost.GoToLine(Sym.LineNumber);
            EditorHost.Focus();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
}

// Renders the Settings floating mini window for user preferences.
void CodeEditor::RenderSettingsWindow() {
    const float HalfFloat = 0.5f;
    const float DefaultWidth = 450;
    const float DefaultHeight = 350;

    if (!ShowSettingsWindow) {
        return;
    }

    ImGuiIO& IoRef = ImGui::GetIO();
    ImVec2 CenterPos(IoRef.DisplaySize.x * HalfFloat, IoRef.DisplaySize.y * HalfFloat);
    ImGui::SetNextWindowPos(CenterPos, ImGuiCond_FirstUseEver, ImVec2(HalfFloat, HalfFloat));
    ImGui::SetNextWindowSize(ImVec2(DefaultWidth, DefaultHeight), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Settings", &ShowSettingsWindow, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("SETTINGS");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("Editor settings will appear here.");
    }

    ImGui::End();
}
