#include "JNDM123DdsBridge.h"

#include "Poco/Exception.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Thread.h"

#include <sstream>

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

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
}

Object::Ptr DdsAcquisitionBridge::latestSnapshot()
{
    ensureStarted();

    Poco::FastMutex::ScopedLock lock(_mutex);
    try
    {
        Object::Ptr payload = cloneFromText(_latestPayloadText);
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
    acquisitionServiceOrThrow()->serviceStatus();

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
                    if (payload) return payload;
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
            Object::Ptr restartPayload = acquisitionServiceOrThrow()->restartProcess();
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
