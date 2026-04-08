#include "JNDM123DdsBridge.h"
#include "JNDM123AgentManager.h"

#include "Poco/Exception.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/String.h"
#include "Poco/Thread.h"

#include <algorithm>
#include <sstream>

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

using Poco::JSON::Array;

namespace {

constexpr std::size_t kSnapshotRetentionLimit = 60;

class SnapshotNotification: public Poco::Notification
{
public:
    explicit SnapshotNotification(std::string payloadText):
        _payloadText(std::move(payloadText))
    {
    }

    const std::string& payloadText() const
    {
        return _payloadText;
    }

private:
    std::string _payloadText;
};

bool isDividerAction(const std::string& action)
{
    return Poco::startsWith(action, std::string("divider-"));
}

bool payloadHasDividerOutputs(Object::Ptr payload)
{
    if (!payload) return false;

    Object::Ptr divider = payload->getObject("divider");
    if (!divider) divider = payload;
    return divider && divider->getArray("outputs");
}

std::string debugSummaryFromPayload(Object::Ptr payload)
{
    if (!payload) return std::string("no payload");

    std::ostringstream stream;
    Object::Ptr debug = payload->getObject("debug");
    if (debug)
    {
        stream << "seq=" << debug->optValue<Poco::UInt64>("sequence", 0)
               << " capturedAt=" << debug->optValue("capturedAt", std::string("--"));

        if (Array::Ptr rawWords = debug->getArray("rawWordsHex"))
        {
            stream << " raw=";
            for (std::size_t index = 0; index < rawWords->size(); ++index)
            {
                if (index > 0) stream << ',';
                stream << rawWords->getElement<std::string>(index);
            }
        }

        if (Array::Ptr hiLo = debug->getArray("hiLoUnsigned"))
        {
            stream << " hiLo=";
            for (std::size_t index = 0; index < hiLo->size(); ++index)
            {
                if (index > 0) stream << ',';
                stream << hiLo->getElement<Poco::UInt64>(index);
            }
        }

        if (Array::Ptr loHi = debug->getArray("loHiUnsigned"))
        {
            stream << " loHi=";
            for (std::size_t index = 0; index < loHi->size(); ++index)
            {
                if (index > 0) stream << ',';
                stream << loHi->getElement<Poco::UInt64>(index);
            }
        }
    }

    Array::Ptr chips = payload->getArray("chips");
    if (chips && chips->size() > 0)
    {
        Object::Ptr chip = chips->getObject(0);
        Array::Ptr channels = chip ? chip->getArray("channels") : nullptr;
        if (channels && channels->size() > 0)
        {
            stream << " chip0=";
            const std::size_t count = std::min<std::size_t>(channels->size(), 4);
            for (std::size_t index = 0; index < count; ++index)
            {
                if (index > 0) stream << ',';
                Object::Ptr channel = channels->getObject(index);
                stream << (channel ? channel->optValue("value", 0) : 0);
            }
        }
    }

    return stream.str();
}

std::string lastFrameSummaryFromPayload(Object::Ptr payload)
{
    if (!payload) return std::string("no payload");

    Array::Ptr chips = payload->getArray("chips");
    if (!chips || chips->size() == 0) return std::string("no chips");

    std::ostringstream stream;
    stream << "lastFrame=";

    for (std::size_t chipIndex = 0; chipIndex < chips->size(); ++chipIndex)
    {
        Object::Ptr chip = chips->getObject(chipIndex);
        if (!chip) continue;

        if (chipIndex > 0) stream << " | ";

        stream << chip->optValue("name", std::string("ADC") + std::to_string(chipIndex + 1)) << '[';

        Array::Ptr channels = chip->getArray("channels");
        if (channels)
        {
            for (std::size_t channelIndex = 0; channelIndex < channels->size(); ++channelIndex)
            {
                if (channelIndex > 0) stream << ',';

                Object::Ptr channel = channels->getObject(channelIndex);
                if (!channel)
                {
                    stream << '?';
                    continue;
                }

                stream << channel->optValue("value", 0);
            }
        }

        stream << ']';
    }

    return stream.str();
}

}

DdsAcquisitionBridge& DdsAcquisitionBridge::instance()
{
    static DdsAcquisitionBridge bridge;
    return bridge;
}

DdsAcquisitionBridge::DdsAcquisitionBridge()
{
}

void DdsAcquisitionBridge::onMessage(const MyIoT::Services::JNDM123AcquisitionAgent::AcquisitionDdsJsonMessage& message)
{
    Poco::UInt64 commandSequence = 0;
    std::string updatedAt(message.updatedAt);

    try
    {
        Object::Ptr payload = cloneFromText(message.payload);
        if (payload)
        {
            commandSequence = payload->optValue<Poco::UInt64>("commandSequence", 0);
            updatedAt = payload->optValue("updatedAt", updatedAt);
            Poco::FastMutex::ScopedLock lock(_mutex);
            ++_debugSnapshotCount;
            if (_debugSnapshotCount <= 5 || (_debugSnapshotCount % 20) == 0)
            {
                logger().information("DDS bridge received snapshot: " + debugSummaryFromPayload(payload));
                logger().information("DDS bridge last frame: " + lastFrameSummaryFromPayload(payload));
            }
        }
    }
    catch (...)
    {
    }

    Poco::FastMutex::ScopedLock lock(_mutex);
    _latestPayloadText = message.payload;
    _lastSnapshotUpdatedAt = updatedAt;
    if (commandSequence > _lastCommandSequenceSeen)
    {
        _lastCommandSequenceSeen = commandSequence;
    }
    while (_pendingSnapshotCount >= kSnapshotRetentionLimit)
    {
        Poco::AutoPtr<Poco::Notification> dropped(_snapshotQueue.dequeueNotification());
        if (!dropped) break;
        --_pendingSnapshotCount;
    }
    _snapshotQueue.enqueueNotification(new SnapshotNotification(message.payload));
    ++_pendingSnapshotCount;
}

Object::Ptr DdsAcquisitionBridge::latestSnapshot()
{
    ensureStarted();

    std::string payloadText;
    Poco::FastMutex::ScopedLock lock(_mutex);
    drainPendingSnapshotsLocked();
    if (!_recentPayloadTexts.empty())
    {
        payloadText = _recentPayloadTexts.back();
    }
    else
    {
        payloadText = _latestPayloadText;
    }

    try
    {
        Object::Ptr payload = cloneFromText(payloadText);
        if (payload) return payload;
    }
    catch (...)
    {
    }

    return createWaitingAcquisitionPayload("Waiting for acquisition snapshots from the external process.");
}

bool DdsAcquisitionBridge::isRunning()
{
    Object::Ptr payload = latestSnapshot();
    return payload && payload->optValue("running", false);
}

Object::Ptr DdsAcquisitionBridge::sendCommandAndAwait(
    const std::string& action,
    const std::string& message,
    Object::Ptr arguments,
    bool restartOnTimeout)
{
    ensureStarted();
    acquisitionProcessStatusOrThrow();

    const Poco::UInt64 commandSequence = _nextCommandSequence.fetch_add(1) + 1;
    MyIoT::Services::JNDM123AcquisitionAgent::AcquisitionDdsJsonMessage request;
    request.sequence = commandSequence;
    request.updatedAt = isoTimestamp();

    Object::Ptr requestPayload = arguments;
    if (!requestPayload) requestPayload = new Object;
    requestPayload->set("action", action);
    requestPayload->set("message", message);

    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(requestPayload, buffer);
    request.payload = buffer.str();

    if (!_publisher.publish(request))
    {
        Object::Ptr payload = createWaitingAcquisitionPayload("Failed to publish acquisition DDS command.");
        payload->set("ok", false);
        return payload;
    }

    const Poco::Timestamp deadline;
    const Poco::Int64 deadlineUs =
        deadline.epochMicroseconds() + static_cast<Poco::Int64>(kAcquisitionCommandTimeoutMs) * 1000;

    while (Poco::Timestamp().epochMicroseconds() < deadlineUs)
    {
        {
            Poco::FastMutex::ScopedLock lock(_mutex);
            if (_lastCommandSequenceSeen >= commandSequence)
            {
                try
                {
                    Object::Ptr payload = cloneFromText(_latestPayloadText);
                    if (payload)
                    {
                        if (!isDividerAction(action)) return payload;

                        if (payloadHasDividerOutputs(payload)) return payload;

                        if (!payload->optValue("ok", true)) return payload;
                    }
                }
                catch (...)
                {
                }
            }
        }

        Poco::Thread::sleep(50);
    }

    Object::Ptr payload = latestSnapshot();
    payload->set("ok", false);
    payload->set(
        "message",
        "No acquisition response within 5 seconds after '" + action + "'. Restarting the acquisition process.");

    if (restartOnTimeout)
    {
        try
        {
            Object::Ptr restartPayload = restartAcquisitionProcessOrThrow();
            payload->set("agentProcessRunning", restartPayload->optValue("agentProcessRunning", false));
            payload->set("agentProcessId", restartPayload->optValue("agentProcessId", -1));
            payload->set("agentRestartCount", restartPayload->optValue<Poco::UInt64>("agentRestartCount", 0));
            payload->set("agentManagerError", restartPayload->optValue("agentManagerError", std::string()));
        }
        catch (const Poco::Exception& exc)
        {
            payload->set("message", payload->getValue<std::string>("message") + " Restart failed: " + exc.displayText());
        }
    }

    return payload;
}

Object::Ptr DdsAcquisitionBridge::sendCommandAndAwait(
    const std::string& action,
    const std::string& message,
    bool restartOnTimeout)
{
    return sendCommandAndAwait(action, message, nullptr, restartOnTimeout);
}

void DdsAcquisitionBridge::resetSnapshotCache()
{
    Poco::FastMutex::ScopedLock lock(_mutex);
    _latestPayloadText.clear();
    _lastSnapshotUpdatedAt.clear();
    _recentPayloadTexts.clear();
    while (true)
    {
        Poco::AutoPtr<Poco::Notification> notification(_snapshotQueue.dequeueNotification());
        if (!notification) break;
    }
    _pendingSnapshotCount = 0;
    _lastCommandSequenceSeen = 0;
}

void DdsAcquisitionBridge::ensureStarted()
{
    Poco::FastMutex::ScopedLock lock(_mutex);
    if (_started) return;

    _publisher.start(
        kAcquisitionDdsDomain,
        MyIoT::Services::JNDM123AcquisitionAgent::acquisitionControlTopicName());
    _subscriber.start(
        kAcquisitionDdsDomain,
        MyIoT::Services::JNDM123AcquisitionAgent::acquisitionSnapshotTopicName(),
        this);
    _started = true;
}

void DdsAcquisitionBridge::drainPendingSnapshotsLocked()
{
    while (true)
    {
        Poco::AutoPtr<Poco::Notification> notification(_snapshotQueue.dequeueNotification());
        if (!notification) break;

        if (_pendingSnapshotCount > 0)
        {
            --_pendingSnapshotCount;
        }

        SnapshotNotification* pSnapshot = dynamic_cast<SnapshotNotification*>(notification.get());
        if (!pSnapshot || pSnapshot->payloadText().empty()) continue;

        _recentPayloadTexts.push_back(pSnapshot->payloadText());
        while (_recentPayloadTexts.size() > kSnapshotRetentionLimit)
        {
            _recentPayloadTexts.pop_front();
        }
    }
}

bool remoteAcquisitionRunning()
{
    try
    {
        return DdsAcquisitionBridge::instance().isRunning();
    }
    catch (...)
    {
        return false;
    }
}

} } } // namespace MyIoT::WebUI::JNDM123
