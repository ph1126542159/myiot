#include "JNDM123RequestHandler.h"

#include "JNDM123AcquisitionService.h"
#include "Poco/AutoPtr.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/Mutex.h"
#include "Poco/Format.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Logger.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Notification.h"
#include "Poco/NotificationQueue.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/String.h"
#include "Poco/Thread.h"
#include "Poco/Timestamp.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <exception>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Poco::JSON::Array;
using Poco::JSON::Object;
using MyIoT::Services::JNDM123AcquisitionAgent::JNDM123AcquisitionService;

constexpr const char* kDefaultI2CDevice = "/dev/i2c-0";
constexpr const char* kDividerStateFile = "/var/lib/myiot/jndm123-divider-state.properties";
constexpr std::size_t kDividerOutputCount = 7;
constexpr std::size_t kDividerStartupOutputCount = 6;
constexpr std::size_t kAd7606Count = 6;
constexpr std::size_t kChannelsPerChip = 8;
constexpr std::size_t kFrameWords = 24;
constexpr std::size_t kFrameColumns = kAd7606Count * kChannelsPerChip;
constexpr std::size_t kHistoryLimit = 240;
constexpr std::size_t kMaxPendingFrames = 4096;
constexpr Poco::UInt64 kFallbackReferenceClockHz = 1000000ULL;
constexpr Poco::UInt64 kMaxSafeAcquisitionClockHz = 50000ULL;
constexpr Poco::Int64 kPreviewLeaseUs = 2 * 1000 * 1000;
constexpr Poco::Int64 kPreviewPublishIntervalUs = 1 * 1000 * 1000;

#if defined(__linux__)
constexpr std::uint32_t kFifoCtrlBase = 0x43C00000UL;
constexpr std::uint32_t kFifoDataPort = 0x83C01000UL;
constexpr std::size_t kMapSize = 4096U;
constexpr std::uint32_t kIsr = 0x00U;
constexpr std::uint32_t kRdfr = 0x18U;
constexpr std::uint32_t kRdfo = 0x1CU;
constexpr std::uint32_t kRlr = 0x24U;
constexpr std::uint32_t kSrr = 0x28U;
constexpr std::uint32_t kIsrRpue = (1u << 29);
constexpr std::uint32_t kIsrRpore = (1u << 30);
constexpr std::uint32_t kIsrRpure = (1u << 31);
constexpr int kGpioReset = 968;
constexpr int kGpioMaskStart = 976;
constexpr bool kMaskStartActiveLow = false;
constexpr std::uint8_t kCdce937RegId = 0x00;
constexpr std::uint8_t kCdce937RegCfg1 = 0x01;
constexpr std::uint8_t kCdce937RegY1Control = 0x02;
constexpr std::uint8_t kCdce937RegMux1 = 0x14;
constexpr std::uint8_t kCdce937RegMux2 = 0x24;
constexpr std::uint8_t kCdce937RegMux3 = 0x34;
constexpr std::uint8_t kProbeAddresses[] = {0x6c, 0x6d, 0x6e, 0x6f};
#endif

struct DividerOutputSpec
{
    int index;
    const char* name;
    const char* pdivName;
    int pin;
};

const std::array<DividerOutputSpec, kDividerOutputCount> kDividerOutputs = {{
    {0, "Y1", "Pdiv1", 17},
    {1, "Y2", "Pdiv1", 15},
    {2, "Y3", "Pdiv1", 14},
    {3, "Y4", "Pdiv4", 7},
    {4, "Y5", "Pdiv4", 8},
    {5, "Y6", "Pdiv4", 12},
    {6, "Y7", "Pdiv7", 11},
}};

Poco::UInt64& configuredReferenceClockHzStorage()
{
    static Poco::UInt64 value = kFallbackReferenceClockHz;
    return value;
}

Poco::UInt64 configuredReferenceClockHz()
{
    return configuredReferenceClockHzStorage();
}

void setConfiguredReferenceClockHz(Poco::UInt64 referenceClockHz)
{
    configuredReferenceClockHzStorage() = referenceClockHz;
}

Poco::OSP::BundleContext::Ptr& runtimeBundleContextStorage()
{
    static Poco::OSP::BundleContext::Ptr context;
    return context;
}

struct SavedDividerConfiguration
{
    std::string devicePath = kDefaultI2CDevice;
    Poco::UInt64 referenceClockHz = configuredReferenceClockHz();
    std::array<int, kDividerStartupOutputCount> dividers{{1, 1, 1, 1, 1, 1}};
    std::array<bool, kDividerStartupOutputCount> savedOutputs{{false, false, false, false, false, false}};
    bool hasAnyOutput = false;
    bool hasAllOutputs = false;
};

Poco::Logger& logger()
{
    static Poco::Logger& instance = Poco::Logger::get("MyIoT.WebUI.JNDM123");
    return instance;
}

std::string isoTimestamp(const Poco::Timestamp& timestamp = Poco::Timestamp())
{
    return Poco::DateTimeFormatter::format(timestamp, Poco::DateTimeFormat::ISO8601_FORMAT);
}

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

void sendJSON(Poco::Net::HTTPServerResponse& response, Object::Ptr payload, Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_OK)
{
    response.setStatus(status);
    response.setChunkedTransferEncoding(true);
    response.setContentType("application/json");
    response.set("Cache-Control", "no-cache");

    try
    {
        std::ostream& out = response.send();
        Poco::JSON::Stringifier::stringify(payload, out);
    }
    catch (const Poco::Net::ConnectionResetException&)
    {
    }
    catch (const Poco::Net::ConnectionAbortedException&)
    {
    }
    catch (const Poco::IOException& exc)
    {
        const std::string text = exc.displayText();
        if (text.find("broken pipe") == std::string::npos && text.find("Broken pipe") == std::string::npos)
        {
            throw;
        }
    }
}

Object::Ptr createUnauthorizedPayload(const std::string& message)
{
    Object::Ptr payload = new Object;
    payload->set("authenticated", false);
    payload->set("ok", false);
    payload->set("message", message);
    payload->set("updatedAt", isoTimestamp());
    return payload;
}

Object::Ptr createErrorPayload(const std::string& message)
{
    Object::Ptr payload = new Object;
    payload->set("authenticated", true);
    payload->set("ok", false);
    payload->set("message", message);
    payload->set("updatedAt", isoTimestamp());
    return payload;
}

JNDM123AcquisitionService::Ptr acquisitionServiceOrThrow()
{
    Poco::OSP::BundleContext::Ptr pContext = runtimeBundleContextStorage();
    if (!pContext)
    {
        throw Poco::IllegalStateException("JNDM123 bundle context is not initialized.");
    }

    return Poco::OSP::ServiceFinder::findByName<JNDM123AcquisitionService>(
        pContext,
        JNDM123AcquisitionService::SERVICE_NAME);
}

void stripWaveformSamples(Object::Ptr payload)
{
    if (!payload) return;

    payload->set("includeWaveform", false);
    payload->set("timelineUs", Array::Ptr(new Array));

    Array::Ptr chips = payload->getArray("chips");
    if (!chips) return;

    for (std::size_t chipIndex = 0; chipIndex < chips->size(); ++chipIndex)
    {
        Object::Ptr chip = chips->getObject(chipIndex);
        if (!chip) continue;

        Array::Ptr channels = chip->getArray("channels");
        if (!channels) continue;

        for (std::size_t channelIndex = 0; channelIndex < channels->size(); ++channelIndex)
        {
            Object::Ptr channel = channels->getObject(channelIndex);
            if (!channel) continue;
            channel->set("samples", Array::Ptr(new Array));
        }
    }
}

bool remoteAcquisitionRunning()
{
    try
    {
        Object::Ptr payload = acquisitionServiceOrThrow()->serviceStatus();
        return payload->optValue("running", false);
    }
    catch (...)
    {
        return false;
    }
}

bool parseIntStrict(const std::string& text, int& value)
{
    if (text.empty()) return false;

    try
    {
        std::size_t pos = 0;
        const int parsed = std::stoi(text, &pos, 10);
        if (pos != text.size()) return false;
        value = parsed;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool parseUInt64Strict(const std::string& text, Poco::UInt64& value)
{
    if (text.empty()) return false;

    try
    {
        std::size_t pos = 0;
        const unsigned long long parsed = std::stoull(text, &pos, 10);
        if (pos != text.size()) return false;
        value = static_cast<Poco::UInt64>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool parseBoolValue(const std::string& text, bool defaultValue = false)
{
    if (text.empty()) return defaultValue;
    if (Poco::icompare(text, std::string("1")) == 0) return true;
    if (Poco::icompare(text, std::string("true")) == 0) return true;
    if (Poco::icompare(text, std::string("yes")) == 0) return true;
    if (Poco::icompare(text, std::string("on")) == 0) return true;
    if (Poco::icompare(text, std::string("0")) == 0) return false;
    if (Poco::icompare(text, std::string("false")) == 0) return false;
    if (Poco::icompare(text, std::string("no")) == 0) return false;
    if (Poco::icompare(text, std::string("off")) == 0) return false;
    return defaultValue;
}

int dividerLimitForOutput(int outputIndex)
{
    return outputIndex >= 0 && outputIndex <= 2 ? 1023 : 127;
}

std::vector<int> normalizeOutputIndicesOrThrow(const std::vector<int>& outputIndices)
{
    if (outputIndices.empty())
    {
        throw Poco::InvalidArgumentException("At least one output must be selected.");
    }

    std::vector<int> normalized = outputIndices;
    std::sort(normalized.begin(), normalized.end());
    normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());

    for (const int outputIndex: normalized)
    {
        if (outputIndex < 0 || outputIndex >= static_cast<int>(kDividerOutputCount))
        {
            throw Poco::InvalidArgumentException("Output index must be in 0..6.");
        }
    }

    return normalized;
}

std::vector<int> parseOutputIndexListOrThrow(const std::string& text)
{
    std::vector<int> outputIndices;
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        Poco::trimInPlace(token);
        if (token.empty()) continue;

        int outputIndex = 0;
        if (!parseIntStrict(token, outputIndex))
        {
            throw Poco::InvalidArgumentException("outputIndices must contain numeric output indexes.");
        }
        outputIndices.push_back(outputIndex);
    }

    return normalizeOutputIndicesOrThrow(outputIndices);
}

void validateDividerOrThrow(int outputIndex, int divider)
{
    if (outputIndex < 0 || outputIndex >= static_cast<int>(kDividerOutputCount))
    {
        throw Poco::InvalidArgumentException("Output index must be in 0..6.");
    }

    const int limit = dividerLimitForOutput(outputIndex);
    if (divider <= 0 || divider > limit)
    {
        throw Poco::InvalidArgumentException(
            Poco::format("Divider for %s must be in 1..%d.", kDividerOutputs[outputIndex].name, limit));
    }
}

void validateReferenceClockHzOrThrow(Poco::UInt64 referenceClockHz)
{
    if (referenceClockHz == 0 || referenceClockHz > 1000000000ULL)
    {
        throw Poco::InvalidArgumentException("referenceClockHz must be in 1..1000000000.");
    }
}

std::string describeOutputs(const std::vector<int>& outputIndices)
{
    const std::vector<int> normalized = normalizeOutputIndicesOrThrow(outputIndices);

    std::ostringstream description;
    for (std::size_t index = 0; index < normalized.size(); ++index)
    {
        if (index > 0)
        {
            description << (index + 1 == normalized.size() ? ", " : ", ");
        }
        description << kDividerOutputs[normalized[index]].name;
    }
    return description.str();
}

struct DividerOutputState
{
    int index = 0;
    std::string name;
    std::string pdivName;
    int pin = 0;
    int divider = 0;
    double frequencyHz = 0.0;
};

struct DividerSnapshot
{
    bool ok = true;
    std::string message = "Divider status synchronized.";
    std::string devicePath = kDefaultI2CDevice;
    Poco::UInt64 referenceClockHz = configuredReferenceClockHz();
    std::string deviceType = "unsupported";
    std::string address = "--";
    std::string inputClock = "--";
    int revisionId = 0;
    bool eepBusy = false;
    bool eepLock = false;
    bool powerDown = false;
    std::vector<DividerOutputState> outputs;
};

double maximumStartupOutputFrequencyHz(const DividerSnapshot& snapshot)
{
    double maximum = 0.0;
    const std::size_t count = std::min(snapshot.outputs.size(), kDividerStartupOutputCount);
    for (std::size_t index = 0; index < count; ++index)
    {
        maximum = std::max(maximum, snapshot.outputs[index].frequencyHz);
    }
    return maximum;
}

std::string acquisitionClockSafetyMessage(const DividerSnapshot& snapshot)
{
    const double maximumFrequencyHz = maximumStartupOutputFrequencyHz(snapshot);
    if (maximumFrequencyHz <= static_cast<double>(kMaxSafeAcquisitionClockHz))
    {
        return std::string();
    }

    return Poco::format(
        "Acquisition remains stopped because the configured clock (%.0f Hz) exceeds the software safety limit (%Lu Hz).",
        maximumFrequencyHz,
        static_cast<unsigned long long>(kMaxSafeAcquisitionClockHz));
}

SavedDividerConfiguration loadSavedDividerConfiguration()
{
    SavedDividerConfiguration configuration;
    std::ifstream input(kDividerStateFile);
    if (!input) return configuration;

    std::string line;
    while (std::getline(input, line))
    {
        Poco::trimInPlace(line);
        if (line.empty() || line[0] == '#') continue;

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;

        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        Poco::trimInPlace(key);
        Poco::trimInPlace(value);

        if (key == "devicePath")
        {
            if (!value.empty()) configuration.devicePath = value;
            continue;
        }

        if (key == "referenceClockHz")
        {
            Poco::UInt64 referenceClockHz = 0;
            if (!parseUInt64Strict(value, referenceClockHz)) continue;

            try
            {
                validateReferenceClockHzOrThrow(referenceClockHz);
            }
            catch (...)
            {
                continue;
            }

            configuration.referenceClockHz = referenceClockHz;
            continue;
        }

        if (key.size() == 2 && key[0] == 'y' && key[1] >= '1' && key[1] <= '6')
        {
            int divider = 0;
            if (!parseIntStrict(value, divider)) continue;

            const int outputIndex = key[1] - '1';
            try
            {
                validateDividerOrThrow(outputIndex, divider);
            }
            catch (...)
            {
                continue;
            }

            configuration.dividers[outputIndex] = divider;
            configuration.savedOutputs[outputIndex] = true;
        }
    }

    configuration.hasAnyOutput =
        std::any_of(configuration.savedOutputs.begin(), configuration.savedOutputs.end(), [](bool value) { return value; });
    configuration.hasAllOutputs =
        std::all_of(configuration.savedOutputs.begin(), configuration.savedOutputs.end(), [](bool value) { return value; });
    return configuration;
}

std::string resolvePreferredDevicePath(const std::string& requestedDevicePath)
{
    if (!requestedDevicePath.empty())
    {
        return requestedDevicePath;
    }

    const SavedDividerConfiguration saved = loadSavedDividerConfiguration();
    if (!saved.devicePath.empty())
    {
        return saved.devicePath;
    }

    return kDefaultI2CDevice;
}

Poco::UInt64 resolvePreferredReferenceClockHz(const std::string& requestedReferenceClockHz)
{
    if (!requestedReferenceClockHz.empty())
    {
        Poco::UInt64 referenceClockHz = 0;
        if (!parseUInt64Strict(requestedReferenceClockHz, referenceClockHz))
        {
            throw Poco::InvalidArgumentException("referenceClockHz must be a numeric value.");
        }
        validateReferenceClockHzOrThrow(referenceClockHz);
        return referenceClockHz;
    }

    const SavedDividerConfiguration saved = loadSavedDividerConfiguration();
    validateReferenceClockHzOrThrow(saved.referenceClockHz);
    return saved.referenceClockHz;
}

void saveDividerConfiguration(const DividerSnapshot& snapshot, const std::string& devicePath)
{
    Poco::Path configPath(kDividerStateFile);
    Poco::File(configPath.parent()).createDirectories();

    std::ofstream output(kDividerStateFile, std::ios::trunc);
    if (!output)
    {
        throw Poco::IOException("Unable to write divider state file " + std::string(kDividerStateFile));
    }

    output << "# Auto-generated by MyIoT WebUI JNDM123\n";
    output << "devicePath=" << (devicePath.empty() ? kDefaultI2CDevice : devicePath) << "\n";
    output << "referenceClockHz=" << snapshot.referenceClockHz << "\n";
    for (std::size_t index = 0; index < kDividerStartupOutputCount; ++index)
    {
        if (index >= snapshot.outputs.size())
        {
            throw Poco::InvalidAccessException("Divider snapshot is missing outputs required for persistence.");
        }
        output << "y" << (index + 1) << "=" << snapshot.outputs[index].divider << "\n";
    }

    if (!output.good())
    {
        throw Poco::IOException("Unable to flush divider state file " + std::string(kDividerStateFile));
    }
}

struct AcquisitionActionResult
{
    bool ok = true;
    std::string message;
};

AcquisitionActionResult acquisitionResultFromPayload(Object::Ptr payload, const std::string& defaultMessage)
{
    AcquisitionActionResult result;
    result.ok = !payload || payload->optValue("ok", true);
    result.message = payload ? payload->optValue("message", defaultMessage) : defaultMessage;
    return result;
}

DividerSnapshot dividerSnapshotFromPayload(Object::Ptr payload)
{
    DividerSnapshot snapshot;
    if (!payload) return snapshot;

    snapshot.ok = payload->optValue("ok", true);
    snapshot.message = payload->optValue("message", snapshot.message);
    snapshot.devicePath = payload->optValue("devicePath", snapshot.devicePath);
    snapshot.referenceClockHz = payload->optValue<Poco::UInt64>("referenceClockHz", snapshot.referenceClockHz);
    snapshot.deviceType = payload->optValue("deviceType", snapshot.deviceType);
    snapshot.address = payload->optValue("address", snapshot.address);
    snapshot.inputClock = payload->optValue("inputClock", snapshot.inputClock);
    snapshot.revisionId = payload->optValue("revisionId", snapshot.revisionId);
    snapshot.eepBusy = payload->optValue("eepBusy", snapshot.eepBusy);
    snapshot.eepLock = payload->optValue("eepLock", snapshot.eepLock);
    snapshot.powerDown = payload->optValue("powerDown", snapshot.powerDown);

    Array::Ptr outputs = payload->getArray("outputs");
    if (!outputs) return snapshot;

    for (std::size_t index = 0; index < outputs->size(); ++index)
    {
        Object::Ptr item = outputs->getObject(index);
        if (!item) continue;

        DividerOutputState output;
        output.index = item->optValue("index", static_cast<int>(index));
        output.name = item->optValue("name", std::string());
        output.pdivName = item->optValue("pdiv", std::string());
        output.pin = item->optValue("pin", 0);
        output.divider = item->optValue("divider", 0);
        output.frequencyHz = item->optValue("frequencyHz", 0.0);
        snapshot.outputs.push_back(output);
    }

    return snapshot;
}

struct FramePacket
{
    std::array<std::uint32_t, kFrameWords> words{};
    Poco::UInt64 sequence = 0;
    Poco::Timestamp capturedAt;
};

class FrameNotification: public Poco::Notification
{
public:
    explicit FrameNotification(FramePacket packet):
        _packet(std::move(packet))
    {
    }

    const FramePacket& packet() const
    {
        return _packet;
    }

private:
    FramePacket _packet;
};

class JNDM123Runtime;

class ReaderRunnable: public Poco::Runnable
{
public:
    explicit ReaderRunnable(JNDM123Runtime& owner):
        _owner(owner)
    {
    }

    void run() override;

private:
    JNDM123Runtime& _owner;
};

class DispatcherRunnable: public Poco::Runnable
{
public:
    explicit DispatcherRunnable(JNDM123Runtime& owner):
        _owner(owner)
    {
    }

    void run() override;

private:
    JNDM123Runtime& _owner;
};

class JNDM123Runtime
{
public:
    static JNDM123Runtime& instance();

    void initializeFromSavedConfiguration();
    DividerSnapshot readDividerStatus(const std::string& devicePath);
    DividerSnapshot applyDivider(const std::string& devicePath, int outputIndex, int divider, Poco::UInt64 referenceClockHz);
    DividerSnapshot applyDividers(const std::string& devicePath, const std::vector<int>& outputIndices, int divider, Poco::UInt64 referenceClockHz);
    DividerSnapshot updateReferenceClock(const std::string& devicePath, Poco::UInt64 referenceClockHz);
    AcquisitionActionResult startAcquisition();
    AcquisitionActionResult stopAcquisition(const std::string& message = "Acquisition stopped by operator.");
    void touchPreviewLease();
    Object::Ptr acquisitionSnapshot(bool includeWaveform);
    void shutdown();
    void readerLoop();
    void dispatcherLoop();

private:
    JNDM123Runtime();
    ~JNDM123Runtime();
    JNDM123Runtime(const JNDM123Runtime&) = delete;
    JNDM123Runtime& operator = (const JNDM123Runtime&) = delete;

    void setStatusMessage(const std::string& message);
    void recordError(const std::string& message);
    bool previewLeaseActive() const;
    void appendHistoryLocked(const std::array<std::int16_t, kFrameColumns>& samples, Poco::Int64 capturedAtUs);
    void clearWaveformHistory();
    void clearWaveformHistoryLocked();
    bool enqueueFrame(FramePacket packet);
    void drainQueue();
    void unpackFrame(const std::array<std::uint32_t, kFrameWords>& words, std::array<std::int16_t, kFrameColumns>& samples) const;

#if defined(__linux__)
public:
    struct Cdce937Device
    {
        std::string path;
        int fd = -1;
        std::uint8_t address = 0x6c;
        bool addressFixed = false;
    };

    struct MappedHardware
    {
        int memFd = -1;
        volatile std::uint32_t* fifoCtrl = nullptr;
        volatile std::uint32_t* fifoData = nullptr;
        bool gpioInitialized = false;
    };

    void ensureMappedHardwareLocked();
    void releaseMappedHardwareLocked();
    void initializeGpioLocked();
    void stopAdcLocked();
    void startAdcLocked();
    void resetFifoLocked();
    void recoverRxFifo();
    int readOneFramePacket(std::array<std::uint32_t, kFrameWords>& frame);
    DividerSnapshot readDividerStatusLocked(const std::string& devicePath, Poco::UInt64 referenceClockHz);
    DividerSnapshot applyDividerLocked(const std::string& devicePath, int outputIndex, int divider, Poco::UInt64 referenceClockHz);
    DividerSnapshot applyDividersLocked(const std::string& devicePath, const std::vector<int>& outputIndices, int divider, Poco::UInt64 referenceClockHz);
    void disablePllModeLocked(Cdce937Device& device);
    DividerSnapshot initializeHardwareFromSavedConfigurationLocked();
    AcquisitionActionResult startAcquisitionLocked();
    AcquisitionActionResult stopAcquisitionLocked(const std::string& message, bool clearHistoryAfterStop);

private:
#endif

    ReaderRunnable _readerRunnable;
    DispatcherRunnable _dispatcherRunnable;
    Poco::Thread _readerThread;
    Poco::Thread _dispatcherThread;
    bool _readerStarted = false;
    bool _dispatcherStarted = false;
    std::atomic<bool> _readerShouldRun{false};
    std::atomic<bool> _acquisitionRunning{false};
    std::atomic<bool> _shuttingDown{false};
    std::atomic<Poco::Int64> _previewLeaseUntilUs{0};
    std::atomic<std::size_t> _queueDepth{0};
    std::atomic<Poco::UInt64> _totalFrames{0};
    std::atomic<Poco::UInt64> _droppedFrames{0};
    std::atomic<Poco::UInt64> _recoveries{0};
    std::atomic<Poco::UInt64> _lastFrameSequence{0};
    Poco::FastMutex _controlMutex;
    Poco::FastMutex _stateMutex;
    Poco::NotificationQueue _queue;
    std::array<std::deque<std::int16_t>, kFrameColumns> _history;
    std::array<std::deque<std::int16_t>, kFrameColumns> _publishedHistory;
    std::deque<Poco::Int64> _historyTimelineUs;
    std::deque<Poco::Int64> _publishedTimelineUs;
    std::array<std::int16_t, kFrameColumns> _latestSamples{};
    bool _hasLatestFrame = false;
    Poco::Int64 _lastPreviewPublishedAtUs = 0;
    std::string _statusMessage = "JNDM123 runtime ready.";
    std::string _lastError;
    std::string _lastFrameAt;
    std::string _lastPreviewPublishedAt;

#if defined(__linux__)
    MappedHardware _hardware;
#endif
};

Object::Ptr dividerSnapshotToJson(const DividerSnapshot& snapshot);

} // namespace

namespace {

JNDM123Runtime& JNDM123Runtime::instance()
{
    static JNDM123Runtime runtime;
    return runtime;
}

JNDM123Runtime::JNDM123Runtime():
    _readerRunnable(*this),
    _dispatcherRunnable(*this)
{
}

JNDM123Runtime::~JNDM123Runtime()
{
    shutdown();
}

void JNDM123Runtime::initializeFromSavedConfiguration()
{
    try
    {
        Poco::FastMutex::ScopedLock lock(_controlMutex);
        if (configuredReferenceClockHz() != kFallbackReferenceClockHz)
        {
            acquisitionServiceOrThrow()->updateReferenceClock(std::string(), configuredReferenceClockHz());
        }

        Object::Ptr payload = acquisitionServiceOrThrow()->serviceStatus();
        const std::string message = payload->optValue("message", std::string("JNDM123 hardware state synchronized from acquisition agent."));
        setStatusMessage(message);
        logger().information(message);
    }
    catch (const Poco::Exception& exc)
    {
        const std::string message = "JNDM123 startup initialization skipped: " + exc.displayText();
        recordError(message);
        setStatusMessage(message);
        logger().warning(message);
    }
    catch (const std::exception& exc)
    {
        const std::string message = std::string("JNDM123 startup initialization skipped: ") + exc.what();
        recordError(message);
        setStatusMessage(message);
        logger().warning(message);
    }
}

void JNDM123Runtime::setStatusMessage(const std::string& message)
{
    Poco::FastMutex::ScopedLock lock(_stateMutex);
    _statusMessage = message;
}

void JNDM123Runtime::recordError(const std::string& message)
{
    Poco::FastMutex::ScopedLock lock(_stateMutex);
    _lastError = message;
}

bool JNDM123Runtime::previewLeaseActive() const
{
    const Poco::Timestamp now;
    return now.epochMicroseconds() <= _previewLeaseUntilUs.load();
}

void JNDM123Runtime::appendHistoryLocked(const std::array<std::int16_t, kFrameColumns>& samples, Poco::Int64 capturedAtUs)
{
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        std::deque<std::int16_t>& history = _history[i];
        history.push_back(samples[i]);
        if (history.size() > kHistoryLimit)
        {
            history.pop_front();
        }
    }

    _historyTimelineUs.push_back(capturedAtUs);
    if (_historyTimelineUs.size() > kHistoryLimit)
    {
        _historyTimelineUs.pop_front();
    }
}

void JNDM123Runtime::clearWaveformHistory()
{
    Poco::FastMutex::ScopedLock lock(_stateMutex);
    clearWaveformHistoryLocked();
}

void JNDM123Runtime::clearWaveformHistoryLocked()
{
    for (auto& history: _history)
    {
        history.clear();
    }
    for (auto& history: _publishedHistory)
    {
        history.clear();
    }
    _historyTimelineUs.clear();
    _publishedTimelineUs.clear();
    _hasLatestFrame = false;
    _latestSamples.fill(0);
    _lastFrameAt.clear();
    _lastPreviewPublishedAt.clear();
    _lastPreviewPublishedAtUs = 0;
}

bool JNDM123Runtime::enqueueFrame(FramePacket packet)
{
    if (_queueDepth.load() >= kMaxPendingFrames)
    {
        return false;
    }

    _queue.enqueueNotification(new FrameNotification(std::move(packet)));
    _queueDepth.fetch_add(1);
    return true;
}

void JNDM123Runtime::drainQueue()
{
    while (true)
    {
        Poco::AutoPtr<Poco::Notification> notification(_queue.dequeueNotification());
        if (!notification) break;
        if (_queueDepth.load() > 0)
        {
            _queueDepth.fetch_sub(1);
        }
    }
}

void JNDM123Runtime::unpackFrame(const std::array<std::uint32_t, kFrameWords>& words, std::array<std::int16_t, kFrameColumns>& samples) const
{
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        const std::uint32_t word = words[i];
        samples[i * 2] = static_cast<std::int16_t>((word >> 16) & 0xFFFFu);
        samples[i * 2 + 1] = static_cast<std::int16_t>(word & 0xFFFFu);
    }
}

DividerSnapshot JNDM123Runtime::readDividerStatus(const std::string& devicePath)
{
    Poco::FastMutex::ScopedLock lock(_controlMutex);
    return dividerSnapshotFromPayload(acquisitionServiceOrThrow()->readDividerStatus(devicePath));
}

DividerSnapshot JNDM123Runtime::applyDivider(const std::string& devicePath, int outputIndex, int divider, Poco::UInt64 referenceClockHz)
{
    return applyDividers(devicePath, std::vector<int>{outputIndex}, divider, referenceClockHz);
}

DividerSnapshot JNDM123Runtime::applyDividers(const std::string& devicePath, const std::vector<int>& outputIndices, int divider, Poco::UInt64 referenceClockHz)
{
    const std::vector<int> normalizedOutputs = normalizeOutputIndicesOrThrow(outputIndices);
    for (const int outputIndex: normalizedOutputs)
    {
        validateDividerOrThrow(outputIndex, divider);
    }
    validateReferenceClockHzOrThrow(referenceClockHz);

    Poco::FastMutex::ScopedLock lock(_controlMutex);
    DividerSnapshot snapshot = dividerSnapshotFromPayload(
        acquisitionServiceOrThrow()->applyDividers(devicePath, normalizedOutputs, divider, referenceClockHz));
    setStatusMessage(snapshot.message);
    return snapshot;
}

DividerSnapshot JNDM123Runtime::updateReferenceClock(const std::string& devicePath, Poco::UInt64 referenceClockHz)
{
    validateReferenceClockHzOrThrow(referenceClockHz);

    Poco::FastMutex::ScopedLock lock(_controlMutex);
    DividerSnapshot snapshot = dividerSnapshotFromPayload(
        acquisitionServiceOrThrow()->updateReferenceClock(devicePath, referenceClockHz));
    setStatusMessage(snapshot.message);
    return snapshot;
}

AcquisitionActionResult JNDM123Runtime::startAcquisition()
{
    Poco::FastMutex::ScopedLock lock(_controlMutex);
    Object::Ptr payload = acquisitionServiceOrThrow()->startAcquisition();
    AcquisitionActionResult result = acquisitionResultFromPayload(payload, "Acquisition started.");
    setStatusMessage(result.message);
    if (!result.ok) recordError(result.message);
    return result;
}

AcquisitionActionResult JNDM123Runtime::stopAcquisition(const std::string& message)
{
    Poco::FastMutex::ScopedLock lock(_controlMutex);
    Object::Ptr payload = acquisitionServiceOrThrow()->stopAcquisition(message);
    AcquisitionActionResult result = acquisitionResultFromPayload(payload, message);
    setStatusMessage(result.message);
    if (!result.ok) recordError(result.message);
    return result;
}

void JNDM123Runtime::touchPreviewLease()
{
    const Poco::Timestamp now;
    _previewLeaseUntilUs.store(now.epochMicroseconds() + kPreviewLeaseUs);
}

Object::Ptr JNDM123Runtime::acquisitionSnapshot(bool includeWaveform)
{
    Object::Ptr payload = acquisitionServiceOrThrow()->latestSnapshot();
    payload->set("authenticated", true);
    if (!includeWaveform)
    {
        stripWaveformSamples(payload);
    }
    return payload;
}

void JNDM123Runtime::shutdown()
{
    const bool alreadyShuttingDown = _shuttingDown.exchange(true);
    if (alreadyShuttingDown) return;
}

void JNDM123Runtime::readerLoop()
{
#if defined(__linux__)
    Poco::UInt64 sequence = _totalFrames.load();
    int idleLoops = 0;

    while (_readerShouldRun.load() && !_shuttingDown.load())
    {
        FramePacket packet;
        packet.sequence = sequence;

        const int result = readOneFramePacket(packet.words);
        if (result == 1)
        {
            packet.capturedAt.update();
            if (enqueueFrame(std::move(packet)))
            {
                ++sequence;
            }
            else
            {
                _droppedFrames.fetch_add(1);
            }
            idleLoops = 0;
        }
        else if (result == 0)
        {
            ++idleLoops;
            if (idleLoops > 200)
            {
                Poco::Thread::yield();
                idleLoops = 0;
            }
        }
        else
        {
            _recoveries.fetch_add(1);
            recordError("FIFO packet error detected, recovery executed.");
            recoverRxFifo();
            idleLoops = 0;
        }
    }
#endif
}

void JNDM123Runtime::dispatcherLoop()
{
    while (true)
    {
        if (_shuttingDown.load() && _queueDepth.load() == 0)
        {
            break;
        }

        Poco::AutoPtr<Poco::Notification> notification(_queue.waitDequeueNotification(200));
        if (!notification)
        {
            continue;
        }

        if (_queueDepth.load() > 0)
        {
            _queueDepth.fetch_sub(1);
        }

        FrameNotification* pFrameNotification = dynamic_cast<FrameNotification*>(notification.get());
        if (!pFrameNotification) continue;

        const FramePacket& packet = pFrameNotification->packet();
        std::array<std::int16_t, kFrameColumns> samples{};
        unpackFrame(packet.words, samples);

        const bool keepWaveform = previewLeaseActive();
        const Poco::Int64 capturedAtUs = packet.capturedAt.epochMicroseconds();

        {
            Poco::FastMutex::ScopedLock lock(_stateMutex);
            _latestSamples = samples;
            _hasLatestFrame = true;
            _lastFrameSequence.store(packet.sequence);
            _totalFrames.store(packet.sequence + 1);
            _lastFrameAt = isoTimestamp(packet.capturedAt);
            if (keepWaveform)
            {
                appendHistoryLocked(samples, capturedAtUs);
                if (_lastPreviewPublishedAtUs == 0 ||
                    (capturedAtUs - _lastPreviewPublishedAtUs) >= kPreviewPublishIntervalUs)
                {
                    _publishedHistory = _history;
                    _publishedTimelineUs = _historyTimelineUs;
                    _lastPreviewPublishedAtUs = capturedAtUs;
                    _lastPreviewPublishedAt = isoTimestamp(packet.capturedAt);
                }
            }
        }

    }
}

void ReaderRunnable::run()
{
    _owner.readerLoop();
}

void DispatcherRunnable::run()
{
    _owner.dispatcherLoop();
}

Object::Ptr dividerSnapshotToJson(const DividerSnapshot& snapshot)
{
    Object::Ptr payload = new Object;
    payload->set("authenticated", true);
    payload->set("ok", snapshot.ok);
    payload->set("message", snapshot.message);
    payload->set("updatedAt", isoTimestamp());
    payload->set("devicePath", snapshot.devicePath);
    payload->set("referenceClockHz", snapshot.referenceClockHz);
    payload->set("deviceType", snapshot.deviceType);
    payload->set("address", snapshot.address);
    payload->set("inputClock", snapshot.inputClock);
    payload->set("revisionId", snapshot.revisionId);
    payload->set("eepBusy", snapshot.eepBusy);
    payload->set("eepLock", snapshot.eepLock);
    payload->set("powerDown", snapshot.powerDown);

    Array::Ptr outputs = new Array;
    for (const auto& output: snapshot.outputs)
    {
        Object::Ptr item = new Object;
        item->set("index", output.index);
        item->set("name", output.name);
        item->set("pdiv", output.pdivName);
        item->set("pin", output.pin);
        item->set("divider", output.divider);
        item->set("frequencyHz", output.frequencyHz);
        outputs->add(item);
    }
    payload->set("outputs", outputs);
    return payload;
}

} // namespace

#if defined(__linux__)
namespace {

void writeSysfsValue(const std::string& path, const std::string& value)
{
    const int fd = ::open(path.c_str(), O_WRONLY);
    if (fd < 0) return;
    ::write(fd, value.c_str(), value.size());
    ::close(fd);
}

void exportGpio(int gpio)
{
    writeSysfsValue("/sys/class/gpio/export", std::to_string(gpio));
}

void setGpioDirection(int gpio, const std::string& direction)
{
    writeSysfsValue("/sys/class/gpio/gpio" + std::to_string(gpio) + "/direction", direction);
}

void setGpioValue(int gpio, bool high)
{
    writeSysfsValue("/sys/class/gpio/gpio" + std::to_string(gpio) + "/value", high ? "1" : "0");
}

bool activeMaskValue(bool active)
{
    return kMaskStartActiveLow ? !active : active;
}

int openCdce937Device(JNDM123Runtime::Cdce937Device& device)
{
    device.fd = ::open(device.path.c_str(), O_RDWR);
    return device.fd >= 0 ? 0 : -1;
}

void closeCdce937Device(JNDM123Runtime::Cdce937Device& device)
{
    if (device.fd >= 0)
    {
        ::close(device.fd);
        device.fd = -1;
    }
}

int i2cByteRead(int fd, std::uint8_t address, std::uint8_t reg, std::uint8_t& value)
{
    std::uint8_t cmd = static_cast<std::uint8_t>(0x80u | reg);
    struct i2c_msg messages[2];
    struct i2c_rdwr_ioctl_data ioctlData;

    messages[0].addr = address;
    messages[0].flags = 0;
    messages[0].len = 1;
    messages[0].buf = &cmd;

    messages[1].addr = address;
    messages[1].flags = I2C_M_RD;
    messages[1].len = 1;
    messages[1].buf = &value;

    ioctlData.msgs = messages;
    ioctlData.nmsgs = 2;
    return ::ioctl(fd, I2C_RDWR, &ioctlData) < 0 ? -1 : 0;
}

int i2cByteWrite(int fd, std::uint8_t address, std::uint8_t reg, std::uint8_t value)
{
    std::uint8_t buffer[2];
    struct i2c_msg message;
    struct i2c_rdwr_ioctl_data ioctlData;

    buffer[0] = static_cast<std::uint8_t>(0x80u | reg);
    buffer[1] = value;

    message.addr = address;
    message.flags = 0;
    message.len = sizeof(buffer);
    message.buf = buffer;

    ioctlData.msgs = &message;
    ioctlData.nmsgs = 1;
    return ::ioctl(fd, I2C_RDWR, &ioctlData) < 0 ? -1 : 0;
}

bool looksLikeCdce937(int fd, std::uint8_t address, std::uint8_t* idReg = nullptr)
{
    std::uint8_t id = 0;
    if (i2cByteRead(fd, address, kCdce937RegId, id) != 0) return false;
    if ((id & 0x0Fu) != 0x01u) return false;
    if (idReg) *idReg = id;
    return true;
}

void autodetectCdce937(JNDM123Runtime::Cdce937Device& device)
{
    if (device.addressFixed) return;

    for (const auto address: kProbeAddresses)
    {
        if (looksLikeCdce937(device.fd, address))
        {
            device.address = address;
            return;
        }
    }

    throw Poco::NotFoundException("No CDCE937 device found on the requested I2C bus.");
}

const char* inclkName(std::uint8_t value)
{
    switch (value)
    {
    case 0: return "crystal";
    case 1: return "vcxo";
    case 2: return "lvcmos";
    default: return "reserved";
    }
}

int readDividerRegister(JNDM123Runtime::Cdce937Device& device, int outputIndex)
{
    if (outputIndex >= 0 && outputIndex <= 2)
    {
        std::uint8_t reg02 = 0;
        std::uint8_t reg03 = 0;
        if (i2cByteRead(device.fd, device.address, 0x02, reg02) != 0) return 0;
        if (i2cByteRead(device.fd, device.address, 0x03, reg03) != 0) return 0;
        return static_cast<int>(((reg02 & 0x03u) << 8) | reg03);
    }

    const std::uint8_t registers[] = {0x26, 0x26, 0x26, 0x37};
    std::uint8_t value = 0;
    if (i2cByteRead(device.fd, device.address, registers[outputIndex - 3], value) != 0) return 0;
    return static_cast<int>(value & 0x7Fu);
}

void writeDividerRegister(JNDM123Runtime::Cdce937Device& device, int outputIndex, int divider)
{
    if (outputIndex >= 0 && outputIndex <= 2)
    {
        std::uint8_t reg02 = 0;
        if (i2cByteRead(device.fd, device.address, 0x02, reg02) != 0)
        {
            throw Poco::IOException("Unable to read CDCE937 register 0x02.");
        }

        reg02 = static_cast<std::uint8_t>((reg02 & 0xFCu) | ((divider >> 8) & 0x03u));
        if (i2cByteWrite(device.fd, device.address, 0x02, reg02) != 0 ||
            i2cByteWrite(device.fd, device.address, 0x03, static_cast<std::uint8_t>(divider & 0xFFu)) != 0)
        {
            throw Poco::IOException("Unable to write Y1 divider registers.");
        }
        return;
    }

    const std::uint8_t registers[] = {0x26, 0x26, 0x26, 0x37};
    std::uint8_t oldValue = 0;
    if (i2cByteRead(device.fd, device.address, registers[outputIndex - 3], oldValue) != 0)
    {
        throw Poco::IOException("Unable to read divider register.");
    }

    const std::uint8_t newValue = static_cast<std::uint8_t>((oldValue & 0x80u) | (divider & 0x7Fu));
    if (i2cByteWrite(device.fd, device.address, registers[outputIndex - 3], newValue) != 0)
    {
        throw Poco::IOException("Unable to write divider register.");
    }
}

void writeMaskedCdce937Register(JNDM123Runtime::Cdce937Device& device, std::uint8_t reg, std::uint8_t mask, std::uint8_t value, const std::string& errorMessage)
{
    std::uint8_t current = 0;
    if (i2cByteRead(device.fd, device.address, reg, current) != 0)
    {
        throw Poco::IOException(errorMessage + " (read failed).");
    }

    const std::uint8_t updated = static_cast<std::uint8_t>((current & ~mask) | (value & mask));
    if (updated == current) return;

    if (i2cByteWrite(device.fd, device.address, reg, updated) != 0)
    {
        throw Poco::IOException(errorMessage + " (write failed).");
    }
}

void forceOutputEnabledLocked(JNDM123Runtime::Cdce937Device& device, int outputIndex)
{
    switch (outputIndex)
    {
    case 0:
        writeMaskedCdce937Register(
            device,
            kCdce937RegY1Control,
            0x3Cu,
            0x3Cu,
            "Unable to force-enable Y1 output");
        break;
    case 1:
    case 2:
        writeMaskedCdce937Register(
            device,
            kCdce937RegMux1,
            0x0Fu,
            0x0Fu,
            "Unable to force-enable Y2/Y3 output group");
        break;
    case 3:
    case 4:
        writeMaskedCdce937Register(
            device,
            kCdce937RegMux2,
            0x0Fu,
            0x0Fu,
            "Unable to force-enable Y4/Y5 output group");
        break;
    case 5:
    case 6:
        writeMaskedCdce937Register(
            device,
            kCdce937RegMux3,
            0x0Fu,
            0x0Fu,
            "Unable to force-enable Y6/Y7 output group");
        break;
    default:
        throw Poco::InvalidArgumentException("Output index must be in 0..6.");
    }
}

void disablePllModeForOutputLocked(JNDM123Runtime::Cdce937Device& device, int outputIndex)
{
    switch (outputIndex)
    {
    case 0:
        writeMaskedCdce937Register(
            device,
            kCdce937RegY1Control,
            0x80u,
            0x00u,
            "Unable to route Y1 directly from the input clock");
        break;
    case 1:
        writeMaskedCdce937Register(
            device,
            kCdce937RegMux1,
            0xC0u,
            0x80u,
            "Unable to route Y2 to shared Pdiv1 in PLL bypass mode");
        break;
    case 2:
        writeMaskedCdce937Register(
            device,
            kCdce937RegMux1,
            0xB0u,
            0x80u,
            "Unable to route Y3 to shared Pdiv1 in PLL bypass mode");
        break;
    case 3:
        writeMaskedCdce937Register(
            device,
            kCdce937RegMux2,
            0xC0u,
            0xC0u,
            "Unable to bypass PLL routing for Y4");
        break;
    case 4:
        writeMaskedCdce937Register(
            device,
            kCdce937RegMux2,
            0xB0u,
            0x90u,
            "Unable to route Y5 to shared Pdiv4 in PLL bypass mode");
        break;
    case 5:
        writeMaskedCdce937Register(
            device,
            kCdce937RegMux3,
            0xC0u,
            0x80u,
            "Unable to route Y6 to shared Pdiv4 in PLL bypass mode");
        break;
    case 6:
        writeMaskedCdce937Register(
            device,
            kCdce937RegMux3,
            0xB0u,
            0xA0u,
            "Unable to bypass PLL routing for Y7");
        break;
    default:
        throw Poco::InvalidArgumentException("Output index must be in 0..6.");
    }
}

void writeOutputDividerLocked(JNDM123Runtime::Cdce937Device& device, int outputIndex, int divider)
{
    forceOutputEnabledLocked(device, outputIndex);
    disablePllModeForOutputLocked(device, outputIndex);
    writeDividerRegister(device, outputIndex, divider);
}

void JNDM123Runtime::disablePllModeLocked(Cdce937Device& device)
{
    writeMaskedCdce937Register(
        device,
        kCdce937RegCfg1,
        0x1Cu,
        0x08u,
        "Unable to switch CDCE937 input clock to external LVCMOS bypass mode");
}

DividerSnapshot JNDM123Runtime::initializeHardwareFromSavedConfigurationLocked()
{
    const SavedDividerConfiguration saved = loadSavedDividerConfiguration();
    if (!saved.hasAnyOutput)
    {
        throw Poco::NotFoundException("No valid saved divider configuration was found for Y1~Y6.");
    }

    Cdce937Device device;
    device.path = saved.devicePath.empty() ? kDefaultI2CDevice : saved.devicePath;

    if (openCdce937Device(device) != 0)
    {
        throw Poco::IOException("Unable to open I2C device " + device.path);
    }

    try
    {
        autodetectCdce937(device);
        disablePllModeLocked(device);

        for (std::size_t index = 0; index < saved.dividers.size(); ++index)
        {
            if (!saved.savedOutputs[index]) continue;
            validateDividerOrThrow(static_cast<int>(index), saved.dividers[index]);
            writeOutputDividerLocked(device, static_cast<int>(index), saved.dividers[index]);
        }
    }
    catch (...)
    {
        closeCdce937Device(device);
        throw;
    }

    closeCdce937Device(device);

    DividerSnapshot snapshot = readDividerStatusLocked(device.path, saved.referenceClockHz);
    snapshot.message = saved.hasAllOutputs
        ? "CDCE937 PLL bypass enabled and saved divider state restored for Y1~Y6."
        : "CDCE937 PLL bypass enabled and saved divider state restored for available outputs.";
    return snapshot;
}

void JNDM123Runtime::ensureMappedHardwareLocked()
{
    if (_hardware.memFd >= 0 && _hardware.fifoCtrl && _hardware.fifoData) return;

    _hardware.memFd = ::open("/dev/mem", O_RDWR | O_SYNC);
    if (_hardware.memFd < 0)
    {
        throw Poco::IOException("Unable to open /dev/mem");
    }

    _hardware.fifoCtrl = static_cast<volatile std::uint32_t*>(::mmap(
        nullptr, kMapSize, PROT_READ | PROT_WRITE, MAP_SHARED, _hardware.memFd, kFifoCtrlBase));
    _hardware.fifoData = static_cast<volatile std::uint32_t*>(::mmap(
        nullptr, kMapSize, PROT_READ, MAP_SHARED, _hardware.memFd, kFifoDataPort));

    if (_hardware.fifoCtrl == MAP_FAILED || _hardware.fifoData == MAP_FAILED)
    {
        releaseMappedHardwareLocked();
        throw Poco::IOException("Unable to map FIFO registers");
    }
}

void JNDM123Runtime::releaseMappedHardwareLocked()
{
    if (_hardware.fifoCtrl && _hardware.fifoCtrl != MAP_FAILED)
    {
        ::munmap((void*) _hardware.fifoCtrl, kMapSize);
    }
    if (_hardware.fifoData && _hardware.fifoData != MAP_FAILED)
    {
        ::munmap((void*) _hardware.fifoData, kMapSize);
    }
    if (_hardware.memFd >= 0)
    {
        ::close(_hardware.memFd);
    }

    _hardware.memFd = -1;
    _hardware.fifoCtrl = nullptr;
    _hardware.fifoData = nullptr;
    _hardware.gpioInitialized = false;
}

void JNDM123Runtime::initializeGpioLocked()
{
    if (_hardware.gpioInitialized) return;

    exportGpio(kGpioReset);
    for (std::size_t chip = 0; chip < kAd7606Count; ++chip)
    {
        exportGpio(kGpioMaskStart + static_cast<int>(chip));
    }

    ::usleep(100000);

    setGpioDirection(kGpioReset, "out");
    for (std::size_t chip = 0; chip < kAd7606Count; ++chip)
    {
        setGpioDirection(kGpioMaskStart + static_cast<int>(chip), "out");
    }

    _hardware.gpioInitialized = true;
}

void JNDM123Runtime::stopAdcLocked()
{
    setGpioValue(kGpioReset, true);
    for (std::size_t chip = 0; chip < kAd7606Count; ++chip)
    {
        setGpioValue(kGpioMaskStart + static_cast<int>(chip), activeMaskValue(false));
    }
}

void JNDM123Runtime::startAdcLocked()
{
    setGpioValue(kGpioReset, true);
    ::usleep(1000);

    for (std::size_t chip = 0; chip < kAd7606Count; ++chip)
    {
        setGpioValue(kGpioMaskStart + static_cast<int>(chip), activeMaskValue(true));
    }

    ::usleep(1000);
    setGpioValue(kGpioReset, false);
}

void JNDM123Runtime::resetFifoLocked()
{
    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
    _hardware.fifoCtrl[kRdfr / 4] = 0xA5u;
    ::usleep(1000);

    while (_hardware.fifoCtrl[kRdfo / 4] > 0)
    {
        (void)_hardware.fifoData[0];
    }

    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
}

void JNDM123Runtime::recoverRxFifo()
{
    if (!_hardware.fifoCtrl || !_hardware.fifoData) return;

    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
    _hardware.fifoCtrl[kRdfr / 4] = 0xA5u;
    ::usleep(1000);

    while (_hardware.fifoCtrl[kRdfo / 4] > 0)
    {
        (void)_hardware.fifoData[0];
    }

    _hardware.fifoCtrl[kSrr / 4] = 0xA5u;
    ::usleep(1000);
    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
}

int JNDM123Runtime::readOneFramePacket(std::array<std::uint32_t, kFrameWords>& frame)
{
    const std::uint32_t isr = _hardware.fifoCtrl[kIsr / 4];
    if (isr & (kIsrRpue | kIsrRpore | kIsrRpure))
    {
        return -2;
    }

    const std::uint32_t occupancy = _hardware.fifoCtrl[kRdfo / 4];
    if (occupancy == 0)
    {
        return 0;
    }

    const std::uint32_t rlr = _hardware.fifoCtrl[kRlr / 4];
    if (rlr & 0x80000000u)
    {
        return 0;
    }

    const std::uint32_t bytes = rlr & 0x007FFFFFu;
    if (bytes != kFrameWords * sizeof(std::uint32_t))
    {
        return -3;
    }

    for (std::size_t i = 0; i < frame.size(); ++i)
    {
        frame[i] = _hardware.fifoData[0];
    }

    return 1;
}

DividerSnapshot JNDM123Runtime::readDividerStatusLocked(const std::string& devicePath, Poco::UInt64 referenceClockHz)
{
    DividerSnapshot snapshot;
    snapshot.devicePath = devicePath.empty() ? kDefaultI2CDevice : devicePath;
    snapshot.referenceClockHz = referenceClockHz;

    Cdce937Device device;
    device.path = snapshot.devicePath;

    if (openCdce937Device(device) != 0)
    {
        throw Poco::IOException("Unable to open I2C device " + snapshot.devicePath);
    }

    try
    {
        autodetectCdce937(device);

        std::uint8_t reg0 = 0;
        std::uint8_t reg1 = 0;
        if (i2cByteRead(device.fd, device.address, kCdce937RegId, reg0) != 0 ||
            i2cByteRead(device.fd, device.address, kCdce937RegCfg1, reg1) != 0)
        {
            throw Poco::IOException("Unable to read CDCE937 status registers.");
        }

        snapshot.deviceType = (reg0 & 0x80u) ? "CDCE937" : "CDCEL937";
        snapshot.address = Poco::format("0x%02x", static_cast<unsigned>(device.address));
        snapshot.inputClock = inclkName((reg1 >> 2) & 0x03u);
        snapshot.revisionId = (reg0 >> 4) & 0x07u;
        snapshot.eepBusy = ((reg1 >> 6) & 0x01u) != 0;
        snapshot.eepLock = ((reg1 >> 5) & 0x01u) != 0;
        snapshot.powerDown = ((reg1 >> 4) & 0x01u) != 0;

        for (const auto& spec: kDividerOutputs)
        {
            DividerOutputState output;
            output.index = spec.index;
            output.name = spec.name;
            output.pdivName = spec.pdivName;
            output.pin = spec.pin;
            output.divider = readDividerRegister(device, spec.index);
            output.frequencyHz = output.divider > 0 ? (static_cast<double>(referenceClockHz) / static_cast<double>(output.divider)) : 0.0;
            snapshot.outputs.push_back(output);
        }

        snapshot.message = "Divider status synchronized from hardware.";
    }
    catch (...)
    {
        closeCdce937Device(device);
        throw;
    }

    closeCdce937Device(device);
    return snapshot;
}

DividerSnapshot JNDM123Runtime::applyDividerLocked(const std::string& devicePath, int outputIndex, int divider, Poco::UInt64 referenceClockHz)
{
    return applyDividersLocked(devicePath, std::vector<int>{outputIndex}, divider, referenceClockHz);
}

DividerSnapshot JNDM123Runtime::applyDividersLocked(const std::string& devicePath, const std::vector<int>& outputIndices, int divider, Poco::UInt64 referenceClockHz)
{
    const std::vector<int> normalizedOutputs = normalizeOutputIndicesOrThrow(outputIndices);

    Cdce937Device device;
    device.path = devicePath.empty() ? kDefaultI2CDevice : devicePath;

    if (openCdce937Device(device) != 0)
    {
        throw Poco::IOException("Unable to open I2C device " + device.path);
    }

    try
    {
        autodetectCdce937(device);
        disablePllModeLocked(device);
        for (const int outputIndex: normalizedOutputs)
        {
            writeOutputDividerLocked(device, outputIndex, divider);
        }
    }
    catch (...)
    {
        closeCdce937Device(device);
        throw;
    }

    closeCdce937Device(device);

    DividerSnapshot snapshot = readDividerStatusLocked(device.path, referenceClockHz);
    saveDividerConfiguration(snapshot, snapshot.devicePath);
    snapshot.message = "Divider applied to " + describeOutputs(normalizedOutputs) + " and read back from hardware.";
    return snapshot;
}

AcquisitionActionResult JNDM123Runtime::startAcquisitionLocked()
{
    AcquisitionActionResult result;
    if (_acquisitionRunning.load())
    {
        result.message = "Acquisition is already running.";
        setStatusMessage(result.message);
        return result;
    }

    try
    {
        const DividerSnapshot safetySnapshot = readDividerStatusLocked(resolvePreferredDevicePath(""), resolvePreferredReferenceClockHz(""));
        const std::string safetyMessage = acquisitionClockSafetyMessage(safetySnapshot);
        if (!safetyMessage.empty())
        {
            result.ok = false;
            result.message = safetyMessage;
            setStatusMessage(result.message);
            return result;
        }

        ensureMappedHardwareLocked();
        initializeGpioLocked();
        stopAdcLocked();
        resetFifoLocked();
        drainQueue();
        clearWaveformHistory();

        _readerShouldRun.store(true);
        startAdcLocked();
        _readerThread.start(_readerRunnable);
        _readerStarted = true;
        _acquisitionRunning.store(true);
        result.message = "Acquisition started.";
        setStatusMessage(result.message);
    }
    catch (const Poco::Exception& exc)
    {
        _readerShouldRun.store(false);
        result.ok = false;
        result.message = exc.displayText();
        recordError(result.message);
        setStatusMessage(result.message);
    }
    catch (const std::exception& exc)
    {
        _readerShouldRun.store(false);
        result.ok = false;
        result.message = exc.what();
        recordError(result.message);
        setStatusMessage(result.message);
    }

    return result;
}

AcquisitionActionResult JNDM123Runtime::stopAcquisitionLocked(const std::string& message, bool clearHistoryAfterStop)
{
    AcquisitionActionResult result;
    result.message = message;

    _readerShouldRun.store(false);
    if (_readerStarted)
    {
        _readerThread.join();
        _readerStarted = false;
    }

    if (_hardware.fifoCtrl && _hardware.fifoData)
    {
        stopAdcLocked();
        resetFifoLocked();
    }

    _acquisitionRunning.store(false);
    drainQueue();
    if (clearHistoryAfterStop)
    {
        clearWaveformHistory();
    }

    setStatusMessage(result.message);
    return result;
}

} // namespace
#endif

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

void initializeJNDM123Runtime(Poco::OSP::BundleContext::Ptr pContext)
{
    Poco::UInt64 referenceClockHz = kFallbackReferenceClockHz;
    runtimeBundleContextStorage() = pContext;

    if (pContext)
    {
        const std::string configuredReferenceClockHzText = pContext->thisBundle()->properties().getString("referenceClockHz", "");
        if (!configuredReferenceClockHzText.empty())
        {
            Poco::UInt64 parsedReferenceClockHz = 0;
            if (!parseUInt64Strict(configuredReferenceClockHzText, parsedReferenceClockHz))
            {
                pContext->logger().warning(
                    "Invalid JNDM123 bundle property referenceClockHz=" + configuredReferenceClockHzText +
                    ", using fallback " + std::to_string(kFallbackReferenceClockHz) + ".");
            }
            else
            {
                try
                {
                    validateReferenceClockHzOrThrow(parsedReferenceClockHz);
                    referenceClockHz = parsedReferenceClockHz;
                }
                catch (const Poco::Exception& exc)
                {
                    pContext->logger().warning(
                        "Invalid JNDM123 bundle property referenceClockHz=" + configuredReferenceClockHzText +
                        ": " + exc.displayText() + ". Using fallback " + std::to_string(kFallbackReferenceClockHz) + ".");
                }
            }
        }

        pContext->logger().information("JNDM123 configured reference clock: " + std::to_string(referenceClockHz) + " Hz.");
    }

    setConfiguredReferenceClockHz(referenceClockHz);
    JNDM123Runtime::instance().initializeFromSavedConfiguration();
}

void stopJNDM123Runtime()
{
    JNDM123Runtime::instance().shutdown();
    runtimeBundleContextStorage().reset();
}

DividerRequestHandler::DividerRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
    _pContext(pContext)
{
}

void DividerRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    if (!isAuthenticated(_pContext, request))
    {
        sendJSON(response, createUnauthorizedPayload("Authentication required for JNDM123 divider control."), Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        return;
    }

    try
    {
        Poco::Net::HTMLForm form(request, request.stream());
        const std::string requestedDevicePath = form.get("devicePath", "");
        const std::string devicePath = resolvePreferredDevicePath(requestedDevicePath);
        const Poco::UInt64 referenceClockHz = resolvePreferredReferenceClockHz(form.get("referenceClockHz", ""));

        if (Poco::icompare(request.getMethod(), std::string("POST")) == 0)
        {
            const bool hasDivider = form.has("divider");
            if (!hasDivider)
            {
                sendJSON(response, dividerSnapshotToJson(JNDM123Runtime::instance().updateReferenceClock(devicePath, referenceClockHz)));
                return;
            }

            int divider = 0;
            if (!parseIntStrict(form.get("divider", ""), divider))
            {
                sendJSON(response, createErrorPayload("divider is a required numeric value."), Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                return;
            }

            std::vector<int> outputIndices;
            const std::string outputIndicesText = form.get("outputIndices", "");
            if (!outputIndicesText.empty())
            {
                outputIndices = parseOutputIndexListOrThrow(outputIndicesText);
            }
            else
            {
                int outputIndex = 0;
                if (!parseIntStrict(form.get("outputIndex", ""), outputIndex))
                {
                    sendJSON(response, createErrorPayload("outputIndex or outputIndices is required."), Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                    return;
                }
                outputIndices.push_back(outputIndex);
            }

            sendJSON(response, dividerSnapshotToJson(JNDM123Runtime::instance().applyDividers(devicePath, outputIndices, divider, referenceClockHz)));
            return;
        }

        sendJSON(response, dividerSnapshotToJson(JNDM123Runtime::instance().readDividerStatus(devicePath)));
    }
    catch (const Poco::InvalidArgumentException& exc)
    {
        logger().warning("Divider request rejected: " + exc.displayText());
        sendJSON(response, createErrorPayload(exc.displayText()), Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
    catch (const Poco::Exception& exc)
    {
        logger().error("Divider request failed: " + exc.displayText());
        sendJSON(response, createErrorPayload(exc.displayText()), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (const std::exception& exc)
    {
        logger().error(std::string("Divider request failed: ") + exc.what());
        sendJSON(response, createErrorPayload(exc.what()), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

AcquisitionRequestHandler::AcquisitionRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
    _pContext(pContext)
{
}

void AcquisitionRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    if (!isAuthenticated(_pContext, request))
    {
        sendJSON(response, createUnauthorizedPayload("Authentication required for JNDM123 acquisition."), Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        return;
    }

    try
    {
        Poco::Net::HTMLForm form(request, request.stream());
        const bool includeWaveform = parseBoolValue(form.get("includeWaveform", "0"));
        if (includeWaveform)
        {
            JNDM123Runtime::instance().touchPreviewLease();
        }

        if (Poco::icompare(request.getMethod(), std::string("POST")) == 0)
        {
            const std::string action = form.get("action", "");
            if (action == "start")
            {
                const AcquisitionActionResult result = JNDM123Runtime::instance().startAcquisition();
                Poco::JSON::Object::Ptr payload = JNDM123Runtime::instance().acquisitionSnapshot(includeWaveform);
                payload->set("ok", result.ok);
                payload->set("message", result.message);
                sendJSON(response, payload, result.ok ? Poco::Net::HTTPResponse::HTTP_OK : Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
            if (action == "stop")
            {
                const AcquisitionActionResult result = JNDM123Runtime::instance().stopAcquisition();
                Poco::JSON::Object::Ptr payload = JNDM123Runtime::instance().acquisitionSnapshot(includeWaveform);
                payload->set("ok", result.ok);
                payload->set("message", result.message);
                sendJSON(response, payload);
                return;
            }
            if (action == "restart-process")
            {
                Object::Ptr payload = acquisitionServiceOrThrow()->restartProcess();
                if (!includeWaveform)
                {
                    stripWaveformSamples(payload);
                }
                sendJSON(
                    response,
                    payload,
                    payload->optValue("ok", true) ? Poco::Net::HTTPResponse::HTTP_OK : Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
        }

        sendJSON(response, JNDM123Runtime::instance().acquisitionSnapshot(includeWaveform));
    }
    catch (const Poco::Exception& exc)
    {
        logger().error("Acquisition request failed: " + exc.displayText());
        sendJSON(response, createErrorPayload(exc.displayText()), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (const std::exception& exc)
    {
        logger().error(std::string("Acquisition request failed: ") + exc.what());
        sendJSON(response, createErrorPayload(exc.what()), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

Poco::Net::HTTPRequestHandler* DividerRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new DividerRequestHandler(context());
}

Poco::Net::HTTPRequestHandler* AcquisitionRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new AcquisitionRequestHandler(context());
}

} } } // namespace MyIoT::WebUI::JNDM123
