#include "Include/DebuggerClient.hpp"
#include <thread>
#include <algorithm>



const int DbgZeroInt = 0;
const int DbgOneInt = 1;
const size_t DbgZeroSize = 0;
const size_t DbgOneSize = 1;
const size_t DbgMaxLogLines = 300;

// Initializes debugger client state.
DebuggerClient::DebuggerClient() {
    GdbProcess = nullptr;
    JobHandle = nullptr;
    StdinWrite = nullptr;
    IsRunning = false;
    IsPaused = false;
    IsDebugMode = false;
    PausedLine = DbgZeroInt;
    HasPendingStopJump = false;
}

// Terminates any active GDB session on destruction.
DebuggerClient::~DebuggerClient() {
    Stop();
}

// Extracts a quoted MI2 key="value" field starting at StartPos.
std::string DebuggerClient::ExtractMiField(const std::string& Line, const std::string& Key, size_t StartPos) {
    std::string Pattern = Key + "=\"";
    size_t Pos = Line.find(Pattern, StartPos);

    if (Pos == std::string::npos) {
        return "";
    }

    std::string Out;

    for (size_t Idx = Pos + Pattern.size(); Idx < Line.size(); ++Idx) {
        if (Line[Idx] == '\\' && (Idx + DbgOneSize) < Line.size()) {
            char Next = Line[Idx + DbgOneSize];

            if (Next == 'n') {
                Out.push_back('\n');
            } else {
                Out.push_back(Next);
            }

            Idx++;
            continue;
        }
        if (Line[Idx] == '"') {
            break;
        }
        Out.push_back(Line[Idx]);
    }

    return Out;
}

// Sends a single MI2 command line to GDB stdin.
void DebuggerClient::SendCommand(const std::string& Command) {
    const DWORD ZeroDword = 0;
    std::string Payload = Command + "\n";
    DWORD BytesWritten = ZeroDword;

    std::lock_guard<std::mutex> Lock(DebugMutex);

    if (StdinWrite != nullptr && IsRunning) {
        WriteFile(StdinWrite, Payload.c_str(), static_cast<DWORD>(Payload.size()), &BytesWritten, nullptr);
    }
}

// Parses a line of GDB MI2 output to update stopped state, call stack, locals, and debug log.
void DebuggerClient::ParseMiLine(const std::string& Line) {
    if (Line.empty()) {
        return;
    }

    if (Line.rfind("*stopped", DbgZeroSize) == DbgZeroSize) {
        std::string Reason = ExtractMiField(Line, "reason", DbgZeroSize);

        if (Reason == "exited-normally" || Reason == "exited" || Reason == "exited-signalled") {
            std::lock_guard<std::mutex> Lock(DebugMutex);
            IsPaused = false;
            DebugLog.push_back("[GDB] Target exited (" + Reason + ").");

            return;
        }

        std::string FullFile = ExtractMiField(Line, "fullname", DbgZeroSize);

        if (FullFile.empty()) {
            FullFile = ExtractMiField(Line, "file", DbgZeroSize);
        }

        std::string LineStr = ExtractMiField(Line, "line", DbgZeroSize);
        int LineNum = DbgZeroInt;

        if (!LineStr.empty()) {
            try {
                LineNum = std::stoi(LineStr);
            } catch (...) {
                LineNum = DbgZeroInt;
            }
        }

        {
            std::lock_guard<std::mutex> Lock(DebugMutex);
            IsPaused = true;
            PausedFile = FullFile;
            PausedLine = LineNum;
            HasPendingStopJump = (!FullFile.empty() && LineNum > DbgZeroInt);
            DebugLog.push_back("[GDB] Paused at " + FullFile + ":" + std::to_string(LineNum));
        }

        SendCommand("-stack-list-frames");
        SendCommand("-stack-list-locals --simple-values");

        return;
    }

    if (Line.rfind("*running", DbgZeroSize) == DbgZeroSize) {
        std::lock_guard<std::mutex> Lock(DebugMutex);
        IsPaused = false;

        return;
    }

    if (Line.rfind("^done,stack=[", DbgZeroSize) == DbgZeroSize) {
        std::vector<DebugStackFrame> Frames;
        size_t Pos = DbgZeroSize;

        while ((Pos = Line.find("frame={", Pos)) != std::string::npos) {
            DebugStackFrame Frame;
            Frame.FunctionName = ExtractMiField(Line, "func", Pos);
            Frame.FilePath = ExtractMiField(Line, "fullname", Pos);

            if (Frame.FilePath.empty()) {
                Frame.FilePath = ExtractMiField(Line, "file", Pos);
            }

            std::string LineStr = ExtractMiField(Line, "line", Pos);
            Frame.LineNumber = DbgZeroInt;

            if (!LineStr.empty()) {
                try {
                    Frame.LineNumber = std::stoi(LineStr);
                } catch (...) {
                    Frame.LineNumber = DbgZeroInt;
                }
            }

            Frames.push_back(Frame);
            Pos += DbgOneSize;
        }

        std::lock_guard<std::mutex> Lock(DebugMutex);
        CallStack = std::move(Frames);

        return;
    }

    if (Line.rfind("^done,locals=[", DbgZeroSize) == DbgZeroSize) {
        std::vector<DebugVariable> ParsedLocals;
        size_t Pos = DbgZeroSize;

        while ((Pos = Line.find("{name=\"", Pos)) != std::string::npos) {
            DebugVariable Var;
            Var.Name = ExtractMiField(Line, "name", Pos);
            Var.Value = ExtractMiField(Line, "value", Pos);

            if (!Var.Name.empty()) {
                ParsedLocals.push_back(Var);
            }

            Pos += DbgOneSize;
        }

        std::lock_guard<std::mutex> Lock(DebugMutex);
        Locals = std::move(ParsedLocals);

        return;
    }

    // Captures console/target/error stream output into DebugLog.
    if (Line[DbgZeroSize] == '~' || Line[DbgZeroSize] == '@' || Line[DbgZeroSize] == '&' || Line.rfind("^error", DbgZeroSize) == DbgZeroSize) {
        std::string DisplayText = Line;

        if (Line.rfind("^error", DbgZeroSize) == DbgZeroSize) {
            std::string Msg = ExtractMiField(Line, "msg", DbgZeroSize);

            if (!Msg.empty()) {
                DisplayText = "[Error] " + Msg;
            }
        } else if (Line.size() > DbgOneSize && Line[DbgOneSize] == '"') {
            DisplayText = ExtractMiField("v=" + Line.substr(DbgOneSize), "v", DbgZeroSize);

            while (!DisplayText.empty() && (DisplayText.back() == '\n' || DisplayText.back() == '\r')) {
                DisplayText.pop_back();
            }
        }

        if (!DisplayText.empty()) {
            std::lock_guard<std::mutex> Lock(DebugMutex);
            DebugLog.push_back(DisplayText);

            if (DebugLog.size() > DbgMaxLogLines) {
                DebugLog.erase(DebugLog.begin());
            }
        }
    }
}

// Launches GDB MI2 for ExecutablePath, inserts all active breakpoints, and begins execution.
bool DebuggerClient::Start(const std::string& ExecutablePath, const std::string& WorkingDir, const std::vector<DebugBreakpoint>& Breakpoints) {
    const DWORD ZeroDword = 0;

    Stop();

    if (ExecutablePath.empty()) {
        return false;
    }

    SECURITY_ATTRIBUTES SecAttr = {};
    SecAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    SecAttr.bInheritHandle = TRUE;
    SecAttr.lpSecurityDescriptor = nullptr;

    HANDLE StdoutRead = nullptr;
    HANDLE StdoutWrite = nullptr;
    HANDLE StdinRead = nullptr;
    HANDLE NewStdinWrite = nullptr;

    if (!CreatePipe(&StdoutRead, &StdoutWrite, &SecAttr, ZeroDword)) {
        return false;
    }

    if (!CreatePipe(&StdinRead, &NewStdinWrite, &SecAttr, ZeroDword)) {
        CloseHandle(StdoutRead);
        CloseHandle(StdoutWrite);

        return false;
    }

    SetHandleInformation(StdoutRead, HANDLE_FLAG_INHERIT, ZeroDword);
    SetHandleInformation(NewStdinWrite, HANDLE_FLAG_INHERIT, ZeroDword);

    STARTUPINFOA StartInfo = {};
    StartInfo.cb = sizeof(STARTUPINFOA);
    StartInfo.hStdOutput = StdoutWrite;
    StartInfo.hStdError = StdoutWrite;
    StartInfo.hStdInput = StdinRead;
    StartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    StartInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION ProcInfo = {};
    std::string LaunchCmd = "gdb --interpreter=mi2 -q \"" + ExecutablePath + "\"";
    std::vector<char> CmdBuf(LaunchCmd.begin(), LaunchCmd.end());
    CmdBuf.push_back('\0');

    const char* DirPtr = WorkingDir.empty() ? nullptr : WorkingDir.c_str();
    BOOL Created = CreateProcessA(
        nullptr,
        CmdBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        DirPtr,
        &StartInfo,
        &ProcInfo
    );

    CloseHandle(StdoutWrite);
    CloseHandle(StdinRead);

    if (!Created) {
        CloseHandle(StdoutRead);
        CloseHandle(NewStdinWrite);

        std::lock_guard<std::mutex> Lock(DebugMutex);
        DebugLog.push_back("[Error] Failed to launch gdb.exe.");

        return false;
    }

    CloseHandle(ProcInfo.hThread);

    HANDLE NewJob = CreateJobObjectW(nullptr, nullptr);

    if (NewJob != nullptr) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION LimitInfo = {};
        LimitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(NewJob, JobObjectExtendedLimitInformation, &LimitInfo, sizeof(LimitInfo));
        AssignProcessToJobObject(NewJob, ProcInfo.hProcess);
    }

    {
        std::lock_guard<std::mutex> Lock(DebugMutex);
        GdbProcess = ProcInfo.hProcess;
        JobHandle = NewJob;
        StdinWrite = NewStdinWrite;
        IsRunning = true;
        IsPaused = false;
        IsDebugMode = true;
        CallStack.clear();
        Locals.clear();
        DebugLog.push_back("[GDB] Debugging " + ExecutablePath);
    }

    // Spawns a background thread to stream and parse GDB MI2 output lines.
    std::thread([this, StdoutRead, ProcInfo, ZeroDword]() {
        constexpr int ReadBufSize = 4095;
        char Buffer[ReadBufSize];
        DWORD BytesRead = ZeroDword;
        std::string Pending;

        while (ReadFile(StdoutRead, Buffer, ReadBufSize - DbgOneInt, &BytesRead, nullptr) && BytesRead > ZeroDword) {
            Pending.append(Buffer, static_cast<size_t>(BytesRead));
            size_t NewlinePos = DbgZeroSize;

            while ((NewlinePos = Pending.find('\n')) != std::string::npos) {
                std::string Line = Pending.substr(DbgZeroSize, NewlinePos);

                if (!Line.empty() && Line.back() == '\r') {
                    Line.pop_back();
                }

                ParseMiLine(Line);
                Pending.erase(DbgZeroSize, NewlinePos + DbgOneSize);
            }
        }

        {
            std::lock_guard<std::mutex> Lock(DebugMutex);

            if (GdbProcess == ProcInfo.hProcess) {
                GdbProcess = nullptr;
                IsRunning = false;
                IsPaused = false;
            }
        }

        CloseHandle(StdoutRead);
        CloseHandle(ProcInfo.hProcess);
    }).detach();

    // Registers all active editor breakpoints before running the target.
    for (const auto& Bp : Breakpoints) {
        std::string ForwardPath = Bp.FilePath;
        std::replace(ForwardPath.begin(), ForwardPath.end(), '\\', '/');
        SendCommand("-break-insert \"" + ForwardPath + ":" + std::to_string(Bp.LineNumber) + "\"");
    }

    SendCommand("-exec-run");

    return true;
}

// Runs an executable directly without GDB, streaming its output into DebugLog inside a managed Job Object.
bool DebuggerClient::RunWithoutDebugging(const std::string& ExecutablePath, const std::string& WorkingDir) {
    const DWORD ZeroDword = 0;

    Stop();

    if (ExecutablePath.empty()) {
        return false;
    }

    SECURITY_ATTRIBUTES SecAttr = {};
    SecAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    SecAttr.bInheritHandle = TRUE;
    SecAttr.lpSecurityDescriptor = nullptr;

    HANDLE StdoutRead = nullptr;
    HANDLE StdoutWrite = nullptr;

    if (!CreatePipe(&StdoutRead, &StdoutWrite, &SecAttr, ZeroDword)) {
        return false;
    }

    SetHandleInformation(StdoutRead, HANDLE_FLAG_INHERIT, ZeroDword);

    STARTUPINFOA StartInfo = {};
    StartInfo.cb = sizeof(STARTUPINFOA);
    StartInfo.hStdOutput = StdoutWrite;
    StartInfo.hStdError = StdoutWrite;
    StartInfo.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION ProcInfo = {};
    std::string LaunchCmd = "\"" + ExecutablePath + "\"";
    std::vector<char> CmdBuf(LaunchCmd.begin(), LaunchCmd.end());
    CmdBuf.push_back('\0');

    const char* DirPtr = WorkingDir.empty() ? nullptr : WorkingDir.c_str();
    BOOL Created = CreateProcessA(
        nullptr,
        CmdBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        ZeroDword,
        nullptr,
        DirPtr,
        &StartInfo,
        &ProcInfo
    );

    CloseHandle(StdoutWrite);

    if (!Created) {
        CloseHandle(StdoutRead);
        std::lock_guard<std::mutex> Lock(DebugMutex);
        DebugLog.push_back("[Error] Failed to launch " + ExecutablePath);

        return false;
    }

    CloseHandle(ProcInfo.hThread);

    HANDLE NewJob = CreateJobObjectW(nullptr, nullptr);

    if (NewJob != nullptr) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION LimitInfo = {};
        LimitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(NewJob, JobObjectExtendedLimitInformation, &LimitInfo, sizeof(LimitInfo));
        AssignProcessToJobObject(NewJob, ProcInfo.hProcess);
    }

    {
        std::lock_guard<std::mutex> Lock(DebugMutex);
        GdbProcess = ProcInfo.hProcess;
        JobHandle = NewJob;
        StdinWrite = nullptr;
        IsRunning = true;
        IsPaused = false;
        IsDebugMode = false;
        CallStack.clear();
        Locals.clear();
        DebugLog.push_back("[Run] Started " + ExecutablePath);
    }

    std::thread([this, StdoutRead, ProcInfo, ZeroDword]() {
        constexpr int ReadBufSize = 4095;
        char Buffer[ReadBufSize];
        DWORD BytesRead = ZeroDword;
        std::string Pending;

        while (ReadFile(StdoutRead, Buffer, ReadBufSize - DbgOneInt, &BytesRead, nullptr) && BytesRead > ZeroDword) {
            Pending.append(Buffer, static_cast<size_t>(BytesRead));
            size_t NewlinePos = DbgZeroSize;

            while ((NewlinePos = Pending.find('\n')) != std::string::npos) {
                std::string Line = Pending.substr(DbgZeroSize, NewlinePos);

                if (!Line.empty() && Line.back() == '\r') {
                    Line.pop_back();
                }

                {
                    std::lock_guard<std::mutex> Lock(DebugMutex);
                    DebugLog.push_back(Line);

                    if (DebugLog.size() > DbgMaxLogLines) {
                        DebugLog.erase(DebugLog.begin());
                    }
                }

                Pending.erase(DbgZeroSize, NewlinePos + DbgOneSize);
            }
        }

        WaitForSingleObject(ProcInfo.hProcess, INFINITE);

        {
            std::lock_guard<std::mutex> Lock(DebugMutex);

            if (GdbProcess == ProcInfo.hProcess) {
                GdbProcess = nullptr;
                IsRunning = false;
                IsPaused = false;
                DebugLog.push_back("[Run] Process exited.");
            }
        }

        CloseHandle(StdoutRead);
        CloseHandle(ProcInfo.hProcess);
    }).detach();

    return true;
}

// Terminates the active GDB and target child process tree via Job Object and resets state cleanly.
void DebuggerClient::Stop() {
    const UINT ExitCodeZero = 0;

    std::lock_guard<std::mutex> Lock(DebugMutex);

    if (StdinWrite != nullptr) {
        CloseHandle(StdinWrite);
        StdinWrite = nullptr;
    }

    if (JobHandle != nullptr) {
        TerminateJobObject(JobHandle, ExitCodeZero);
        CloseHandle(JobHandle);
        JobHandle = nullptr;
    }

    if (GdbProcess != nullptr) {
        TerminateProcess(GdbProcess, ExitCodeZero);
        GdbProcess = nullptr;
    }

    IsRunning = false;
    IsPaused = false;
    IsDebugMode = false;
    HasPendingStopJump = false;
    CallStack.clear();
    Locals.clear();
}

// Resumes execution of the paused debug target (F5).
void DebuggerClient::Continue() {
    SendCommand("-exec-continue");
}

// Steps over the current line (F10).
void DebuggerClient::StepOver() {
    SendCommand("-exec-next");
}

// Steps into the function on the current line (F11).
void DebuggerClient::StepInto() {
    SendCommand("-exec-step");
}

// Steps out of the current function (Shift+F11).
void DebuggerClient::StepOut() {
    SendCommand("-exec-finish");
}

// Sends a custom GDB command from the debug console.
void DebuggerClient::SendRawCommand(const std::string& Command) {
    if (!Command.empty()) {
        SendCommand(Command);
    }
}

// Pops a pending debugger stop file and line so the UI can jump to it.
bool DebuggerClient::PopStopLocation(std::string& OutFile, int& OutLine) {
    std::lock_guard<std::mutex> Lock(DebugMutex);

    if (!HasPendingStopJump) {
        return false;
    }

    OutFile = PausedFile;
    OutLine = PausedLine;
    HasPendingStopJump = false;

    return true;
}

// Returns a thread-safe snapshot of the current call stack.
std::vector<DebugStackFrame> DebuggerClient::GetCallStack() {
    std::lock_guard<std::mutex> Lock(DebugMutex);

    return CallStack;
}

// Returns a thread-safe snapshot of local variables.
std::vector<DebugVariable> DebuggerClient::GetLocals() {
    std::lock_guard<std::mutex> Lock(DebugMutex);

    return Locals;
}

// Returns a thread-safe snapshot of the debug console log.
std::vector<std::string> DebuggerClient::GetLogLines() {
    std::lock_guard<std::mutex> Lock(DebugMutex);

    return DebugLog;
}

// Clears the debug console log buffer.
void DebuggerClient::ClearLog() {
    std::lock_guard<std::mutex> Lock(DebugMutex);
    DebugLog.clear();
}
