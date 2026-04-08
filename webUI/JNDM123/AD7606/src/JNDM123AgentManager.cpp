#include "JNDM123AgentManager.h"

#include "JNDM123Support.h"

#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/JSON/Object.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include "Poco/NumberParser.h"
#include "Poco/Path.h"
#include "Poco/Pipe.h"
#include "Poco/PipeStream.h"
#include "Poco/Process.h"
#include "Poco/StreamCopier.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Thread.h"

#include <string>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

namespace {

using Poco::JSON::Object;

constexpr int kDdsDomain = 37;
constexpr const char* kAgentExecutableName = "myiot-jndm123-acquisition-agent";

Poco::Logger& agentLogger()
{
    static Poco::Logger& instance = Poco::Logger::get("MyIoT.WebUI.JNDM123.AgentManager");
    return instance;
}

Poco::FastMutex& agentManagerMutex()
{
    static Poco::FastMutex mutex;
    return mutex;
}

unsigned& restartCountStorage()
{
    static unsigned count = 0;
    return count;
}

std::string& managerErrorStorage()
{
    static std::string error;
    return error;
}

std::string currentExecutablePath()
{
#if defined(__linux__)
    char pathBuffer[4096] = {0};
    const ssize_t length = ::readlink("/proc/self/exe", pathBuffer, sizeof(pathBuffer) - 1);
    if (length > 0)
    {
        return std::string(pathBuffer, static_cast<std::size_t>(length));
    }
#endif
    return "/opt/myiot/bin/macchina";
}

std::string agentExecutablePath()
{
    Poco::Path path(currentExecutablePath());
    path.setFileName(kAgentExecutableName);
    return path.toString();
}

std::string makeLaunchCommand(const std::string& executablePath)
{
    Poco::Path executable(executablePath);
    const std::string libPath = executable.parent().append("../lib").toString();
    return "LD_LIBRARY_PATH=\"" + libPath + "${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}\" "
        "MYIOT_JNDM123_AGENT_DDS_DOMAIN=\"" + std::to_string(kDdsDomain) + "\" "
        "exec \"" + executablePath + "\"";
}

std::string runShellCapture(const std::string& command, int* exitStatus = nullptr)
{
    Poco::Pipe outPipe;
    Poco::Process::Args args;
    args.push_back("-c");
    args.push_back(command);
    Poco::ProcessHandle handle(Poco::Process::launch("/bin/sh", args, nullptr, &outPipe, &outPipe));
    Poco::PipeInputStream stream(outPipe);
    std::string output;
    Poco::StreamCopier::copyToString(stream, output);
    const int rc = handle.wait();
    if (exitStatus) *exitStatus = rc;
    return output;
}

int currentAgentPidLocked()
{
    int exitStatus = 0;
    const std::string output = runShellCapture("pidof myiot-jndm123-acquisition-agent 2>/dev/null || true", &exitStatus);
    Poco::StringTokenizer tokens(output, " \t\r\n", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
    if (tokens.count() > 0)
    {
        try
        {
            return Poco::NumberParser::parse(tokens[0]);
        }
        catch (...)
        {
        }
    }
    return -1;
}

void ensureProcessRunningLocked()
{
    if (currentAgentPidLocked() > 0) return;

    const std::string executablePath = agentExecutablePath();
    if (!Poco::File(executablePath).exists())
    {
        throw Poco::FileNotFoundException("Acquisition agent executable not found", executablePath);
    }

    Poco::Process::Args args;
    args.push_back("-lc");
    args.push_back(makeLaunchCommand(executablePath));
    Poco::ProcessHandle handle(Poco::Process::launch("/bin/sh", args, Poco::Path(executablePath).parent().toString()));
    Poco::Thread::sleep(300);

    const int pid = currentAgentPidLocked();
    if (pid <= 0)
    {
        throw Poco::RuntimeException("Acquisition agent did not appear after launch.");
    }

    ++restartCountStorage();
    managerErrorStorage().clear();
    agentLogger().information("Started acquisition process pid=" + std::to_string(pid) +
        " shellPid=" + std::to_string(handle.id()));
}

void terminateProcessLocked()
{
    (void) runShellCapture("for pid in $(pidof myiot-jndm123-acquisition-agent 2>/dev/null); do kill \"$pid\" 2>/dev/null || true; done");
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        if (currentAgentPidLocked() <= 0) break;
        Poco::Thread::sleep(100);
    }
}

Object::Ptr createStatusPayload(
    bool running,
    int pid,
    unsigned restartCount,
    const std::string& managerError,
    const std::string& message)
{
    Object::Ptr payload = new Object;
    payload->set("authenticated", true);
    payload->set("ok", true);
    payload->set("updatedAt", isoTimestamp());
    payload->set("message", message);
    payload->set("agentProcessRunning", running);
    payload->set("agentProcessId", pid);
    payload->set("agentRestartCount", static_cast<Poco::UInt64>(restartCount));
    payload->set("agentManagerError", managerError);
    return payload;
}

}

Object::Ptr acquisitionProcessStatusOrThrow()
{
    Poco::FastMutex::ScopedLock lock(agentManagerMutex());
    ensureProcessRunningLocked();
    const int pid = currentAgentPidLocked();
    return createStatusPayload(
        pid > 0,
        pid,
        restartCountStorage(),
        managerErrorStorage(),
        pid > 0 ? "Acquisition process is running." : "Acquisition process is not running.");
}

Object::Ptr restartAcquisitionProcessOrThrow()
{
    Poco::FastMutex::ScopedLock lock(agentManagerMutex());
    terminateProcessLocked();
    ensureProcessRunningLocked();
    const int pid = currentAgentPidLocked();
    return createStatusPayload(
        pid > 0,
        pid,
        restartCountStorage(),
        managerErrorStorage(),
        "Acquisition process restarted.");
}

} } } // namespace MyIoT::WebUI::JNDM123
