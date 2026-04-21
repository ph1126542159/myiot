#include "ProcessConsoleService.h"
#include "Poco/Environment.h"
#include "Poco/File.h"
#include "Poco/Format.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/NumberParser.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OpenTelemetry/TelemetryClient.h"
#include "Poco/Path.h"
#include "Poco/Pipe.h"
#include "Poco/PipeStream.h"
#include "Poco/Process.h"
#include "Poco/String.h"
#include "Poco/Thread.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits.h>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <dbghelp.h>
#include <tlhelp32.h>
#include <windows.h>
#else
#include <dirent.h>
#include <execinfo.h>
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

namespace MyIoT {
namespace Services {
namespace ProcessConsole {

const std::string ProcessConsoleService::SERVICE_NAME("io.myiot.services.processconsole");

namespace {

void appendLine(Poco::JSON::Array::Ptr output, const std::string& line)
{
    output->add(line);
}

std::string formatPointer(std::uintptr_t value)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << value;
    return oss.str();
}

Poco::JSON::Object::Ptr createResponse(const std::string& commandLine, const std::string& normalizedCommand)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("ok", true);
    payload->set("prompt", "process-console>");
    payload->set("command", commandLine);
    payload->set("normalizedCommand", normalizedCommand);
    payload->set("output", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    payload->set("data", Poco::JSON::Object::Ptr(new Poco::JSON::Object));
    return payload;
}

Poco::JSON::Object::Ptr createErrorResponse(const std::string& commandLine, const std::string& message)
{
    Poco::JSON::Object::Ptr payload = createResponse(commandLine, std::string());
    payload->set("ok", false);
    payload->set("message", message);
    appendLine(payload->getArray("output"), "ERROR: " + message);
    return payload;
}

std::vector<std::string> tokenize(const std::string& commandLine)
{
    std::istringstream iss(commandLine);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token)
    {
        tokens.push_back(token);
    }
    return tokens;
}

int clampLimit(int limit, int defaultValue)
{
    if (limit < 1) return defaultValue;
    if (limit > 200) return 200;
    return limit;
}

int chooseLimit(const std::vector<std::string>& tokens, int explicitLimit, int defaultValue)
{
    if (explicitLimit > 0) return clampLimit(explicitLimit, defaultValue);
    if (tokens.size() > 1)
    {
        try
        {
            return clampLimit(Poco::NumberParser::parse(tokens[1]), defaultValue);
        }
        catch (...)
        {
        }
    }
    return defaultValue;
}

std::string commandRemainder(const std::string& commandLine)
{
    const std::size_t separator = commandLine.find_first_of(" \t");
    if (separator == std::string::npos) return "";
    return Poco::trim(commandLine.substr(separator + 1));
}

std::string stripMatchingQuotes(std::string value)
{
    value = Poco::trim(value);
    if (value.size() >= 2)
    {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
        {
            value = value.substr(1, value.size() - 2);
        }
    }
    return value;
}

std::string filesystemPath(const std::filesystem::path& path)
{
    return path.lexically_normal().string();
}

std::string defaultShellWorkingDirectory()
{
    try
    {
        return filesystemPath(std::filesystem::current_path());
    }
    catch (...)
    {
        return Poco::Path::current();
    }
}

std::string shellHomeDirectory()
{
    try
    {
        return filesystemPath(std::filesystem::path(Poco::Path::home()));
    }
    catch (...)
    {
        return Poco::Path::home();
    }
}

std::string effectiveWorkingDirectory(const std::string& workingDirectory)
{
    const std::string fallback = defaultShellWorkingDirectory();
    if (workingDirectory.empty()) return fallback;

    try
    {
        std::filesystem::path candidate(workingDirectory);
        if (candidate.is_relative())
        {
            candidate = std::filesystem::path(fallback) / candidate;
        }

        std::error_code error;
        std::filesystem::path normalized = std::filesystem::weakly_canonical(candidate, error);
        if (error) normalized = candidate.lexically_normal();

        if (std::filesystem::exists(normalized, error) &&
            std::filesystem::is_directory(normalized, error))
        {
            return filesystemPath(normalized);
        }
    }
    catch (...)
    {
    }

    return fallback;
}

std::string resolveDirectoryTarget(
    const std::string& currentWorkingDirectory,
    const std::string& argument,
    const std::string& homeDirectory,
    std::string& detail)
{
    const std::string trimmedArgument = stripMatchingQuotes(argument);
    std::filesystem::path targetPath;

    if (trimmedArgument.empty() || trimmedArgument == "~")
    {
        targetPath = std::filesystem::path(homeDirectory);
    }
    else if (trimmedArgument == "-")
    {
        detail = "暂不支持 cd -，请直接输入目标目录。";
        return currentWorkingDirectory;
    }
    else if (Poco::startsWith(trimmedArgument, std::string("~/")) ||
             Poco::startsWith(trimmedArgument, std::string("~\\")))
    {
        targetPath = std::filesystem::path(homeDirectory) / trimmedArgument.substr(2);
    }
    else
    {
        targetPath = std::filesystem::path(trimmedArgument);
    }

    if (targetPath.is_relative())
    {
        targetPath = std::filesystem::path(currentWorkingDirectory) / targetPath;
    }

    std::error_code error;
    std::filesystem::path normalized = std::filesystem::weakly_canonical(targetPath, error);
    if (error) normalized = targetPath.lexically_normal();

    if (!std::filesystem::exists(normalized, error) || error)
    {
        detail = "目录不存在: " + filesystemPath(normalized);
        return currentWorkingDirectory;
    }

    if (!std::filesystem::is_directory(normalized, error) || error)
    {
        detail = "目标不是目录: " + filesystemPath(normalized);
        return currentWorkingDirectory;
    }

    return filesystemPath(normalized);
}

std::vector<std::string> splitOutputLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::string current;

    for (char ch: text)
    {
        if (ch == '\r') continue;

        if (ch == '\n')
        {
            lines.push_back(current);
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty())
    {
        lines.push_back(current);
    }

    while (!lines.empty() && lines.back().empty())
    {
        lines.pop_back();
    }

    return lines;
}

#if defined(_WIN32)
std::string normalizeCapturedWindowsOutput(const std::string& value);
#endif

std::vector<std::string> captureCommandOutput(
    const std::string& executable,
    const Poco::Process::Args& args,
    const std::string& workingDirectory,
    int timeoutMs,
    int& exitCode,
    bool& timedOut)
{
    Poco::Pipe outputPipe;
    std::string buffer;
    timedOut = false;
    exitCode = -1;

    Poco::ProcessHandle processHandle =
        Poco::Process::launch(executable, args, workingDirectory, nullptr, &outputPipe, &outputPipe);

    std::thread reader([&buffer, &outputPipe]()
    {
        Poco::PipeInputStream input(outputPipe);
        char chunk[4096] = {0};
        while (input.good())
        {
            input.read(chunk, sizeof(chunk));
            const std::streamsize count = input.gcount();
            if (count > 0)
            {
                buffer.append(chunk, static_cast<std::size_t>(count));
            }
        }
    });

    const int pollIntervalMs = 50;
    int waitedMs = 0;

    while ((exitCode = processHandle.tryWait()) == -1 && waitedMs < timeoutMs)
    {
        Poco::Thread::sleep(pollIntervalMs);
        waitedMs += pollIntervalMs;
    }

    if (exitCode == -1)
    {
        timedOut = true;
        Poco::Process::kill(processHandle);
        exitCode = processHandle.wait();
    }

    if (reader.joinable())
    {
        reader.join();
    }

#if defined(_WIN32)
    // Native Windows tools may still emit the active code page even when the
    // hosting shell is configured for UTF-8, so normalize before JSON encoding.
    buffer = normalizeCapturedWindowsOutput(buffer);
#endif

    return splitOutputLines(buffer);
}

std::string escapePowerShellCommand(const std::string& commandLine)
{
    std::string escaped = commandLine;
    Poco::replaceInPlace(escaped, std::string("'"), std::string("''"));
    return escaped;
}

std::string escapeSingleQuotedShellValue(const std::string& value)
{
    std::string escaped = value;
    Poco::replaceInPlace(escaped, std::string("'"), std::string("'\\''"));
    return escaped;
}

enum class ShellBackendKind
{
    posixShell,
    gitBash,
    wslShell,
    windowsPowerShell
};

struct ShellBackendInfo
{
    ShellBackendKind kind = ShellBackendKind::windowsPowerShell;
    std::string id;
    std::string displayName;
    std::string executable;
    bool linuxLike = false;
};

#if defined(_WIN32)

bool isValidUtf8(std::string_view value)
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(value.data());
    std::size_t index = 0;

    while (index < value.size())
    {
        const unsigned char lead = bytes[index];
        if (lead <= 0x7F)
        {
            ++index;
            continue;
        }

        std::size_t continuationCount = 0;
        unsigned char secondMin = 0x80;
        unsigned char secondMax = 0xBF;

        if (lead >= 0xC2 && lead <= 0xDF)
        {
            continuationCount = 1;
        }
        else if (lead == 0xE0)
        {
            continuationCount = 2;
            secondMin = 0xA0;
        }
        else if (lead >= 0xE1 && lead <= 0xEC)
        {
            continuationCount = 2;
        }
        else if (lead == 0xED)
        {
            continuationCount = 2;
            secondMax = 0x9F;
        }
        else if (lead >= 0xEE && lead <= 0xEF)
        {
            continuationCount = 2;
        }
        else if (lead == 0xF0)
        {
            continuationCount = 3;
            secondMin = 0x90;
        }
        else if (lead >= 0xF1 && lead <= 0xF3)
        {
            continuationCount = 3;
        }
        else if (lead == 0xF4)
        {
            continuationCount = 3;
            secondMax = 0x8F;
        }
        else
        {
            return false;
        }

        if (index + continuationCount >= value.size()) return false;

        const unsigned char second = bytes[index + 1];
        if (second < secondMin || second > secondMax) return false;

        for (std::size_t offset = 2; offset <= continuationCount; ++offset)
        {
            const unsigned char continuation = bytes[index + offset];
            if (continuation < 0x80 || continuation > 0xBF) return false;
        }

        index += continuationCount + 1;
    }

    return true;
}

std::string utf8FromWide(const std::wstring& value)
{
    if (value.empty()) return std::string();

    const int utf8Length =
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        utf8Length,
        nullptr,
        nullptr);
    return result;
}

std::string utf8FromCodePage(const std::string& value, UINT codePage)
{
    if (value.empty() || codePage == 0) return std::string();

    const int wideLength = MultiByteToWideChar(
        codePage,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (wideLength <= 0) return std::string();

    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    const int converted = MultiByteToWideChar(
        codePage,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        wide.data(),
        wideLength);
    if (converted != wideLength) return std::string();

    return utf8FromWide(wide);
}

std::string normalizeCapturedWindowsOutput(const std::string& value)
{
    if (value.empty() || isValidUtf8(value)) return value;

    std::vector<UINT> codePages;
    const auto tryAddCodePage = [&codePages](UINT codePage)
    {
        if (codePage == 0 || codePage == CP_UTF8) return;
        if (std::find(codePages.begin(), codePages.end(), codePage) != codePages.end()) return;
        codePages.push_back(codePage);
    };

    tryAddCodePage(GetConsoleOutputCP());
    tryAddCodePage(GetOEMCP());
    tryAddCodePage(GetACP());

    for (UINT codePage: codePages)
    {
        const std::string converted = utf8FromCodePage(value, codePage);
        if (!converted.empty()) return converted;
    }

    return value;
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

#endif

ShellBackendInfo detectShellBackend()
{
#if defined(_WIN32)
    const std::string gitBash = gitBashExecutable();
    if (!gitBash.empty())
    {
        return {ShellBackendKind::gitBash, "git-bash", "Git Bash", gitBash, true};
    }

    const std::string wsl = searchExecutable(L"wsl.exe");
    if (!wsl.empty())
    {
        return {ShellBackendKind::wslShell, "wsl", "WSL", wsl, true};
    }

    const std::string powershell = searchExecutable(L"powershell.exe");
    if (!powershell.empty())
    {
        return {ShellBackendKind::windowsPowerShell, "powershell", "Windows PowerShell", powershell, false};
    }

    return {ShellBackendKind::windowsPowerShell, "powershell", "Windows PowerShell", "powershell.exe", false};
#else
    return {ShellBackendKind::posixShell, "sh", "POSIX Shell", "/bin/sh", true};
#endif
}

const ShellBackendInfo& shellBackend()
{
    static const ShellBackendInfo backend = detectShellBackend();
    return backend;
}

bool isWindowsDrivePath(const std::string& value)
{
    return value.size() >= 2 &&
        std::isalpha(static_cast<unsigned char>(value[0])) &&
        value[1] == ':';
}

std::string normalizeSlashPath(std::string value)
{
    Poco::replaceInPlace(value, std::string("\\"), std::string("/"));
    return value;
}

std::string windowsPathToGitBashPath(const std::string& value)
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

std::string windowsPathToWslPath(const std::string& value)
{
    std::string normalized = normalizeSlashPath(value);
    if (!isWindowsDrivePath(normalized)) return normalized;

    std::string result("/mnt/");
    result += static_cast<char>(std::tolower(static_cast<unsigned char>(normalized[0])));
    if (normalized.size() > 2)
    {
        if (normalized[2] != '/') result += '/';
        result += normalized.substr(2);
    }
    return result;
}

std::string shellDisplayPathForBackend(const std::string& value)
{
    if (value.empty()) return value;

    switch (shellBackend().kind)
    {
    case ShellBackendKind::gitBash:
        return windowsPathToGitBashPath(value);
    case ShellBackendKind::wslShell:
        return windowsPathToWslPath(value);
    case ShellBackendKind::posixShell:
    case ShellBackendKind::windowsPowerShell:
    default:
        return value;
    }
}

std::string shellCommandWithWorkingDirectory(const std::string& workingDirectory, const std::string& commandLine)
{
    const std::string shellWorkingDirectory = shellDisplayPathForBackend(workingDirectory);
    if (shellWorkingDirectory.empty()) return commandLine;

    return "cd -- '" + escapeSingleQuotedShellValue(shellWorkingDirectory) + "' && " + commandLine;
}

void setShellContext(
    Poco::JSON::Object::Ptr payload,
    const std::string& workingDirectory,
    const std::string& homeDirectory)
{
    const ShellBackendInfo& backend = shellBackend();
    const std::string displayWorkingDirectory = shellDisplayPathForBackend(workingDirectory);
    const std::string displayHomeDirectory = shellDisplayPathForBackend(homeDirectory);

    payload->set("workingDirectory", workingDirectory);
    payload->set("homeDirectory", homeDirectory);
    payload->set("displayWorkingDirectory", displayWorkingDirectory);
    payload->set("displayHomeDirectory", displayHomeDirectory);
    payload->set("shellBackend", backend.id);
    payload->set("shellBackendDisplayName", backend.displayName);
    payload->set("shellLinuxLike", backend.linuxLike);

    Poco::JSON::Object::Ptr data = payload->getObject("data");
    if (data)
    {
        data->set("workingDirectory", workingDirectory);
        data->set("homeDirectory", homeDirectory);
        data->set("displayWorkingDirectory", displayWorkingDirectory);
        data->set("displayHomeDirectory", displayHomeDirectory);
        data->set("shellBackend", backend.id);
        data->set("shellBackendDisplayName", backend.displayName);
        data->set("shellLinuxLike", backend.linuxLike);
    }
}

std::string executablePath()
{
#if defined(_WIN32)
    std::vector<wchar_t> buffer(MAX_PATH, L'\0');
    for (;;)
    {
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return std::string();
        if (length < buffer.size() - 1)
        {
            int utf8Length = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), static_cast<int>(length), nullptr, 0, nullptr, nullptr);
            std::string result(static_cast<std::size_t>(utf8Length), '\0');
            WideCharToMultiByte(CP_UTF8, 0, buffer.data(), static_cast<int>(length), &result[0], utf8Length, nullptr, nullptr);
            return result;
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    std::vector<char> buffer(PATH_MAX, '\0');
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length > 0)
    {
        return std::string(buffer.data(), static_cast<std::size_t>(length));
    }
    return Poco::Path::current();
#endif
}

#if !defined(_WIN32)

struct LinuxThreadInfo
{
    std::uint64_t threadId = 0;
    char state = '?';
    std::string name;
    bool current = false;
};

struct LinuxModuleInfo
{
    std::string path;
    std::uintptr_t baseAddress = 0;
};

bool isNumericString(const std::string& value)
{
    if (value.empty()) return false;
    for (char ch: value)
    {
        if (ch < '0' || ch > '9') return false;
    }
    return true;
}

std::vector<LinuxThreadInfo> enumerateLinuxThreads()
{
    std::vector<LinuxThreadInfo> threads;
    DIR* taskDir = opendir("/proc/self/task");
    if (!taskDir) return threads;

    const std::uint64_t currentTid = static_cast<std::uint64_t>(Poco::Thread::currentTid());
    dirent* entry = nullptr;
    while ((entry = readdir(taskDir)) != nullptr)
    {
        std::string tidName(entry->d_name);
        if (!isNumericString(tidName)) continue;

        LinuxThreadInfo info;
        info.threadId = static_cast<std::uint64_t>(std::strtoull(tidName.c_str(), nullptr, 10));
        info.current = info.threadId == currentTid;

        std::ifstream statusFile("/proc/self/task/" + tidName + "/status");
        std::string line;
        while (std::getline(statusFile, line))
        {
            if (line.rfind("Name:", 0) == 0)
            {
                info.name = Poco::trim(line.substr(5));
            }
            else if (line.rfind("State:", 0) == 0)
            {
                const std::size_t tabPos = line.find('\t');
                const std::size_t valuePos = tabPos != std::string::npos ? tabPos + 1 : 6;
                if (valuePos < line.size()) info.state = line[valuePos];
            }
        }

        threads.push_back(info);
    }

    closedir(taskDir);
    std::sort(threads.begin(), threads.end(), [](const LinuxThreadInfo& left, const LinuxThreadInfo& right) {
        return left.threadId < right.threadId;
    });
    return threads;
}

std::vector<LinuxModuleInfo> enumerateLinuxModules()
{
    std::vector<LinuxModuleInfo> modules;
    std::ifstream mapsFile("/proc/self/maps");
    if (!mapsFile.good()) return modules;

    std::set<std::string> seen;
    std::string line;
    while (std::getline(mapsFile, line))
    {
        const std::size_t dashPos = line.find('-');
        const std::size_t spacePos = line.find(' ');
        if (dashPos == std::string::npos || spacePos == std::string::npos || dashPos >= spacePos) continue;

        const std::size_t pathPos = line.find('/');
        if (pathPos == std::string::npos) continue;
        const std::string path = line.substr(pathPos);
        if (!seen.insert(path).second) continue;

        const std::string baseHex = line.substr(0, dashPos);
        LinuxModuleInfo info;
        info.path = path;
        info.baseAddress = static_cast<std::uintptr_t>(std::strtoull(baseHex.c_str(), nullptr, 16));
        modules.push_back(info);
    }

    std::sort(modules.begin(), modules.end(), [](const LinuxModuleInfo& left, const LinuxModuleInfo& right) {
        return left.baseAddress < right.baseAddress;
    });
    return modules;
}

std::vector<std::string> captureLinuxStack(int limit)
{
    if (limit < 1) limit = 16;
    std::vector<void*> addresses(static_cast<std::size_t>(limit), nullptr);
    const int count = backtrace(addresses.data(), static_cast<int>(addresses.size()));
    std::vector<std::string> frames;
    if (count <= 0) return frames;

    char** symbols = backtrace_symbols(addresses.data(), count);
    if (!symbols) return frames;
    for (int i = 0; i < count; ++i)
    {
        frames.emplace_back(symbols[i] ? symbols[i] : "<unresolved>");
    }
    std::free(symbols);
    return frames;
}

#endif

#if defined(_WIN32)

std::string utf8FromWide(const wchar_t* value)
{
    if (!value || !*value) return std::string();

    const int wideLength = static_cast<int>(wcslen(value));
    const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, value, wideLength, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, wideLength, &result[0], utf8Length, nullptr, nullptr);
    return result;
}

struct ThreadInfo
{
    DWORD threadId = 0;
    LONG basePriority = 0;
    LONG deltaPriority = 0;
    bool current = false;
};

struct ModuleInfo
{
    std::string name;
    std::string path;
    std::uintptr_t baseAddress = 0;
    DWORD size = 0;
};

struct StackFrameInfo
{
    std::uintptr_t address = 0;
    std::string symbol;
    std::string module;
    std::string sourceFile;
    DWORD sourceLine = 0;
};

std::vector<ThreadInfo> enumerateThreads()
{
    std::vector<ThreadInfo> threads;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return threads;

    const DWORD currentPid = GetCurrentProcessId();
    const DWORD currentTid = GetCurrentThreadId();
    THREADENTRY32 entry;
    entry.dwSize = sizeof(entry);

    if (Thread32First(snapshot, &entry))
    {
        do
        {
            if (entry.th32OwnerProcessID != currentPid) continue;

            ThreadInfo info;
            info.threadId = entry.th32ThreadID;
            info.basePriority = entry.tpBasePri;
            info.deltaPriority = entry.tpDeltaPri;
            info.current = entry.th32ThreadID == currentTid;
            threads.push_back(info);
        }
        while (Thread32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    std::sort(threads.begin(), threads.end(), [](const ThreadInfo& left, const ThreadInfo& right) {
        return left.threadId < right.threadId;
    });
    return threads;
}

std::vector<ModuleInfo> enumerateModules()
{
    std::vector<ModuleInfo> modules;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return modules;

    MODULEENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry))
    {
        do
        {
            ModuleInfo info;
            info.name = utf8FromWide(entry.szModule);
            info.path = utf8FromWide(entry.szExePath);
            info.baseAddress = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
            info.size = entry.modBaseSize;
            modules.push_back(info);
        }
        while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    std::sort(modules.begin(), modules.end(), [](const ModuleInfo& left, const ModuleInfo& right) {
        return left.baseAddress < right.baseAddress;
    });
    return modules;
}

class SymbolResolver
{
public:
    SymbolResolver():
        _process(GetCurrentProcess()),
        _initialized(false)
    {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        _initialized = SymInitialize(_process, nullptr, TRUE) == TRUE;
    }

    ~SymbolResolver()
    {
        if (_initialized)
        {
            SymCleanup(_process);
        }
    }

    bool initialized() const
    {
        return _initialized;
    }

    StackFrameInfo describe(std::uintptr_t address) const
    {
        StackFrameInfo frame;
        frame.address = address;

        std::vector<unsigned char> symbolBuffer(sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer.data());
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;

        if (SymFromAddr(_process, static_cast<DWORD64>(address), &displacement, symbol))
        {
            frame.symbol.assign(symbol->Name, symbol->NameLen);
            if (displacement != 0)
            {
                frame.symbol += "+" + std::to_string(static_cast<unsigned long long>(displacement));
            }
        }
        else
        {
            frame.symbol = "<unresolved>";
        }

        IMAGEHLP_LINE64 lineInfo;
        std::memset(&lineInfo, 0, sizeof(lineInfo));
        lineInfo.SizeOfStruct = sizeof(lineInfo);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(_process, static_cast<DWORD64>(address), &lineDisplacement, &lineInfo))
        {
            frame.sourceFile = lineInfo.FileName ? lineInfo.FileName : "";
            frame.sourceLine = lineInfo.LineNumber;
        }

        HMODULE moduleHandle = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address), &moduleHandle))
        {
            wchar_t modulePath[MAX_PATH] = {0};
            const DWORD length = GetModuleFileNameW(moduleHandle, modulePath, MAX_PATH);
            if (length > 0)
            {
                frame.module = utf8FromWide(modulePath);
            }
        }

        return frame;
    }

private:
    HANDLE _process;
    bool _initialized;
};

std::vector<StackFrameInfo> captureCurrentStack(int limit)
{
    std::vector<StackFrameInfo> frames;
    std::vector<void*> addresses(static_cast<std::size_t>(limit), nullptr);
    const USHORT count = CaptureStackBackTrace(0, static_cast<DWORD>(addresses.size()), addresses.data(), nullptr);
    SymbolResolver resolver;

    for (USHORT i = 0; i < count; ++i)
    {
        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(addresses[i]);
        if (resolver.initialized())
        {
            frames.push_back(resolver.describe(address));
        }
        else
        {
            StackFrameInfo frame;
            frame.address = address;
            frame.symbol = "<symbols unavailable>";
            frames.push_back(frame);
        }
    }

    return frames;
}

#endif

class ProcessConsoleServiceImpl: public ProcessConsoleService
{
public:
    explicit ProcessConsoleServiceImpl(Poco::OSP::BundleContext::Ptr pContext):
        _pContext(pContext)
    {
    }

    Poco::JSON::Object::Ptr execute(
        const std::string& commandLine,
        int limit = -1,
        const std::string& workingDirectory = std::string()) override
    {
        const std::string trimmed = Poco::trim(commandLine);
        const std::vector<std::string> tokens = tokenize(trimmed);
        const std::string command = tokens.empty() ? "help" : Poco::toLower(tokens.front());
        const std::string currentWorkingDirectory = effectiveWorkingDirectory(workingDirectory);
        const std::string homeDirectory = shellHomeDirectory();
        Poco::OpenTelemetry::TelemetryClient telemetry(_pContext);
        auto activity = telemetry.beginActivity(
            "processconsole.execute",
            "service.command",
            trimmed.empty() ? "help" : trimmed,
            {
                {"bundle.symbolic_name", _pContext->thisBundle()->symbolicName()},
                {"processconsole.command", command},
                {"processconsole.working_directory", currentWorkingDirectory}
            });
        activity.tag("processconsole.limit", std::to_string(limit));
        activity.step("command.dispatch", command);

        if (command == "help" || command == "?") return help(trimmed.empty() ? "help" : trimmed, currentWorkingDirectory, homeDirectory);
        if (command == "pwd") return pwd(trimmed.empty() ? "pwd" : trimmed, currentWorkingDirectory, homeDirectory);
        if (command == "cd") return changeDirectory(trimmed.empty() ? "cd" : trimmed, currentWorkingDirectory, homeDirectory);
        if (command == "summary" || command == "status") return summary(trimmed, currentWorkingDirectory, homeDirectory);
        if (command == "threads") return threads(trimmed, currentWorkingDirectory, homeDirectory, chooseLimit(tokens, limit, 64));
        if (command == "modules" || command == "lsmod") return modules(trimmed, currentWorkingDirectory, homeDirectory, chooseLimit(tokens, limit, 48));
        if (command == "stack" || command == "bt" || command == "where") return stack(trimmed, currentWorkingDirectory, homeDirectory, chooseLimit(tokens, limit, 24));
        if (command == "functions" || command == "funcs") return functions(trimmed, currentWorkingDirectory, homeDirectory, chooseLimit(tokens, limit, 24));

        return shell(trimmed, currentWorkingDirectory, homeDirectory, clampLimit(limit, 200));
    }

    const std::type_info& type() const override
    {
        return typeid(ProcessConsoleService);
    }

    bool isA(const std::type_info& otherType) const override
    {
        const std::string name(typeid(ProcessConsoleService).name());
        return name == otherType.name() || Poco::OSP::Service::isA(otherType);
    }

private:
    Poco::JSON::Object::Ptr help(
        const std::string& commandLine,
        const std::string& workingDirectory,
        const std::string& homeDirectory)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "help");
        setShellContext(payload, workingDirectory, homeDirectory);
        Poco::JSON::Array::Ptr output = payload->getArray("output");
        Poco::JSON::Object::Ptr data = payload->getObject("data");
        Poco::JSON::Array::Ptr commands = new Poco::JSON::Array;

        appendLine(output, "MyIoT Process Console");
        appendLine(output, "Shell backend: " + shellBackend().displayName);
        appendLine(output, "Shell commands:");
        appendLine(output, "pwd                  显示当前目录");
        appendLine(output, "cd <dir>             切换当前目录");
        appendLine(output, "ls / dir / cat       交给系统 shell 执行");
        appendLine(output, "echo / type / whoami 交给系统 shell 执行");
        appendLine(output, "Diagnostics:");
        appendLine(output, "summary              当前进程概要");
        appendLine(output, "threads [limit]      当前进程线程列表");
        appendLine(output, "modules [limit]      当前进程模块列表");
        appendLine(output, "stack [limit]        当前请求线程调用栈");
        appendLine(output, "functions [limit]    当前调用栈函数名");

        commands->add("help");
        commands->add("pwd");
        commands->add("cd");
        commands->add("ls");
        commands->add("cat");
        commands->add("summary");
        commands->add("threads");
        commands->add("modules");
        commands->add("stack");
        commands->add("functions");
        data->set("commands", commands);
        data->set("pid", static_cast<Poco::UInt32>(Poco::Process::id()));
        payload->set("message", "终端帮助已加载。");
        return payload;
    }

    Poco::JSON::Object::Ptr pwd(
        const std::string& commandLine,
        const std::string& workingDirectory,
        const std::string& homeDirectory)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "pwd");
        setShellContext(payload, workingDirectory, homeDirectory);
        const std::string displayWorkingDirectory = shellDisplayPathForBackend(workingDirectory);
        appendLine(payload->getArray("output"), displayWorkingDirectory);
        payload->set("message", displayWorkingDirectory);
        return payload;
    }

    Poco::JSON::Object::Ptr changeDirectory(
        const std::string& commandLine,
        const std::string& currentWorkingDirectory,
        const std::string& homeDirectory)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "cd");
        Poco::JSON::Array::Ptr output = payload->getArray("output");
        Poco::JSON::Object::Ptr data = payload->getObject("data");

        std::string detail;
        const std::string resolvedDirectory =
            resolveDirectoryTarget(currentWorkingDirectory, commandRemainder(commandLine), homeDirectory, detail);
        setShellContext(payload, detail.empty() ? resolvedDirectory : currentWorkingDirectory, homeDirectory);
        data->set("changed", detail.empty());

        if (!detail.empty())
        {
            payload->set("ok", false);
            payload->set("message", detail);
            appendLine(output, "ERROR: " + detail);
            return payload;
        }

        const std::string displayWorkingDirectory = shellDisplayPathForBackend(resolvedDirectory);
        appendLine(output, displayWorkingDirectory);
        payload->set("message", "当前目录已切换到: " + resolvedDirectory);
        return payload;
    }

    Poco::JSON::Object::Ptr summary(
        const std::string& commandLine,
        const std::string& workingDirectory,
        const std::string& homeDirectory)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "summary");
        setShellContext(payload, workingDirectory, homeDirectory);
        Poco::JSON::Array::Ptr output = payload->getArray("output");
        Poco::JSON::Object::Ptr data = payload->getObject("data");

        data->set("pid", static_cast<Poco::UInt32>(Poco::Process::id()));
        data->set("threadId", static_cast<Poco::UInt64>(Poco::Thread::currentTid()));
        data->set("executable", executablePath());
        data->set("processWorkingDirectory", Poco::Path::current());
        data->set("shellWorkingDirectory", workingDirectory);
        data->set("shellDisplayWorkingDirectory", shellDisplayPathForBackend(workingDirectory));
        data->set("osName", Poco::Environment::osDisplayName());
        data->set("osVersion", Poco::Environment::osVersion());
        data->set("osArchitecture", Poco::Environment::osArchitecture());
        data->set("cpuCount", static_cast<int>(Poco::Environment::processorCount()));

        appendLine(output, Poco::format("PID: %u", static_cast<unsigned>(Poco::Process::id())));
        appendLine(output, Poco::format("Current Thread ID: %Lu", static_cast<Poco::UInt64>(Poco::Thread::currentTid())));
        appendLine(output, "Executable: " + executablePath());
        appendLine(output, "Shell Backend: " + shellBackend().displayName);
        appendLine(output, "Shell Working Directory: " + shellDisplayPathForBackend(workingDirectory));
        appendLine(output, "Process Working Directory: " + Poco::Path::current());
        appendLine(output, "OS: " + Poco::Environment::osDisplayName() + " " + Poco::Environment::osVersion() + " (" + Poco::Environment::osArchitecture() + ")");

#if defined(_WIN32)
        const auto threadList = enumerateThreads();
        const auto moduleList = enumerateModules();
        data->set("threadCount", static_cast<int>(threadList.size()));
        data->set("moduleCount", static_cast<int>(moduleList.size()));
        appendLine(output, "Threads: " + std::to_string(threadList.size()));
        appendLine(output, "Modules: " + std::to_string(moduleList.size()));
#else
        const auto threadList = enumerateLinuxThreads();
        const auto moduleList = enumerateLinuxModules();
        data->set("threadCount", static_cast<int>(threadList.size()));
        data->set("moduleCount", static_cast<int>(moduleList.size()));
        appendLine(output, "Threads: " + std::to_string(threadList.size()));
        appendLine(output, "Modules: " + std::to_string(moduleList.size()));
#endif

        payload->set("message", "进程概要已刷新。");
        return payload;
    }

    Poco::JSON::Object::Ptr threads(
        const std::string& commandLine,
        const std::string& workingDirectory,
        const std::string& homeDirectory,
        int limit)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "threads");
        setShellContext(payload, workingDirectory, homeDirectory);
        Poco::JSON::Array::Ptr output = payload->getArray("output");
        Poco::JSON::Object::Ptr data = payload->getObject("data");

#if defined(_WIN32)
        Poco::JSON::Array::Ptr items = new Poco::JSON::Array;
        const auto threadList = enumerateThreads();
        int returned = 0;

        for (const auto& thread: threadList)
        {
            if (returned >= limit) break;

            Poco::JSON::Object::Ptr item = new Poco::JSON::Object;
            item->set("threadId", static_cast<Poco::UInt32>(thread.threadId));
            item->set("basePriority", thread.basePriority);
            item->set("deltaPriority", thread.deltaPriority);
            item->set("current", thread.current);
            items->add(item);

            std::ostringstream line;
            line << (thread.current ? "* " : "  ")
                 << "TID=" << thread.threadId
                 << " basePri=" << thread.basePriority
                 << " deltaPri=" << thread.deltaPriority;
            appendLine(output, line.str());
            ++returned;
        }

        data->set("threads", items);
        data->set("threadCount", static_cast<int>(threadList.size()));
        data->set("returnedCount", returned);
        appendLine(output, "共发现 " + std::to_string(threadList.size()) + " 个线程，本次返回 " + std::to_string(returned) + " 个。");
#else
        Poco::JSON::Array::Ptr items = new Poco::JSON::Array;
        const auto threadList = enumerateLinuxThreads();
        int returned = 0;

        for (const auto& thread: threadList)
        {
            if (returned >= limit) break;

            Poco::JSON::Object::Ptr item = new Poco::JSON::Object;
            item->set("threadId", static_cast<Poco::UInt64>(thread.threadId));
            item->set("state", std::string(1, thread.state));
            item->set("name", thread.name);
            item->set("current", thread.current);
            items->add(item);

            std::ostringstream line;
            line << (thread.current ? "* " : "  ")
                 << "TID=" << thread.threadId
                 << " state=" << thread.state
                 << " name=" << (thread.name.empty() ? "<unnamed>" : thread.name);
            appendLine(output, line.str());
            ++returned;
        }

        data->set("threads", items);
        data->set("threadCount", static_cast<int>(threadList.size()));
        data->set("returnedCount", returned);
        appendLine(output, "共发现 " + std::to_string(threadList.size()) + " 个线程，本次返回 " + std::to_string(returned) + " 个。");
#endif

        return payload;
    }

    Poco::JSON::Object::Ptr modules(
        const std::string& commandLine,
        const std::string& workingDirectory,
        const std::string& homeDirectory,
        int limit)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "modules");
        setShellContext(payload, workingDirectory, homeDirectory);
        Poco::JSON::Array::Ptr output = payload->getArray("output");
        Poco::JSON::Object::Ptr data = payload->getObject("data");

#if defined(_WIN32)
        Poco::JSON::Array::Ptr items = new Poco::JSON::Array;
        const auto moduleList = enumerateModules();
        int returned = 0;

        for (const auto& module: moduleList)
        {
            if (returned >= limit) break;

            Poco::JSON::Object::Ptr item = new Poco::JSON::Object;
            item->set("name", module.name);
            item->set("path", module.path);
            item->set("baseAddress", formatPointer(module.baseAddress));
            item->set("size", static_cast<Poco::UInt32>(module.size));
            items->add(item);

            appendLine(output, module.name + " @ " + formatPointer(module.baseAddress) + " size=" + std::to_string(module.size));
            ++returned;
        }

        data->set("modules", items);
        data->set("moduleCount", static_cast<int>(moduleList.size()));
        data->set("returnedCount", returned);
        appendLine(output, "共发现 " + std::to_string(moduleList.size()) + " 个模块，本次返回 " + std::to_string(returned) + " 个。");
#else
        Poco::JSON::Array::Ptr items = new Poco::JSON::Array;
        const auto moduleList = enumerateLinuxModules();
        int returned = 0;

        for (const auto& module: moduleList)
        {
            if (returned >= limit) break;

            Poco::JSON::Object::Ptr item = new Poco::JSON::Object;
            item->set("path", module.path);
            item->set("baseAddress", formatPointer(module.baseAddress));
            items->add(item);

            appendLine(output, module.path + " @ " + formatPointer(module.baseAddress));
            ++returned;
        }

        data->set("modules", items);
        data->set("moduleCount", static_cast<int>(moduleList.size()));
        data->set("returnedCount", returned);
        appendLine(output, "共发现 " + std::to_string(moduleList.size()) + " 个模块，本次返回 " + std::to_string(returned) + " 个。");
#endif

        payload->set("message", "模块列表已刷新。");
        return payload;
    }

    Poco::JSON::Object::Ptr stack(
        const std::string& commandLine,
        const std::string& workingDirectory,
        const std::string& homeDirectory,
        int limit)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "stack");
        setShellContext(payload, workingDirectory, homeDirectory);
        Poco::JSON::Array::Ptr output = payload->getArray("output");
        Poco::JSON::Object::Ptr data = payload->getObject("data");

#if defined(_WIN32)
        Poco::JSON::Array::Ptr items = new Poco::JSON::Array;
        const auto frames = captureCurrentStack(limit);

        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            const auto& frame = frames[i];
            Poco::JSON::Object::Ptr item = new Poco::JSON::Object;
            item->set("index", static_cast<int>(i));
            item->set("address", formatPointer(frame.address));
            item->set("symbol", frame.symbol);
            item->set("module", frame.module);
            item->set("sourceFile", frame.sourceFile);
            item->set("sourceLine", static_cast<Poco::UInt32>(frame.sourceLine));
            items->add(item);

            std::string line = "#" + std::to_string(i) + " " + frame.symbol + " [" + formatPointer(frame.address) + "]";
            if (!frame.sourceFile.empty())
            {
                line += " " + frame.sourceFile + ":" + std::to_string(frame.sourceLine);
            }
            appendLine(output, line);
        }

        data->set("frames", items);
        data->set("frameCount", static_cast<int>(frames.size()));
        appendLine(output, "说明: 这里抓取的是当前处理该请求线程的调用栈。");
#else
        Poco::JSON::Array::Ptr items = new Poco::JSON::Array;
        const auto frames = captureLinuxStack(limit);

        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            Poco::JSON::Object::Ptr item = new Poco::JSON::Object;
            item->set("index", static_cast<int>(i));
            item->set("symbol", frames[i]);
            items->add(item);
            appendLine(output, "#" + std::to_string(i) + " " + frames[i]);
        }

        data->set("frames", items);
        data->set("frameCount", static_cast<int>(frames.size()));
        appendLine(output, "说明: 这里抓取的是当前处理该请求线程的调用栈。");
#endif

        payload->set("message", "调用栈已刷新。");
        return payload;
    }

    Poco::JSON::Object::Ptr functions(
        const std::string& commandLine,
        const std::string& workingDirectory,
        const std::string& homeDirectory,
        int limit)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "functions");
        setShellContext(payload, workingDirectory, homeDirectory);
        Poco::JSON::Array::Ptr output = payload->getArray("output");
        Poco::JSON::Object::Ptr data = payload->getObject("data");

#if defined(_WIN32)
        Poco::JSON::Array::Ptr items = new Poco::JSON::Array;
        std::set<std::string> seen;
        const auto frames = captureCurrentStack(limit);

        for (const auto& frame: frames)
        {
            if (frame.symbol.empty()) continue;
            if (!seen.insert(frame.symbol).second) continue;

            items->add(frame.symbol);
            appendLine(output, frame.symbol);
        }

        data->set("functions", items);
        data->set("functionCount", static_cast<int>(items->size()));
        appendLine(output, "说明: 函数名来自当前调用栈的符号解析结果。");
#else
        Poco::JSON::Array::Ptr items = new Poco::JSON::Array;
        std::set<std::string> seen;
        const auto frames = captureLinuxStack(limit);

        for (const auto& frame: frames)
        {
            if (frame.empty()) continue;
            if (!seen.insert(frame).second) continue;

            items->add(frame);
            appendLine(output, frame);
        }

        data->set("functions", items);
        data->set("functionCount", static_cast<int>(items->size()));
        appendLine(output, "说明: 函数名来自当前调用栈符号文本。");
#endif

        payload->set("message", "函数列表已刷新。");
        return payload;
    }

    Poco::JSON::Object::Ptr shell(
        const std::string& commandLine,
        const std::string& workingDirectory,
        const std::string& homeDirectory,
        int limit)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "shell");
        setShellContext(payload, workingDirectory, homeDirectory);
        Poco::JSON::Array::Ptr output = payload->getArray("output");
        Poco::JSON::Object::Ptr data = payload->getObject("data");

        if (commandLine.empty())
        {
            payload->set("message", "请输入命令。");
            return payload;
        }

        const int timeoutMs = 10000;
        const int outputLimit = clampLimit(limit, 200);
        int exitCode = -1;
        bool timedOut = false;
        std::vector<std::string> lines;
        const ShellBackendInfo& backend = shellBackend();

        try
        {
            Poco::Process::Args args;
            switch (backend.kind)
            {
            case ShellBackendKind::gitBash:
                args.push_back("--noprofile");
                args.push_back("--norc");
                args.push_back("-lc");
                args.push_back(shellCommandWithWorkingDirectory(workingDirectory, commandLine));
                lines = captureCommandOutput(backend.executable, args, workingDirectory, timeoutMs, exitCode, timedOut);
                break;
            case ShellBackendKind::wslShell:
                args.push_back("sh");
                args.push_back("-lc");
                args.push_back(shellCommandWithWorkingDirectory(workingDirectory, commandLine));
                lines = captureCommandOutput(backend.executable, args, workingDirectory, timeoutMs, exitCode, timedOut);
                break;
            case ShellBackendKind::posixShell:
                args.push_back("-lc");
                args.push_back(shellCommandWithWorkingDirectory(workingDirectory, commandLine));
                lines = captureCommandOutput(backend.executable, args, workingDirectory, timeoutMs, exitCode, timedOut);
                break;
            case ShellBackendKind::windowsPowerShell:
            default:
                args.push_back("-NoLogo");
                args.push_back("-NoProfile");
                args.push_back("-NonInteractive");
                args.push_back("-ExecutionPolicy");
                args.push_back("Bypass");
                args.push_back("-Command");
                args.push_back(
                    "$ProgressPreference='SilentlyContinue'; "
                    "$ErrorActionPreference='Continue'; "
                    "[Console]::InputEncoding=[System.Text.UTF8Encoding]::new($false); "
                    "[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false); "
                    "$OutputEncoding=[System.Text.UTF8Encoding]::new($false); "
                    "Invoke-Expression '" + escapePowerShellCommand(commandLine) + "'");
                lines = captureCommandOutput(backend.executable, args, workingDirectory, timeoutMs, exitCode, timedOut);
                break;
            }
        }
        catch (const Poco::Exception& exc)
        {
            payload->set("ok", false);
            payload->set("message", "命令执行失败: " + exc.displayText());
            appendLine(output, "ERROR: 命令执行失败: " + exc.displayText());
            return payload;
        }
        catch (const std::exception& exc)
        {
            payload->set("ok", false);
            payload->set("message", "命令执行失败: " + std::string(exc.what()));
            appendLine(output, "ERROR: 命令执行失败: " + std::string(exc.what()));
            return payload;
        }

        const int originalLineCount = static_cast<int>(lines.size());
        if (static_cast<int>(lines.size()) > outputLimit)
        {
            lines.resize(static_cast<std::size_t>(outputLimit));
            lines.push_back("... 输出已截断，请缩小结果范围后重试。");
        }

        for (const auto& line: lines)
        {
            appendLine(output, line);
        }

        data->set("exitCode", exitCode);
        data->set("timedOut", timedOut);
        data->set("lineCount", originalLineCount);
        data->set("returnedCount", static_cast<int>(lines.size()));

        if (timedOut)
        {
            payload->set("ok", false);
            payload->set("message", "命令执行超时，已终止。");
            appendLine(output, "ERROR: 命令执行超时，已终止。");
            return payload;
        }

        if (exitCode != 0)
        {
            payload->set("ok", false);
            payload->set("message", "命令退出码: " + std::to_string(exitCode));
            if (lines.empty())
            {
                appendLine(output, "ERROR: 命令退出码: " + std::to_string(exitCode));
            }
            return payload;
        }

        payload->set("message", "命令执行完成。");
        return payload;
    }

    Poco::OSP::BundleContext::Ptr _pContext;
};

} // namespace

ProcessConsoleService::Ptr createProcessConsoleService(Poco::OSP::BundleContext::Ptr pContext)
{
    return new ProcessConsoleServiceImpl(pContext);
}

} } } // namespace MyIoT::Services::ProcessConsole
