#ifndef DEBUGGER_CLIENT_HPP
#define DEBUGGER_CLIENT_HPP
#include <windows.h>
#include <string>
#include <vector>
#include <mutex>



// Represents a breakpoint location sent to GDB.
struct DebugBreakpoint {
    std::string FilePath;
    int LineNumber;
};

// Represents one stack frame reported by GDB.
struct DebugStackFrame {
    std::string FunctionName;
    std::string FilePath;
    int LineNumber;
};

// Represents one local variable reported by GDB.
struct DebugVariable {
    std::string Name;
    std::string Value;
};

// Manages an integrated GDB MI2 debugging session over stdio pipes.
class DebuggerClient {
private:
    HANDLE GdbProcess;
    HANDLE JobHandle;
    HANDLE StdinWrite;
    bool IsRunning;
    bool IsPaused;
    bool IsDebugMode;
    std::string PausedFile;
    int PausedLine;
    bool HasPendingStopJump;
    std::mutex DebugMutex;
    std::vector<std::string> DebugLog;
    std::vector<DebugStackFrame> CallStack;
    std::vector<DebugVariable> Locals;

    void SendCommand(const std::string& Command);
    void ParseMiLine(const std::string& Line);
    static std::string ExtractMiField(const std::string& Line, const std::string& Key, size_t StartPos);

public:
    DebuggerClient();
    ~DebuggerClient();

    bool Start(const std::string& ExecutablePath, const std::string& WorkingDir, const std::vector<DebugBreakpoint>& Breakpoints);
    bool RunWithoutDebugging(const std::string& ExecutablePath, const std::string& WorkingDir);
    void Stop();
    void Continue();
    void StepOver();
    void StepInto();
    void StepOut();
    void SendRawCommand(const std::string& Command);

    bool GetIsRunning() const { return IsRunning; }
    bool GetIsPaused() const { return IsPaused; }
    bool GetIsDebugMode() const { return IsDebugMode; }
    bool PopStopLocation(std::string& OutFile, int& OutLine);
    std::vector<DebugStackFrame> GetCallStack();
    std::vector<DebugVariable> GetLocals();
    std::vector<std::string> GetLogLines();
    void ClearLog();
};

#endif
