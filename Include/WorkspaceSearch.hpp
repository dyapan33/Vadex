#ifndef WORKSPACE_SEARCH_HPP
#define WORKSPACE_SEARCH_HPP
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cctype>



// Represents a single text match found during a workspace-wide search.
struct WorkspaceSearchMatch {
    std::string FilePath;
    std::string RelativePath;
    int LineNumber;
    std::string LinePreview;
};

// Performs fast recursive multi-file text search across an open workspace directory.
class WorkspaceSearch {
private:
    // Checks whether a file extension represents a binary file that should be skipped.
    static bool IsBinaryExtension(const std::string& Ext) {
        return (Ext == ".exe" || Ext == ".dll" || Ext == ".o" || Ext == ".obj" || Ext == ".a" || Ext == ".lib" || Ext == ".png" || Ext == ".jpg" || Ext == ".jpeg" || Ext == ".ico" || Ext == ".ttf" || Ext == ".zip" || Ext == ".pdf");
    }

    // Converts a string to lowercase for case-insensitive matching.
    static std::string ToLowerCopy(const std::string& Input) {
        std::string Out = Input;
        std::transform(Out.begin(), Out.end(), Out.begin(), [](unsigned char Ch) {
            return static_cast<char>(std::tolower(Ch));
        });

        return Out;
    }

public:
    // Scans all text files under RootFolder for Query and returns up to MaxResults matches.
    static std::vector<WorkspaceSearchMatch> Search(const std::string& RootFolder, const std::string& Query, bool CaseSensitive) {
        const size_t MaxResults = 300;
        const size_t MaxPreviewLength = 95;
        const int OneLine = 1;
        const size_t ZeroIdx = 0;

        std::vector<WorkspaceSearchMatch> Results;

        if (RootFolder.empty() || Query.empty()) {
            return Results;
        }

        std::string EffectiveQuery = CaseSensitive ? Query : ToLowerCopy(Query);
        std::error_code ErrorCode;
        std::filesystem::path RootPath(RootFolder);

        for (auto Iter = std::filesystem::recursive_directory_iterator(RootPath, std::filesystem::directory_options::skip_permission_denied, ErrorCode);
             Iter != std::filesystem::recursive_directory_iterator();
             Iter.increment(ErrorCode)) {
            if (ErrorCode || Results.size() >= MaxResults) {
                break;
            }

            const std::filesystem::directory_entry& Entry = *Iter;
            std::string EntryName = Entry.path().filename().string();

            if (Entry.is_directory()) {
                if (EntryName == ".git" || EntryName == "node_modules" || EntryName == "ObjFiles" || EntryName == "Build" || EntryName == "Inspiration") {
                    Iter.disable_recursion_pending();
                }
                continue;
            }

            if (!Entry.is_regular_file()) {
                continue;
            }

            std::string LowerExt = ToLowerCopy(Entry.path().extension().string());

            if (IsBinaryExtension(LowerExt)) {
                continue;
            }

            std::ifstream FileStream(Entry.path());

            if (!FileStream.is_open()) {
                continue;
            }

            std::error_code RelErr;
            std::string RelPath = std::filesystem::relative(Entry.path(), RootPath, RelErr).generic_string();

            if (RelErr || RelPath.empty()) {
                RelPath = EntryName;
            }

            std::string Line;
            int LineNumber = OneLine;

            while (std::getline(FileStream, Line)) {
                if (Results.size() >= MaxResults) {
                    break;
                }

                std::string CompareLine = CaseSensitive ? Line : ToLowerCopy(Line);

                if (CompareLine.find(EffectiveQuery) != std::string::npos) {
                    size_t TrimStart = Line.find_first_not_of(" \t");
                    std::string Preview = (TrimStart == std::string::npos) ? Line : Line.substr(TrimStart);

                    if (Preview.size() > MaxPreviewLength) {
                        Preview = Preview.substr(ZeroIdx, MaxPreviewLength) + "...";
                    }

                    Results.push_back({ Entry.path().string(), RelPath, LineNumber, Preview });
                }

                LineNumber++;
            }
        }

        return Results;
    }

    // Replaces all occurrences of Query with Replacement across text files in RootFolder.
    static int ReplaceAll(const std::string& RootFolder, const std::string& Query, const std::string& Replacement, bool CaseSensitive, int& OutFilesChanged) {
        const size_t ZeroIdx = 0;
        const int ZeroInt = 0;

        OutFilesChanged = ZeroInt;
        int TotalReplacements = ZeroInt;

        if (RootFolder.empty() || Query.empty()) {
            return TotalReplacements;
        }

        std::string EffectiveQuery = CaseSensitive ? Query : ToLowerCopy(Query);
        std::error_code ErrorCode;
        std::filesystem::path RootPath(RootFolder);

        for (auto Iter = std::filesystem::recursive_directory_iterator(RootPath, std::filesystem::directory_options::skip_permission_denied, ErrorCode);
             Iter != std::filesystem::recursive_directory_iterator();
             Iter.increment(ErrorCode)) {
            if (ErrorCode) {
                break;
            }

            const std::filesystem::directory_entry& Entry = *Iter;
            std::string EntryName = Entry.path().filename().string();

            if (Entry.is_directory()) {
                if (EntryName == ".git" || EntryName == "node_modules" || EntryName == "ObjFiles" || EntryName == "Build" || EntryName == "Inspiration") {
                    Iter.disable_recursion_pending();
                }
                continue;
            }

            if (!Entry.is_regular_file()) {
                continue;
            }

            std::string LowerExt = ToLowerCopy(Entry.path().extension().string());

            if (IsBinaryExtension(LowerExt)) {
                continue;
            }

            std::ifstream InFile(Entry.path(), std::ios::binary);

            if (!InFile.is_open()) {
                continue;
            }

            std::string Content((std::istreambuf_iterator<char>(InFile)), std::istreambuf_iterator<char>());
            InFile.close();

            std::string CompareBuf = CaseSensitive ? Content : ToLowerCopy(Content);
            std::string NewContent;
            size_t Pos = ZeroIdx;
            int FileCount = ZeroInt;

            while (Pos < Content.size()) {
                size_t Found = CompareBuf.find(EffectiveQuery, Pos);

                if (Found == std::string::npos) {
                    NewContent.append(Content, Pos, std::string::npos);
                    break;
                }

                NewContent.append(Content, Pos, Found - Pos);
                NewContent.append(Replacement);
                Pos = Found + Query.size();
                FileCount++;
            }

            if (FileCount > ZeroInt) {
                std::ofstream OutFile(Entry.path(), std::ios::binary | std::ios::trunc);

                if (OutFile.is_open()) {
                    OutFile.write(NewContent.data(), static_cast<std::streamsize>(NewContent.size()));
                    OutFile.close();
                    TotalReplacements += FileCount;
                    OutFilesChanged++;
                }
            }
        }

        return TotalReplacements;
    }
};

#endif
