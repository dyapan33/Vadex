#ifndef MINIMAP_HPP
#define MINIMAP_HPP
#include <string>
#include <algorithm>
#include "ImGUI/imgui.h"
#include "Include/ScintillaHost.hpp"



// Renders a Monaco-style clickable and draggable code overview minimap beside Scintilla.
class MinimapPanel {
public:
    // Draws the minimap inside the specified width and height, syncing viewport scroll with ScintillaHost.
    static void Render(ScintillaHost& Host, float Width, float Height) {
        const int ZeroInt = 0;
        const int OneInt = 1;
        const int TwoInt = 1 + 1;
        const int MaxSampleLines = 300;
        const float ZeroFloat = 0;
        const float FullFloat = 1;
        const float HalfFloat = 0.5f;
        const float LineHeightPx = 3;
        const float LineGapPx = 1;
        const float CharWidthPx = 1;
        const float LeftMarginPx = 5;
        const float MinSliderHeight = 15;
        const float BgChannel = 0.09f;
        const float BgBlueChannel = 0.1f;
        const float SliderAlpha = 0.15f;
        const float BorderAlpha = 0.25f;
        const float CodeAlpha = 0.45f;
        const float CommentGreen = 0.65f;
        const float CommentRedBlue = 0.35f;
        const float PreprocBlue = 0.85f;
        const float PreprocRed = 0.35f;
        const float PreprocGreen = 0.60f;
        const float DefaultRgb = 0.75f;

        if (!Host.IsReady() || Width <= ZeroFloat || Height <= ZeroFloat) {
            return;
        }

        ImVec2 MinimapOrigin = ImGui::GetCursorScreenPos();
        ImVec2 MinimapEnd(MinimapOrigin.x + Width, MinimapOrigin.y + Height);
        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        ImU32 BgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(BgChannel, BgChannel, BgBlueChannel, FullFloat));
        ImU32 BorderColor = ImGui::ColorConvertFloat4ToU32(ImVec4(FullFloat, FullFloat, FullFloat, BorderAlpha));
        DrawList->AddRectFilled(MinimapOrigin, MinimapEnd, BgColor);
        DrawList->AddLine(MinimapOrigin, ImVec2(MinimapOrigin.x, MinimapEnd.y), BorderColor);

        int TotalLines = Host.GetLineCount();

        if (TotalLines <= ZeroInt) {
            TotalLines = OneInt;
        }

        int FirstVisible = Host.GetFirstVisibleLine();
        int LinesOnScreen = Host.GetLinesOnScreen();
        int MaxDrawableLines = static_cast<int>(Height / LineHeightPx);
        int Step = OneInt;

        if (TotalLines > MaxDrawableLines && MaxDrawableLines > ZeroInt) {
            Step = (TotalLines + MaxDrawableLines - OneInt) / MaxDrawableLines;
        }

        int SampleLimit = (TotalLines < MaxSampleLines * Step) ? TotalLines : (MaxSampleLines * Step);
        float RowY = MinimapOrigin.y + LeftMarginPx;
        float MaxLineWidth = Width - (LeftMarginPx * static_cast<float>(TwoInt));

        ImU32 DefaultCodeColor = ImGui::ColorConvertFloat4ToU32(ImVec4(DefaultRgb, DefaultRgb, DefaultRgb, CodeAlpha));
        ImU32 CommentCodeColor = ImGui::ColorConvertFloat4ToU32(ImVec4(CommentRedBlue, CommentGreen, CommentRedBlue, CodeAlpha));
        ImU32 PreprocCodeColor = ImGui::ColorConvertFloat4ToU32(ImVec4(PreprocRed, PreprocGreen, PreprocBlue, CodeAlpha));

        for (int LineIdx = ZeroInt; LineIdx < SampleLimit && RowY < MinimapEnd.y - LeftMarginPx; LineIdx += Step) {
            std::string LineText = Host.GetLineText(LineIdx);
            size_t FirstNonSpace = LineText.find_first_not_of(" \t");

            if (FirstNonSpace != std::string::npos) {
                size_t LastNonSpace = LineText.find_last_not_of(" \t");
                float IndentPx = static_cast<float>(FirstNonSpace) * CharWidthPx;
                float SpanPx = static_cast<float>(LastNonSpace - FirstNonSpace + OneInt) * CharWidthPx;

                if (IndentPx > MaxLineWidth) {
                    IndentPx = MaxLineWidth;
                }
                if (IndentPx + SpanPx > MaxLineWidth) {
                    SpanPx = MaxLineWidth - IndentPx;
                }

                if (SpanPx > ZeroFloat) {
                    ImU32 LineColor = DefaultCodeColor;
                    char LeadChar = LineText[FirstNonSpace];

                    if (LeadChar == '/' || LeadChar == '*' || LeadChar == '#') {
                        LineColor = (LeadChar == '#') ? PreprocCodeColor : CommentCodeColor;
                    }

                    float X1 = MinimapOrigin.x + LeftMarginPx + IndentPx;
                    float X2 = X1 + SpanPx;
                    DrawList->AddRectFilled(ImVec2(X1, RowY), ImVec2(X2, RowY + (LineHeightPx - LineGapPx)), LineColor);
                }
            }

            RowY += LineHeightPx;
        }

        // Draws the translucent viewport slider indicator.
        float TopFraction = static_cast<float>(FirstVisible) / static_cast<float>(TotalLines);
        float VisibleFraction = static_cast<float>(LinesOnScreen) / static_cast<float>(TotalLines);
        float SliderY = MinimapOrigin.y + TopFraction * Height;
        float SliderH = VisibleFraction * Height;

        if (SliderH < MinSliderHeight) {
            SliderH = MinSliderHeight;
        }
        if (SliderY + SliderH > MinimapEnd.y) {
            SliderY = MinimapEnd.y - SliderH;
        }
        if (SliderY < MinimapOrigin.y) {
            SliderY = MinimapOrigin.y;
        }

        ImU32 SliderColor = ImGui::ColorConvertFloat4ToU32(ImVec4(FullFloat, FullFloat, FullFloat, SliderAlpha));
        DrawList->AddRectFilled(ImVec2(MinimapOrigin.x, SliderY), ImVec2(MinimapEnd.x, SliderY + SliderH), SliderColor);

        // Handles click and drag interaction to scroll the Scintilla viewport.
        ImGui::InvisibleButton("##MinimapHitArea", ImVec2(Width, Height));

        if (ImGui::IsItemActive() || (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
            float MouseY = ImGui::GetIO().MousePos.y;
            float ClickFraction = (MouseY - MinimapOrigin.y) / Height;

            if (ClickFraction < ZeroFloat) {
                ClickFraction = ZeroFloat;
            }
            if (ClickFraction > FullFloat) {
                ClickFraction = FullFloat;
            }

            int TargetLine = static_cast<int>(ClickFraction * static_cast<float>(TotalLines)) - static_cast<int>(static_cast<float>(LinesOnScreen) * HalfFloat);

            if (TargetLine < ZeroInt) {
                TargetLine = ZeroInt;
            }

            Host.SetFirstVisibleLine(TargetLine);
        }
    }
};

#endif
