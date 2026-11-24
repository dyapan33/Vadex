#ifndef STICKY_SCROLL_HPP
#define STICKY_SCROLL_HPP
#include <string>
#include <vector>
#include <algorithm>
#include "ImGUI/imgui.h"
#include "Include/ScintillaHost.hpp"



// Renders a Monaco-style Sticky Scroll header bar showing enclosing scopes above Scintilla.
class StickyScrollBar {
public:
    // Draws pinned enclosing scope headers and returns the vertical pixel height consumed.
    static float Render(ScintillaHost& Host, ImFont* CodeFont, float AvailableWidth) {
        const int ZeroInt = 0;
        const int OneInt = 1;
        const int NegativeOneInt = -1;
        const int MaxStickyScopes = 3;
        const float ZeroFloat = 0;
        const float FullFloat = 1;
        const float RowHeight = 21;
        const float TextIndentX = 11;
        const float BgChannel = 0.11f;
        const float BgBlue = 0.13f;
        const float BorderAlpha = 0.3f;
        const float HoverAlpha = 0.1f;

        if (!Host.IsReady() || AvailableWidth <= ZeroFloat) {
            return ZeroFloat;
        }

        int FirstVisible = Host.GetFirstVisibleLine();

        if (FirstVisible <= ZeroInt) {
            return ZeroFloat;
        }

        // Walks fold parents upward from the first visible line.
        std::vector<int> ScopeLines;
        int ParentLine = Host.GetFoldParent(FirstVisible);

        while (ParentLine >= ZeroInt && static_cast<int>(ScopeLines.size()) < MaxStickyScopes) {
            if (ParentLine < FirstVisible) {
                ScopeLines.push_back(ParentLine);
            }
            ParentLine = Host.GetFoldParent(ParentLine);
        }

        if (ScopeLines.empty()) {
            return ZeroFloat;
        }

        std::reverse(ScopeLines.begin(), ScopeLines.end());

        if (CodeFont != nullptr) {
            ImGui::PushFont(CodeFont);
        }

        float TotalHeight = static_cast<float>(ScopeLines.size()) * RowHeight;
        ImVec2 Origin = ImGui::GetCursorScreenPos();
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        ImU32 BgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(BgChannel, BgChannel, BgBlue, FullFloat));
        ImU32 BorderColor = ImGui::ColorConvertFloat4ToU32(ImVec4(FullFloat, FullFloat, FullFloat, BorderAlpha));
        ImU32 HoverColor = ImGui::ColorConvertFloat4ToU32(ImVec4(FullFloat, FullFloat, FullFloat, HoverAlpha));

        DrawList->AddRectFilled(Origin, ImVec2(Origin.x + AvailableWidth, Origin.y + TotalHeight), BgColor);

        for (size_t Idx = ZeroInt; Idx < ScopeLines.size(); ++Idx) {
            int ScopeLine = ScopeLines[Idx];
            std::string ScopeText = std::to_string(ScopeLine + OneInt) + "  " + Host.GetLineText(ScopeLine);

            ImGui::PushID(ScopeLine);
            ImVec2 RowMin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##StickyScopeRow", ImVec2(AvailableWidth, RowHeight));

            if (ImGui::IsItemHovered()) {
                DrawList->AddRectFilled(RowMin, ImVec2(RowMin.x + AvailableWidth, RowMin.y + RowHeight), HoverColor);
            }

            if (ImGui::IsItemClicked()) {
                Host.GoToLine(ScopeLine + OneInt);
                Host.Focus();
            }

            ImU32 TextColor = ImGui::GetColorU32(ImGuiCol_Text);
            DrawList->AddText(ImVec2(RowMin.x + TextIndentX, RowMin.y + static_cast<float>(OneInt)), TextColor, ScopeText.c_str());
            ImGui::PopID();
        }

        DrawList->AddLine(ImVec2(Origin.x, Origin.y + TotalHeight), ImVec2(Origin.x + AvailableWidth, Origin.y + TotalHeight), BorderColor);

        if (CodeFont != nullptr) {
            ImGui::PopFont();
        }

        return TotalHeight;
    }
};

#endif
