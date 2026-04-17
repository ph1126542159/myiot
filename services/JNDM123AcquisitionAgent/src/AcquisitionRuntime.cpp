#include "AcquisitionRuntime.h"

#include "Poco/Exception.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/Stringifier.h"

#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

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

std::string requestStringField(Object::Ptr payload, const std::string& name)
{
    if (!payload || !payload->has(name)) return std::string();

    try
    {
        return payload->getValue<std::string>(name);
    }
    catch (...)
    {
    }

    try
    {
        return payload->get(name).convert<std::string>();
    }
    catch (...)
    {
    }

    return std::string();
}

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
    if (!payload || !payload->has("referenceClockHz"))
    {
        return resolvePreferredReferenceClockHz(std::string());
    }

    try
    {
        const Poco::UInt64 value = payload->getValue<Poco::UInt64>("referenceClockHz");
        validateReferenceClockHzOrThrow(value);
        return value;
    }
    catch (...)
    {
    }

    const std::string text = requestStringField(payload, "referenceClockHz");
    if (!text.empty())
    {
        return resolvePreferredReferenceClockHz(text);
    }

    throw Poco::InvalidArgumentException("referenceClockHz must be numeric.");
}

std::string requestDevicePath(Object::Ptr payload)
{
    return resolvePreferredDevicePath(requestStringField(payload, "devicePath"));
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

template <typename Value>
Array::Ptr toJsonArray(const Value* values, std::size_t count)
{
    Array::Ptr array = new Array;
    for (std::size_t index = 0; index < count; ++index)
    {
        array->add(static_cast<Poco::UInt64>(values[index]));
    }
    return array;
}

Array::Ptr toSignedJsonArray(const std::uint16_t* values, std::size_t count)
{
    Array::Ptr array = new Array;
    for (std::size_t index = 0; index < count; ++index)
    {
        array->add(static_cast<int>(static_cast<std::int16_t>(values[index])));
    }
    return array;
}

Array::Ptr toHexJsonArray(const std::uint32_t* values, std::size_t count)
{
    Array::Ptr array = new Array;
    for (std::size_t index = 0; index < count; ++index)
    {
        std::ostringstream stream;
        stream << "0x"
               << std::uppercase
               << std::hex
               << std::setw(8)
               << std::setfill('0')
               << values[index];
        array->add(stream.str());
    }
    return array;
}

void unpackFrameLoHiPreview(
    const std::array<std::uint32_t, kFrameWords>& words,
    std::array<std::uint16_t, 8>& preview)
{
    preview.fill(0);
    const std::size_t pairCount = std::min<std::size_t>(words.size(), preview.size() / 2);
    for (std::size_t index = 0; index < pairCount; ++index)
    {
        const std::uint32_t word = words[index];
        preview[index * 2] = static_cast<std::uint16_t>(word & 0xFFFFu);
        preview[index * 2 + 1] = static_cast<std::uint16_t>((word >> 16) & 0xFFFFu);
    }
}

void copyHiLoPreview(
    const std::array<std::int16_t, kFrameColumns>& samples,
    std::array<std::uint16_t, 8>& preview)
{
    preview.fill(0);
    const std::size_t count = std::min(preview.size(), samples.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        preview[index] = static_cast<std::uint16_t>(samples[index]);
    }
}

std::string buildDebugSummary(
    Poco::UInt64 sequence,
    const std::array<std::uint32_t, 4>& rawWords,
    const std::array<std::uint16_t, 8>& hiLo,
    const std::array<std::uint16_t, 8>& loHi)
{
    std::ostringstream stream;
    stream << "seq=" << sequence << " raw=[";
    for (std::size_t index = 0; index < rawWords.size(); ++index)
    {
        if (index > 0) stream << ", ";
        stream << "0x"
               << std::uppercase
               << std::hex
               << std::setw(8)
               << std::setfill('0')
               << rawWords[index];
    }
    stream << std::dec << "] hiLo=[";
    for (std::size_t index = 0; index < hiLo.size(); ++index)
    {
        if (index > 0) stream << ", ";
        stream << hiLo[index];
    }
    stream << "] loHi=[";
    for (std::size_t index = 0; index < loHi.size(); ++index)
    {
        if (index > 0) stream << ", ";
        stream << loHi[index];
    }
    stream << ']';
    return stream.str();
}

struct FramePacket
{
    Poco::UInt64 sequence = 0;
    Poco::Timestamp capturedAt;
    std::array<std::uint32_t, kFrameWords> words{};
};

class FrameBatchNotification: public Poco::Notification
{
public:
    explicit FrameBatchNotification(std::vector<FramePacket> frames):
        _frames(std::move(frames))
    {
    }

    const std::vector<FramePacket>& frames() const
    {
        return _frames;
    }

private:
    std::vector<FramePacket> _frames;
};

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
    AcquisitionUdpStreamSettings udpSettings;
    udpSettings.enabled = parseBoolEnv("MYIOT_JNDM123_AGENT_UDP_ENABLE", true);
    udpSettings.host = parseStringEnv("MYIOT_JNDM123_AGENT_UDP_HOST", "192.168.16.255");
    udpSettings.port = parseIntEnv("MYIOT_JNDM123_AGENT_UDP_PORT", 50000);
    udpSettings.maxFramesPerPacket = parseIntEnv("MYIOT_JNDM123_AGENT_UDP_MAX_FRAMES_PER_PACKET", 10);

    _udpConfigured = udpSettings.enabled;
    _udpHost = udpSettings.host;
    _udpPort = udpSettings.port;
    _udpMaxFramesPerPacket = udpSettings.maxFramesPerPacket;
    _udpPublisher.reset(new AcquisitionUdpStreamPublisher(udpSettings));

    _publisher.start(_ddsDomain, acquisitionSnapshotTopicName());
    _commandSubscriber.start(_ddsDomain, acquisitionControlTopicName(), &_controlListener);
    _publisherThread.start(_publisherRunnable);
    _publisherStarted = true;
    logger().information(
        std::string("JNDM123 UDP stream ") +
        (_udpPublisher && _udpPublisher->enabled() ? "enabled" : "disabled") +
        " target=" + _udpHost + ":" + std::to_string(_udpPort) +
        " framesPerPacket=" + std::to_string(_udpMaxFramesPerPacket));
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
    std::string udpHost;
    std::string udpLastError;
    int udpPort = 0;
    int udpMaxFramesPerPacket = 0;
    bool udpConfigured = false;
    bool udpActive = false;
    std::array<std::uint32_t, 4> debugWords{};
    std::array<std::uint16_t, 8> debugHiLo{};
    std::array<std::uint16_t, 8> debugLoHi{};
    Poco::UInt64 debugSequence = 0;
    std::string debugCapturedAt;
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
        udpConfigured = _udpConfigured;
        udpActive = _udpPublisher && _udpPublisher->enabled();
        udpHost = _udpHost;
        udpPort = _udpPort;
        udpMaxFramesPerPacket = _udpMaxFramesPerPacket;
        udpLastError = _udpLastError;
        debugWords = _lastDebugWords;
        debugHiLo = _lastDebugHiLo;
        debugLoHi = _lastDebugLoHi;
        debugSequence = _lastDebugSequence;
        debugCapturedAt = _lastDebugCapturedAt;
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
    payload->set("queueDepth", static_cast<Poco::UInt64>(_queueDepth.load()));
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

    Object::Ptr udp = new Object;
    udp->set("configured", udpConfigured);
    udp->set("active", udpActive);
    udp->set("host", udpHost);
    udp->set("port", udpPort);
    udp->set("maxFramesPerPacket", udpMaxFramesPerPacket);
    udp->set("packetsSent", static_cast<Poco::UInt64>(_udpPacketsSent.load()));
    udp->set("framesSent", static_cast<Poco::UInt64>(_udpFramesSent.load()));
    udp->set("sendErrors", static_cast<Poco::UInt64>(_udpSendErrors.load()));
    udp->set("lastError", udpLastError);
    payload->set("udp", udp);

    Object::Ptr debug = new Object;
    debug->set("sequence", debugSequence);
    debug->set("capturedAt", debugCapturedAt);
    debug->set("rawWordsHex", toHexJsonArray(debugWords.data(), debugWords.size()));
    debug->set("hiLoUnsigned", toJsonArray(debugHiLo.data(), debugHiLo.size()));
    debug->set("hiLoSigned", toSignedJsonArray(debugHiLo.data(), debugHiLo.size()));
    debug->set("loHiUnsigned", toJsonArray(debugLoHi.data(), debugLoHi.size()));
    debug->set("loHiSigned", toSignedJsonArray(debugLoHi.data(), debugLoHi.size()));
    payload->set("debug", debug);

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
    elevateCurrentThreadPriority();

    std::array<std::uint32_t, kFrameWords> words{};
    std::vector<FramePacket> batch;
    Poco::UInt64 sequence = _totalFrames.load();
    Poco::Int64 batchStartedAtUs = 0;

    auto flushBatch = [&]() {
        if (batch.empty()) return;

        const Poco::UInt64 batchSize = static_cast<Poco::UInt64>(batch.size());
        if ((_queueDepth.load() + batchSize) > kMaxPendingFrames)
        {
            _droppedFrames.fetch_add(batchSize);
            batch.clear();
            batchStartedAtUs = 0;
            return;
        }

        _frameQueue.enqueueNotification(new FrameBatchNotification(std::move(batch)));
        _queueDepth.fetch_add(batchSize);
        batch = std::vector<FramePacket>();
        batchStartedAtUs = 0;
    };

    while (_readerShouldRun.load() && !_shutdown.load())
    {
        ::usleep(static_cast<useconds_t>(kReaderDrainIntervalUs));

        Poco::Timestamp loopTimestamp;
        loopTimestamp.update();
        const Poco::Int64 loopNowUs = loopTimestamp.epochMicroseconds();
        if (batchStartedAtUs != 0 && (loopNowUs - batchStartedAtUs) >= kFrameBatchWindowUs)
        {
            flushBatch();
        }

        while (_readerShouldRun.load() && !_shutdown.load())
        {
            const int result = readOneFramePacket(words);
            if (result == 0)
            {
                break;
            }
            if (result < 0)
            {
                ++_recoveries;
                _lastError = "RX FIFO recovered after a packet error.";
                recoverRxFifo();
                break;
            }

            Poco::Timestamp capturedAt;
            capturedAt.update();
            FramePacket packet;
            packet.sequence = sequence;
            packet.capturedAt = capturedAt;
            packet.words = words;
            batch.push_back(std::move(packet));
            if (batchStartedAtUs == 0)
            {
                batchStartedAtUs = capturedAt.epochMicroseconds();
            }

            ++sequence;

            const Poco::Int64 batchAgeUs = capturedAt.epochMicroseconds() - batchStartedAtUs;
            if (batchAgeUs >= kFrameBatchWindowUs)
            {
                flushBatch();
            }
        }
    }

    flushBatch();
#endif
}

void AcquisitionRuntime::publisherLoop()
{
    while (true)
    {
        if (_shutdown.load() && _queueDepth.load() == 0)
        {
            break;
        }

        Poco::AutoPtr<Poco::Notification> notification(_frameQueue.waitDequeueNotification(200));
        if (!notification)
        {
            if (_shutdown.load()) continue;
            continue;
        }

        FrameBatchNotification* pBatchNotification = dynamic_cast<FrameBatchNotification*>(notification.get());
        if (!pBatchNotification) continue;

        const std::vector<FramePacket>& frames = pBatchNotification->frames();
        if (frames.empty()) continue;

        const Poco::UInt64 batchSize = static_cast<Poco::UInt64>(frames.size());
        if (_queueDepth.load() >= batchSize)
        {
            _queueDepth.fetch_sub(batchSize);
        }
        else
        {
            _queueDepth.store(0);
        }

        std::array<std::int16_t, kFrameColumns> samples{};
        std::array<std::uint16_t, 8> debugHiLo{};
        std::array<std::uint16_t, 8> debugLoHi{};
        std::array<std::uint32_t, 4> debugWords{};
        Poco::UInt64 lastSequence = 0;
        std::string lastCapturedAt;
        Poco::Int64 lastCapturedAtUs = 0;
        bool shouldLogDebug = false;
        AcquisitionUdpStreamPublisher* udpPublisher = _udpPublisher.get();

        {
            Poco::FastMutex::ScopedLock lock(_stateMutex);
            for (const auto& packet: frames)
            {
                unpackFrame(packet.words, samples);
                if (udpPublisher && udpPublisher->enabled())
                {
                    udpPublisher->pushFrame(packet.sequence, packet.capturedAt.epochMicroseconds(), samples);
                }
                copyHiLoPreview(samples, debugHiLo);
                unpackFrameLoHiPreview(packet.words, debugLoHi);
                for (std::size_t index = 0; index < debugWords.size(); ++index)
                {
                    debugWords[index] = packet.words[index];
                }

                lastSequence = packet.sequence;
                lastCapturedAtUs = packet.capturedAt.epochMicroseconds();
                lastCapturedAt = isoTimestamp(packet.capturedAt);

                _latestSamples = samples;
                _lastDebugWords = debugWords;
                _lastDebugHiLo = debugHiLo;
                _lastDebugLoHi = debugLoHi;
                _lastDebugSequence = packet.sequence;
                _lastDebugCapturedAt = lastCapturedAt;
                _hasLatestFrame = true;
                _lastFrameSequence.store(packet.sequence);
                _totalFrames.store(packet.sequence + 1);
                _lastFrameAt = lastCapturedAt;

                const bool shouldAppendHistory =
                    (_lastHistorySampleAtUs == 0) ||
                    ((lastCapturedAtUs - _lastHistorySampleAtUs) >= kHistorySampleIntervalUs);
                if (shouldAppendHistory)
                {
                    appendHistoryLocked(samples, lastCapturedAtUs);
                    _lastHistorySampleAtUs = lastCapturedAtUs;
                }
            }

            _lastWaveformPublishAtUs = lastCapturedAtUs;
            ++_debugPublishCount;
            shouldLogDebug = (_debugPublishCount <= 3) || (_debugPublishCount % 100 == 0);
        }

        if (udpPublisher && udpPublisher->enabled())
        {
            udpPublisher->flush();
            const AcquisitionUdpStreamStats& stats = udpPublisher->stats();
            _udpPacketsSent.store(stats.packetsSent);
            _udpFramesSent.store(stats.framesSent);
            _udpSendErrors.store(stats.sendErrors);

            Poco::FastMutex::ScopedLock lock(_stateMutex);
            _udpLastError = stats.lastError;
        }

        if (shouldLogDebug)
        {
            logger().information(
                "Waveform publish debug: frames=" + std::to_string(batchSize) + " " +
                buildDebugSummary(lastSequence, debugWords, debugHiLo, debugLoHi));
        }
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
        _lastCommandSequence.store(message.sequence);
        _lastCommandAction = action;
        _lastCommandUpdatedAt = message.updatedAt;
        _lastError = "Invalid DDS acquisition command payload: " + exc.displayText();
        _statusMessage = _lastError;
        _lastCommandOk = false;
    }

    try
    {
        if (action == "start")
        {
            {
                Poco::FastMutex::ScopedLock lock(_controlMutex);
                startAcquisitionLocked();
            }
            {
                Poco::FastMutex::ScopedLock lock(_stateMutex);
                _lastCommandSequence.store(message.sequence);
                _lastCommandAction = action;
                _lastCommandUpdatedAt = message.updatedAt;
            }
            publishSnapshot();
            return;
        }

        if (action == "stop")
        {
            {
                Poco::FastMutex::ScopedLock lock(_controlMutex);
                stopAcquisitionLocked(requestMessage);
            }
            {
                Poco::FastMutex::ScopedLock lock(_stateMutex);
                _lastCommandSequence.store(message.sequence);
                _lastCommandAction = action;
                _lastCommandUpdatedAt = message.updatedAt;
                _lastCommandOk = true;
            }
            publishSnapshot();
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
                _lastCommandSequence.store(message.sequence);
                _lastCommandAction = action;
                _lastCommandUpdatedAt = message.updatedAt;
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
                _lastCommandSequence.store(message.sequence);
                _lastCommandAction = action;
                _lastCommandUpdatedAt = message.updatedAt;
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

            _lastCommandSequence.store(message.sequence);
            _lastCommandAction = action;
            _lastCommandUpdatedAt = message.updatedAt;
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
        _lastCommandSequence.store(message.sequence);
        _lastCommandAction = action;
        _lastCommandUpdatedAt = message.updatedAt;
        _statusMessage = exc.displayText();
        _lastError = _statusMessage;
        _lastCommandOk = false;
    }
    catch (const std::exception& exc)
    {
        Poco::FastMutex::ScopedLock lock(_stateMutex);
        _lastCommandSequence.store(message.sequence);
        _lastCommandAction = action;
        _lastCommandUpdatedAt = message.updatedAt;
        _statusMessage = exc.what();
        _lastError = _statusMessage;
        _lastCommandOk = false;
    }

    {
        Poco::FastMutex::ScopedLock lock(_stateMutex);
        if (_lastCommandSequence.load() != message.sequence)
        {
            _lastCommandSequence.store(message.sequence);
            _lastCommandAction = action;
            _lastCommandUpdatedAt = message.updatedAt;
        }
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

void AcquisitionRuntime::drainQueue()
{
    while (true)
    {
        Poco::AutoPtr<Poco::Notification> notification(_frameQueue.dequeueNotification());
        if (!notification) break;
    }
    _queueDepth.store(0);
}

void AcquisitionRuntime::stopAcquisitionLocked(const std::string& message)
{
    _readerShouldRun.store(false);
    if (_readerStarted)
    {
        _readerThread.join();
        _readerStarted = false;
    }

    _frameQueue.wakeUpAll();
    drainQueue();

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
    _lastWaveformPublishAtUs = 0;
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
        drainQueue();
        _readerShouldRun.store(true);
        startAdcLocked();
        _readerThread.start(_readerRunnable);
        _readerStarted = true;
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
    _latestSamples.fill(0);
    _lastDebugWords.fill(0);
    _lastDebugHiLo.fill(0);
    _lastDebugLoHi.fill(0);
    _lastDebugSequence = 0;
    _lastDebugCapturedAt.clear();
    _debugPublishCount = 0;
    _totalFrames.store(0);
    _droppedFrames.store(0);
    _recoveries.store(0);
    _lastFrameSequence.store(0);
    _queueDepth.store(0);
    _lastFrameAt.clear();
    _lastPublishedAt.clear();
    _lastWaveformPublishAtUs = 0;
    _lastHistorySampleAtUs = 0;
    _udpPacketsSent.store(0);
    _udpFramesSent.store(0);
    _udpSendErrors.store(0);
    _udpLastError.clear();
}

void AcquisitionRuntime::unpackFrame(
        const std::array<std::uint32_t, kFrameWords>& words,
        std::array<std::int16_t, kFrameColumns>& samples) const
{
    for (std::size_t index = 0; index < words.size(); ++index)
    {
        const std::uint32_t word = words[index];
        samples[index * 2] = static_cast<std::int16_t>(static_cast<std::uint16_t>((word >> 16) & 0xFFFFu));
        samples[index * 2 + 1] = static_cast<std::int16_t>(static_cast<std::uint16_t>(word & 0xFFFFu));
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
    constexpr std::uint32_t kFrameBytes = static_cast<std::uint32_t>(kFrameWords * sizeof(std::uint32_t));
    const std::uint32_t isr = _hardware.fifoCtrl[kIsr / 4];
    if (isr & (kIsrRpue | kIsrRpore | kIsrRpure)) return -2;

    const std::uint32_t occupancy = _hardware.fifoCtrl[kRdfo / 4];
    if (occupancy < kFrameWords) return 0;

    const std::uint32_t rlr = _hardware.fifoCtrl[kRlr / 4];
    if (rlr & 0x80000000u) return 0;

    const std::uint32_t bytes = rlr & 0x007FFFFFu;
    if (bytes < kFrameBytes) return 0;
    if ((bytes % sizeof(std::uint32_t)) != 0) return -3;
    if ((bytes % kFrameBytes) != 0) return -3;

    for (std::size_t index = 0; index < frame.size(); ++index)
    {
        frame[index] = _hardware.fifoData[0];
    }
    return 1;
}
#endif

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
