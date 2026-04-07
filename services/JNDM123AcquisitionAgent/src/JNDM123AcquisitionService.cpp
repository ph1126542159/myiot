#include "JNDM123AcquisitionService.h"

#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/JSON/Object.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include "Poco/Path.h"
#include "Poco/Process.h"
#include "Poco/Thread.h"
#include "Poco/Timestamp.h"

#include <memory>
#include <string>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

const std::string JNDM123AcquisitionService::SERVICE_NAME("io.myiot.services.jndm123acquisition");

namespace {

using Poco::JSON::Object;

constexpr int kDdsDomain = 37;
constexpr const char* kAgentExecutableName = "myiot-jndm123-acquisition-agent";

Poco::Logger& logger()
{
    static Poco::Logger& instance = Poco::Logger::get("MyIoT.Services.JNDM123Acquisition");
    return instance;
}

std::string isoTimestamp(const Poco::Timestamp& timestamp = Poco::Timestamp())
{
    return Poco::DateTimeFormatter::format(timestamp, Poco::DateTimeFormat::ISO8601_FORMAT);
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

bool processRunning(const std::unique_ptr<Poco::ProcessHandle>& handle)
{
    return handle && Poco::Process::isRunning(*handle);
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

class AcquisitionServiceImpl: public JNDM123AcquisitionService
{
public:
    explicit AcquisitionServiceImpl(Poco::OSP::BundleContext::Ptr pContext):
        _pContext(std::move(pContext))
    {
        Poco::FastMutex::ScopedLock lock(_mutex);
        ensureProcessRunningLocked();
    }

    ~AcquisitionServiceImpl() override
    {
        try
        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            terminateProcessLocked();
        }
        catch (...)
        {
        }
    }

    Object::Ptr restartProcess() override
    {
        Poco::FastMutex::ScopedLock lock(_mutex);
        terminateProcessLocked();
        ensureProcessRunningLocked();
        _lastManagerError.clear();
        return statusPayloadLocked("Acquisition process restarted.");
    }

    Object::Ptr serviceStatus() override
    {
        Poco::FastMutex::ScopedLock lock(_mutex);
        ensureProcessRunningLocked();
        return statusPayloadLocked(
            processRunning(_handle)
                ? "Acquisition process is running."
                : "Acquisition process is not running.");
    }

private:
    void ensureProcessRunningLocked()
    {
        if (processRunning(_handle)) return;

        const std::string executablePath = agentExecutablePath();
        if (!Poco::File(executablePath).exists())
        {
            throw Poco::FileNotFoundException("Acquisition agent executable not found", executablePath);
        }

        Poco::Process::Args args;
        args.push_back("-lc");
        args.push_back(makeLaunchCommand(executablePath));
        _handle.reset(new Poco::ProcessHandle(
            Poco::Process::launch("/bin/sh", args, Poco::Path(executablePath).parent().toString())));
        ++_restartCount;
        logger().information("Started JNDM123 acquisition process pid=" + std::to_string(_handle->id()));
    }

    void terminateProcessLocked()
    {
        if (!_handle) return;

        if (Poco::Process::isRunning(*_handle))
        {
            Poco::Process::kill(*_handle);
            for (int attempt = 0; attempt < 20; ++attempt)
            {
                if (_handle->tryWait() != -1) break;
                Poco::Thread::sleep(100);
            }
        }

        logger().information("Stopped JNDM123 acquisition process pid=" + std::to_string(_handle->id()));
        _handle.reset();
    }

    int currentPidLocked() const
    {
        return _handle ? static_cast<int>(_handle->id()) : -1;
    }

    Object::Ptr statusPayloadLocked(const std::string& message) const
    {
        return createStatusPayload(
            processRunning(_handle),
            currentPidLocked(),
            _restartCount,
            _lastManagerError,
            message);
    }

    Poco::OSP::BundleContext::Ptr _pContext;
    mutable Poco::FastMutex _mutex;
    std::unique_ptr<Poco::ProcessHandle> _handle;
    std::string _lastManagerError;
    unsigned _restartCount = 0;
};

} // namespace

JNDM123AcquisitionService::Ptr createJNDM123AcquisitionService(Poco::OSP::BundleContext::Ptr pContext)
{
    return new AcquisitionServiceImpl(std::move(pContext));
}

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
