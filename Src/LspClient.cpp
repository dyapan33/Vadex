#include "Include/LspClient.hpp"
#include <thread>
#include <sstream>
#include <algorithm>



const int LspZeroInt = 0;
const int LspOneInt = 1;
const int LspTwoInt = 1 + 1;
const int LspThreeInt = 3;
const int LspNegativeOneInt = -1;
const int LspErrorSeverity = 1;
const int LspWarningSeverity = 1 + 1;

// Initializes LSP client state.
LspClient::LspClient() {
    ServerProcess = nullptr;
    StdinWrite = nullptr;
    IsRunning = false;
    NextRequestId = LspOneInt;
    LastCompletionRequestId = LspNegativeOneInt;
    LastHoverRequestId = LspNegativeOneInt;
    LastDefinitionRequestId = LspNegativeOneInt;
    PendingHoverPos = LspZeroInt;
    PendingDefinition = { false, "", LspOneInt, LspOneInt };
    DocumentVersion = LspOneInt;
}

// Ensures the language server process and pipes are cleaned up on destruction.
LspClient::~LspClient() {
    Stop();
}

// Escapes special characters for safe embedding inside a JSON string literal.
std::string LspClient::EscapeJson(const std::string& Input) {
    const unsigned char MinPrintableAscii = 31 + 1;

    std::string Out;
    Out.reserve(Input.size());

    for (char Ch : Input) {
        if (Ch == '\\') {
            Out += "\\\\";
        } else if (Ch == '"') {
            Out += "\\\"";
        } else if (Ch == '\n') {
            Out += "\\n";
        } else if (Ch == '\r') {
            Out += "\\r";
        } else if (Ch == '\t') {
            Out += "\\t";
        } else if (static_cast<unsigned char>(Ch) >= MinPrintableAscii) {
            Out.push_back(Ch);
        }
    }

    return Out;
}

// Converts a Windows filesystem path into a file:/// URI.
std::string LspClient::PathToFileUri(const std::string& FilePath) {
    std::string Normalized = FilePath;
    std::replace(Normalized.begin(), Normalized.end(), '\\', '/');

    if (!Normalized.empty() && Normalized[LspZeroInt] != '/') {
        return "file:///" + Normalized;
    }

    return "file://" + Normalized;
}

// Converts a file:/// URI back into a Windows filesystem path.
std::string LspClient::FileUriToPath(const std::string& Uri) {
    const std::string Prefix = "file:///";
    std::string Result = Uri;

    if (Result.rfind(Prefix, LspZeroInt) == LspZeroInt) {
        Result = Result.substr(Prefix.size());
    }

    // Decodes percent-encoded colons and spaces.
    size_t Pos = LspZeroInt;

    while ((Pos = Result.find("%3A", Pos)) != std::string::npos) {
        Result.replace(Pos, LspThreeInt, ":");
    }

    Pos = LspZeroInt;

    while ((Pos = Result.find("%3a", Pos)) != std::string::npos) {
        Result.replace(Pos, LspThreeInt, ":");
    }

    Pos = LspZeroInt;

    while ((Pos = Result.find("%20", Pos)) != std::string::npos) {
        Result.replace(Pos, LspThreeInt, " ");
    }

    std::replace(Result.begin(), Result.end(), '/', '\\');

    return Result;
}

// Extracts the value of a JSON string field starting from a search offset.
std::string LspClient::ExtractJsonStringField(const std::string& Json, const std::string& Key, size_t StartPos) {
    std::string Pattern = "\"" + Key + "\"";
    size_t KeyPos = Json.find(Pattern, StartPos);

    if (KeyPos == std::string::npos) {
        return "";
    }

    size_t ColonPos = Json.find(':', KeyPos + Pattern.size());

    if (ColonPos == std::string::npos) {
        return "";
    }

    size_t QuoteStart = Json.find('"', ColonPos + LspOneInt);

    if (QuoteStart == std::string::npos) {
        return "";
    }

    std::string Value;

    for (size_t Idx = QuoteStart + LspOneInt; Idx < Json.size(); ++Idx) {
        if (Json[Idx] == '\\' && (Idx + LspOneInt) < Json.size()) {
            char Esc = Json[Idx + LspOneInt];

            if (Esc == 'n') {
                Value.push_back('\n');
            } else if (Esc == 't') {
                Value.push_back('\t');
            } else {
                Value.push_back(Esc);
            }

            Idx++;
            continue;
        }

        if (Json[Idx] == '"') {
            break;
        }

        Value.push_back(Json[Idx]);
    }

    return Value;
}

// Extracts the integer value of a JSON numeric field starting from a search offset.
int LspClient::ExtractJsonIntField(const std::string& Json, const std::string& Key, size_t StartPos, int DefaultVal) {
    std::string Pattern = "\"" + Key + "\"";
    size_t KeyPos = Json.find(Pattern, StartPos);

    if (KeyPos == std::string::npos) {
        return DefaultVal;
    }

    size_t ColonPos = Json.find(':', KeyPos + Pattern.size());

    if (ColonPos == std::string::npos) {
        return DefaultVal;
    }

    size_t NumStart = Json.find_first_of("-0123456789", ColonPos + LspOneInt);

    if (NumStart == std::string::npos) {
        return DefaultVal;
    }

    try {
        return std::stoi(Json.substr(NumStart));
    } catch (...) {
        return DefaultVal;
    }
}

// Sends a Content-Length framed JSON-RPC 2.0 payload to the language server stdin.
void LspClient::SendJsonRpc(const std::string& JsonBody) {
    const DWORD ZeroDword = 0;

    std::string Frame = "Content-Length: " + std::to_string(JsonBody.size()) + "\r\n\r\n" + JsonBody;
    DWORD BytesWritten = ZeroDword;

    std::lock_guard<std::mutex> Lock(LspMutex);

    if (StdinWrite != nullptr && IsRunning) {
        WriteFile(StdinWrite, Frame.c_str(), static_cast<DWORD>(Frame.size()), &BytesWritten, nullptr);
    }
}

// Parses incoming LSP responses and notifications for diagnostics, completions, hover, and definitions.
void LspClient::ProcessIncomingMessage(const std::string& JsonMessage) {
    // Handles publishDiagnostics notifications.
    if (JsonMessage.find("textDocument/publishDiagnostics") != std::string::npos) {
        std::string Uri = ExtractJsonStringField(JsonMessage, "uri", LspZeroInt);
        std::vector<LspDiagnostic> ParsedDiags;
        size_t SearchPos = JsonMessage.find("\"diagnostics\"");

        if (SearchPos != std::string::npos) {
            while ((SearchPos = JsonMessage.find("\"range\"", SearchPos)) != std::string::npos) {
                LspDiagnostic Diag;
                size_t StartObj = JsonMessage.find("\"start\"", SearchPos);
                size_t EndObj = JsonMessage.find("\"end\"", SearchPos);

                if (StartObj == std::string::npos || EndObj == std::string::npos) {
                    break;
                }

                Diag.Line = ExtractJsonIntField(JsonMessage, "line", StartObj, LspZeroInt);
                Diag.StartChar = ExtractJsonIntField(JsonMessage, "character", StartObj, LspZeroInt);
                Diag.EndChar = ExtractJsonIntField(JsonMessage, "character", EndObj, Diag.StartChar + LspOneInt);
                Diag.Severity = ExtractJsonIntField(JsonMessage, "severity", EndObj, LspErrorSeverity);
                Diag.Message = ExtractJsonStringField(JsonMessage, "message", EndObj);

                ParsedDiags.push_back(Diag);
                SearchPos = EndObj + LspOneInt;
            }
        }

        std::lock_guard<std::mutex> Lock(LspMutex);
        DiagnosticsByUri[Uri] = std::move(ParsedDiags);

        return;
    }

    int ResponseId = ExtractJsonIntField(JsonMessage, "id", LspZeroInt, LspNegativeOneInt);

    if (ResponseId == LspNegativeOneInt) {
        return;
    }

    std::lock_guard<std::mutex> Lock(LspMutex);

    if (ResponseId == LastCompletionRequestId) {
        std::vector<std::string> Labels;
        size_t SearchPos = LspZeroInt;
        const size_t MaxCompletionItems = 25;

        while (Labels.size() < MaxCompletionItems) {
            size_t LabelKey = JsonMessage.find("\"label\"", SearchPos);

            if (LabelKey == std::string::npos) {
                break;
            }

            std::string Label = ExtractJsonStringField(JsonMessage, "label", LabelKey);

            if (!Label.empty()) {
                Labels.push_back(Label);
            }

            SearchPos = LabelKey + LspThreeInt;
        }

        std::string Joined;

        for (size_t Idx = LspZeroInt; Idx < Labels.size(); ++Idx) {
            if (Idx > LspZeroInt) {
                Joined.push_back(' ');
            }
            Joined += Labels[Idx];
        }

        PendingCompletions = Joined;
        LastCompletionRequestId = LspNegativeOneInt;
    } else if (ResponseId == LastHoverRequestId) {
        std::string HoverVal = ExtractJsonStringField(JsonMessage, "value", LspZeroInt);

        if (!HoverVal.empty()) {
            PendingHoverText = HoverVal;
        }

        LastHoverRequestId = LspNegativeOneInt;
    } else if (ResponseId == LastDefinitionRequestId) {
        std::string TargetUri = ExtractJsonStringField(JsonMessage, "uri", LspZeroInt);

        if (!TargetUri.empty()) {
            size_t StartObj = JsonMessage.find("\"start\"");
            int LineVal = (StartObj != std::string::npos) ? ExtractJsonIntField(JsonMessage, "line", StartObj, LspZeroInt) : LspZeroInt;
            int CharVal = (StartObj != std::string::npos) ? ExtractJsonIntField(JsonMessage, "character", StartObj, LspZeroInt) : LspZeroInt;

            PendingDefinition.HasTarget = true;
            PendingDefinition.FilePath = FileUriToPath(TargetUri);
            PendingDefinition.Line = LineVal + LspOneInt;
            PendingDefinition.Column = CharVal + LspOneInt;
        }

        LastDefinitionRequestId = LspNegativeOneInt;
    }
}

// Spawns a language server process and sends the LSP initialize handshake.
bool LspClient::StartForWorkspace(const std::string& ServerCommand, const std::string& WorkspacePath) {
    const DWORD ZeroDword = 0;

    Stop();

    if (ServerCommand.empty() || WorkspacePath.empty()) {
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
    std::vector<char> CmdBuf(ServerCommand.begin(), ServerCommand.end());
    CmdBuf.push_back('\0');

    BOOL Created = CreateProcessA(
        nullptr,
        CmdBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        WorkspacePath.c_str(),
        &StartInfo,
        &ProcInfo
    );

    CloseHandle(StdoutWrite);
    CloseHandle(StdinRead);

    if (!Created) {
        CloseHandle(StdoutRead);
        CloseHandle(NewStdinWrite);

        return false;
    }

    CloseHandle(ProcInfo.hThread);

    {
        std::lock_guard<std::mutex> Lock(LspMutex);
        ServerProcess = ProcInfo.hProcess;
        StdinWrite = NewStdinWrite;
        IsRunning = true;
        ActiveServerCommand = ServerCommand;
    }

    // Spawns a background reader thread to parse Content-Length JSON-RPC frames.
    std::thread([this, StdoutRead, ProcInfo, ZeroDword]() {
        constexpr int ReadChunkSize = 4095;
        char Chunk[ReadChunkSize];
        DWORD BytesRead = ZeroDword;
        std::string Buffer;
        const std::string HeaderPrefix = "Content-Length:";
        const std::string HeaderDelim = "\r\n\r\n";
        const size_t HeaderDelimLen = 5 - 1;

        while (ReadFile(StdoutRead, Chunk, ReadChunkSize - LspOneInt, &BytesRead, nullptr) && BytesRead > ZeroDword) {
            Buffer.append(Chunk, static_cast<size_t>(BytesRead));

            while (true) {
                size_t HeaderEnd = Buffer.find(HeaderDelim);

                if (HeaderEnd == std::string::npos) {
                    break;
                }

                size_t LenPos = Buffer.find(HeaderPrefix);
                int ContentLength = LspZeroInt;

                if (LenPos != std::string::npos && LenPos < HeaderEnd) {
                    try {
                        ContentLength = std::stoi(Buffer.substr(LenPos + HeaderPrefix.size()));
                    } catch (...) {
                        ContentLength = LspZeroInt;
                    }
                }

                size_t BodyStart = HeaderEnd + HeaderDelimLen;

                if (Buffer.size() < BodyStart + static_cast<size_t>(ContentLength)) {
                    break;
                }

                std::string Body = Buffer.substr(BodyStart, static_cast<size_t>(ContentLength));
                Buffer.erase(LspZeroInt, BodyStart + static_cast<size_t>(ContentLength));

                if (!Body.empty()) {
                    ProcessIncomingMessage(Body);
                }
            }
        }

        {
            std::lock_guard<std::mutex> Lock(LspMutex);

            if (ServerProcess == ProcInfo.hProcess) {
                ServerProcess = nullptr;
                IsRunning = false;
            }
        }

        CloseHandle(StdoutRead);
        CloseHandle(ProcInfo.hProcess);
    }).detach();

    // Sends LSP initialize request and initialized notification.
    int InitId = NextRequestId++;
    std::string RootUri = EscapeJson(PathToFileUri(WorkspacePath));
    std::string InitJson = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(InitId) + ",\"method\":\"initialize\",\"params\":{\"processId\":null,\"rootUri\":\"" + RootUri + "\",\"capabilities\":{}}}";
    SendJsonRpc(InitJson);
    SendJsonRpc("{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}");

    return true;
}

// Terminates the active language server process and closes pipes safely.
void LspClient::Stop() {
    const UINT ExitCodeZero = 0;

    std::lock_guard<std::mutex> Lock(LspMutex);

    if (StdinWrite != nullptr) {
        CloseHandle(StdinWrite);
        StdinWrite = nullptr;
    }

    if (ServerProcess != nullptr) {
        TerminateProcess(ServerProcess, ExitCodeZero);
        ServerProcess = nullptr;
    }

    IsRunning = false;
    DiagnosticsByUri.clear();
}

// Sends textDocument/didOpen to the language server.
void LspClient::NotifyDidOpen(const std::string& FilePath, const std::string& LanguageId, const std::string& Text) {
    if (!IsRunning) {
        return;
    }

    std::string Uri = EscapeJson(PathToFileUri(FilePath));
    std::string EscText = EscapeJson(Text);
    std::string Body = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"" + Uri + "\",\"languageId\":\"" + LanguageId + "\",\"version\":1,\"text\":\"" + EscText + "\"}}}";
    SendJsonRpc(Body);
}

// Sends textDocument/didChange with full document text to the language server.
void LspClient::NotifyDidChange(const std::string& FilePath, const std::string& Text) {
    if (!IsRunning) {
        return;
    }

    DocumentVersion++;
    std::string Uri = EscapeJson(PathToFileUri(FilePath));
    std::string EscText = EscapeJson(Text);
    std::string Body = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":\"" + Uri + "\",\"version\":" + std::to_string(DocumentVersion) + "},\"contentChanges\":[{\"text\":\"" + EscText + "\"}]}}";
    SendJsonRpc(Body);
}

// Sends textDocument/didSave to the language server.
void LspClient::NotifyDidSave(const std::string& FilePath) {
    if (!IsRunning) {
        return;
    }

    std::string Uri = EscapeJson(PathToFileUri(FilePath));
    std::string Body = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didSave\",\"params\":{\"textDocument\":{\"uri\":\"" + Uri + "\"}}}";
    SendJsonRpc(Body);
}

// Sends textDocument/completion request at the current caret position.
void LspClient::RequestCompletion(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar) {
    if (!IsRunning) {
        return;
    }

    int ReqId = NextRequestId++;
    LastCompletionRequestId = ReqId;
    std::string Uri = EscapeJson(PathToFileUri(FilePath));
    std::string Body = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(ReqId) + ",\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":\"" + Uri + "\"},\"position\":{\"line\":" + std::to_string(ZeroBasedLine) + ",\"character\":" + std::to_string(ZeroBasedChar) + "}}}";
    SendJsonRpc(Body);
}

// Sends textDocument/hover request for a hovered symbol position.
void LspClient::RequestHover(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar, int BytePos) {
    if (!IsRunning) {
        return;
    }

    int ReqId = NextRequestId++;
    LastHoverRequestId = ReqId;
    PendingHoverPos = BytePos;
    std::string Uri = EscapeJson(PathToFileUri(FilePath));
    std::string Body = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(ReqId) + ",\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"" + Uri + "\"},\"position\":{\"line\":" + std::to_string(ZeroBasedLine) + ",\"character\":" + std::to_string(ZeroBasedChar) + "}}}";
    SendJsonRpc(Body);
}

// Sends textDocument/definition request (F12).
void LspClient::RequestDefinition(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar) {
    if (!IsRunning) {
        return;
    }

    int ReqId = NextRequestId++;
    LastDefinitionRequestId = ReqId;
    std::string Uri = EscapeJson(PathToFileUri(FilePath));
    std::string Body = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(ReqId) + ",\"method\":\"textDocument/definition\",\"params\":{\"textDocument\":{\"uri\":\"" + Uri + "\"},\"position\":{\"line\":" + std::to_string(ZeroBasedLine) + ",\"character\":" + std::to_string(ZeroBasedChar) + "}}}";
    SendJsonRpc(Body);
}

// Sends textDocument/declaration request.
void LspClient::RequestDeclaration(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar) {
    if (!IsRunning) {
        return;
    }

    int ReqId = NextRequestId++;
    LastDefinitionRequestId = ReqId;
    std::string Uri = EscapeJson(PathToFileUri(FilePath));
    std::string Body = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(ReqId) + ",\"method\":\"textDocument/declaration\",\"params\":{\"textDocument\":{\"uri\":\"" + Uri + "\"},\"position\":{\"line\":" + std::to_string(ZeroBasedLine) + ",\"character\":" + std::to_string(ZeroBasedChar) + "}}}";
    SendJsonRpc(Body);
}

// Sends textDocument/typeDefinition request.
void LspClient::RequestTypeDefinition(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar) {
    if (!IsRunning) {
        return;
    }

    int ReqId = NextRequestId++;
    LastDefinitionRequestId = ReqId;
    std::string Uri = EscapeJson(PathToFileUri(FilePath));
    std::string Body = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(ReqId) + ",\"method\":\"textDocument/typeDefinition\",\"params\":{\"textDocument\":{\"uri\":\"" + Uri + "\"},\"position\":{\"line\":" + std::to_string(ZeroBasedLine) + ",\"character\":" + std::to_string(ZeroBasedChar) + "}}}";
    SendJsonRpc(Body);
}

// Sends textDocument/implementation request.
void LspClient::RequestImplementation(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar) {
    if (!IsRunning) {
        return;
    }

    int ReqId = NextRequestId++;
    LastDefinitionRequestId = ReqId;
    std::string Uri = EscapeJson(PathToFileUri(FilePath));
    std::string Body = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(ReqId) + ",\"method\":\"textDocument/implementation\",\"params\":{\"textDocument\":{\"uri\":\"" + Uri + "\"},\"position\":{\"line\":" + std::to_string(ZeroBasedLine) + ",\"character\":" + std::to_string(ZeroBasedChar) + "}}}";
    SendJsonRpc(Body);
}

// Sends textDocument/references request.
void LspClient::RequestReferences(const std::string& FilePath, int ZeroBasedLine, int ZeroBasedChar) {
    if (!IsRunning) {
        return;
    }

    int ReqId = NextRequestId++;
    LastDefinitionRequestId = ReqId;
    std::string Uri = EscapeJson(PathToFileUri(FilePath));
    std::string Body = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(ReqId) + ",\"method\":\"textDocument/references\",\"params\":{\"textDocument\":{\"uri\":\"" + Uri + "\"},\"position\":{\"line\":" + std::to_string(ZeroBasedLine) + ",\"character\":" + std::to_string(ZeroBasedChar) + "},\"context\":{\"includeDeclaration\":true}}}";
    SendJsonRpc(Body);
}

// Retrieves and clears pending autocomplete items if available.
bool LspClient::PopCompletions(std::string& OutList) {
    std::lock_guard<std::mutex> Lock(LspMutex);

    if (PendingCompletions.empty()) {
        return false;
    }

    OutList = std::move(PendingCompletions);
    PendingCompletions.clear();

    return true;
}

// Retrieves and clears pending hover documentation text if available.
bool LspClient::PopHover(std::string& OutText, int& OutPos) {
    std::lock_guard<std::mutex> Lock(LspMutex);

    if (PendingHoverText.empty()) {
        return false;
    }

    OutText = std::move(PendingHoverText);
    OutPos = PendingHoverPos;
    PendingHoverText.clear();

    return true;
}

// Retrieves and clears a pending go-to-definition target if available.
bool LspClient::PopDefinition(LspDefinitionTarget& OutTarget) {
    std::lock_guard<std::mutex> Lock(LspMutex);

    if (!PendingDefinition.HasTarget) {
        return false;
    }

    OutTarget = PendingDefinition;
    PendingDefinition.HasTarget = false;

    return true;
}

// Returns the latest diagnostics for the specified file path.
std::vector<LspDiagnostic> LspClient::GetDiagnosticsForFile(const std::string& FilePath) {
    std::string Uri = PathToFileUri(FilePath);
    std::lock_guard<std::mutex> Lock(LspMutex);

    auto Iter = DiagnosticsByUri.find(Uri);

    if (Iter == DiagnosticsByUri.end()) {
        return {};
    }

    return Iter->second;
}

// Returns the total number of diagnostics across all open workspace files.
int LspClient::GetTotalDiagnosticCount() {
    std::lock_guard<std::mutex> Lock(LspMutex);
    int Total = LspZeroInt;

    for (const auto& Entry : DiagnosticsByUri) {
        Total += static_cast<int>(Entry.second.size());
    }

    return Total;
}
