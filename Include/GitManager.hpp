#ifndef GIT_MANAGER_HPP
#define GIT_MANAGER_HPP
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <filesystem>



// Represents one changed file reported by git status --porcelain.
struct GitFileStatus {
    std::string RelativePath;
    std::string FullPath;
    char IndexStatus;
    char WorkTreeStatus;
    bool IsStaged;
};

// Manages cached Git branch, file statuses, staging, and commits for a workspace.
class GitManager {
private:
    std::string ActiveBranch;
    std::string LastStatusMessage;
    bool IsGitRepo;
    std::vector<GitFileStatus> ChangedFiles;
    std::unordered_map<std::string, char> StatusByFullPath;

    // Executes a git command in WorkingDir and captures stdout/stderr.
    static std::string RunGit(const std::string& WorkingDir, const std::string& Args) {
        const DWORD ZeroDword = 0;
        const int OneOffset = 1;
        constexpr int ReadBufSize = 4095;

        if (WorkingDir.empty()) {
            return "";
        }

        SECURITY_ATTRIBUTES SecAttr = {};
        SecAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        SecAttr.bInheritHandle = TRUE;
        SecAttr.lpSecurityDescriptor = nullptr;

        HANDLE ReadPipe = nullptr;
        HANDLE WritePipe = nullptr;

        if (!CreatePipe(&ReadPipe, &WritePipe, &SecAttr, ZeroDword)) {
            return "";
        }

        SetHandleInformation(ReadPipe, HANDLE_FLAG_INHERIT, ZeroDword);

        STARTUPINFOA StartInfo = {};
        StartInfo.cb = sizeof(STARTUPINFOA);
        StartInfo.hStdOutput = WritePipe;
        StartInfo.hStdError = WritePipe;
        StartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        StartInfo.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION ProcInfo = {};
        std::string FullCmd = "git " + Args;
        std::vector<char> CmdBuf(FullCmd.begin(), FullCmd.end());
        CmdBuf.push_back('\0');

        BOOL Created = CreateProcessA(
            nullptr,
            CmdBuf.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            WorkingDir.c_str(),
            &StartInfo,
            &ProcInfo
        );

        CloseHandle(WritePipe);

        if (!Created) {
            CloseHandle(ReadPipe);

            return "";
        }

        std::string Output;
        char Buffer[ReadBufSize];
        DWORD BytesRead = ZeroDword;

        while (ReadFile(ReadPipe, Buffer, ReadBufSize - OneOffset, &BytesRead, nullptr) && BytesRead > ZeroDword) {
            Output.append(Buffer, static_cast<size_t>(BytesRead));
        }

        WaitForSingleObject(ProcInfo.hProcess, INFINITE);
        CloseHandle(ReadPipe);
        CloseHandle(ProcInfo.hProcess);
        CloseHandle(ProcInfo.hThread);

        return Output;
    }

public:
    // Initializes GitManager in non-repository state.
    GitManager() {
        IsGitRepo = false;
    }

    // Refreshes the cached Git branch and porcelain file statuses for WorkspacePath.
    void Refresh(const std::string& WorkspacePath) {
        const size_t ZeroIdx = 0;
        const size_t OneIdx = 1;
        const size_t PathOffset = 3;

        ActiveBranch.clear();
        ChangedFiles.clear();
        StatusByFullPath.clear();
        IsGitRepo = false;

        if (WorkspacePath.empty()) {
            return;
        }

        std::string BranchOut = RunGit(WorkspacePath, "rev-parse --abbrev-ref HEAD");

        while (!BranchOut.empty() && (BranchOut.back() == '\n' || BranchOut.back() == '\r')) {
            BranchOut.pop_back();
        }

        if (BranchOut.empty() || BranchOut.find("fatal:") != std::string::npos || BranchOut.find("not recognized") != std::string::npos) {
            return;
        }

        IsGitRepo = true;
        ActiveBranch = BranchOut;

        std::string StatusOut = RunGit(WorkspacePath, "status --porcelain");
        std::istringstream Stream(StatusOut);
        std::string Line;
        std::filesystem::path RootPath(WorkspacePath);

        while (std::getline(Stream, Line)) {
            if (Line.size() <= PathOffset) {
                continue;
            }

            if (!Line.empty() && Line.back() == '\r') {
                Line.pop_back();
            }

            GitFileStatus Item;
            Item.IndexStatus = Line[ZeroIdx];
            Item.WorkTreeStatus = Line[OneIdx];
            Item.RelativePath = Line.substr(PathOffset);
            Item.FullPath = (RootPath / Item.RelativePath).lexically_normal().string();
            Item.IsStaged = (Item.IndexStatus != ' ' && Item.IndexStatus != '?');

            char DisplayStatus = (Item.IndexStatus == '?') ? '?' : ((Item.WorkTreeStatus != ' ') ? Item.WorkTreeStatus : Item.IndexStatus);
            StatusByFullPath[Item.FullPath] = DisplayStatus;
            ChangedFiles.push_back(Item);
        }
    }

    // Stages a single file relative path.
    void StageFile(const std::string& WorkspacePath, const std::string& RelativePath) {
        RunGit(WorkspacePath, "add \"" + RelativePath + "\"");
        Refresh(WorkspacePath);
    }

    // Stages all changes in the workspace.
    void StageAll(const std::string& WorkspacePath) {
        RunGit(WorkspacePath, "add -A");
        Refresh(WorkspacePath);
    }

    // Unstages a single file relative path.
    void UnstageFile(const std::string& WorkspacePath, const std::string& RelativePath) {
        RunGit(WorkspacePath, "reset HEAD -- \"" + RelativePath + "\"");
        Refresh(WorkspacePath);
    }

    // Discards working-tree and staged changes for a single file (or deletes it if untracked).
    std::string DiscardFile(const std::string& WorkspacePath, const std::string& RelativePath, bool IsUntracked) {
        std::string Result;

        if (IsUntracked) {
            std::error_code Err;
            std::filesystem::path FullTarget = std::filesystem::path(WorkspacePath) / RelativePath;
            std::filesystem::remove_all(FullTarget, Err);
            Result = Err ? ("Failed to discard untracked file: " + Err.message()) : ("Discarded untracked file: " + RelativePath);
        } else {
            RunGit(WorkspacePath, "reset HEAD -- \"" + RelativePath + "\"");
            Result = RunGit(WorkspacePath, "checkout -- \"" + RelativePath + "\"");

            if (Result.empty()) {
                Result = "Discarded changes: " + RelativePath;
            }
        }

        Refresh(WorkspacePath);
        LastStatusMessage = Result;

        return LastStatusMessage;
    }

    // Discards all uncommitted changes across the entire workspace repository.
    std::string DiscardAllChanges(const std::string& WorkspacePath) {
        RunGit(WorkspacePath, "reset HEAD");
        std::string CheckoutOut = RunGit(WorkspacePath, "checkout -- .");
        std::string CleanOut = RunGit(WorkspacePath, "clean -fd");
        Refresh(WorkspacePath);
        LastStatusMessage = (CheckoutOut.empty() && CleanOut.empty()) ? "Discarded all changes." : (CheckoutOut + CleanOut);

        return LastStatusMessage;
    }

    // Commits staged changes (auto-staging all changes if none are staged yet).
    std::string Commit(const std::string& WorkspacePath, const std::string& Message) {
        if (Message.empty()) {
            LastStatusMessage = "Enter a commit message first.";

            return LastStatusMessage;
        }

        bool HasStaged = false;

        for (const auto& Item : ChangedFiles) {
            if (Item.IsStaged) {
                HasStaged = true;
                break;
            }
        }

        if (!HasStaged) {
            RunGit(WorkspacePath, "add -A");
        }

        std::string Escaped = Message;

        for (char& Ch : Escaped) {
            if (Ch == '"') {
                Ch = '\'';
            }
        }

        std::string Result = RunGit(WorkspacePath, "commit -m \"" + Escaped + "\"");
        Refresh(WorkspacePath);
        LastStatusMessage = Result.empty() ? "Commit completed." : Result;

        return LastStatusMessage;
    }

    // Pushes local commits to the remote repository.
    std::string Push(const std::string& WorkspacePath) {
        std::string Result = RunGit(WorkspacePath, "push");
        Refresh(WorkspacePath);
        LastStatusMessage = Result.empty() ? "Push completed." : Result;

        return LastStatusMessage;
    }

    // Pulls remote changes into the active branch.
    std::string Pull(const std::string& WorkspacePath) {
        std::string Result = RunGit(WorkspacePath, "pull");
        Refresh(WorkspacePath);
        LastStatusMessage = Result.empty() ? "Pull completed." : Result;

        return LastStatusMessage;
    }

    // Fetches updates from all remotes without merging.
    std::string Fetch(const std::string& WorkspacePath) {
        std::string Result = RunGit(WorkspacePath, "fetch --all --prune");
        Refresh(WorkspacePath);
        LastStatusMessage = Result.empty() ? "Fetch completed." : Result;

        return LastStatusMessage;
    }

    // Returns the latest Git operation status message.
    const std::string& GetLastStatusMessage() const { return LastStatusMessage; }

    // Returns whether the active workspace is a Git repository.
    bool GetIsGitRepo() const { return IsGitRepo; }

    // Returns the active Git branch name.
    const std::string& GetBranch() const { return ActiveBranch; }

    // Returns the cached list of modified and untracked files.
    const std::vector<GitFileStatus>& GetChangedFiles() const { return ChangedFiles; }

    // Looks up the cached Git status character for a full file path, or '\0' if clean.
    char GetStatusForPath(const std::string& FullPath) const {
        std::string Normalized = std::filesystem::path(FullPath).lexically_normal().string();
        auto Iter = StatusByFullPath.find(Normalized);

        if (Iter == StatusByFullPath.end()) {
            return '\0';
        }

        return Iter->second;
    }
};

#endif
