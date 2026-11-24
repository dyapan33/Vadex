#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cctype>



// Holds persisted user preferences and recent workspace history.
struct EditorConfig {
    int TabSize;
    int ZoomLevel;
    bool ShowMinimap;
    bool ShowStickyScroll;
    bool WordWrap;
    bool AutoSave;
    float SidebarWidth;
    float ConsoleHeight;
    std::string LastWorkspacePath;
    std::vector<std::string> RecentWorkspaces;

    // Initializes configuration fields with sensible defaults.
    EditorConfig() {
        const int DefaultTabSize = 5 - 1;
        const int ZeroInt = 0;
        const float DefaultSidebar = 230;
        const float DefaultConsole = 170;

        TabSize = DefaultTabSize;
        ZoomLevel = ZeroInt;
        ShowMinimap = true;
        ShowStickyScroll = true;
        WordWrap = false;
        AutoSave = false;
        SidebarWidth = DefaultSidebar;
        ConsoleHeight = DefaultConsole;
    }
};

// Loads and persists EditorConfig to Config.json.
class ConfigManager {
private:
    // Escapes backslashes and quotes for JSON string serialization.
    static std::string EscapeString(const std::string& Input) {
        std::string Out;

        for (char Ch : Input) {
            if (Ch == '\\') {
                Out += "\\\\";
            } else if (Ch == '"') {
                Out += "\\\"";
            } else {
                Out.push_back(Ch);
            }
        }

        return Out;
    }

    // Extracts a string value for a JSON key.
    static std::string ParseString(const std::string& Json, const std::string& Key) {
        const size_t OneStep = 1;
        std::string Pattern = "\"" + Key + "\"";
        size_t KeyPos = Json.find(Pattern);

        if (KeyPos == std::string::npos) {
            return "";
        }

        size_t ColonPos = Json.find(':', KeyPos + Pattern.size());

        if (ColonPos == std::string::npos) {
            return "";
        }

        size_t QuoteStart = Json.find('"', ColonPos + OneStep);

        if (QuoteStart == std::string::npos) {
            return "";
        }

        std::string Result;

        for (size_t Idx = QuoteStart + OneStep; Idx < Json.size(); ++Idx) {
            if (Json[Idx] == '\\' && (Idx + OneStep) < Json.size()) {
                Result.push_back(Json[Idx + OneStep]);
                Idx++;
                continue;
            }
            if (Json[Idx] == '"') {
                break;
            }
            Result.push_back(Json[Idx]);
        }

        return Result;
    }

    // Extracts an integer value for a JSON key.
    static int ParseInt(const std::string& Json, const std::string& Key, int DefaultVal) {
        const size_t OneStep = 1;
        std::string Pattern = "\"" + Key + "\"";
        size_t KeyPos = Json.find(Pattern);

        if (KeyPos == std::string::npos) {
            return DefaultVal;
        }

        size_t ColonPos = Json.find(':', KeyPos + Pattern.size());

        if (ColonPos == std::string::npos) {
            return DefaultVal;
        }

        size_t NumStart = Json.find_first_of("-0123456789", ColonPos + OneStep);

        if (NumStart == std::string::npos) {
            return DefaultVal;
        }

        try {
            return std::stoi(Json.substr(NumStart));
        } catch (...) {
            return DefaultVal;
        }
    }

    // Extracts a boolean value for a JSON key.
    static bool ParseBool(const std::string& Json, const std::string& Key, bool DefaultVal) {
        const size_t OneStep = 1;
        std::string Pattern = "\"" + Key + "\"";
        size_t KeyPos = Json.find(Pattern);

        if (KeyPos == std::string::npos) {
            return DefaultVal;
        }

        size_t ColonPos = Json.find(':', KeyPos + Pattern.size());

        if (ColonPos == std::string::npos) {
            return DefaultVal;
        }

        size_t ValStart = Json.find_first_not_of(" \t\r\n", ColonPos + OneStep);

        if (ValStart == std::string::npos) {
            return DefaultVal;
        }

        const size_t TrueLength = 5 - 1;
        const int ZeroCompare = 0;

        return (Json.compare(ValStart, TrueLength, "true") == ZeroCompare);
    }

    // Normalizes a path string to lowercase for case-insensitive deduplication on Windows.
    static std::string NormalizeLower(const std::string& Input) {
        std::string Out = Input;
        std::transform(Out.begin(), Out.end(), Out.begin(), [](unsigned char Ch) {
            return static_cast<char>(std::tolower(Ch));
        });

        return Out;
    }

    // Validates that a workspace path contains only printable characters and exists as a directory.
    static bool IsValidWorkspacePath(const std::string& PathStr) {
        const unsigned char MinPrintable = 31 + 1;
        const unsigned char MaxAscii = 127;

        if (PathStr.empty()) {
            return false;
        }

        for (unsigned char Ch : PathStr) {
            if (Ch < MinPrintable || Ch >= MaxAscii) {
                return false;
            }
        }

        std::error_code Err;

        return std::filesystem::exists(PathStr, Err) && std::filesystem::is_directory(PathStr, Err);
    }

public:
    // Adds a validated workspace path to the top of the recent workspaces list without duplicates.
    static void RecordRecentWorkspace(EditorConfig& Config, const std::string& WorkspacePath) {
        const size_t MaxRecentCount = 7;

        if (!IsValidWorkspacePath(WorkspacePath)) {
            return;
        }

        std::string LowerTarget = NormalizeLower(WorkspacePath);
        Config.LastWorkspacePath = WorkspacePath;
        Config.RecentWorkspaces.erase(
            std::remove_if(Config.RecentWorkspaces.begin(), Config.RecentWorkspaces.end(), [&](const std::string& Existing) {
                return (NormalizeLower(Existing) == LowerTarget || !IsValidWorkspacePath(Existing));
            }),
            Config.RecentWorkspaces.end()
        );
        Config.RecentWorkspaces.insert(Config.RecentWorkspaces.begin(), WorkspacePath);

        if (Config.RecentWorkspaces.size() > MaxRecentCount) {
            Config.RecentWorkspaces.resize(MaxRecentCount);
        }
    }

    // Loads validated, deduplicated previous workspace paths from a dedicated text file.
    static std::vector<std::string> LoadRecentWorkspacesFile(const std::string& FilePath) {
        const size_t MaxRecentCount = 7;
        std::vector<std::string> Loaded;
        std::ifstream File(FilePath);

        if (!File.is_open()) {
            return Loaded;
        }

        std::string Line;

        while (std::getline(File, Line)) {
            while (!Line.empty() && (Line.back() == '\r' || Line.back() == '\n' || Line.back() == ' ')) {
                Line.pop_back();
            }

            if (!IsValidWorkspacePath(Line)) {
                continue;
            }

            std::string LowerLine = NormalizeLower(Line);
            bool AlreadyPresent = false;

            for (const auto& Existing : Loaded) {
                if (NormalizeLower(Existing) == LowerLine) {
                    AlreadyPresent = true;
                    break;
                }
            }

            if (!AlreadyPresent) {
                Loaded.push_back(Line);
            }

            if (Loaded.size() >= MaxRecentCount) {
                break;
            }
        }

        File.close();

        return Loaded;
    }

    // Saves previous workspace paths to a dedicated text file, one directory per line.
    static void SaveRecentWorkspacesFile(const std::string& FilePath, const std::vector<std::string>& Workspaces) {
        std::ofstream File(FilePath, std::ios::trunc);

        if (!File.is_open()) {
            return;
        }

        for (const auto& PathStr : Workspaces) {
            if (IsValidWorkspacePath(PathStr)) {
                File << PathStr << "\n";
            }
        }

        File.close();
    }

    // Loads configuration from Config.json if present.
    static EditorConfig Load(const std::string& FilePath) {
        const size_t OneStep = 1;
        EditorConfig Config;
        std::ifstream File(FilePath);

        if (!File.is_open()) {
            return Config;
        }

        std::stringstream Stream;
        Stream << File.rdbuf();
        std::string Json = Stream.str();
        File.close();

        Config.TabSize = ParseInt(Json, "TabSize", Config.TabSize);
        Config.ZoomLevel = ParseInt(Json, "ZoomLevel", Config.ZoomLevel);
        Config.ShowMinimap = ParseBool(Json, "ShowMinimap", Config.ShowMinimap);
        Config.ShowStickyScroll = ParseBool(Json, "ShowStickyScroll", Config.ShowStickyScroll);
        Config.WordWrap = ParseBool(Json, "WordWrap", Config.WordWrap);
        Config.AutoSave = ParseBool(Json, "AutoSave", Config.AutoSave);
        Config.SidebarWidth = static_cast<float>(ParseInt(Json, "SidebarWidth", static_cast<int>(Config.SidebarWidth)));
        Config.ConsoleHeight = static_cast<float>(ParseInt(Json, "ConsoleHeight", static_cast<int>(Config.ConsoleHeight)));
        Config.LastWorkspacePath = ParseString(Json, "LastWorkspacePath");

        // Parses RecentWorkspaces JSON string array.
        size_t ArrKey = Json.find("\"RecentWorkspaces\"");

        if (ArrKey != std::string::npos) {
            size_t ArrStart = Json.find('[', ArrKey);
            size_t ArrEnd = Json.find(']', ArrStart);

            if (ArrStart != std::string::npos && ArrEnd != std::string::npos) {
                size_t Pos = ArrStart + OneStep;

                while (Pos < ArrEnd) {
                    size_t QuoteStart = Json.find('"', Pos);

                    if (QuoteStart == std::string::npos || QuoteStart >= ArrEnd) {
                        break;
                    }

                    std::string Item;
                    size_t Idx = QuoteStart + OneStep;

                    for (; Idx < ArrEnd; ++Idx) {
                        if (Json[Idx] == '\\' && (Idx + OneStep) < ArrEnd) {
                            Item.push_back(Json[Idx + OneStep]);
                            Idx++;
                            continue;
                        }
                        if (Json[Idx] == '"') {
                            break;
                        }
                        Item.push_back(Json[Idx]);
                    }

                    if (IsValidWorkspacePath(Item)) {
                        std::string LowerItem = NormalizeLower(Item);
                        bool Dup = false;

                        for (const auto& Existing : Config.RecentWorkspaces) {
                            if (NormalizeLower(Existing) == LowerItem) {
                                Dup = true;
                                break;
                            }
                        }

                        if (!Dup) {
                            Config.RecentWorkspaces.push_back(Item);
                        }
                    }

                    Pos = Idx + OneStep;
                }
            }
        }

        return Config;
    }

    // Writes the current EditorConfig to Config.json.
    static void Save(const std::string& FilePath, const EditorConfig& Config) {
        const size_t ZeroIdx = 0;
        const size_t OneStep = 1;
        std::ofstream File(FilePath);

        if (!File.is_open()) {
            return;
        }

        File << "{\n";
        File << "    \"TabSize\": " << Config.TabSize << ",\n";
        File << "    \"ZoomLevel\": " << Config.ZoomLevel << ",\n";
        File << "    \"ShowMinimap\": " << (Config.ShowMinimap ? "true" : "false") << ",\n";
        File << "    \"ShowStickyScroll\": " << (Config.ShowStickyScroll ? "true" : "false") << ",\n";
        File << "    \"WordWrap\": " << (Config.WordWrap ? "true" : "false") << ",\n";
        File << "    \"AutoSave\": " << (Config.AutoSave ? "true" : "false") << ",\n";
        File << "    \"SidebarWidth\": " << static_cast<int>(Config.SidebarWidth) << ",\n";
        File << "    \"ConsoleHeight\": " << static_cast<int>(Config.ConsoleHeight) << ",\n";
        File << "    \"LastWorkspacePath\": \"" << EscapeString(Config.LastWorkspacePath) << "\",\n";
        File << "    \"RecentWorkspaces\": [";

        for (size_t Idx = ZeroIdx; Idx < Config.RecentWorkspaces.size(); ++Idx) {
            if (Idx > ZeroIdx) {
                File << ", ";
            }
            File << "\"" << EscapeString(Config.RecentWorkspaces[Idx]) << "\"";
        }

        File << "]\n";
        File << "}\n";
        File.close();
    }
};

#endif
