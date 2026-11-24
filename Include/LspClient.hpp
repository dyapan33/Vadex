#ifndef LSP_CLIENT_HPP
#define LSP_CLIENT_HPP
#include <windows.h>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>



// Represents a single diagnostic reported by a language server.
struct LspDiagnostic {
    int Line;
    int StartChar;
    int EndChar;
    int Severity;
    std::string Message;
};

// Represents a pending go-to-definition target returned by a language server.
struct LspDefinitionTarget {
    bool HasTarget;
    std::string FilePath;
    int Line;
    int Column;
};

// Manages a JSON-RPC 2.0 Language Server Protocol session over stdio pipes.
class LspClient {
private:
    HANDLE ServerProcess;
    HANDLE StdinWrite;
    bool IsRunning;
    std::string ActiveServerCommand;
    int NextRequestId;
    int LastCompletionRequestId;
    int LastHoverRequestId;
    int LastDefinitionRequestId;
    std::mutex LspMutex;
    std::unordered_map<std::string, std::vector<LspDiagnostic>> DiagnosticsByUri;
    std::string PendingCompletions;
    std::string PendingHoverText;
    int PendingHoverPos;
    LspDefinitionTarget PendingDefinition;
    int DocumentVersion;

    void SendJsonRpc(const std::string& JsonBody);
    void ProcessIncomingMessage(const std::string& JsonMessage);
    static std::string EscapeJson(const std::string& Input);
    static std::string PathToFileUri(const std::string& FilePath);
    static std::string FileUriToPath(const std::string& Uri);
    static std::string ExtractJsonStringField(const std::string& Json, const std::string& Key, size_t StartPos);
    static int ExtractJsonIntField(const std::string& Json, const std::string& Key, size_t StartPos, int DefaultVal);

public:
    LspClient();
    ~LspClient();

    bool StartForWorkspace(const std::string& ServerCommand, const std::string& WorkspacePath);
    void Stop();
    bool GetIsRunning() const { return IsRunning; }

    void NotifyDidOpen(const std::string& FilePath, const std::string& LanguageId, const std::string& Text);
    void NotifyDidChange(const std::string& FilePath, const std::string& Text);
    void NotifyDidSave(const std::string& FilePath);
    void RequestCompletion(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar);
    void RequestHover(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar, int BytePos);
    void RequestDefinition(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar);
    void RequestDeclaration(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar);
    void RequestTypeDefinition(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar);
    void RequestImplementation(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar);
    void RequestReferences(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar);

    bool PopCompletions(std::string& OutList);
    bool PopHover(std::string& OutText, int& OutPos);
    bool PopDefinition(LspDefinitionTarget& OutTarget);
    std::vector<LspDiagnostic> GetDiagnosticsForFile(const std::string& FilePath);
    int GetTotalDiagnosticCount();
};

#endif
