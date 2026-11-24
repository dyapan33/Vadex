#ifndef ICON_HPP
#define ICON_HPP
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include "ImGUI/imgui.h"



const float DefaultIconStrokeThickness = 1;
const float DefaultIconDimension = 10;

// Represents a 2D point coordinate.
struct IconPoint {
    float X;
    float Y;
};

// Represents a polyline path.
struct IconPolyline {
    std::vector<IconPoint> Points;
};

// Represents an SVG vector icon that loads from the Lucide API or disk and draws onto an ImGui draw list.
class Icon {
private:
    std::vector<IconPolyline> Polylines;
    float ViewBoxSize;

    // Checks if a character is whitespace or separator.
    bool IsSeparator(char Character) const {
        return (Character == ' ' || Character == ',' || Character == '\t' || Character == '\r' || Character == '\n');
    }

    // Checks if a character is an SVG path command letter.
    bool IsCommand(char Character) const {
        const std::string Commands = "MmLlHhVvAaCcSsQqZz";

        return (Commands.find(Character) != std::string::npos);
    }

    // Reads the next floating point number from the path string.
    float ReadNumber(const std::string& PathString, size_t& CharIndex) const {
        const float ZeroVal = 0;

        while (CharIndex < PathString.size() && IsSeparator(PathString[CharIndex])) {
            CharIndex++;
        }

        if (CharIndex >= PathString.size()) {
            return ZeroVal;
        }

        size_t StartIndex = CharIndex;

        if (PathString[CharIndex] == '+' || PathString[CharIndex] == '-') {
            CharIndex++;
        }

        bool HasDecimalPoint = false;

        while (CharIndex < PathString.size()) {
            char CurrentChar = PathString[CharIndex];

            if (CurrentChar >= '0' && CurrentChar <= '9') {
                CharIndex++;
            } else if (CurrentChar == '.' && !HasDecimalPoint) {
                HasDecimalPoint = true;
                CharIndex++;
            } else {
                break;
            }
        }

        std::string NumberString = PathString.substr(StartIndex, CharIndex - StartIndex);

        if (NumberString.empty() || NumberString == "+" || NumberString == "-") {
            return ZeroVal;
        }

        return std::stof(NumberString);
    }

    // Extracts a floating-point attribute value from an SVG tag string.
    float ReadTagAttribute(const std::string& TagStr, const std::string& AttrName, float DefaultVal) const {
        const size_t OneStep = 1;

        std::string Pattern = AttrName + "=\"";
        size_t Pos = TagStr.find(Pattern);

        if (Pos == std::string::npos) {
            return DefaultVal;
        }

        size_t ValStart = Pos + Pattern.size();
        size_t ValEnd = TagStr.find('"', ValStart);

        if (ValEnd == std::string::npos || ValEnd <= ValStart) {
            return DefaultVal;
        }

        try {
            return std::stof(TagStr.substr(ValStart, ValEnd - ValStart));
        } catch (...) {
            return DefaultVal;
        }

        (void)OneStep;
    }

    // Extracts a string attribute value from an SVG tag string.
    std::string ReadTagStringAttribute(const std::string& TagStr, const std::string& AttrName) const {
        std::string Pattern = AttrName + "=\"";
        size_t Pos = TagStr.find(Pattern);

        if (Pos == std::string::npos) {
            return "";
        }

        size_t ValStart = Pos + Pattern.size();
        size_t ValEnd = TagStr.find('"', ValStart);

        if (ValEnd == std::string::npos || ValEnd <= ValStart) {
            return "";
        }

        return TagStr.substr(ValStart, ValEnd - ValStart);
    }

    // Appends cubic Bezier curve points to the active polyline.
    void AppendCubicBezierPoints(std::vector<IconPoint>& Points, float X0, float Y0, float X1, float Y1, float X2, float Y2, float X3, float Y3) {
        const int CurveSegments = 9;
        const int OneInt = 1;
        const float OneVal = 1;
        const float ThreeVal = 3;

        for (int SegIdx = OneInt; SegIdx <= CurveSegments; ++SegIdx) {
            float T = static_cast<float>(SegIdx) / static_cast<float>(CurveSegments);
            float InvT = OneVal - T;
            float Px = (InvT * InvT * InvT * X0) + (ThreeVal * InvT * InvT * T * X1) + (ThreeVal * InvT * T * T * X2) + (T * T * T * X3);
            float Py = (InvT * InvT * InvT * Y0) + (ThreeVal * InvT * InvT * T * Y1) + (ThreeVal * InvT * T * T * Y2) + (T * T * T * Y3);

            IconPoint Pt;
            Pt.X = Px;
            Pt.Y = Py;
            Points.push_back(Pt);
        }
    }

    // Appends quadratic Bezier curve points to the active polyline.
    void AppendQuadBezierPoints(std::vector<IconPoint>& Points, float X0, float Y0, float X1, float Y1, float X2, float Y2) {
        const int CurveSegments = 7;
        const int OneInt = 1;
        const float OneVal = 1;
        const float TwoVal = 1 + 1;

        for (int SegIdx = OneInt; SegIdx <= CurveSegments; ++SegIdx) {
            float T = static_cast<float>(SegIdx) / static_cast<float>(CurveSegments);
            float InvT = OneVal - T;
            float Px = (InvT * InvT * X0) + (TwoVal * InvT * T * X1) + (T * T * X2);
            float Py = (InvT * InvT * Y0) + (TwoVal * InvT * T * Y1) + (T * T * Y2);

            IconPoint Pt;
            Pt.X = Px;
            Pt.Y = Py;
            Points.push_back(Pt);
        }
    }

    // Calculates and appends arc points to the polyline using center parameterization.
    void AppendArcPoints(std::vector<IconPoint>& Points, float StartX, float StartY, float RadiusX, float RadiusY, float AngleDeg, int LargeArcFlag, int SweepFlag, float EndX, float EndY) {
        const float ZeroVal = 0;
        const float OneVal = 1;
        const float NegativeOneVal = -1;
        const float HalfConstant = 0.5f;
        const float PiConstant = 3.14159f;
        const float AngleConversion = 180;
        const int SweepZero = 0;
        const int SweepOne = 1;
        const int ArcSegments = 10;

        if (StartX == EndX && StartY == EndY) {
            return;
        }

        if (RadiusX == ZeroVal || RadiusY == ZeroVal) {
            IconPoint TargetPoint;
            TargetPoint.X = EndX;
            TargetPoint.Y = EndY;

            Points.push_back(TargetPoint);

            return;
        }

        RadiusX = std::abs(RadiusX);
        RadiusY = std::abs(RadiusY);

        float PhiAngle = AngleDeg * (PiConstant / AngleConversion);
        float CosPhi = std::cos(PhiAngle);
        float SinPhi = std::sin(PhiAngle);

        float DeltaX = (StartX - EndX) * HalfConstant;
        float DeltaY = (StartY - EndY) * HalfConstant;
        float X1Prime = CosPhi * DeltaX + SinPhi * DeltaY;
        float Y1Prime = -SinPhi * DeltaX + CosPhi * DeltaY;

        float RadiiCheck = (X1Prime * X1Prime) / (RadiusX * RadiusX) + (Y1Prime * Y1Prime) / (RadiusY * RadiusY);

        if (RadiiCheck > OneVal) {
            float ScaleFactor = std::sqrt(RadiiCheck);
            RadiusX *= ScaleFactor;
            RadiusY *= ScaleFactor;
        }

        float SignMultiplier = (LargeArcFlag == SweepFlag) ? NegativeOneVal : OneVal;
        float NumeratorVal = (RadiusX * RadiusX * RadiusY * RadiusY) - (RadiusX * RadiusX * Y1Prime * Y1Prime) - (RadiusY * RadiusY * X1Prime * X1Prime);

        if (NumeratorVal < ZeroVal) {
            NumeratorVal = ZeroVal;
        }

        float DenominatorVal = (RadiusX * RadiusX * Y1Prime * Y1Prime) + (RadiusY * RadiusY * X1Prime * X1Prime);
        float SqrtTerm = (DenominatorVal > ZeroVal) ? (NumeratorVal / DenominatorVal) : ZeroVal;
        float CoeffVal = SignMultiplier * std::sqrt(SqrtTerm);

        float CxPrime = CoeffVal * ((RadiusX * Y1Prime) / RadiusY);
        float CyPrime = CoeffVal * (-(RadiusY * X1Prime) / RadiusX);
        float CenterX = CosPhi * CxPrime - SinPhi * CyPrime + (StartX + EndX) * HalfConstant;
        float CenterY = SinPhi * CxPrime + CosPhi * CyPrime + (StartY + EndY) * HalfConstant;

        float Ux = (X1Prime - CxPrime) / RadiusX;
        float Uy = (Y1Prime - CyPrime) / RadiusY;
        float Vx = (-X1Prime - CxPrime) / RadiusX;
        float Vy = (-Y1Prime - CyPrime) / RadiusY;

        // Computes angle between two unit vectors.
        auto ComputeAngle = [ZeroVal, OneVal, NegativeOneVal](float FirstX, float FirstY, float SecondX, float SecondY) {
            float DotProduct = FirstX * SecondX + FirstY * SecondY;
            float FirstLength = std::sqrt(FirstX * FirstX + FirstY * FirstY);
            float SecondLength = std::sqrt(SecondX * SecondX + SecondY * SecondY);
            float CosineVal = DotProduct / (FirstLength * SecondLength);

            if (CosineVal < NegativeOneVal) {
                CosineVal = NegativeOneVal;
            }
            if (CosineVal > OneVal) {
                CosineVal = OneVal;
            }

            float ComputedRad = std::acos(CosineVal);

            if ((FirstX * SecondY - FirstY * SecondX) < ZeroVal) {
                ComputedRad = -ComputedRad;
            }

            return ComputedRad;
        };

        float StartAngle = ComputeAngle(OneVal, ZeroVal, Ux, Uy);
        float DeltaAngle = ComputeAngle(Ux, Uy, Vx, Vy);
        const float FullCircleAngle = (OneVal + OneVal) * PiConstant;

        if (SweepFlag == SweepZero && DeltaAngle > ZeroVal) {
            DeltaAngle -= FullCircleAngle;
        } else if (SweepFlag == SweepOne && DeltaAngle < ZeroVal) {
            DeltaAngle += FullCircleAngle;
        }

        for (int SegmentIndex = SweepOne; SegmentIndex <= ArcSegments; ++SegmentIndex) {
            float FractionVal = static_cast<float>(SegmentIndex) / static_cast<float>(ArcSegments);
            float CurrentAngle = StartAngle + DeltaAngle * FractionVal;
            float EllipseX = RadiusX * std::cos(CurrentAngle);
            float EllipseY = RadiusY * std::sin(CurrentAngle);
            float PointX = CosPhi * EllipseX - SinPhi * EllipseY + CenterX;
            float PointY = SinPhi * EllipseX + CosPhi * EllipseY + CenterY;

            IconPoint InterpolatedPoint;
            InterpolatedPoint.X = PointX;
            InterpolatedPoint.Y = PointY;

            Points.push_back(InterpolatedPoint);
        }
    }

    // Parses SVG path commands into polyline points.
    void ParsePathData(const std::string& PathData) {
        const size_t StartIndex = 0;
        const float ZeroCoord = 0;
        const float TwoFactor = 1 + 1;

        size_t DataIndex = StartIndex;
        char ActiveCommand = '\0';
        float CurrentX = ZeroCoord;
        float CurrentY = ZeroCoord;
        float LastCtrlX = ZeroCoord;
        float LastCtrlY = ZeroCoord;
        bool HasLastCtrl = false;
        IconPolyline ActivePolyline;

        while (DataIndex < PathData.size()) {
            while (DataIndex < PathData.size() && IsSeparator(PathData[DataIndex])) {
                DataIndex++;
            }

            if (DataIndex >= PathData.size()) {
                break;
            }

            if (IsCommand(PathData[DataIndex])) {
                ActiveCommand = PathData[DataIndex];
                DataIndex++;
            }

            if (ActiveCommand == 'M' || ActiveCommand == 'm') {
                float CoordX = ReadNumber(PathData, DataIndex);
                float CoordY = ReadNumber(PathData, DataIndex);

                if (ActiveCommand == 'm') {
                    CurrentX += CoordX;
                    CurrentY += CoordY;
                } else {
                    CurrentX = CoordX;
                    CurrentY = CoordY;
                }

                if (!ActivePolyline.Points.empty()) {
                    Polylines.push_back(ActivePolyline);
                    ActivePolyline.Points.clear();
                }

                IconPoint StartPoint;
                StartPoint.X = CurrentX;
                StartPoint.Y = CurrentY;

                ActivePolyline.Points.push_back(StartPoint);
                ActiveCommand = (ActiveCommand == 'm') ? 'l' : 'L';
                HasLastCtrl = false;
            } else if (ActiveCommand == 'L' || ActiveCommand == 'l') {
                float CoordX = ReadNumber(PathData, DataIndex);
                float CoordY = ReadNumber(PathData, DataIndex);

                if (ActiveCommand == 'l') {
                    CurrentX += CoordX;
                    CurrentY += CoordY;
                } else {
                    CurrentX = CoordX;
                    CurrentY = CoordY;
                }

                IconPoint LinePoint;
                LinePoint.X = CurrentX;
                LinePoint.Y = CurrentY;

                ActivePolyline.Points.push_back(LinePoint);
                HasLastCtrl = false;
            } else if (ActiveCommand == 'H' || ActiveCommand == 'h') {
                float CoordX = ReadNumber(PathData, DataIndex);

                if (ActiveCommand == 'h') {
                    CurrentX += CoordX;
                } else {
                    CurrentX = CoordX;
                }

                IconPoint HorizontalPoint;
                HorizontalPoint.X = CurrentX;
                HorizontalPoint.Y = CurrentY;

                ActivePolyline.Points.push_back(HorizontalPoint);
                HasLastCtrl = false;
            } else if (ActiveCommand == 'V' || ActiveCommand == 'v') {
                float CoordY = ReadNumber(PathData, DataIndex);

                if (ActiveCommand == 'v') {
                    CurrentY += CoordY;
                } else {
                    CurrentY = CoordY;
                }

                IconPoint VerticalPoint;
                VerticalPoint.X = CurrentX;
                VerticalPoint.Y = CurrentY;

                ActivePolyline.Points.push_back(VerticalPoint);
                HasLastCtrl = false;
            } else if (ActiveCommand == 'C' || ActiveCommand == 'c') {
                float X1 = ReadNumber(PathData, DataIndex);
                float Y1 = ReadNumber(PathData, DataIndex);
                float X2 = ReadNumber(PathData, DataIndex);
                float Y2 = ReadNumber(PathData, DataIndex);
                float X3 = ReadNumber(PathData, DataIndex);
                float Y3 = ReadNumber(PathData, DataIndex);

                if (ActiveCommand == 'c') {
                    X1 += CurrentX;
                    Y1 += CurrentY;
                    X2 += CurrentX;
                    Y2 += CurrentY;
                    X3 += CurrentX;
                    Y3 += CurrentY;
                }

                AppendCubicBezierPoints(ActivePolyline.Points, CurrentX, CurrentY, X1, Y1, X2, Y2, X3, Y3);
                LastCtrlX = X2;
                LastCtrlY = Y2;
                HasLastCtrl = true;
                CurrentX = X3;
                CurrentY = Y3;
            } else if (ActiveCommand == 'S' || ActiveCommand == 's') {
                float X1 = HasLastCtrl ? (TwoFactor * CurrentX - LastCtrlX) : CurrentX;
                float Y1 = HasLastCtrl ? (TwoFactor * CurrentY - LastCtrlY) : CurrentY;
                float X2 = ReadNumber(PathData, DataIndex);
                float Y2 = ReadNumber(PathData, DataIndex);
                float X3 = ReadNumber(PathData, DataIndex);
                float Y3 = ReadNumber(PathData, DataIndex);

                if (ActiveCommand == 's') {
                    X2 += CurrentX;
                    Y2 += CurrentY;
                    X3 += CurrentX;
                    Y3 += CurrentY;
                }

                AppendCubicBezierPoints(ActivePolyline.Points, CurrentX, CurrentY, X1, Y1, X2, Y2, X3, Y3);
                LastCtrlX = X2;
                LastCtrlY = Y2;
                HasLastCtrl = true;
                CurrentX = X3;
                CurrentY = Y3;
            } else if (ActiveCommand == 'Q' || ActiveCommand == 'q') {
                float X1 = ReadNumber(PathData, DataIndex);
                float Y1 = ReadNumber(PathData, DataIndex);
                float X2 = ReadNumber(PathData, DataIndex);
                float Y2 = ReadNumber(PathData, DataIndex);

                if (ActiveCommand == 'q') {
                    X1 += CurrentX;
                    Y1 += CurrentY;
                    X2 += CurrentX;
                    Y2 += CurrentY;
                }

                AppendQuadBezierPoints(ActivePolyline.Points, CurrentX, CurrentY, X1, Y1, X2, Y2);
                LastCtrlX = X1;
                LastCtrlY = Y1;
                HasLastCtrl = true;
                CurrentX = X2;
                CurrentY = Y2;
            } else if (ActiveCommand == 'A' || ActiveCommand == 'a') {
                float RadiusX = ReadNumber(PathData, DataIndex);
                float RadiusY = ReadNumber(PathData, DataIndex);
                float RotationAngle = ReadNumber(PathData, DataIndex);
                int LargeArc = static_cast<int>(ReadNumber(PathData, DataIndex));
                int SweepFlag = static_cast<int>(ReadNumber(PathData, DataIndex));
                float TargetCoordX = ReadNumber(PathData, DataIndex);
                float TargetCoordY = ReadNumber(PathData, DataIndex);
                float FinalX = (ActiveCommand == 'a') ? (CurrentX + TargetCoordX) : TargetCoordX;
                float FinalY = (ActiveCommand == 'a') ? (CurrentY + TargetCoordY) : TargetCoordY;

                AppendArcPoints(ActivePolyline.Points, CurrentX, CurrentY, RadiusX, RadiusY, RotationAngle, LargeArc, SweepFlag, FinalX, FinalY);

                CurrentX = FinalX;
                CurrentY = FinalY;
                HasLastCtrl = false;
            } else if (ActiveCommand == 'Z' || ActiveCommand == 'z') {
                if (!ActivePolyline.Points.empty()) {
                    ActivePolyline.Points.push_back(ActivePolyline.Points.front());
                    Polylines.push_back(ActivePolyline);
                    ActivePolyline.Points.clear();
                }

                HasLastCtrl = false;
            } else {
                break;
            }
        }

        if (!ActivePolyline.Points.empty()) {
            Polylines.push_back(ActivePolyline);
        }
    }

    // Parses all Lucide SVG primitives (<path>, <line>, <rect>, <circle>, <polyline>, <polygon>).
    bool ParseSvgString(const std::string& SvgContent) {
        const size_t StartPos = 0;
        const size_t OneStep = 1;
        const float ZeroFloat = 0;
        const float PiVal = 3.14159f;
        const float TwoVal = 1 + 1;
        const int CircleSegments = 19 + 1;
        const int ZeroInt = 0;

        Polylines.clear();

        size_t TagPos = StartPos;

        while ((TagPos = SvgContent.find('<', TagPos)) != std::string::npos) {
            size_t TagEnd = SvgContent.find('>', TagPos);

            if (TagEnd == std::string::npos) {
                break;
            }

            std::string Tag = SvgContent.substr(TagPos, TagEnd - TagPos + OneStep);
            TagPos = TagEnd + OneStep;

            if (Tag.rfind("<path", StartPos) == StartPos) {
                std::string PathStr = ReadTagStringAttribute(Tag, "d");

                if (!PathStr.empty()) {
                    ParsePathData(PathStr);
                }
            } else if (Tag.rfind("<line", StartPos) == StartPos) {
                float X1 = ReadTagAttribute(Tag, "x1", ZeroFloat);
                float Y1 = ReadTagAttribute(Tag, "y1", ZeroFloat);
                float X2 = ReadTagAttribute(Tag, "x2", ZeroFloat);
                float Y2 = ReadTagAttribute(Tag, "y2", ZeroFloat);

                IconPolyline LinePoly;
                LinePoly.Points.push_back({ X1, Y1 });
                LinePoly.Points.push_back({ X2, Y2 });
                Polylines.push_back(LinePoly);
            } else if (Tag.rfind("<rect", StartPos) == StartPos) {
                float X = ReadTagAttribute(Tag, "x", ZeroFloat);
                float Y = ReadTagAttribute(Tag, "y", ZeroFloat);
                float W = ReadTagAttribute(Tag, "width", ZeroFloat);
                float H = ReadTagAttribute(Tag, "height", ZeroFloat);
                float Rx = ReadTagAttribute(Tag, "rx", ZeroFloat);

                if (Rx > W / TwoVal) {
                    Rx = W / TwoVal;
                }
                if (Rx > H / TwoVal) {
                    Rx = H / TwoVal;
                }

                IconPolyline RectPoly;

                if (Rx > ZeroFloat) {
                    const int ZeroFlag = 0;
                    const int OneFlag = 1;

                    RectPoly.Points.push_back({ X + Rx, Y });
                    RectPoly.Points.push_back({ X + W - Rx, Y });
                    AppendArcPoints(RectPoly.Points, X + W - Rx, Y, Rx, Rx, ZeroFloat, ZeroFlag, OneFlag, X + W, Y + Rx);
                    RectPoly.Points.push_back({ X + W, Y + H - Rx });
                    AppendArcPoints(RectPoly.Points, X + W, Y + H - Rx, Rx, Rx, ZeroFloat, ZeroFlag, OneFlag, X + W - Rx, Y + H);
                    RectPoly.Points.push_back({ X + Rx, Y + H });
                    AppendArcPoints(RectPoly.Points, X + Rx, Y + H, Rx, Rx, ZeroFloat, ZeroFlag, OneFlag, X, Y + H - Rx);
                    RectPoly.Points.push_back({ X, Y + Rx });
                    AppendArcPoints(RectPoly.Points, X, Y + Rx, Rx, Rx, ZeroFloat, ZeroFlag, OneFlag, X + Rx, Y);
                } else {
                    RectPoly.Points.push_back({ X, Y });
                    RectPoly.Points.push_back({ X + W, Y });
                    RectPoly.Points.push_back({ X + W, Y + H });
                    RectPoly.Points.push_back({ X, Y + H });
                    RectPoly.Points.push_back({ X, Y });
                }

                Polylines.push_back(RectPoly);
            } else if (Tag.rfind("<circle", StartPos) == StartPos) {
                float Cx = ReadTagAttribute(Tag, "cx", ZeroFloat);
                float Cy = ReadTagAttribute(Tag, "cy", ZeroFloat);
                float Radius = ReadTagAttribute(Tag, "r", ZeroFloat);

                if (Radius > ZeroFloat) {
                    IconPolyline CirclePoly;

                    for (int Seg = ZeroInt; Seg <= CircleSegments; ++Seg) {
                        float Angle = (TwoVal * PiVal * static_cast<float>(Seg)) / static_cast<float>(CircleSegments);
                        CirclePoly.Points.push_back({ Cx + Radius * std::cos(Angle), Cy + Radius * std::sin(Angle) });
                    }

                    Polylines.push_back(CirclePoly);
                }
            } else if (Tag.rfind("<polyline", StartPos) == StartPos || Tag.rfind("<polygon", StartPos) == StartPos) {
                bool IsPolygon = (Tag.rfind("<polygon", StartPos) == StartPos);
                std::string PointsStr = ReadTagStringAttribute(Tag, "points");

                if (!PointsStr.empty()) {
                    size_t Idx = StartPos;
                    IconPolyline Poly;

                    while (Idx < PointsStr.size()) {
                        while (Idx < PointsStr.size() && IsSeparator(PointsStr[Idx])) {
                            Idx++;
                        }

                        if (Idx >= PointsStr.size()) {
                            break;
                        }

                        float Px = ReadNumber(PointsStr, Idx);
                        float Py = ReadNumber(PointsStr, Idx);
                        Poly.Points.push_back({ Px, Py });
                    }

                    if (IsPolygon && !Poly.Points.empty()) {
                        Poly.Points.push_back(Poly.Points.front());
                    }

                    if (!Poly.Points.empty()) {
                        Polylines.push_back(Poly);
                    }
                }
            }
        }

        return !Polylines.empty();
    }

    // Downloads an SVG string over HTTPS from a host and path using WinHTTP.
    static std::string FetchHttpsSvg(const std::wstring& HostName, const std::wstring& RequestPath) {
        const DWORD ZeroDword = 0;
        const int TimeoutMs = 1500;
        const int ReadBufSize = 4095;

        HINTERNET Session = WinHttpOpen(L"VadexEditor/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, ZeroDword);

        if (Session == nullptr) {
            return "";
        }

        WinHttpSetTimeouts(Session, TimeoutMs, TimeoutMs, TimeoutMs, TimeoutMs);

        HINTERNET Connect = WinHttpConnect(Session, HostName.c_str(), INTERNET_DEFAULT_HTTPS_PORT, ZeroDword);

        if (Connect == nullptr) {
            WinHttpCloseHandle(Session);

            return "";
        }

        HINTERNET Request = WinHttpOpenRequest(Connect, L"GET", RequestPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

        if (Request == nullptr) {
            WinHttpCloseHandle(Connect);
            WinHttpCloseHandle(Session);

            return "";
        }

        std::string ResponseBody = "";

        if (WinHttpSendRequest(Request, WINHTTP_NO_ADDITIONAL_HEADERS, ZeroDword, WINHTTP_NO_REQUEST_DATA, ZeroDword, ZeroDword, ZeroDword) && WinHttpReceiveResponse(Request, nullptr)) {
            DWORD StatusCode = ZeroDword;
            DWORD StatusSize = sizeof(StatusCode);
            const DWORD HttpOkStatus = 200;

            WinHttpQueryHeaders(Request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &StatusCode, &StatusSize, WINHTTP_NO_HEADER_INDEX);

            if (StatusCode == HttpOkStatus) {
                DWORD BytesAvail = ZeroDword;

                while (WinHttpQueryDataAvailable(Request, &BytesAvail) && BytesAvail > ZeroDword) {
                    char Chunk[ReadBufSize];
                    DWORD ToRead = (BytesAvail < static_cast<DWORD>(ReadBufSize)) ? BytesAvail : static_cast<DWORD>(ReadBufSize);
                    DWORD BytesRead = ZeroDword;

                    if (WinHttpReadData(Request, Chunk, ToRead, &BytesRead) && BytesRead > ZeroDword) {
                        ResponseBody.append(Chunk, static_cast<size_t>(BytesRead));
                    } else {
                        break;
                    }
                }
            }
        }

        WinHttpCloseHandle(Request);
        WinHttpCloseHandle(Connect);
        WinHttpCloseHandle(Session);

        return ResponseBody;
    }

public:
    // Initializes the icon with the default Lucide 24x24 SVG viewbox size.
    Icon() {
        const float BaseViewBox = 25;
        const float OneUnit = 1;
        const float DefaultSize = BaseViewBox - OneUnit;

        ViewBoxSize = DefaultSize;
    }

    // Loads SVG paths from a file on disk.
    bool LoadFromFile(const std::string& FilePath) {
        std::ifstream FileStream(FilePath);

        if (!FileStream.is_open()) {
            return false;
        }

        std::stringstream StringStream;
        StringStream << FileStream.rdbuf();
        std::string SvgContent = StringStream.str();
        FileStream.close();

        return ParseSvgString(SvgContent);
    }

    // Loads a Lucide icon via the Lucide CDN API, caching it locally and falling back to disk if offline.
    bool LoadFromLucideApi(const std::string& LucideIconName, const std::string& LocalCachePath) {
        std::wstring WideIconName(LucideIconName.begin(), LucideIconName.end());
        std::wstring UnpkgPath = L"/lucide-static@latest/icons/" + WideIconName + L".svg";
        std::string SvgContent = FetchHttpsSvg(L"unpkg.com", UnpkgPath);

        if (SvgContent.find("<svg") == std::string::npos) {
            std::wstring JsDelivrPath = L"/npm/lucide-static@latest/icons/" + WideIconName + L".svg";
            SvgContent = FetchHttpsSvg(L"cdn.jsdelivr.net", JsDelivrPath);
        }

        if (SvgContent.find("<svg") != std::string::npos && ParseSvgString(SvgContent)) {
            if (!LocalCachePath.empty()) {
                std::ofstream OutFile(LocalCachePath);

                if (OutFile.is_open()) {
                    OutFile << SvgContent;
                    OutFile.close();
                }
            }

            return true;
        }

        if (!LocalCachePath.empty()) {
            return LoadFromFile(LocalCachePath);
        }

        return false;
    }

    // Draws the icon inside the given screen bounding box.
    void Draw(ImDrawList* DrawList, const ImVec2& RectMin, const ImVec2& RectMax, ImU32 Color, float StrokeThickness = DefaultIconStrokeThickness, float IconDimension = DefaultIconDimension) const {
        const float HalfConstant = 0.5f;
        const size_t OnePoint = 1;
        const size_t MinPolylinePoints = OnePoint + OnePoint;
        const size_t ZeroIndex = 0;

        if (DrawList == nullptr || Polylines.empty()) {
            return;
        }

        float CenterX = (RectMin.x + RectMax.x) * HalfConstant;
        float CenterY = (RectMin.y + RectMax.y) * HalfConstant;
        float HalfDimension = IconDimension * HalfConstant;
        ImVec2 BoundsMin(CenterX - HalfDimension, CenterY - HalfDimension);
        float ScalingRatio = IconDimension / ViewBoxSize;

        for (size_t PolyIndex = ZeroIndex; PolyIndex < Polylines.size(); ++PolyIndex) {
            const auto& CurrentPoly = Polylines[PolyIndex];

            if (CurrentPoly.Points.size() < MinPolylinePoints) {
                continue;
            }

            std::vector<ImVec2> ScreenPoints;
            ScreenPoints.reserve(CurrentPoly.Points.size());

            for (size_t PointIndex = ZeroIndex; PointIndex < CurrentPoly.Points.size(); ++PointIndex) {
                float PosX = BoundsMin.x + CurrentPoly.Points[PointIndex].X * ScalingRatio;
                float PosY = BoundsMin.y + CurrentPoly.Points[PointIndex].Y * ScalingRatio;

                ScreenPoints.push_back(ImVec2(PosX, PosY));
            }

            DrawList->AddPolyline(ScreenPoints.data(), static_cast<int>(ScreenPoints.size()), Color, ImDrawFlags_None, StrokeThickness);
        }
    }
};

#endif
