#include "ProcessConsoleService.h"
#include "Poco/Environment.h"
#include "Poco/Format.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/NumberParser.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Path.h"
#include "Poco/Process.h"
#include "Poco/String.h"
#include "Poco/Thread.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <dbghelp.h>
#include <tlhelp32.h>
#include <windows.h>
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
    return Poco::Path::current();
#endif
}

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

    Poco::JSON::Object::Ptr execute(const std::string& commandLine, int limit = -1) override
    {
        const std::string trimmed = Poco::trim(commandLine);
        const std::vector<std::string> tokens = tokenize(trimmed);
        const std::string command = tokens.empty() ? "help" : Poco::toLower(tokens.front());

        if (command == "help" || command == "?") return help(trimmed.empty() ? "help" : trimmed);
        if (command == "summary" || command == "status") return summary(trimmed);
        if (command == "threads") return threads(trimmed, chooseLimit(tokens, limit, 64));
        if (command == "modules" || command == "lsmod") return modules(trimmed, chooseLimit(tokens, limit, 48));
        if (command == "stack" || command == "bt" || command == "where") return stack(trimmed, chooseLimit(tokens, limit, 24));
        if (command == "functions" || command == "funcs") return functions(trimmed, chooseLimit(tokens, limit, 24));

        return createErrorResponse(trimmed, "不支持的命令。可用命令: help, summary, threads, modules, stack, functions");
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
    Poco::JSON::Object::Ptr help(const std::string& commandLine)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "help");
        Poco::JSON::Array::Ptr output = payload->getArray("output");
        Poco::JSON::Object::Ptr data = payload->getObject("data");
        Poco::JSON::Array::Ptr commands = new Poco::JSON::Array;

        appendLine(output, "MyIoT Process Console");
        appendLine(output, "help                 显示帮助");
        appendLine(output, "summary              当前进程概要");
        appendLine(output, "threads [limit]      当前进程线程列表");
        appendLine(output, "modules [limit]      当前进程模块列表");
        appendLine(output, "stack [limit]        当前请求线程调用栈");
        appendLine(output, "functions [limit]    当前调用栈函数名");

        commands->add("help");
        commands->add("summary");
        commands->add("threads");
        commands->add("modules");
        commands->add("stack");
        commands->add("functions");
        data->set("commands", commands);
        data->set("pid", static_cast<Poco::UInt32>(Poco::Process::id()));
        return payload;
    }

    Poco::JSON::Object::Ptr summary(const std::string& commandLine)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "summary");
        Poco::JSON::Array::Ptr output = payload->getArray("output");
        Poco::JSON::Object::Ptr data = payload->getObject("data");

        data->set("pid", static_cast<Poco::UInt32>(Poco::Process::id()));
        data->set("threadId", static_cast<Poco::UInt64>(Poco::Thread::currentTid()));
        data->set("executable", executablePath());
        data->set("workingDirectory", Poco::Path::current());
        data->set("osName", Poco::Environment::osDisplayName());
        data->set("osVersion", Poco::Environment::osVersion());
        data->set("osArchitecture", Poco::Environment::osArchitecture());
        data->set("cpuCount", static_cast<int>(Poco::Environment::processorCount()));

        appendLine(output, Poco::format("PID: %u", static_cast<unsigned>(Poco::Process::id())));
        appendLine(output, Poco::format("Current Thread ID: %Lu", static_cast<Poco::UInt64>(Poco::Thread::currentTid())));
        appendLine(output, "Executable: " + executablePath());
        appendLine(output, "Working Directory: " + Poco::Path::current());
        appendLine(output, "OS: " + Poco::Environment::osDisplayName() + " " + Poco::Environment::osVersion() + " (" + Poco::Environment::osArchitecture() + ")");

#if defined(_WIN32)
        const auto threadList = enumerateThreads();
        const auto moduleList = enumerateModules();
        data->set("threadCount", static_cast<int>(threadList.size()));
        data->set("moduleCount", static_cast<int>(moduleList.size()));
        appendLine(output, "Threads: " + std::to_string(threadList.size()));
        appendLine(output, "Modules: " + std::to_string(moduleList.size()));
#else
        appendLine(output, "Threads/Modules: 当前平台未启用 Windows 诊断扩展。");
#endif

        return payload;
    }

    Poco::JSON::Object::Ptr threads(const std::string& commandLine, int limit)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "threads");
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
        payload->set("ok", false);
        payload->set("message", "当前平台未实现线程枚举。");
        appendLine(output, "当前平台未实现线程枚举。");
#endif

        return payload;
    }

    Poco::JSON::Object::Ptr modules(const std::string& commandLine, int limit)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "modules");
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
        payload->set("ok", false);
        payload->set("message", "当前平台未实现模块枚举。");
        appendLine(output, "当前平台未实现模块枚举。");
#endif

        return payload;
    }

    Poco::JSON::Object::Ptr stack(const std::string& commandLine, int limit)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "stack");
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
        payload->set("ok", false);
        payload->set("message", "当前平台未实现堆栈抓取。");
        appendLine(output, "当前平台未实现堆栈抓取。");
#endif

        return payload;
    }

    Poco::JSON::Object::Ptr functions(const std::string& commandLine, int limit)
    {
        Poco::JSON::Object::Ptr payload = createResponse(commandLine, "functions");
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
        payload->set("ok", false);
        payload->set("message", "当前平台未实现函数符号解析。");
        appendLine(output, "当前平台未实现函数符号解析。");
#endif

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
