#ifndef THEME_HPP
#define THEME_HPP
#include "ImGUI/imgui.h"



// Manages the visual theme styling for the editor interface.
class EditorTheme {
public:
    // Applies the unified dark color palette and spacing style to ImGui.
    static void ApplyDarkTheme() {
        const float Zero = 0;
        const float Full = 1;
        const float AlphaHalf = 0.5f;
        const float AlphaLow = 0.05f;
        const float AlphaDim = 0.20f;
        const float AlphaModalDim = 0.35f;
        const float AlphaNavHighlight = 0.7f;
        const float AlphaDragDrop = 0.9f;
        const float Channel010 = 0.1f;
        const float Channel011 = 0.11f;
        const float Channel013 = 0.13f;
        const float Channel015 = 0.15f;
        const float Channel017 = 0.17f;
        const float Channel019 = 0.19f;
        const float Channel020 = 0.20f;
        const float Channel021 = 0.21f;
        const float Channel023 = 0.23f;
        const float Channel025 = 0.25f;
        const float Channel027 = 0.27f;
        const float Channel030 = 0.3f;
        const float Channel031 = 0.31f;
        const float Channel035 = 0.35f;
        const float Channel040 = 0.40f;
        const float Channel043 = 0.43f;
        const float Channel045 = 0.45f;
        const float Channel050 = 0.5f;
        const float Channel060 = 0.60f;
        const float Channel061 = 0.61f;
        const float Channel065 = 0.65f;
        const float Channel070 = 0.7f;
        const float Channel075 = 0.75f;
        const float Channel080 = 0.80f;
        const float Channel090 = 0.9f;
        const float Channel095 = 0.95f;
        const float RoundingSmall = 3;
        const float RoundingMedium = 5;
        const float RoundingMenuItem = 7;
        const float RoundingScrollbar = 9;
        const float WindowPaddingX = 11;
        const float WindowPaddingY = 9;
        const float FramePaddingX = 9;
        const float FramePaddingY = 5;
        const float ItemSpacingX = 9;
        const float ItemSpacingY = 7;

        ImGuiStyle& Style = ImGui::GetStyle();
        ImVec4* Colors = Style.Colors;

        Colors[ImGuiCol_Text] = ImVec4(Channel095, Channel095, Channel095, Full);
        Colors[ImGuiCol_TextDisabled] = ImVec4(Channel050, Channel050, Channel050, Full);
        Colors[ImGuiCol_WindowBg] = ImVec4(Channel013, Channel013, Channel015, Full);
        Colors[ImGuiCol_ChildBg] = ImVec4(Channel010, Channel010, Channel011, Full);
        Colors[ImGuiCol_PopupBg] = ImVec4(Channel015, Channel015, Channel019, Full);
        Colors[ImGuiCol_Border] = ImVec4(Channel025, Channel025, Channel030, AlphaHalf);
        Colors[ImGuiCol_BorderShadow] = ImVec4(Zero, Zero, Zero, Zero);
        Colors[ImGuiCol_FrameBg] = ImVec4(Channel017, Channel017, Channel021, Full);
        Colors[ImGuiCol_FrameBgHovered] = ImVec4(Channel023, Channel023, Channel027, Full);
        Colors[ImGuiCol_FrameBgActive] = ImVec4(Channel027, Channel027, Channel031, Full);
        Colors[ImGuiCol_TitleBg] = ImVec4(Channel010, Channel010, Channel011, Full);
        Colors[ImGuiCol_TitleBgActive] = ImVec4(Channel015, Channel015, Channel019, Full);
        Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(Channel010, Channel010, Channel011, Full);
        Colors[ImGuiCol_MenuBarBg] = ImVec4(Channel013, Channel013, Channel015, Full);
        Colors[ImGuiCol_ScrollbarBg] = ImVec4(Channel010, Channel010, Channel011, Full);
        Colors[ImGuiCol_ScrollbarGrab] = ImVec4(Channel025, Channel025, Channel030, Full);
        Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(Channel035, Channel035, Channel040, Full);
        Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(Channel045, Channel045, Channel050, Full);
        Colors[ImGuiCol_CheckMark] = ImVec4(Channel035, Channel065, Channel095, Full);
        Colors[ImGuiCol_SliderGrab] = ImVec4(Channel035, Channel065, Channel095, Full);
        Colors[ImGuiCol_SliderGrabActive] = ImVec4(Channel045, Channel075, Channel095, Full);
        Colors[ImGuiCol_Button] = ImVec4(Channel019, Channel019, Channel023, Full);
        Colors[ImGuiCol_ButtonHovered] = ImVec4(Channel027, Channel027, Channel031, Full);
        Colors[ImGuiCol_ButtonActive] = ImVec4(Channel031, Channel031, Channel035, Full);
        Colors[ImGuiCol_Header] = ImVec4(Channel023, Channel023, Channel027, Full);
        Colors[ImGuiCol_HeaderHovered] = ImVec4(Channel027, Channel027, Channel031, Full);
        Colors[ImGuiCol_HeaderActive] = ImVec4(Channel031, Channel031, Channel035, Full);
        Colors[ImGuiCol_Separator] = ImVec4(Channel025, Channel025, Channel030, Full);
        Colors[ImGuiCol_SeparatorHovered] = ImVec4(Channel035, Channel065, Channel095, Full);
        Colors[ImGuiCol_SeparatorActive] = ImVec4(Channel045, Channel075, Channel095, Full);
        Colors[ImGuiCol_ResizeGrip] = ImVec4(Channel025, Channel025, Channel030, Full);
        Colors[ImGuiCol_ResizeGripHovered] = ImVec4(Channel035, Channel065, Channel095, Full);
        Colors[ImGuiCol_ResizeGripActive] = ImVec4(Channel045, Channel075, Channel095, Full);
        Colors[ImGuiCol_Tab] = ImVec4(Channel015, Channel015, Channel019, Full);
        Colors[ImGuiCol_TabHovered] = ImVec4(Channel025, Channel025, Channel030, Full);
        Colors[ImGuiCol_TabSelected] = ImVec4(Channel021, Channel021, Channel025, Full);
        Colors[ImGuiCol_TabSelectedOverline] = ImVec4(Channel035, Channel065, Channel095, Full);
        Colors[ImGuiCol_TabDimmed] = ImVec4(Channel015, Channel015, Channel019, Full);
        Colors[ImGuiCol_TabDimmedSelected] = ImVec4(Channel019, Channel019, Channel023, Full);
        Colors[ImGuiCol_PlotLines] = ImVec4(Channel061, Channel061, Channel061, Full);
        Colors[ImGuiCol_PlotLinesHovered] = ImVec4(Full, Channel043, Channel035, Full);
        Colors[ImGuiCol_PlotHistogram] = ImVec4(Channel090, Channel070, Zero, Full);
        Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(Full, Channel060, Zero, Full);
        Colors[ImGuiCol_TableHeaderBg] = ImVec4(Channel019, Channel019, Channel023, Full);
        Colors[ImGuiCol_TableBorderStrong] = ImVec4(Channel031, Channel031, Channel035, Full);
        Colors[ImGuiCol_TableBorderLight] = ImVec4(Channel023, Channel023, Channel025, Full);
        Colors[ImGuiCol_TableRowBg] = ImVec4(Zero, Zero, Zero, Zero);
        Colors[ImGuiCol_TableRowBgAlt] = ImVec4(Full, Full, Full, AlphaLow);
        Colors[ImGuiCol_TextSelectedBg] = ImVec4(Channel025, Channel045, Channel075, AlphaHalf);
        Colors[ImGuiCol_DragDropTarget] = ImVec4(Full, Full, Zero, AlphaDragDrop);
        Colors[ImGuiCol_NavHighlight] = ImVec4(Channel035, Channel065, Channel095, Full);
        Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(Full, Full, Full, AlphaNavHighlight);
        Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(Channel080, Channel080, Channel080, AlphaDim);
        Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(Channel080, Channel080, Channel080, AlphaModalDim);

        Style.WindowRounding = RoundingMedium;
        Style.ChildRounding = RoundingMedium;
        Style.FrameRounding = RoundingSmall;
        Style.PopupRounding = RoundingMedium;
        Style.ScrollbarRounding = RoundingScrollbar;
        Style.GrabRounding = RoundingSmall;
        Style.TabRounding = RoundingMedium;
        Style.SelectableRounding = RoundingMedium;
        Style.MenuItemRounding = RoundingMenuItem;
        Style.WindowPadding = ImVec2(WindowPaddingX, WindowPaddingY);
        Style.FramePadding = ImVec2(FramePaddingX, FramePaddingY);
        Style.ItemSpacing = ImVec2(ItemSpacingX, ItemSpacingY);
    }
};

#endif
