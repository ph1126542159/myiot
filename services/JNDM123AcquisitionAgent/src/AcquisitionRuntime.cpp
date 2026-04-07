#include "AcquisitionRuntime.h"

#include "Poco/Exception.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/Stringifier.h"

#include <sstream>

#if defined(__linux__)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

using Poco::JSON::Array;
using Poco::JSON::Object;

namespace {

std::vector<int> parseOutputIndices(Object::Ptr payload)
{
    Array::Ptr outputIndices = payload ? payload->getArray("outputIndices") : nullptr;
    if (!outputIndices || outputIndices->size() == 0)
    {
        throw Poco::InvalidArgumentException("outputIndices must contain at least one output.");
    }

    std::vector<int> values;
    values.reserve(outputIndices->size());
    for (std::size_t index = 0; index < outputIndices->size(); ++index)
    {
        values.push_back(outputIndices->getElement<int>(index));
    }
    return normalizeOutputIndicesOrThrow(values);
}

Poco::UInt64 requestReferenceClockHz(Object::Ptr payload)
{
    return resolvePreferredReferenceClockHz(
        payload ? payload->optValue("referenceClockHz", std::string()) : std::string());
}

std::string requestDevicePath(Object::Ptr payload)
{
    return resolvePreferredDevicePath(
        payload ? payload->optValue("devicePath", std::string()) : std::string());
}

void attachDividerPayload(Object::Ptr payload, const std::string& dividerPayloadText)
{
    if (dividerPayloadText.empty()) return;

    try
    {
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var parsed = parser.parse(dividerPayloadText);
        payload->set("divider", parsed.extract<Object::Ptr>());
    }
    catch (...)
    {
    }
}

} // namespace

ReaderRunnable::ReaderRunnable(AcquisitionRuntime& owner): _owner(owner) {}
void ReaderRunnable::run() { _owner.readerLoop(); }

PublisherRunnable::PublisherRunnable(AcquisitionRuntime& owner): _owner(owner) {}
void PublisherRunnable::run() { _owner.publisherLoop(); }

ControlListener::ControlListener(AcquisitionRuntime& runtime): _runtime(runtime) {}
void ControlListener::onMessage(const AcquisitionDdsJsonMessage& message) { _runtime.handleControlMessage(message); }

AcquisitionRuntime::AcquisitionRuntime(int ddsDomain):
    _readerRunnable(*this),
    _publisherRunnable(*this),
    _ddsDomain(ddsDomain),
    _controlListener(*this)
{
    _publisher.start(_ddsDomain, acquisitionSnapshotTopicName());
    _commandSubscriber.start(_ddsDomain, acquisitionControlTopicName(), &_controlListener);
    _publisherThread.start(_publisherRunnable);
    _publisherStarted = true;
    publishSnapshot();
}

AcquisitionRuntime::~AcquisitionRuntime()
{
    shutdown();
}

Object::Ptr AcquisitionRuntime::startAcquisition()
{
    Poco::FastMutex::ScopedLock lock(_controlMutex);
    Object::Ptr payload = startAcquisitionLocked();
    publishSnapshot();
    return payload;
}

Object::Ptr AcquisitionRuntime::stopAcquisition(const std::string& message)
{
    Poco::FastMutex::ScopedLock lock(_controlMutex);
    stopAcquisitionLocked(message.empty() ? "Acquisition stopped by operator." : message);
    Object::Ptr payload = snapshot();
    publishSnapshot();
    return payload;
}

Object::Ptr AcquisitionRuntime::snapshot()
{
    std::array<std::int16_t, kFrameColumns> latestSamples{};
    std::array<std::deque<std::int16_t>, kFrameColumns> historyCopy;
    std::deque<Poco::Int64> timelineCopy;
    std::string lastFrameAt;
    std::string lastPublishedAt;
    std::string statusMessage;
    std::string lastError;
    std::string dividerPayloadText;
    bool hasLatestFrame = false;

    {
        Poco::FastMutex::ScopedLock lock(_stateMutex);
        latestSamples = _latestSamples;
        historyCopy = _history;
        timelineCopy = _timelineUs;
        lastFrameAt = _lastFrameAt;
        lastPublishedAt = _lastPublishedAt;
        statusMessage = _statusMessage.empty()
            ? (_running.load() ? "Acquisition running." : "Acquisition idle.")
            : _statusMessage;
        lastError = _lastError;
        dividerPayloadText = _latestDividerPayloadText;
        hasLatestFrame = _hasLatestFrame;
    }

    Object::Ptr payload = new Object;
    payload->set("authenticated", true);
    payload->set("ok", _lastCommandOk);
    payload->set("updatedAt", isoTimestamp());
    payload->set("message", statusMessage);
    payload->set("running", _running.load());
    payload->set("previewActive", true);
    payload->set("historyLimit", static_cast<int>(kHistoryLimit));
    payload->set("queueDepth", 0);
    payload->set("totalFrames", static_cast<Poco::UInt64>(_totalFrames.load()));
    payload->set("droppedFrames", static_cast<Poco::UInt64>(_droppedFrames.load()));
    payload->set("recoveries", static_cast<Poco::UInt64>(_recoveries.load()));
    payload->set("lastFrameSequence", static_cast<Poco::UInt64>(_lastFrameSequence.load()));
    payload->set("lastFrameAt", lastFrameAt);
    payload->set("waveformUpdatedAt", lastPublishedAt);
    payload->set("lastError", lastError);
    payload->set("includeWaveform", true);
    payload->set("commandSequence", static_cast<Poco::UInt64>(_lastCommandSequence.load()));
    payload->set("commandAction", _lastCommandAction);
    payload->set("commandUpdatedAt", _lastCommandUpdatedAt);
    attachDividerPayload(payload, dividerPayloadText);

    Array::Ptr timelineUs = new Array;
    for (const auto value: timelineCopy) timelineUs->add(static_cast<Poco::Int64>(value));
    payload->set("timelineUs", timelineUs);

    Array::Ptr chips = new Array;
    for (std::size_t chipIndex = 0; chipIndex < kAd7606Count; ++chipIndex)
    {
        Object::Ptr chip = new Object;
        chip->set("index", static_cast<int>(chipIndex));
        chip->set("name", "ADC" + std::to_string(chipIndex + 1));

        Array::Ptr channels = new Array;
        for (std::size_t channelIndex = 0; channelIndex < kChannelsPerChip; ++channelIndex)
        {
            const std::size_t sampleIndex = chipIndex * kChannelsPerChip + channelIndex;
            Object::Ptr channel = new Object;
            channel->set("index", static_cast<int>(channelIndex));
            channel->set("name", "CH" + std::to_string(channelIndex + 1));
            channel->set("hasValue", hasLatestFrame);
            channel->set("value", static_cast<int>(latestSamples[sampleIndex]));

            Array::Ptr samples = new Array;
            for (const auto sample: historyCopy[sampleIndex]) samples->add(static_cast<int>(sample));
            channel->set("samples", samples);
            channels->add(channel);
        }

        chip->set("channels", channels);
        chips->add(chip);
    }

    payload->set("chips", chips);
    return payload;
}

void AcquisitionRuntime::readerLoop()
{
#if defined(__linux__)
    std::array<std::uint32_t, kFrameWords> words{};
    while (_readerShouldRun.load() && !_shutdown.load())
    {
        const int result = readOneFramePacket(words);
        if (result == 0)
        {
            ::usleep(1000);
            continue;
        }
        if (result < 0)
        {
            ++_recoveries;
            _lastError = "RX FIFO recovered after a packet error.";
            recoverRxFifo();
            ::usleep(1000);
            continue;
        }

        std::array<std::int16_t, kFrameColumns> samples{};
        unpackFrame(words, samples);
        const Poco::Timestamp now;
        const Poco::Int64 capturedAtUs = now.epochMicroseconds();

        {
            Poco::FastMutex::ScopedLock lock(_stateMutex);
            _latestSamples = samples;
            _hasLatestFrame = true;
            appendHistoryLocked(samples, capturedAtUs);
            _lastFrameAt = isoTimestamp(now);
        }

        _totalFrames.fetch_add(1);
        _lastFrameSequence.store(_totalFrames.load());
    }
#endif
}

void AcquisitionRuntime::publisherLoop()
{
    while (!_shutdown.load())
    {
        _publishWake.tryWait(1000);
        if (_shutdown.load()) break;
        publishSnapshot();
    }
}

void AcquisitionRuntime::shutdown()
{
    const bool alreadyShuttingDown = _shutdown.exchange(true);
    if (alreadyShuttingDown) return;

    {
        Poco::FastMutex::ScopedLock lock(_controlMutex);
        stopAcquisitionLocked("Acquisition stopped during process shutdown.");
    }

    _publishWake.set();
    if (_publisherStarted)
    {
        _publisherThread.join();
        _publisherStarted = false;
    }
    _publisher.stop();
    _commandSubscriber.stop();

#if defined(__linux__)
    releaseMappedHardwareLocked();
#endif
}

void AcquisitionRuntime::handleControlMessage(const AcquisitionDdsJsonMessage& message)
{
    std::string action("status");
    std::string requestMessage;
    Object::Ptr requestPayload = new Object;

    try
    {
        if (!message.payload.empty())
        {
            Poco::JSON::Parser parser;
            Poco::Dynamic::Var parsed = parser.parse(message.payload);
            requestPayload = parsed.extract<Object::Ptr>();
            action = requestPayload->optValue("action", std::string("status"));
            requestMessage = requestPayload->optValue("message", std::string());
        }
    }
    catch (const Poco::Exception& exc)
    {
        Poco::FastMutex::ScopedLock lock(_stateMutex);
        _lastError = "Invalid DDS acquisition command payload: " + exc.displayText();
        _statusMessage = _lastError;
        _lastCommandOk = false;
    }

    {
        Poco::FastMutex::ScopedLock lock(_stateMutex);
        _lastCommandSequence.store(message.sequence);
        _lastCommandAction = action;
        _lastCommandUpdatedAt = message.updatedAt;
    }

    try
    {
        if (action == "start")
        {
            startAcquisition();
            return;
        }

        if (action == "stop")
        {
            stopAcquisition(requestMessage);
            return;
        }

        if (action == "divider-status" || action == "divider-update-reference")
        {
            const DividerSnapshot dividerSnapshot =
                readDividerStatus(requestDevicePath(requestPayload), requestReferenceClockHz(requestPayload));
            if (action == "divider-update-reference")
            {
                saveDividerConfiguration(dividerSnapshot, dividerSnapshot.devicePath);
            }

            {
                Poco::FastMutex::ScopedLock lock(_stateMutex);
                updateDividerSnapshotLocked(dividerSnapshot);
                _statusMessage = dividerSnapshot.message;
                _lastError.clear();
                _lastCommandOk = dividerSnapshot.ok;
            }

            publishSnapshot();
            return;
        }

        if (action == "divider-init")
        {
            const DividerSnapshot dividerSnapshot = initializeHardwareFromSavedConfiguration();
            {
                Poco::FastMutex::ScopedLock lock(_stateMutex);
                updateDividerSnapshotLocked(dividerSnapshot);
                _statusMessage = dividerSnapshot.message;
                _lastError.clear();
                _lastCommandOk = dividerSnapshot.ok;
            }

            publishSnapshot();
            return;
        }

        if (action == "divider-apply")
        {
            Poco::FastMutex::ScopedLock lock(_controlMutex);
            const bool wasRunning = _running.load();
            if (wasRunning)
            {
                stopAcquisitionLocked("Acquisition paused for divider update.");
            }

            const DividerSnapshot dividerSnapshot = applyDividers(
                requestDevicePath(requestPayload),
                parseOutputIndices(requestPayload),
                requestPayload->getValue<int>("divider"),
                requestReferenceClockHz(requestPayload));

            updateDividerSnapshotLocked(dividerSnapshot);
            _statusMessage = dividerSnapshot.message;
            _lastError.clear();
            _lastCommandOk = dividerSnapshot.ok;

            if (wasRunning)
            {
                const std::string safetyMessage = acquisitionClockSafetyMessage(dividerSnapshot);
                if (!safetyMessage.empty())
                {
                    _statusMessage += " " + safetyMessage;
                    _lastError = safetyMessage;
                    _lastCommandOk = false;
                }
                else
                {
                    Object::Ptr restartPayload = startAcquisitionLocked();
                    _lastCommandOk = restartPayload->optValue("ok", false);
                    _statusMessage = restartPayload->optValue("message", _statusMessage);
                }
            }

            publishSnapshot();
            return;
        }
    }
    catch (const Poco::Exception& exc)
    {
        Poco::FastMutex::ScopedLock lock(_stateMutex);
        _statusMessage = exc.displayText();
        _lastError = _statusMessage;
        _lastCommandOk = false;
    }
    catch (const std::exception& exc)
    {
        Poco::FastMutex::ScopedLock lock(_stateMutex);
        _statusMessage = exc.what();
        _lastError = _statusMessage;
        _lastCommandOk = false;
    }

    publishSnapshot();
}

void AcquisitionRuntime::publishSnapshot()
{
    Object::Ptr payload = snapshot();
    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(payload, buffer);

    AcquisitionDdsJsonMessage message;
    message.sequence = _lastFrameSequence.load();
    message.updatedAt = payload->getValue<std::string>("updatedAt");
    message.payload = buffer.str();
    _publisher.publish(message);

    Poco::FastMutex::ScopedLock lock(_stateMutex);
    _lastPublishedAt = message.updatedAt;
}

void AcquisitionRuntime::stopAcquisitionLocked(const std::string& message)
{
    _readerShouldRun.store(false);
    if (_readerStarted)
    {
        _readerThread.join();
        _readerStarted = false;
    }

#if defined(__linux__)
    if (_hardware.fifoCtrl && _hardware.fifoData)
    {
        stopAdcLocked();
        resetFifoLocked();
    }
#endif

    _running.store(false);
    _statusMessage = message;
    _lastCommandOk = true;
}

Object::Ptr AcquisitionRuntime::startAcquisitionLocked()
{
    if (_running.load())
    {
        Object::Ptr payload = snapshot();
        payload->set("ok", true);
        payload->set("message", "Acquisition is already running.");
        _statusMessage = "Acquisition is already running.";
        _lastError.clear();
        _lastCommandOk = true;
        return payload;
    }

    try
    {
        const DividerSnapshot dividerSnapshot =
            readDividerStatus(resolvePreferredDevicePath(std::string()), resolvePreferredReferenceClockHz(std::string()));
        updateDividerSnapshotLocked(dividerSnapshot);

        const std::string safetyMessage = acquisitionClockSafetyMessage(dividerSnapshot);
        if (!safetyMessage.empty())
        {
            _statusMessage = safetyMessage;
            _lastError = safetyMessage;
            _lastCommandOk = false;
            Object::Ptr payload = snapshot();
            payload->set("ok", false);
            payload->set("message", safetyMessage);
            return payload;
        }

#if defined(__linux__)
        ensureMappedHardwareLocked();
        initializeGpioLocked();
        stopAdcLocked();
        resetFifoLocked();
        clearHistoryLocked();
        _readerShouldRun.store(true);
        _readerThread.start(_readerRunnable);
        _readerStarted = true;
        startAdcLocked();
        _running.store(true);
        _statusMessage = "Acquisition started.";
        _lastError.clear();
        _lastCommandOk = true;
#else
        _statusMessage = "Acquisition is only supported on Linux.";
        _lastError = _statusMessage;
        _lastCommandOk = false;
#endif
    }
    catch (const Poco::Exception& exc)
    {
        _readerShouldRun.store(false);
        _statusMessage = exc.displayText();
        _lastError = _statusMessage;
        _lastCommandOk = false;
    }
    catch (const std::exception& exc)
    {
        _readerShouldRun.store(false);
        _statusMessage = exc.what();
        _lastError = _statusMessage;
        _lastCommandOk = false;
    }

    Object::Ptr payload = snapshot();
    payload->set("ok", _lastCommandOk);
    payload->set("message", _statusMessage);
    return payload;
}

void AcquisitionRuntime::updateDividerSnapshotLocked(const DividerSnapshot& snapshot)
{
    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(dividerSnapshotToJson(snapshot), buffer);
    _latestDividerPayloadText = buffer.str();
}

void AcquisitionRuntime::appendHistoryLocked(
        const std::array<std::int16_t, kFrameColumns>& samples,
        Poco::Int64 capturedAtUs)
{
    _timelineUs.push_back(capturedAtUs);
    while (_timelineUs.size() > kHistoryLimit) _timelineUs.pop_front();

    for (std::size_t index = 0; index < samples.size(); ++index)
    {
        auto& series = _history[index];
        series.push_back(samples[index]);
        while (series.size() > kHistoryLimit) series.pop_front();
    }
}

void AcquisitionRuntime::clearHistoryLocked()
{
    for (auto& series: _history) series.clear();
    _timelineUs.clear();
    _hasLatestFrame = false;
    _lastFrameAt.clear();
}

void AcquisitionRuntime::unpackFrame(
        const std::array<std::uint32_t, kFrameWords>& words,
        std::array<std::int16_t, kFrameColumns>& samples) const
{
    for (std::size_t index = 0; index < words.size(); ++index)
    {
        const std::uint32_t word = words[index];
        samples[index * 2] = static_cast<std::int16_t>(word & 0xFFFFu);
        samples[index * 2 + 1] = static_cast<std::int16_t>((word >> 16) & 0xFFFFu);
    }
}

#if defined(__linux__)
void AcquisitionRuntime::ensureMappedHardwareLocked()
{
    if (_hardware.memFd >= 0 && _hardware.fifoCtrl && _hardware.fifoData) return;

    _hardware.memFd = ::open("/dev/mem", O_RDWR | O_SYNC);
    if (_hardware.memFd < 0) throw Poco::IOException("Unable to open /dev/mem");

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

void AcquisitionRuntime::releaseMappedHardwareLocked()
{
    if (_hardware.fifoCtrl && _hardware.fifoCtrl != MAP_FAILED) ::munmap((void*) _hardware.fifoCtrl, kMapSize);
    if (_hardware.fifoData && _hardware.fifoData != MAP_FAILED) ::munmap((void*) _hardware.fifoData, kMapSize);
    if (_hardware.memFd >= 0) ::close(_hardware.memFd);
    _hardware.memFd = -1;
    _hardware.fifoCtrl = nullptr;
    _hardware.fifoData = nullptr;
    _hardware.gpioInitialized = false;
}

void AcquisitionRuntime::initializeGpioLocked()
{
    if (_hardware.gpioInitialized) return;

    exportGpio(kGpioReset);
    for (std::size_t chip = 0; chip < kAd7606Count; ++chip) exportGpio(kGpioMaskStart + static_cast<int>(chip));

    ::usleep(100000);
    setGpioDirection(kGpioReset, "out");
    for (std::size_t chip = 0; chip < kAd7606Count; ++chip)
    {
        setGpioDirection(kGpioMaskStart + static_cast<int>(chip), "out");
    }
    _hardware.gpioInitialized = true;
}

void AcquisitionRuntime::stopAdcLocked()
{
    setGpioValue(kGpioReset, true);
    for (std::size_t chip = 0; chip < kAd7606Count; ++chip)
    {
        setGpioValue(kGpioMaskStart + static_cast<int>(chip), activeMaskValue(false));
    }
}

void AcquisitionRuntime::startAdcLocked()
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

void AcquisitionRuntime::resetFifoLocked()
{
    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
    _hardware.fifoCtrl[kRdfr / 4] = 0xA5u;
    ::usleep(1000);
    while (_hardware.fifoCtrl[kRdfo / 4] > 0) (void) _hardware.fifoData[0];
    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
}

void AcquisitionRuntime::recoverRxFifo()
{
    if (!_hardware.fifoCtrl || !_hardware.fifoData) return;

    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
    _hardware.fifoCtrl[kRdfr / 4] = 0xA5u;
    ::usleep(1000);
    while (_hardware.fifoCtrl[kRdfo / 4] > 0) (void) _hardware.fifoData[0];
    _hardware.fifoCtrl[kSrr / 4] = 0xA5u;
    ::usleep(1000);
    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
}

int AcquisitionRuntime::readOneFramePacket(std::array<std::uint32_t, kFrameWords>& frame)
{
    const std::uint32_t isr = _hardware.fifoCtrl[kIsr / 4];
    if (isr & (kIsrRpue | kIsrRpore | kIsrRpure)) return -2;

    const std::uint32_t occupancy = _hardware.fifoCtrl[kRdfo / 4];
    if (occupancy == 0) return 0;

    const std::uint32_t rlr = _hardware.fifoCtrl[kRlr / 4];
    if (rlr & 0x80000000u) return 0;

    const std::uint32_t bytes = rlr & 0x007FFFFFu;
    if (bytes != kFrameWords * sizeof(std::uint32_t)) return -3;

    for (std::size_t index = 0; index < frame.size(); ++index) frame[index] = _hardware.fifoData[0];
    return 1;
}
#endif

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
