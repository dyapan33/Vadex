#ifndef COMMAND_PALETTE_HPP
#define COMMAND_PALETTE_HPP
#include <string>
#include <vector>
#include <functional>
#include <cctype>
#include "ImGUI/imgui.h"



const int PaletteBufferSize = 255;

// Identifies the active mode of the quick command palette modal.
enum PaletteMode {
    PaletteModeClosed,
    PaletteModeCommands,
    PaletteModeQuickOpen,
    PaletteModeGoToLine,
    PaletteModeGoToSymbol
};

// Represents an executable command shown in the Command Palette.
struct PaletteCommand {
    std::string Label;
    std::string Shortcut;
    std::function<void()> Action;
};

// Represents a workspace file entry shown in Quick Open.
struct QuickOpenEntry {
    std::string DisplayPath;
    std::string FullPath;
};

// Represents a symbol entry shown in Go to Symbol.
struct QuickSymbolEntry {
    std::string Kind;
    std::string Name;
    int LineNumber;
};

// Implements Monaco-style Command Palette (Ctrl+Shift+P), Quick Open (Ctrl+P), and Go to Line (Ctrl+G).
class CommandPalette {
private:
    PaletteMode ActiveMode;
    char QueryBuffer[PaletteBufferSize];
    int SelectedIndex;
    bool FocusInputNextFrame;

    // Performs case-insensitive subsequence fuzzy matching.
    static bool FuzzyMatch(const std::string& Query, const std::string& Candidate) {
        const size_t ZeroIdx = 0;

        if (Query.empty()) {
            return true;
        }

        size_t QueryIdx = ZeroIdx;

        for (size_t CandIdx = ZeroIdx; CandIdx < Candidate.size() && QueryIdx < Query.size(); ++CandIdx) {
            char QChar = static_cast<char>(std::tolower(static_cast<unsigned char>(Query[QueryIdx])));
            char CChar = static_cast<char>(std::tolower(static_cast<unsigned char>(Candidate[CandIdx])));

            if (QChar == CChar) {
                QueryIdx++;
            }
        }

        return (QueryIdx == Query.size());
    }

public:
    // Initializes the command palette in closed state.
    CommandPalette() {
        const int ZeroInt = 0;

        ActiveMode = PaletteModeClosed;
        QueryBuffer[ZeroInt] = '\0';
        SelectedIndex = ZeroInt;
        FocusInputNextFrame = false;
    }

    // Returns whether any palette modal is currently open.
    bool IsOpen() const {
        return (ActiveMode != PaletteModeClosed);
    }

    // Opens the palette in the requested mode and resets the query filter.
    void Open(PaletteMode Mode) {
        const int ZeroInt = 0;

        ActiveMode = Mode;
        QueryBuffer[ZeroInt] = '\0';
        SelectedIndex = ZeroInt;
        FocusInputNextFrame = true;
    }

    // Closes the palette modal.
    void Close() {
        ActiveMode = PaletteModeClosed;
    }

    // Renders the active palette modal and dispatches selected commands, files, symbols, or line jumps.
    void Render(
        const std::vector<PaletteCommand>& Commands,
        const std::vector<QuickOpenEntry>& Files,
        const std::vector<QuickSymbolEntry>& Symbols,
        const std::function<void(const std::string&)>& OnOpenFile,
        const std::function<void(int, int)>& OnGoToLine,
        const std::function<void(int)>& OnGoToSymbol
    ) {
        const int ZeroInt = 0;
        const int OneInt = 1;
        const float HalfFloat = 0.5f;
        const float TopOffsetY = 75;
        const float ModalWidth = 510;
        const float ListMaxHeight = 290;
        const float FullWidth = -1;

        if (ActiveMode == PaletteModeClosed) {
            return;
        }

        const char* ModalTitle = "Command Palette";

        if (ActiveMode == PaletteModeQuickOpen) {
            ModalTitle = "Quick Open File";
        } else if (ActiveMode == PaletteModeGoToLine) {
            ModalTitle = "Go to Line / Column";
        } else if (ActiveMode == PaletteModeGoToSymbol) {
            ModalTitle = "Go to Symbol";
        }

        ImGui::OpenPopup(ModalTitle);

        ImGuiIO& IoRef = ImGui::GetIO();
        ImVec2 ModalPos(IoRef.DisplaySize.x * HalfFloat, TopOffsetY);
        ImGui::SetNextWindowPos(ModalPos, ImGuiCond_Always, ImVec2(HalfFloat, static_cast<float>(ZeroInt)));
        ImGui::SetNextWindowSize(ImVec2(ModalWidth, static_cast<float>(ZeroInt)));

        ImGuiWindowFlags Flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        if (ImGui::BeginPopupModal(ModalTitle, nullptr, Flags)) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                Close();
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();

                return;
            }

            if (FocusInputNextFrame) {
                ImGui::SetKeyboardFocusHere();
                FocusInputNextFrame = false;
            }

            const char* HintText = "Type a command...";

            if (ActiveMode == PaletteModeQuickOpen) {
                HintText = "Type a file name...";
            } else if (ActiveMode == PaletteModeGoToLine) {
                HintText = "Enter line or line:column (e.g. 45 or 45:10)...";
            } else if (ActiveMode == PaletteModeGoToSymbol) {
                HintText = "Type a symbol name...";
            }

            ImGui::SetNextItemWidth(FullWidth);
            bool EnterPressed = ImGui::InputTextWithHint("##PaletteQuery", HintText, QueryBuffer, PaletteBufferSize, ImGuiInputTextFlags_EnterReturnsTrue);

            if (ActiveMode == PaletteModeGoToLine) {
                if (EnterPressed && QueryBuffer[ZeroInt] != '\0') {
                    std::string InputStr(QueryBuffer);
                    size_t ColonPos = InputStr.find(':');
                    int TargetLine = OneInt;
                    int TargetCol = OneInt;

                    try {
                        if (ColonPos != std::string::npos) {
                            TargetLine = std::stoi(InputStr.substr(ZeroInt, ColonPos));
                            TargetCol = std::stoi(InputStr.substr(ColonPos + OneInt));
                        } else {
                            TargetLine = std::stoi(InputStr);
                        }
                    } catch (...) {
                        TargetLine = OneInt;
                        TargetCol = OneInt;
                    }

                    if (OnGoToLine) {
                        OnGoToLine(TargetLine, TargetCol);
                    }

                    Close();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();

                return;
            }

            std::string Query(QueryBuffer);
            ImGui::Separator();
            ImGui::BeginChild("PaletteResults", ImVec2(static_cast<float>(ZeroInt), ListMaxHeight), false);

            if (ActiveMode == PaletteModeCommands) {
                std::vector<int> MatchingIndices;

                for (int Idx = ZeroInt; Idx < static_cast<int>(Commands.size()); ++Idx) {
                    if (FuzzyMatch(Query, Commands[static_cast<size_t>(Idx)].Label)) {
                        MatchingIndices.push_back(Idx);
                    }
                }

                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && !MatchingIndices.empty()) {
                    SelectedIndex = (SelectedIndex + OneInt) % static_cast<int>(MatchingIndices.size());
                }
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && !MatchingIndices.empty()) {
                    SelectedIndex = (SelectedIndex - OneInt + static_cast<int>(MatchingIndices.size())) % static_cast<int>(MatchingIndices.size());
                }
                if (SelectedIndex >= static_cast<int>(MatchingIndices.size())) {
                    SelectedIndex = ZeroInt;
                }

                for (int ListPos = ZeroInt; ListPos < static_cast<int>(MatchingIndices.size()); ++ListPos) {
                    const PaletteCommand& Cmd = Commands[static_cast<size_t>(MatchingIndices[static_cast<size_t>(ListPos)])];
                    bool IsSelected = (ListPos == SelectedIndex);
                    std::string RowLabel = Cmd.Shortcut.empty() ? Cmd.Label : (Cmd.Label + "   [" + Cmd.Shortcut + "]");

                    if (ImGui::Selectable(RowLabel.c_str(), IsSelected) || (EnterPressed && IsSelected)) {
                        Close();
                        ImGui::CloseCurrentPopup();

                        if (Cmd.Action) {
                            Cmd.Action();
                        }

                        break;
                    }
                }
            } else if (ActiveMode == PaletteModeQuickOpen) {
                std::vector<int> MatchingIndices;

                for (int Idx = ZeroInt; Idx < static_cast<int>(Files.size()); ++Idx) {
                    if (FuzzyMatch(Query, Files[static_cast<size_t>(Idx)].DisplayPath)) {
                        MatchingIndices.push_back(Idx);
                    }
                }

                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && !MatchingIndices.empty()) {
                    SelectedIndex = (SelectedIndex + OneInt) % static_cast<int>(MatchingIndices.size());
                }
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && !MatchingIndices.empty()) {
                    SelectedIndex = (SelectedIndex - OneInt + static_cast<int>(MatchingIndices.size())) % static_cast<int>(MatchingIndices.size());
                }
                if (SelectedIndex >= static_cast<int>(MatchingIndices.size())) {
                    SelectedIndex = ZeroInt;
                }

                for (int ListPos = ZeroInt; ListPos < static_cast<int>(MatchingIndices.size()); ++ListPos) {
                    const QuickOpenEntry& FileEntry = Files[static_cast<size_t>(MatchingIndices[static_cast<size_t>(ListPos)])];
                    bool IsSelected = (ListPos == SelectedIndex);

                    if (ImGui::Selectable(FileEntry.DisplayPath.c_str(), IsSelected) || (EnterPressed && IsSelected)) {
                        Close();
                        ImGui::CloseCurrentPopup();

                        if (OnOpenFile) {
                            OnOpenFile(FileEntry.FullPath);
                        }

                        break;
                    }
                }
            } else if (ActiveMode == PaletteModeGoToSymbol) {
                std::vector<int> MatchingIndices;

                for (int Idx = ZeroInt; Idx < static_cast<int>(Symbols.size()); ++Idx) {
                    if (FuzzyMatch(Query, Symbols[static_cast<size_t>(Idx)].Name)) {
                        MatchingIndices.push_back(Idx);
                    }
                }

                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && !MatchingIndices.empty()) {
                    SelectedIndex = (SelectedIndex + OneInt) % static_cast<int>(MatchingIndices.size());
                }
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && !MatchingIndices.empty()) {
                    SelectedIndex = (SelectedIndex - OneInt + static_cast<int>(MatchingIndices.size())) % static_cast<int>(MatchingIndices.size());
                }
                if (SelectedIndex >= static_cast<int>(MatchingIndices.size())) {
                    SelectedIndex = ZeroInt;
                }

                for (int ListPos = ZeroInt; ListPos < static_cast<int>(MatchingIndices.size()); ++ListPos) {
                    const QuickSymbolEntry& SymEntry = Symbols[static_cast<size_t>(MatchingIndices[static_cast<size_t>(ListPos)])];
                    bool IsSelected = (ListPos == SelectedIndex);
                    std::string RowLabel = "[" + SymEntry.Kind + "] " + SymEntry.Name + "  (Ln " + std::to_string(SymEntry.LineNumber) + ")";

                    if (ImGui::Selectable(RowLabel.c_str(), IsSelected) || (EnterPressed && IsSelected)) {
                        Close();
                        ImGui::CloseCurrentPopup();

                        if (OnGoToSymbol) {
                            OnGoToSymbol(SymEntry.LineNumber);
                        }

                        break;
                    }
                }
            }

            ImGui::EndChild();
            ImGui::EndPopup();
        }
    }
};

#endif
