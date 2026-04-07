#include "JNDM123AcquisitionService.h"
#include "JNDM123AcquisitionDds.h"

#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include "Poco/Path.h"
#include "Poco/Process.h"
#include "Poco/Thread.h"
#include "Poco/Timestamp.h"

#include <array>
#include <memory>
#include <sstream>
#include <string>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

const std::string JNDM123AcquisitionService::SERVICE_NAME("io.myiot.services.jndm123acquisition");

namespace {

using Poco::JSON::Array;
using Poco::JSON::Object;

constexpr int kDdsDomain = 37;
constexpr const char* kAgentExecutableName = "myiot-jndm123-acquisition-agent";

struct DividerOutputSpec
{
    int index;
    const char* name;
    const char* pdivName;
    int pin;
};

const std::array<DividerOutputSpec, kJndm123DividerOutputCount> kDividerOutputs = {{
    {0, "Y1", "Pdiv1", 17},
    {1, "Y2", "Pdiv1", 15},
    {2, "Y3", "Pdiv1", 14},
    {3, "Y4", "Pdiv4", 7},
    {4, "Y5", "Pdiv4", 8},
    {5, "Y6", "Pdiv4", 12},
    {6, "Y7", "Pdiv7", 11},
}};

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
    std::array<char, 4096> pathBuffer{};
    const ssize_t length = ::readlink("/proc/self/exe", pathBuffer.data(), pathBuffer.size() - 1);
    if (length > 0)
    {
        return std::string(pathBuffer.data(), static_cast<std::size_t>(length));
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

Object::Ptr createEmptySnapshot(const std::string& message)
{
    Object::Ptr payload = new Object;
    payload->set("authenticated", true);
    payload->set("ok", false);
    payload->set("updatedAt", isoTimestamp());
    payload->set("message", message);
    payload->set("running", false);
    payload->set("previewActive", false);
    payload->set("historyLimit", 240);
    payload->set("queueDepth", 0);
    payload->set("totalFrames", static_cast<Poco::UInt64>(0));
    payload->set("droppedFrames", static_cast<Poco::UInt64>(0));
    payload->set("recoveries", static_cast<Poco::UInt64>(0));
    payload->set("lastFrameSequence", static_cast<Poco::UInt64>(0));
    payload->set("lastFrameAt", std::string());
    payload->set("waveformUpdatedAt", std::string());
    payload->set("lastError", std::string());
    payload->set("devicePath", std::string("/dev/i2c-0"));
    payload->set("referenceClockHz", static_cast<Poco::UInt64>(1000000));
    payload->set("deviceType", "unsupported");
    payload->set("address", "--");
    payload->set("inputClock", "--");
    payload->set("revisionId", 0);
    payload->set("eepBusy", false);
    payload->set("eepLock", false);
    payload->set("powerDown", false);
    payload->set("timelineUs", Array::Ptr(new Array));

    Array::Ptr outputs = new Array;
    for (const auto& spec: kDividerOutputs)
    {
        Object::Ptr item = new Object;
        item->set("index", spec.index);
        item->set("name", spec.name);
        item->set("pdiv", spec.pdivName);
        item->set("pin", spec.pin);
        item->set("divider", 0);
        item->set("frequencyHz", 0.0);
        outputs->add(item);
    }
    payload->set("outputs", outputs);

    Array::Ptr chips = new Array;
    for (std::size_t chipIndex = 0; chipIndex < kJndm123Ad7606Count; ++chipIndex)
    {
        Object::Ptr chip = new Object;
        chip->set("index", static_cast<int>(chipIndex));
        chip->set("name", "ADC" + std::to_string(chipIndex + 1));

        Array::Ptr channels = new Array;
        for (std::size_t channelIndex = 0; channelIndex < kJndm123ChannelsPerChip; ++channelIndex)
        {
            Object::Ptr channel = new Object;
            channel->set("index", static_cast<int>(channelIndex));
            channel->set("name", "CH" + std::to_string(channelIndex + 1));
            channel->set("hasValue", false);
            channel->set("value", 0);
            channel->set("samples", Array::Ptr(new Array));
            channels->add(channel);
        }
        chip->set("channels", channels);
        chips->add(chip);
    }
    payload->set("chips", chips);
    return payload;
}

Object::Ptr snapshotToJson(const AcquisitionDdsSnapshot& snapshot)
{
    Object::Ptr payload = new Object;
    payload->set("authenticated", true);
    payload->set("ok", snapshot.ok);
    payload->set("updatedAt", snapshot.updatedAt);
    payload->set("message", snapshot.message);
    payload->set("running", snapshot.running);
    payload->set("previewActive", snapshot.previewActive);
    payload->set("historyLimit", static_cast<int>(snapshot.historyLimit));
    payload->set("queueDepth", static_cast<Poco::UInt64>(snapshot.queueDepth));
    payload->set("totalFrames", static_cast<Poco::UInt64>(snapshot.totalFrames));
    payload->set("droppedFrames", static_cast<Poco::UInt64>(snapshot.droppedFrames));
    payload->set("recoveries", static_cast<Poco::UInt64>(snapshot.recoveries));
    payload->set("lastFrameSequence", static_cast<Poco::UInt64>(snapshot.lastFrameSequence));
    payload->set("lastFrameAt", snapshot.lastFrameAt);
    payload->set("waveformUpdatedAt", snapshot.waveformUpdatedAt);
    payload->set("lastError", snapshot.lastError);
    payload->set("devicePath", snapshot.devicePath);
    payload->set("referenceClockHz", static_cast<Poco::UInt64>(snapshot.referenceClockHz));
    payload->set("deviceType", snapshot.deviceType);
    payload->set("address", snapshot.address);
    payload->set("inputClock", snapshot.inputClock);
    payload->set("revisionId", snapshot.revisionId);
    payload->set("eepBusy", snapshot.eepBusy);
    payload->set("eepLock", snapshot.eepLock);
    payload->set("powerDown", snapshot.powerDown);

    Array::Ptr outputs = new Array;
    for (std::size_t index = 0; index < kDividerOutputs.size(); ++index)
    {
        Object::Ptr item = new Object;
        item->set("index", kDividerOutputs[index].index);
        item->set("name", kDividerOutputs[index].name);
        item->set("pdiv", kDividerOutputs[index].pdivName);
        item->set("pin", kDividerOutputs[index].pin);
        item->set("divider", snapshot.dividerValues[index]);
        item->set("frequencyHz", snapshot.dividerFrequenciesHz[index]);
        outputs->add(item);
    }
    payload->set("outputs", outputs);

    Array::Ptr timelineUs = new Array;
    for (const auto value: snapshot.timelineUs)
    {
        timelineUs->add(static_cast<Poco::Int64>(value));
    }
    payload->set("timelineUs", timelineUs);

    const std::size_t pointsPerChannel = snapshot.timelineUs.size();
    Array::Ptr chips = new Array;
    for (std::size_t chipIndex = 0; chipIndex < kJndm123Ad7606Count; ++chipIndex)
    {
        Object::Ptr chip = new Object;
        chip->set("index", static_cast<int>(chipIndex));
        chip->set("name", "ADC" + std::to_string(chipIndex + 1));

        Array::Ptr channels = new Array;
        for (std::size_t channelIndex = 0; channelIndex < kJndm123ChannelsPerChip; ++channelIndex)
        {
            const std::size_t flatIndex = chipIndex * kJndm123ChannelsPerChip + channelIndex;

            Object::Ptr channel = new Object;
            channel->set("index", static_cast<int>(channelIndex));
            channel->set("name", "CH" + std::to_string(channelIndex + 1));
            channel->set("hasValue", snapshot.latestValueValid[flatIndex]);
            channel->set("value", snapshot.latestValues[flatIndex]);

            Array::Ptr samples = new Array;
            const std::size_t start = flatIndex * pointsPerChannel;
            const std::size_t end = std::min(snapshot.historySamples.size(), start + pointsPerChannel);
            for (std::size_t sampleIndex = start; sampleIndex < end; ++sampleIndex)
            {
                samples->add(snapshot.historySamples[sampleIndex]);
            }
            channel->set("samples", samples);
            channels->add(channel);
        }

        chip->set("channels", channels);
        chips->add(chip);
    }
    payload->set("chips", chips);
    return payload;
}

void enrichPayload(
        Object::Ptr payload,
        bool running,
        int pid,
        unsigned restartCount,
        const std::string& managerError)
{
    payload->set("agentProcessRunning", running);
    payload->set("agentProcessId", pid);
    payload->set("agentRestartCount", static_cast<Poco::UInt64>(restartCount));
    payload->set("agentManagerError", managerError);
}

class AcquisitionServiceImpl: public JNDM123AcquisitionService
{
public:
    explicit AcquisitionServiceImpl(Poco::OSP::BundleContext::Ptr pContext):
        _pContext(std::move(pContext))
    {
        _snapshotSubscriber.start(kDdsDomain, [this](const AcquisitionDdsSnapshot& snapshot) {
            Poco::FastMutex::ScopedLock lock(_mutex);
            _latestSnapshot = snapshot;
            _hasSnapshot = true;
            _lastManagerError.clear();
        });
        _commandClient.start(kDdsDomain, "osp-jndm123-supervisor");

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
        _commandClient.stop();
        _snapshotSubscriber.stop();
    }

    Poco::JSON::Object::Ptr latestSnapshot() override
    {
        Poco::FastMutex::ScopedLock lock(_mutex);
        ensureProcessRunningLocked();
        Object::Ptr payload = sendStatusCommandLocked(std::string());
        enrichPayload(payload, processRunning(_handle), currentPidLocked(), _restartCount, _lastManagerError);
        return payload;
    }

    Poco::JSON::Object::Ptr readDividerStatus(const std::string& devicePath = std::string()) override
    {
        Poco::FastMutex::ScopedLock lock(_mutex);
        ensureProcessRunningLocked();
        Object::Ptr payload = sendStatusCommandLocked(devicePath);
        enrichPayload(payload, processRunning(_handle), currentPidLocked(), _restartCount, _lastManagerError);
        return payload;
    }

    Poco::JSON::Object::Ptr applyDividers(
            const std::string& devicePath,
            const std::vector<int>& outputIndices,
            int divider,
            Poco::UInt64 referenceClockHz) override
    {
        Poco::FastMutex::ScopedLock lock(_mutex);
        ensureProcessRunningLocked();

        AcquisitionDdsCommand command;
        command.kind = AcquisitionCommandKind::ApplyDividers;
        command.devicePath = devicePath;
        command.referenceClockHz = referenceClockHz;
        command.divider = divider;
        for (const int outputIndex: outputIndices)
        {
            if (outputIndex >= 0 && outputIndex < static_cast<int>(kJndm123DividerOutputCount))
            {
                command.outputMask |= (1u << outputIndex);
            }
        }

        Object::Ptr payload = sendCommandLocked(command);
        enrichPayload(payload, processRunning(_handle), currentPidLocked(), _restartCount, _lastManagerError);
        return payload;
    }

    Poco::JSON::Object::Ptr updateReferenceClock(const std::string& devicePath, Poco::UInt64 referenceClockHz) override
    {
        Poco::FastMutex::ScopedLock lock(_mutex);
        ensureProcessRunningLocked();

        AcquisitionDdsCommand command;
        command.kind = AcquisitionCommandKind::UpdateReferenceClock;
        command.devicePath = devicePath;
        command.referenceClockHz = referenceClockHz;

        Object::Ptr payload = sendCommandLocked(command);
        enrichPayload(payload, processRunning(_handle), currentPidLocked(), _restartCount, _lastManagerError);
        return payload;
    }

    Poco::JSON::Object::Ptr startAcquisition() override
    {
        Poco::FastMutex::ScopedLock lock(_mutex);
        ensureProcessRunningLocked();

        AcquisitionDdsCommand command;
        command.kind = AcquisitionCommandKind::Start;
        Object::Ptr payload = sendCommandLocked(command);
        enrichPayload(payload, processRunning(_handle), currentPidLocked(), _restartCount, _lastManagerError);
        return payload;
    }

    Poco::JSON::Object::Ptr stopAcquisition(const std::string& message = std::string()) override
    {
        Poco::FastMutex::ScopedLock lock(_mutex);
        ensureProcessRunningLocked();

        AcquisitionDdsCommand command;
        command.kind = AcquisitionCommandKind::Stop;
        command.message = message;
        Object::Ptr payload = sendCommandLocked(command);
        enrichPayload(payload, processRunning(_handle), currentPidLocked(), _restartCount, _lastManagerError);
        return payload;
    }

    Poco::JSON::Object::Ptr restartProcess() override
    {
        Poco::FastMutex::ScopedLock lock(_mutex);
        terminateProcessLocked();
        ensureProcessRunningLocked();

        for (int attempt = 0; attempt < 10; ++attempt)
        {
            Poco::Thread::sleep(200);
            try
            {
                Object::Ptr payload = sendStatusCommandLocked(std::string());
                payload->set("message", "Acquisition process restarted.");
                enrichPayload(payload, processRunning(_handle), currentPidLocked(), _restartCount, _lastManagerError);
                return payload;
            }
            catch (...)
            {
            }
        }

        Object::Ptr payload = fallbackSnapshotLocked("Acquisition process restarted, waiting for the first DDS response.");
        enrichPayload(payload, processRunning(_handle), currentPidLocked(), _restartCount, _lastManagerError);
        return payload;
    }

    Poco::JSON::Object::Ptr serviceStatus() override
    {
        Poco::FastMutex::ScopedLock lock(_mutex);
        ensureProcessRunningLocked();
        Object::Ptr payload = sendStatusCommandLocked(std::string());
        enrichPayload(payload, processRunning(_handle), currentPidLocked(), _restartCount, _lastManagerError);
        return payload;
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
        _lastManagerError.clear();
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

    Object::Ptr fallbackSnapshotLocked(const std::string& message)
    {
        if (_hasSnapshot)
        {
            Object::Ptr payload = snapshotToJson(_latestSnapshot);
            payload->set("message", message);
            payload->set("ok", false);
            return payload;
        }
        return createEmptySnapshot(message);
    }

    Object::Ptr sendStatusCommandLocked(const std::string& devicePath)
    {
        AcquisitionDdsCommand command;
        command.kind = AcquisitionCommandKind::Status;
        command.devicePath = devicePath;
        return sendCommandLocked(command);
    }

    Object::Ptr sendCommandLocked(const AcquisitionDdsCommand& command)
    {
        std::string lastError;
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            try
            {
                const AcquisitionDdsCommandResult result = _commandClient.sendCommand(command, 1000);
                _latestSnapshot = result.snapshot;
                _hasSnapshot = true;
                _lastManagerError = result.ok ? std::string() : result.message;
                return snapshotToJson(result.snapshot);
            }
            catch (const std::exception& exc)
            {
                lastError = exc.what();
                _lastManagerError = lastError;
                Poco::Thread::sleep(150);
            }
        }

        throw Poco::TimeoutException("Timed out waiting for DDS command result", lastError);
    }

    Poco::OSP::BundleContext::Ptr _pContext;
    mutable Poco::FastMutex _mutex;
    AcquisitionCommandClient _commandClient;
    AcquisitionSnapshotSubscriber _snapshotSubscriber;
    std::unique_ptr<Poco::ProcessHandle> _handle;
    AcquisitionDdsSnapshot _latestSnapshot;
    bool _hasSnapshot = false;
    unsigned _restartCount = 0;
    std::string _lastManagerError;
};

} // namespace

JNDM123AcquisitionService::Ptr createJNDM123AcquisitionService(Poco::OSP::BundleContext::Ptr pContext)
{
    return new AcquisitionServiceImpl(std::move(pContext));
}

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
