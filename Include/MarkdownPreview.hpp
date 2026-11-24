#ifndef MARKDOWN_PREVIEW_HPP
#define MARKDOWN_PREVIEW_HPP
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include "ImGUI/imgui.h"



// Renders a live Markdown preview panel alongside the raw Markdown source editor.
class MarkdownPreview {
private:
    // Checks whether a filename has a Markdown extension (.md or .markdown).
    static std::string ToLowerExt(const std::string& FileName) {
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

    // Renders a line of Markdown text with inline code, bold, italic, and link styling.
    static void RenderInlineFormattedText(const std::string& Text, ImFont* BoldFont, ImFont* CodeFont) {
        const size_t ZeroIdx = 0;
        const size_t OneStep = 1;
        const size_t TwoStep = 1 + 1;
        const float ZeroFloat = 0;
        const float FullAlpha = 1;
        const float CodeRed = 0.85f;
        const float CodeGreen = 0.65f;
        const float CodeBlue = 0.95f;
        const float LinkRed = 0.35f;
        const float LinkGreen = 0.65f;
        const float LinkBlue = 0.95f;
        const float ItalicRgb = 0.85f;

        if (Text.empty()) {
            ImGui::Spacing();

            return;
        }

        // Fast path when no inline markdown tokens appear on the line.
        bool HasInlineToken = (Text.find('`') != std::string::npos || Text.find("**") != std::string::npos || Text.find('[') != std::string::npos || Text.find('*') != std::string::npos);

        if (!HasInlineToken) {
            ImGui::TextWrapped("%s", Text.c_str());

            return;
        }

        size_t Pos = ZeroIdx;
        bool IsFirstSegment = true;

        auto EmitSegment = [&](const std::string& Segment, int StyleType) {
            const int StylePlain = 0;
            const int StyleBold = 1;
            const int StyleCode = 1 + 1;
            const int StyleLink = 3;
            const int StyleItalic = 5 - 1;

            if (Segment.empty()) {
                return;
            }

            if (!IsFirstSegment) {
                ImGui::SameLine(ZeroFloat, ZeroFloat);
            }

            IsFirstSegment = false;

            if (StyleType == StyleBold) {
                if (BoldFont != nullptr) {
                    ImGui::PushFont(BoldFont);
                }
                ImGui::TextUnformatted(Segment.c_str());
                if (BoldFont != nullptr) {
                    ImGui::PopFont();
                }
            } else if (StyleType == StyleCode) {
                if (CodeFont != nullptr) {
                    ImGui::PushFont(CodeFont);
                }
                ImGui::TextColored(ImVec4(CodeRed, CodeGreen, CodeBlue, FullAlpha), "%s", Segment.c_str());
                if (CodeFont != nullptr) {
                    ImGui::PopFont();
                }
            } else if (StyleType == StyleLink) {
                ImGui::TextColored(ImVec4(LinkRed, LinkGreen, LinkBlue, FullAlpha), "%s", Segment.c_str());
            } else if (StyleType == StyleItalic) {
                ImGui::TextColored(ImVec4(ItalicRgb, ItalicRgb, FullAlpha, FullAlpha), "%s", Segment.c_str());
            } else {
                ImGui::TextUnformatted(Segment.c_str());
            }

            (void)StylePlain;
        };

        const int StylePlain = 0;
        const int StyleBold = 1;
        const int StyleCode = 1 + 1;
        const int StyleLink = 3;
        const int StyleItalic = 5 - 1;

        while (Pos < Text.size()) {
            if (Text[Pos] == '`') {
                size_t EndTick = Text.find('`', Pos + OneStep);

                if (EndTick != std::string::npos) {
                    EmitSegment(Text.substr(Pos + OneStep, EndTick - Pos - OneStep), StyleCode);
                    Pos = EndTick + OneStep;
                    continue;
                }
            }

            if (Pos + OneStep < Text.size() && Text[Pos] == '*' && Text[Pos + OneStep] == '*') {
                size_t EndBold = Text.find("**", Pos + TwoStep);

                if (EndBold != std::string::npos) {
                    EmitSegment(Text.substr(Pos + TwoStep, EndBold - Pos - TwoStep), StyleBold);
                    Pos = EndBold + TwoStep;
                    continue;
                }
            }

            if (Text[Pos] == '*') {
                size_t EndItalic = Text.find('*', Pos + OneStep);

                if (EndItalic != std::string::npos && EndItalic > Pos + OneStep) {
                    EmitSegment(Text.substr(Pos + OneStep, EndItalic - Pos - OneStep), StyleItalic);
                    Pos = EndItalic + OneStep;
                    continue;
                }
            }

            if (Text[Pos] == '[') {
                size_t CloseBracket = Text.find(']', Pos + OneStep);

                if (CloseBracket != std::string::npos && CloseBracket + OneStep < Text.size() && Text[CloseBracket + OneStep] == '(') {
                    size_t CloseParen = Text.find(')', CloseBracket + TwoStep);

                    if (CloseParen != std::string::npos) {
                        EmitSegment(Text.substr(Pos + OneStep, CloseBracket - Pos - OneStep), StyleLink);
                        Pos = CloseParen + OneStep;
                        continue;
                    }
                }
            }

            // Consumes plain text until the next formatting token.
            size_t NextSpecial = Text.find_first_of("`*[", Pos + OneStep);

            if (NextSpecial == std::string::npos) {
                EmitSegment(Text.substr(Pos), StylePlain);
                break;
            }

            EmitSegment(Text.substr(Pos, NextSpecial - Pos), StylePlain);
            Pos = NextSpecial;
        }
    }

public:
    // Returns whether the given filename is a Markdown document.
    static bool IsMarkdownFile(const std::string& FileName) {
        std::string Ext = ToLowerExt(FileName);

        return (Ext == ".md" || Ext == ".markdown");
    }

    // Renders parsed Markdown content inside a scrollable ImGui preview panel.
    static void Render(const std::string& MarkdownSource, const std::string& FileName, ImFont* BoldFont, ImFont* CodeFont, float Width, float Height) {
        const size_t ZeroIdx = 0;
        const size_t OneStep = 1;
        const size_t TwoStep = 1 + 1;
        const size_t ThreeStep = 3;
        const size_t TaskPrefixLen = 5 + 1;
        const float ZeroFloat = 0;
        const float FullAlpha = 1;
        const float HeadingScaleOne = 1.5f;
        const float HeadingScaleTwo = 1.3f;
        const float HeadingScaleThree = 1.15f;
        const float HeadingScaleFour = 1.05f;
        const float HeadingRed = 0.95f;
        const float HeadingGreen = 0.95f;
        const float HeadingBlue = 0.99f;
        const float QuoteRed = 0.65f;
        const float QuoteGreen = 0.75f;
        const float QuoteBlue = 0.85f;
        const float CodeBgChannel = 0.13f;
        const float CodeTextRed = 0.85f;
        const float CodeTextGreen = 0.90f;
        const float CodeTextBlue = 0.95f;
        const float HeaderBarPadding = 11;
        const float ListIndent = 17;
        const float CheckGreen = 0.35f;
        const float CheckRed = 0.85f;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(HeaderBarPadding, HeaderBarPadding));
        ImGui::BeginChild("MarkdownPreviewPane", ImVec2(Width, Height), true);

        ImGui::TextDisabled("MARKDOWN PREVIEW - %s", FileName.c_str());
        ImGui::Separator();
        ImGui::Spacing();

        std::istringstream Stream(MarkdownSource);
        std::string RawLine;
        bool InCodeBlock = false;
        std::string CodeBlockBuffer = "";

        while (std::getline(Stream, RawLine)) {
            if (!RawLine.empty() && RawLine.back() == '\r') {
                RawLine.pop_back();
            }

            // Handles fenced code blocks delimited by triple backticks.
            if (RawLine.rfind("```", ZeroIdx) == ZeroIdx) {
                if (!InCodeBlock) {
                    InCodeBlock = true;
                    CodeBlockBuffer.clear();
                } else {
                    InCodeBlock = false;
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(CodeBgChannel, CodeBgChannel, CodeBgChannel, FullAlpha));
                    ImGui::BeginChild("##MdCodeBlock", ImVec2(ZeroFloat, ZeroFloat), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

                    if (CodeFont != nullptr) {
                        ImGui::PushFont(CodeFont);
                    }

                    ImGui::TextColored(ImVec4(CodeTextRed, CodeTextGreen, CodeTextBlue, FullAlpha), "%s", CodeBlockBuffer.c_str());

                    if (CodeFont != nullptr) {
                        ImGui::PopFont();
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::Spacing();
                }

                continue;
            }

            if (InCodeBlock) {
                if (!CodeBlockBuffer.empty()) {
                    CodeBlockBuffer += "\n";
                }
                CodeBlockBuffer += RawLine;
                continue;
            }

            size_t FirstNonSpace = RawLine.find_first_not_of(" \t");

            if (FirstNonSpace == std::string::npos) {
                ImGui::Spacing();
                continue;
            }

            std::string Trimmed = RawLine.substr(FirstNonSpace);

            // Renders horizontal rules (---, ***, ___).
            if (Trimmed == "---" || Trimmed == "***" || Trimmed == "___") {
                ImGui::Separator();
                ImGui::Spacing();
                continue;
            }

            // Renders Markdown headings (# through ####).
            if (Trimmed[ZeroIdx] == '#') {
                size_t HashCount = ZeroIdx;

                while (HashCount < Trimmed.size() && Trimmed[HashCount] == '#') {
                    HashCount++;
                }

                if (HashCount < Trimmed.size() && Trimmed[HashCount] == ' ') {
                    std::string HeadingText = Trimmed.substr(HashCount + OneStep);
                    float Scale = HeadingScaleFour;

                    if (HashCount == OneStep) {
                        Scale = HeadingScaleOne;
                    } else if (HashCount == TwoStep) {
                        Scale = HeadingScaleTwo;
                    } else if (HashCount == ThreeStep) {
                        Scale = HeadingScaleThree;
                    }

                    if (BoldFont != nullptr) {
                        ImGui::PushFont(BoldFont);
                    }

                    ImGui::SetWindowFontScale(Scale);
                    ImGui::TextColored(ImVec4(HeadingRed, HeadingGreen, HeadingBlue, FullAlpha), "%s", HeadingText.c_str());
                    ImGui::SetWindowFontScale(FullAlpha);

                    if (BoldFont != nullptr) {
                        ImGui::PopFont();
                    }

                    if (HashCount <= TwoStep) {
                        ImGui::Separator();
                    }

                    ImGui::Spacing();
                    continue;
                }
            }

            // Renders blockquotes (> ...).
            if (Trimmed[ZeroIdx] == '>') {
                std::string QuoteText = (Trimmed.size() > OneStep && Trimmed[OneStep] == ' ') ? Trimmed.substr(TwoStep) : Trimmed.substr(OneStep);
                ImGui::Indent(ListIndent);
                ImGui::TextColored(ImVec4(QuoteRed, QuoteGreen, QuoteBlue, FullAlpha), "| %s", QuoteText.c_str());
                ImGui::Unindent(ListIndent);
                continue;
            }

            // Renders task list items (- [ ] / - [x]) and bullet lists (- / * / +).
            if (Trimmed.size() >= TwoStep && (Trimmed[ZeroIdx] == '-' || Trimmed[ZeroIdx] == '*' || Trimmed[ZeroIdx] == '+') && Trimmed[OneStep] == ' ') {
                ImGui::Indent(ListIndent);

                if (Trimmed.size() >= TaskPrefixLen && (Trimmed.rfind("- [ ] ", ZeroIdx) == ZeroIdx || Trimmed.rfind("- [x] ", ZeroIdx) == ZeroIdx || Trimmed.rfind("- [X] ", ZeroIdx) == ZeroIdx)) {
                    bool Checked = (Trimmed[ThreeStep] == 'x' || Trimmed[ThreeStep] == 'X');
                    ImGui::TextColored(Checked ? ImVec4(CheckGreen, CheckRed, CheckGreen, FullAlpha) : ImVec4(QuoteRed, QuoteGreen, QuoteBlue, FullAlpha), Checked ? "[x]" : "[ ]");
                    ImGui::SameLine();
                    RenderInlineFormattedText(Trimmed.substr(TaskPrefixLen), BoldFont, CodeFont);
                } else {
                    ImGui::Bullet();
                    RenderInlineFormattedText(Trimmed.substr(TwoStep), BoldFont, CodeFont);
                }

                ImGui::Unindent(ListIndent);
                continue;
            }

            // Renders numbered list items (1. ...).
            size_t DotPos = Trimmed.find(". ");

            if (DotPos != std::string::npos && DotPos > ZeroIdx && DotPos <= ThreeStep) {
                const int ZeroDigit = 0;
                bool AllDigits = true;

                for (size_t Idx = ZeroIdx; Idx < DotPos; ++Idx) {
                    if (std::isdigit(static_cast<unsigned char>(Trimmed[Idx])) == ZeroDigit) {
                        AllDigits = false;
                        break;
                    }
                }

                if (AllDigits) {
                    ImGui::Indent(ListIndent);
                    std::string NumPrefix = Trimmed.substr(ZeroIdx, DotPos + OneStep);
                    ImGui::TextUnformatted(NumPrefix.c_str());
                    ImGui::SameLine();
                    RenderInlineFormattedText(Trimmed.substr(DotPos + TwoStep), BoldFont, CodeFont);
                    ImGui::Unindent(ListIndent);
                    continue;
                }
            }

            RenderInlineFormattedText(Trimmed, BoldFont, CodeFont);
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
};

#endif
