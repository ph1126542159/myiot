#include "TerminalWebSocketRequestHandler.h"

#include "Poco/Dynamic/Var.h"
#include "Poco/File.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"
#include "Poco/Path.h"
#include "Poco/String.h"
#include "Poco/UUIDGenerator.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <consoleapi.h>
#include <processthreadsapi.h>
#include <synchapi.h>
#include <windows.h>
#endif

namespace {

Poco::OSP::Web::WebSession::Ptr findSession(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request)
{
    Poco::OSP::Web::WebSessionManager::Ptr pSessionManager =
        Poco::OSP::ServiceFinder::find<Poco::OSP::Web::WebSessionManager>(pContext);
    return pSessionManager->find(pContext->thisBundle()->properties().getString("websession.id", "myiot.webui"), request);
}

bool isAuthenticated(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request)
{
    try
    {
        Poco::OSP::Web::WebSession::Ptr pSession = findSession(pContext, request);
        return pSession && !pSession->getValue<std::string>("username", "").empty();
    }
    catch (...)
    {
        return false;
    }
}

std::string sessionUsername(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request)
{
    try
    {
        Poco::OSP::Web::WebSession::Ptr pSession = findSession(pContext, request);
        return pSession ? pSession->getValue<std::string>("username", "") : "";
    }
    catch (...)
    {
        return std::string();
    }
}

void sendJSON(
    Poco::Net::HTTPServerResponse& response,
    Poco::JSON::Object::Ptr payload,
    Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_OK)
{
    response.setStatus(status);
    response.setChunkedTransferEncoding(true);
    response.setContentType("application/json");
    response.set("Cache-Control", "no-cache");
    std::ostream& out = response.send();
    Poco::JSON::Stringifier::stringify(payload, out);
}

Poco::JSON::Object::Ptr createUnauthorizedPayload()
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", false);
    payload->set("ok", false);
    payload->set("message", "Not authenticated.");
    return payload;
}

#if defined(_WIN32)

using CreatePseudoConsoleFn = HRESULT (WINAPI*)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
using ResizePseudoConsoleFn = HRESULT (WINAPI*)(HPCON, COORD);
using ClosePseudoConsoleFn = void (WINAPI*)(HPCON);

struct ConPtyApi
{
    CreatePseudoConsoleFn createPseudoConsole = nullptr;
    ResizePseudoConsoleFn resizePseudoConsole = nullptr;
    ClosePseudoConsoleFn closePseudoConsole = nullptr;

    bool available() const
    {
        return createPseudoConsole && resizePseudoConsole && closePseudoConsole;
    }
};

ConPtyApi loadConPtyApi()
{
    ConPtyApi api;

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) return api;

    api.createPseudoConsole =
        reinterpret_cast<CreatePseudoConsoleFn>(GetProcAddress(kernel32, "CreatePseudoConsole"));
    api.resizePseudoConsole =
        reinterpret_cast<ResizePseudoConsoleFn>(GetProcAddress(kernel32, "ResizePseudoConsole"));
    api.closePseudoConsole =
        reinterpret_cast<ClosePseudoConsoleFn>(GetProcAddress(kernel32, "ClosePseudoConsole"));

    return api;
}

const ConPtyApi& conPtyApi()
{
    static const ConPtyApi api = loadConPtyApi();
    return api;
}

std::wstring wideFromUtf8(const std::string& value)
{
    if (value.empty()) return std::wstring();

    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::string utf8FromWide(const std::wstring& value)
{
    if (value.empty()) return std::string();

    const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}

std::string searchExecutable(const std::wstring& executableName)
{
    const DWORD required = SearchPathW(nullptr, executableName.c_str(), nullptr, 0, nullptr, nullptr);
    if (required == 0) return std::string();

    std::wstring buffer(static_cast<std::size_t>(required), L'\0');
    const DWORD length = SearchPathW(nullptr, executableName.c_str(), nullptr, required, buffer.data(), nullptr);
    if (length == 0 || length >= required) return std::string();

    return utf8FromWide(std::wstring(buffer.c_str(), static_cast<std::size_t>(length)));
}

std::string firstExistingExecutable(const std::vector<std::string>& candidates)
{
    for (const auto& candidate: candidates)
    {
        std::error_code error;
        if (!candidate.empty() && std::filesystem::exists(candidate, error))
        {
            return candidate;
        }
    }

    return std::string();
}

std::string gitBashExecutable()
{
    const std::string preferred = firstExistingExecutable({
        "C:\\Program Files\\Git\\bin\\bash.exe",
        "C:\\Program Files\\Git\\usr\\bin\\bash.exe",
        "C:\\Program Files (x86)\\Git\\bin\\bash.exe",
        "C:\\Program Files (x86)\\Git\\usr\\bin\\bash.exe"
    });
    if (!preferred.empty()) return preferred;

    return searchExecutable(L"bash.exe");
}

enum class TerminalShellKind
{
    gitBash,
    windowsPowerShell
};

struct TerminalShellBackend
{
    TerminalShellKind kind = TerminalShellKind::windowsPowerShell;
    std::string executable;
    std::string displayName;
};

TerminalShellBackend detectTerminalShellBackend()
{
    const std::string gitBash = gitBashExecutable();
    if (!gitBash.empty())
    {
        return {TerminalShellKind::gitBash, gitBash, "Git Bash"};
    }

    const std::string powershell = searchExecutable(L"powershell.exe");
    if (!powershell.empty())
    {
        return {TerminalShellKind::windowsPowerShell, powershell, "Windows PowerShell"};
    }

    return {TerminalShellKind::windowsPowerShell, "powershell.exe", "Windows PowerShell"};
}

std::string normalizeSlashPath(std::string value)
{
    Poco::replaceInPlace(value, std::string("\\"), std::string("/"));
    return value;
}

bool isWindowsDrivePath(const std::string& value)
{
    return value.size() >= 2 &&
        ((value[0] >= 'a' && value[0] <= 'z') || (value[0] >= 'A' && value[0] <= 'Z')) &&
        value[1] == ':';
}

std::string bashPathFromWindowsPath(const std::string& value)
{
    std::string normalized = normalizeSlashPath(value);
    if (!isWindowsDrivePath(normalized)) return normalized;

    std::string result("/");
    result += static_cast<char>(std::tolower(static_cast<unsigned char>(normalized[0])));
    if (normalized.size() > 2)
    {
        if (normalized[2] != '/') result += '/';
        result += normalized.substr(2);
    }
    return result;
}

std::string escapeSingleQuotedBashValue(const std::string& value)
{
    std::string escaped = value;
    Poco::replaceInPlace(escaped, std::string("'"), std::string("'\\''"));
    return escaped;
}

std::wstring quoteWindowsArgument(const std::wstring& value)
{
    std::wstring escaped = value;
    Poco::replaceInPlace(escaped, std::wstring(L"\""), std::wstring(L"\\\""));
    return L"\"" + escaped + L"\"";
}

void closeHandleIfValid(HANDLE& handle)
{
    if (handle && handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
}

std::string createDiagnosticHelperScript()
{
    const std::string id = Poco::UUIDGenerator::defaultGenerator().createOne().toString();
    const std::filesystem::path scriptPath =
        std::filesystem::temp_directory_path() / ("myiot-process-console-" + id + ".ps1");

    std::ofstream out(scriptPath.string(), std::ios::binary | std::ios::trunc);
    out <<
R"PS([CmdletBinding()]
param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$CommandName,

    [Parameter(Position=1, ValueFromRemainingArguments=$true)]
    [string[]]$Arguments
)

$commandLine = $CommandName
if ($Arguments -and $Arguments.Count -gt 0)
{
    $commandLine = "$CommandName $($Arguments -join ' ')"
}

try
{
    [Console]::InputEncoding = [System.Text.UTF8Encoding]::new($false)
    [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
    $ProgressPreference = 'SilentlyContinue'

    $baseUri = [Uri]'http://127.0.0.1:22080'
    $webSession = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $cookieHeader = $env:MYIOT_WEB_COOKIE_HEADER
    if ($null -eq $cookieHeader) { $cookieHeader = '' }
    foreach ($cookiePair in ($cookieHeader -split ';\s*'))
    {
        if ([string]::IsNullOrWhiteSpace($cookiePair)) { continue }
        $separatorIndex = $cookiePair.IndexOf('=')
        if ($separatorIndex -lt 1) { continue }

        $cookieName = $cookiePair.Substring(0, $separatorIndex).Trim()
        $cookieValue = $cookiePair.Substring($separatorIndex + 1)
        $cookie = New-Object System.Net.Cookie($cookieName, $cookieValue, '/', $baseUri.Host)
        $webSession.Cookies.Add($baseUri, $cookie)
    }

    if ($CommandName -eq 'top-snapshot')
    {
        Get-Process |
            Sort-Object CPU -Descending |
            Select-Object -First 25 `
                @{Name='PID';Expression={$_.Id}}, `
                @{Name='NAME';Expression={$_.ProcessName}}, `
                @{Name='CPU(s)';Expression={ if ($_.CPU -ne $null) { '{0:N1}' -f $_.CPU } else { '0.0' } }}, `
                @{Name='WS(MB)';Expression={ '{0:N1}' -f ($_.WS / 1MB) }}, `
                @{Name='PM(MB)';Expression={ '{0:N1}' -f ($_.PM / 1MB) }} |
            Format-Table -AutoSize |
            Out-String -Width 220 |
            Write-Output
        exit 0
    }

    $body = 'command=' + [System.Uri]::EscapeDataString($commandLine)
    $response = Invoke-WebRequest -UseBasicParsing `
        -Uri 'http://127.0.0.1:22080/myiot/services/process-console/exec' `
        -Method Post `
        -Headers @{ Accept = 'application/json' } `
        -WebSession $webSession `
        -ContentType 'application/x-www-form-urlencoded' `
        -Body $body

    $response.RawContentStream.Position = 0
    $reader = New-Object System.IO.StreamReader($response.RawContentStream, [System.Text.UTF8Encoding]::new($false))
    $jsonText = $reader.ReadToEnd()
    $reader.Dispose()
    $responsePayload = $jsonText | ConvertFrom-Json

    if ($responsePayload.output)
    {
        foreach ($line in $responsePayload.output)
        {
            [Console]::Out.WriteLine([string]$line)
        }
    }

    if (-not $responsePayload.ok)
    {
        if ($responsePayload.message)
        {
            [Console]::Error.WriteLine([string]$responsePayload.message)
        }
        exit 1
    }
}
catch
{
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
)PS";

    return scriptPath.string();
}

std::string createGitBashRcFile(
    const std::string& cookieHeader,
    const std::string& helperScriptPath,
    const std::string& sessionUsername)
{
    const std::string id = Poco::UUIDGenerator::defaultGenerator().createOne().toString();
    const std::filesystem::path scriptPath =
        std::filesystem::temp_directory_path() / ("myiot-process-console-" + id + ".bashrc");
    const std::string helperPath = bashPathFromWindowsPath(helperScriptPath);
    const std::string promptUser = sessionUsername.empty() ? "user" : sessionUsername;

    std::ofstream out(scriptPath.string(), std::ios::binary | std::ios::trunc);
    out <<
        "#!/usr/bin/env bash\n"
        "export TERM=xterm-256color\n"
        "export MYIOT_WEB_COOKIE_HEADER='" << escapeSingleQuotedBashValue(cookieHeader) << "'\n"
        "export MYIOT_CONSOLE_HELPER='" << escapeSingleQuotedBashValue(helperPath) << "'\n"
        "\n"
        "myiot_diag(){\n"
        "  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File \"$MYIOT_CONSOLE_HELPER\" \"$@\"\n"
        "}\n"
        "\n"
        "summary(){ myiot_diag summary \"$@\"; }\n"
        "status(){ myiot_diag summary \"$@\"; }\n"
        "threads(){ myiot_diag threads \"$@\"; }\n"
        "modules(){ myiot_diag modules \"$@\"; }\n"
        "stack(){ myiot_diag stack \"$@\"; }\n"
        "functions(){ myiot_diag functions \"$@\"; }\n"
        "help(){\n"
        "  if [ \"$#\" -eq 0 ]; then\n"
        "    myiot_diag help\n"
        "    printf '\\nLinux-compatible shell commands run directly in Git Bash.\\n'\n"
        "  else\n"
        "    builtin help \"$@\"\n"
        "  fi\n"
        "}\n"
        "\n"
        "__myiot_emit_cwd(){\n"
        "  printf '\\033]633;Cwd=%s\\007' \"$PWD\"\n"
        "}\n"
        "\n"
        "__myiot_top_snapshot(){\n"
        "  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File \"$MYIOT_CONSOLE_HELPER\" top-snapshot\n"
        "}\n"
        "\n"
        "if ! command -v top >/dev/null 2>&1; then\n"
        "  top(){\n"
        "    while true; do\n"
        "      clear\n"
        "      printf 'myiot top compatibility view (press q to quit)\\n\\n'\n"
        "      __myiot_top_snapshot\n"
        "      IFS= read -rsn1 -t 1 key || true\n"
        "      [ \"$key\" = \"q\" ] && break\n"
        "    done\n"
        "  }\n"
        "fi\n"
        "\n"
        "if ! command -v htop >/dev/null 2>&1; then\n"
        "  htop(){ top \"$@\"; }\n"
        "fi\n"
        "\n"
        "PROMPT_COMMAND=\"__myiot_emit_cwd${PROMPT_COMMAND:+;$PROMPT_COMMAND}\"\n"
        "PS1='" << escapeSingleQuotedBashValue(promptUser) << "@myiot:\\w\\$ '\n"
        "clear\n"
        "printf 'MyIoT PTY terminal ready. Shell backend: Git Bash\\n'\n"
        "__myiot_emit_cwd\n";

    return scriptPath.string();
}

class TerminalPtySession
{
public:
    TerminalPtySession(
        Poco::OSP::BundleContext::Ptr pContext,
        Poco::Net::WebSocket& webSocket,
        Poco::Net::HTTPServerRequest& request):
        _pContext(pContext),
        _webSocket(webSocket),
        _cookieHeader(request.get("Cookie", "")),
        _username(sessionUsername(pContext, request)),
        _backend(detectTerminalShellBackend())
    {
        try
        {
            _workingDirectory = std::filesystem::current_path().string();
        }
        catch (...)
        {
            _workingDirectory = Poco::Path::current();
        }
    }

    ~TerminalPtySession()
    {
        cleanup();
    }

    void run()
    {
        if (!conPtyApi().available())
        {
            _pContext->logger().error("PTY session rejected: ConPTY runtime unavailable.");
            sendError("Current Windows runtime does not provide ConPTY. Install WSL or use a newer Windows console runtime.");
            return;
        }

        if (!initialize())
        {
            return;
        }

        _running = true;
        _outputThread = std::thread([this]() { outputLoop(); });
        _pContext->logger().information(
            "PTY session started: user=" + (_username.empty() ? std::string("-") : _username) +
            " backend=" + _backend.displayName +
            " cwd=" + _workingDirectory);

        sendReady();

        _webSocket.setReceiveTimeout(Poco::Timespan(0, 200000));

        char buffer[8192] = {0};
        while (_running)
        {
            try
            {
                int flags = 0;
                const int n = _webSocket.receiveFrame(buffer, static_cast<int>(sizeof(buffer)), flags);
                if ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_CLOSE || n <= 0)
                {
                    _pContext->logger().information("PTY session closing: websocket close frame received.");
                    break;
                }

                if ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_TEXT)
                {
                    handleMessage(std::string(buffer, static_cast<std::size_t>(n)));
                }
            }
            catch (const Poco::TimeoutException&)
            {
            }
            catch (const Poco::Exception& exc)
            {
                _pContext->logger().warning("PTY session receive loop aborted: " + exc.displayText());
                break;
            }

            if (_processInfo.hProcess && WaitForSingleObject(_processInfo.hProcess, 0) == WAIT_OBJECT_0)
            {
                DWORD exitCode = 0;
                if (GetExitCodeProcess(_processInfo.hProcess, &exitCode))
                {
                    _pContext->logger().information("PTY shell exited with code " + std::to_string(exitCode) + ".");
                }
                _running = false;
                break;
            }
        }

        _running = false;
        closeHandleIfValid(_inputWrite);

        if (_outputThread.joinable())
        {
            _outputThread.join();
        }

        _pContext->logger().information("PTY session stopped.");
        sendExit();
    }

private:
    bool initialize()
    {
        if (_backend.kind == TerminalShellKind::gitBash)
        {
            _helperScriptPath = createDiagnosticHelperScript();
            _shellBootstrapPath = createGitBashRcFile(_cookieHeader, _helperScriptPath, _username);
        }

        SECURITY_ATTRIBUTES securityAttributes;
        ZeroMemory(&securityAttributes, sizeof(securityAttributes));
        securityAttributes.nLength = sizeof(securityAttributes);
        securityAttributes.bInheritHandle = TRUE;

        HANDLE ptyInputRead = INVALID_HANDLE_VALUE;
        HANDLE ptyOutputWrite = INVALID_HANDLE_VALUE;

        if (!CreatePipe(&ptyInputRead, &_inputWrite, &securityAttributes, 0) ||
            !CreatePipe(&_outputRead, &ptyOutputWrite, &securityAttributes, 0))
        {
            closeHandleIfValid(ptyInputRead);
            closeHandleIfValid(ptyOutputWrite);
            _pContext->logger().error("PTY initialization failed: unable to create PTY pipes.");
            sendError("Unable to create PTY pipes.");
            return false;
        }

        SetHandleInformation(_inputWrite, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(_outputRead, HANDLE_FLAG_INHERIT, 0);

        const COORD size = {static_cast<SHORT>(_cols), static_cast<SHORT>(_rows)};
        const HRESULT hr =
            conPtyApi().createPseudoConsole(size, ptyInputRead, ptyOutputWrite, 0, &_pseudoConsole);
        closeHandleIfValid(ptyInputRead);
        closeHandleIfValid(ptyOutputWrite);

        if (FAILED(hr))
        {
            _pContext->logger().error("PTY initialization failed: unable to create pseudo console.");
            sendError("Unable to create pseudo console.");
            return false;
        }

        SIZE_T attributeListSize = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeListSize);
        _attributeListBuffer.resize(attributeListSize);
        _startupInfo.lpAttributeList =
            reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(_attributeListBuffer.data());
        _startupInfo.StartupInfo.cb = sizeof(_startupInfo);

        if (!InitializeProcThreadAttributeList(_startupInfo.lpAttributeList, 1, 0, &attributeListSize))
        {
            _pContext->logger().error("PTY initialization failed: unable to initialize attribute list.");
            sendError("Unable to initialize PTY attribute list.");
            return false;
        }

        if (!UpdateProcThreadAttribute(
                _startupInfo.lpAttributeList,
                0,
                PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                _pseudoConsole,
                sizeof(_pseudoConsole),
                nullptr,
                nullptr))
        {
            _pContext->logger().error("PTY initialization failed: unable to attach pseudo console.");
            sendError("Unable to attach pseudo console to child process.");
            return false;
        }

        std::wstring commandLine = buildCommandLine();
        std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
        commandBuffer.push_back(L'\0');

        const BOOL created = CreateProcessW(
            wideFromUtf8(_backend.executable).c_str(),
            commandBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
            nullptr,
            wideFromUtf8(_workingDirectory).c_str(),
            &_startupInfo.StartupInfo,
            &_processInfo);

        DeleteProcThreadAttributeList(_startupInfo.lpAttributeList);
        _startupInfo.lpAttributeList = nullptr;

        if (!created)
        {
            _pContext->logger().error(
                "PTY initialization failed: unable to launch terminal shell process for backend " + _backend.displayName + ".");
            sendError("Unable to launch terminal shell process.");
            return false;
        }

        closeHandleIfValid(_processInfo.hThread);
        return true;
    }

    std::wstring buildCommandLine() const
    {
        switch (_backend.kind)
        {
        case TerminalShellKind::gitBash:
            return L"bash.exe --noprofile --rcfile " + quoteWindowsArgument(wideFromUtf8(_shellBootstrapPath)) + L" -i";
        case TerminalShellKind::windowsPowerShell:
        default:
            return L"powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -NoExit";
        }
    }

    void outputLoop()
    {
        std::vector<char> buffer(8192, 0);
        while (_running)
        {
            DWORD bytesRead = 0;
            const BOOL ok = ReadFile(_outputRead, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr);
            if (!ok || bytesRead == 0)
            {
                break;
            }

            try
            {
                std::lock_guard<std::mutex> lock(_sendMutex);
                _webSocket.sendFrame(buffer.data(), static_cast<int>(bytesRead), Poco::Net::WebSocket::FRAME_BINARY);
            }
            catch (...)
            {
                _pContext->logger().warning("PTY output loop aborted while forwarding terminal bytes.");
                _running = false;
                break;
            }
        }
    }

    void handleMessage(const std::string& message)
    {
        try
        {
            Poco::JSON::Parser parser;
            Poco::Dynamic::Var parsed = parser.parse(message);
            Poco::JSON::Object::Ptr payload = parsed.extract<Poco::JSON::Object::Ptr>();
            if (!payload) return;

            const std::string type = payload->getValue<std::string>("type");
            if (type == "input")
            {
                writeToPty(payload->getValue<std::string>("data"));
            }
            else if (type == "resize")
            {
                const int cols = payload->getValue<int>("cols");
                const int rows = payload->getValue<int>("rows");
                resize(cols, rows);
            }
            else if (type == "command")
            {
                writeToPty(payload->getValue<std::string>("data") + "\r");
            }
        }
        catch (const Poco::Exception& exc)
        {
            _pContext->logger().warning("PTY session ignored malformed client payload: " + exc.displayText());
        }
    }

    void writeToPty(const std::string& data)
    {
        if (!_inputWrite || _inputWrite == INVALID_HANDLE_VALUE || data.empty()) return;

        DWORD written = 0;
        WriteFile(_inputWrite, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
    }

    void resize(int cols, int rows)
    {
        if (cols < 20 || rows < 5) return;
        _cols = cols;
        _rows = rows;
        if (_pseudoConsole)
        {
            const COORD size = {static_cast<SHORT>(_cols), static_cast<SHORT>(_rows)};
            conPtyApi().resizePseudoConsole(_pseudoConsole, size);
        }
    }

    void sendReady()
    {
        Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
        payload->set("type", "ready");
        payload->set("shellBackendDisplayName", _backend.displayName);
        payload->set("displayWorkingDirectory", _backend.kind == TerminalShellKind::gitBash
            ? bashPathFromWindowsPath(_workingDirectory)
            : _workingDirectory);
        payload->set("username", _username);
        sendControl(payload);
    }

    void sendExit()
    {
        Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
        payload->set("type", "exit");
        sendControl(payload);
    }

    void sendError(const std::string& message)
    {
        Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
        payload->set("type", "error");
        payload->set("message", message);
        sendControl(payload);
    }

    void sendControl(Poco::JSON::Object::Ptr payload)
    {
        try
        {
            std::ostringstream oss;
            Poco::JSON::Stringifier::stringify(payload, oss);
            const std::string message = oss.str();
            std::lock_guard<std::mutex> lock(_sendMutex);
            _webSocket.sendFrame(message.data(), static_cast<int>(message.size()), Poco::Net::WebSocket::FRAME_TEXT);
        }
        catch (...)
        {
        }
    }

    void cleanup()
    {
        _running = false;

        closeHandleIfValid(_inputWrite);
        closeHandleIfValid(_outputRead);

        if (_processInfo.hProcess)
        {
            TerminateProcess(_processInfo.hProcess, 0);
            closeHandleIfValid(_processInfo.hProcess);
        }

        if (_pseudoConsole)
        {
            conPtyApi().closePseudoConsole(_pseudoConsole);
            _pseudoConsole = nullptr;
        }

        if (!_helperScriptPath.empty())
        {
            std::error_code error;
            std::filesystem::remove(_helperScriptPath, error);
        }

        if (!_shellBootstrapPath.empty())
        {
            std::error_code error;
            std::filesystem::remove(_shellBootstrapPath, error);
        }
    }

    Poco::OSP::BundleContext::Ptr _pContext;
    Poco::Net::WebSocket& _webSocket;
    std::string _cookieHeader;
    std::string _username;
    std::string _workingDirectory;
    std::string _helperScriptPath;
    std::string _shellBootstrapPath;
    TerminalShellBackend _backend;

    std::atomic<bool> _running{false};
    std::mutex _sendMutex;
    std::thread _outputThread;

    HANDLE _inputWrite = INVALID_HANDLE_VALUE;
    HANDLE _outputRead = INVALID_HANDLE_VALUE;
    HPCON _pseudoConsole = nullptr;
    STARTUPINFOEXW _startupInfo{};
    PROCESS_INFORMATION _processInfo{};
    std::vector<unsigned char> _attributeListBuffer;
    int _cols = 120;
    int _rows = 32;
};

#endif

} // namespace

namespace MyIoT {
namespace Services {
namespace ProcessConsole {

TerminalWebSocketRequestHandler::TerminalWebSocketRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
    _pContext(pContext)
{
}

void TerminalWebSocketRequestHandler::handleRequest(
    Poco::Net::HTTPServerRequest& request,
    Poco::Net::HTTPServerResponse& response)
{
    auto activity = Poco::OpenTelemetry::beginRequestActivity(_pContext, request, "processconsole.terminal.websocket");
    if (!isAuthenticated(_pContext, request))
    {
        Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED, "user is not authenticated");
        sendJSON(response, createUnauthorizedPayload(), Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        return;
    }

#if defined(_WIN32)
    try
    {
        Poco::Net::WebSocket webSocket(request, response);
        webSocket.setNoDelay(true);
        activity.step("websocket.accepted");

        TerminalPtySession session(_pContext, webSocket, request);
        session.run();
        Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, "pty session closed");
    }
    catch (const Poco::Exception& exc)
    {
        Poco::OpenTelemetry::failException(activity, exc);
        _pContext->logger().error("PTY websocket request failed: " + exc.displayText());
    }
#else
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("ok", false);
    payload->set("message", "PTY terminal is currently only implemented for Windows hosts.");
    Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_NOT_IMPLEMENTED, "pty terminal unsupported on this host");
    sendJSON(response, payload, Poco::Net::HTTPResponse::HTTP_NOT_IMPLEMENTED);
#endif
}

Poco::Net::HTTPRequestHandler* TerminalWebSocketRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new TerminalWebSocketRequestHandler(context());
}

} } } // namespace MyIoT::Services::ProcessConsole
