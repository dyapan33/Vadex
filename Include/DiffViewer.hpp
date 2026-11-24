#ifndef DIFF_VIEWER_HPP
#define DIFF_VIEWER_HPP
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include "ImGUI/imgui.h"



// Identifies the change type of a single diff line.
enum DiffLineKind {
    DiffLineUnchanged,
    DiffLineAdded,
    DiffLineRemoved
};

// Represents one aligned row in the computed diff output.
struct DiffRow {
    DiffLineKind Kind;
    int OldLineNumber;
    int NewLineNumber;
    std::string Text;
};

// Computes and renders a clean GitHub/Monaco-style line diff viewer modal.
class DiffViewer {
private:
    bool IsOpenState;
    std::string LeftTitle;
    std::string RightTitle;
    std::vector<DiffRow> Rows;
    int AddedCount;
    int RemovedCount;

    // Splits a string into individual lines without trailing CR characters.
    static std::vector<std::string> SplitLines(const std::string& Content) {
        std::vector<std::string> Result;
        std::istringstream Stream(Content);
        std::string Line;

        while (std::getline(Stream, Line)) {
            if (!Line.empty() && Line.back() == '\r') {
                Line.pop_back();
            }
            Result.push_back(Line);
        }

        return Result;
    }

    // Formats a right-aligned fixed-width line number string.
    static std::string FormatLineNumber(int LineNumber) {
        const int ZeroInt = 0;
        const int ColumnWidth = 5;

        std::ostringstream Stream;

        if (LineNumber > ZeroInt) {
            Stream << std::setw(ColumnWidth) << LineNumber;
        } else {
            Stream << std::setw(ColumnWidth) << " ";
        }

        return Stream.str();
    }

public:
    // Initializes the diff viewer in closed state.
    DiffViewer() {
        const int ZeroInt = 0;

        IsOpenState = false;
        AddedCount = ZeroInt;
        RemovedCount = ZeroInt;
    }

    // Returns whether the diff viewer modal is currently open.
    bool IsOpen() const {
        return IsOpenState;
    }

    // Closes the diff viewer modal.
    void Close() {
        const int ZeroInt = 0;

        IsOpenState = false;
        Rows.clear();
        AddedCount = ZeroInt;
        RemovedCount = ZeroInt;
    }

    // Computes an LCS line diff between two text buffers and opens the diff modal.
    void OpenDiff(const std::string& InLeftTitle, const std::string& OldText, const std::string& InRightTitle, const std::string& NewText) {
        const int ZeroInt = 0;
        const int OneInt = 1;
        const int NegativeOneInt = -1;
        const size_t MaxLcsLines = 1500;

        LeftTitle = InLeftTitle;
        RightTitle = InRightTitle;
        Rows.clear();
        AddedCount = ZeroInt;
        RemovedCount = ZeroInt;

        std::vector<std::string> OldLines = SplitLines(OldText);
        std::vector<std::string> NewLines = SplitLines(NewText);
        size_t OldCount = (OldLines.size() > MaxLcsLines) ? MaxLcsLines : OldLines.size();
        size_t NewCount = (NewLines.size() > MaxLcsLines) ? MaxLcsLines : NewLines.size();

        std::vector<std::vector<int>> Table(OldCount + OneInt, std::vector<int>(NewCount + OneInt, ZeroInt));

        for (size_t OldIdx = OldCount; OldIdx > ZeroInt; --OldIdx) {
            for (size_t NewIdx = NewCount; NewIdx > ZeroInt; --NewIdx) {
                if (OldLines[OldIdx - OneInt] == NewLines[NewIdx - OneInt]) {
                    Table[OldIdx - OneInt][NewIdx - OneInt] = Table[OldIdx][NewIdx] + OneInt;
                } else {
                    Table[OldIdx - OneInt][NewIdx - OneInt] = std::max(Table[OldIdx][NewIdx - OneInt], Table[OldIdx - OneInt][NewIdx]);
                }
            }
        }

        size_t OldCursor = ZeroInt;
        size_t NewCursor = ZeroInt;

        while (OldCursor < OldCount && NewCursor < NewCount) {
            if (OldLines[OldCursor] == NewLines[NewCursor]) {
                Rows.push_back({ DiffLineUnchanged, static_cast<int>(OldCursor) + OneInt, static_cast<int>(NewCursor) + OneInt, OldLines[OldCursor] });
                OldCursor++;
                NewCursor++;
            } else if (Table[OldCursor + OneInt][NewCursor] >= Table[OldCursor][NewCursor + OneInt]) {
                Rows.push_back({ DiffLineRemoved, static_cast<int>(OldCursor) + OneInt, NegativeOneInt, OldLines[OldCursor] });
                RemovedCount++;
                OldCursor++;
            } else {
                Rows.push_back({ DiffLineAdded, NegativeOneInt, static_cast<int>(NewCursor) + OneInt, NewLines[NewCursor] });
                AddedCount++;
                NewCursor++;
            }
        }

        while (OldCursor < OldCount) {
            Rows.push_back({ DiffLineRemoved, static_cast<int>(OldCursor) + OneInt, NegativeOneInt, OldLines[OldCursor] });
            RemovedCount++;
            OldCursor++;
        }

        while (NewCursor < NewCount) {
            Rows.push_back({ DiffLineAdded, NegativeOneInt, static_cast<int>(NewCursor) + OneInt, NewLines[NewCursor] });
            AddedCount++;
            NewCursor++;
        }

        IsOpenState = true;
    }

    // Renders the diff viewer modal with aligned line numbers, summary badges, and tinted row highlights.
    void Render(ImFont* CodeFont) {
        const float ZeroFloat = 0;
        const float HalfFloat = 0.5f;
        const float FullFloat = 1;
        const float WidthRatio = 0.85f;
        const float HeightRatio = 0.80f;
        const float CloseBtnWidth = 85;
        const float BadgeSpacing = 15;
        const float RowTintAlpha = 0.13f;
        const float AddedRed = 0.25f;
        const float AddedGreen = 0.85f;
        const float AddedBlue = 0.40f;
        const float RemovedRed = 0.95f;
        const float RemovedGreenBlue = 0.35f;
        const float NeutralChannel = 0.80f;
        const float GutterChannel = 0.5f;

        if (!IsOpenState) {
            return;
        }

        ImGui::OpenPopup("Git Diff Viewer");

        ImGuiIO& IoRef = ImGui::GetIO();
        ImVec2 CenterPos(IoRef.DisplaySize.x * HalfFloat, IoRef.DisplaySize.y * HalfFloat);
        ImVec2 ModalSize(IoRef.DisplaySize.x * WidthRatio, IoRef.DisplaySize.y * HeightRatio);
        ImGui::SetNextWindowPos(CenterPos, ImGuiCond_Always, ImVec2(HalfFloat, HalfFloat));
        ImGui::SetNextWindowSize(ModalSize, ImGuiCond_Always);

        if (ImGui::BeginPopupModal("Git Diff Viewer", &IsOpenState, ImGuiWindowFlags_NoResize)) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s  ->  %s", LeftTitle.c_str(), RightTitle.c_str());
            ImGui::SameLine(ZeroFloat, BadgeSpacing);
            ImGui::TextColored(ImVec4(AddedRed, AddedGreen, AddedBlue, FullFloat), "+%d", AddedCount);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(RemovedRed, RemovedGreenBlue, RemovedGreenBlue, FullFloat), "-%d", RemovedCount);

            float RightButtonX = ImGui::GetContentRegionMax().x - CloseBtnWidth;

            if (RightButtonX > ImGui::GetCursorPosX()) {
                ImGui::SameLine();
                ImGui::SetCursorPosX(RightButtonX);
            } else {
                ImGui::SameLine();
            }

            if (ImGui::Button("Close", ImVec2(CloseBtnWidth, ZeroFloat)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                Close();
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();

                return;
            }

            ImGui::Separator();

            if (CodeFont != nullptr) {
                ImGui::PushFont(CodeFont);
            }

            ImGui::BeginChild("DiffScrollRegion", ImVec2(ZeroFloat, ZeroFloat), true, ImGuiWindowFlags_HorizontalScrollbar);

            ImDrawList* DrawList = ImGui::GetWindowDrawList();
            float RowWidth = ImGui::GetContentRegionAvail().x;
            float LineHeight = ImGui::GetTextLineHeightWithSpacing();
            ImU32 AddedBg = ImGui::ColorConvertFloat4ToU32(ImVec4(AddedRed, AddedGreen, AddedBlue, RowTintAlpha));
            ImU32 RemovedBg = ImGui::ColorConvertFloat4ToU32(ImVec4(RemovedRed, RemovedGreenBlue, RemovedGreenBlue, RowTintAlpha));
            ImVec4 GutterColor(GutterChannel, GutterChannel, GutterChannel, FullFloat);

            for (const auto& Row : Rows) {
                ImVec2 RowPos = ImGui::GetCursorScreenPos();

                if (Row.Kind == DiffLineAdded) {
                    DrawList->AddRectFilled(RowPos, ImVec2(RowPos.x + RowWidth, RowPos.y + LineHeight), AddedBg);
                } else if (Row.Kind == DiffLineRemoved) {
                    DrawList->AddRectFilled(RowPos, ImVec2(RowPos.x + RowWidth, RowPos.y + LineHeight), RemovedBg);
                }

                std::string GutterStr = FormatLineNumber(Row.OldLineNumber) + " | " + FormatLineNumber(Row.NewLineNumber) + " | ";
                ImGui::TextColored(GutterColor, "%s", GutterStr.c_str());
                ImGui::SameLine(ZeroFloat, ZeroFloat);

                const char* Prefix = "  ";
                ImVec4 TextColor(NeutralChannel, NeutralChannel, NeutralChannel, FullFloat);

                if (Row.Kind == DiffLineAdded) {
                    Prefix = "+ ";
                    TextColor = ImVec4(AddedRed, AddedGreen, AddedBlue, FullFloat);
                } else if (Row.Kind == DiffLineRemoved) {
                    Prefix = "- ";
                    TextColor = ImVec4(RemovedRed, RemovedGreenBlue, RemovedGreenBlue, FullFloat);
                }

                std::string CodeLine = std::string(Prefix) + Row.Text;
                ImGui::TextColored(TextColor, "%s", CodeLine.c_str());
            }

            ImGui::EndChild();

            if (CodeFont != nullptr) {
                ImGui::PopFont();
            }

            ImGui::EndPopup();
        }
    }
};

#endif
