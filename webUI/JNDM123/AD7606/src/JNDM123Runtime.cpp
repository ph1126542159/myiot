#include "JNDM123Runtime.h"

#include "JNDM123DdsBridge.h"

#include "Poco/Exception.h"
#include "Poco/JSON/Array.h"

#include <algorithm>
#include <atomic>
#include <sstream>

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

using Poco::JSON::Array;

namespace {

Object::Ptr createDividerRequestPayload(const std::string& devicePath, Poco::UInt64 referenceClockHz)
{
    Object::Ptr payload = new Object;
    payload->set("devicePath", devicePath);
    payload->set("referenceClockHz", referenceClockHz);
    return payload;
}

std::string httpSnapshotDebugSummary(Object::Ptr payload)
{
    if (!payload) return std::string("no payload");

    std::ostringstream stream;
    Object::Ptr debug = payload->getObject("debug");
    if (debug)
    {
        stream << "seq=" << debug->optValue<Poco::UInt64>("sequence", 0);
        if (Array::Ptr hiLo = debug->getArray("hiLoUnsigned"))
        {
            stream << " hiLo=";
            for (std::size_t index = 0; index < hiLo->size(); ++index)
            {
                if (index > 0) stream << ',';
                stream << hiLo->getElement<Poco::UInt64>(index);
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

} // namespace

JNDM123Runtime& JNDM123Runtime::instance()
{
    static JNDM123Runtime runtime;
    return runtime;
}

void JNDM123Runtime::initializeFromSavedConfiguration()
{
#if defined(__linux__)
    try
    {
        Object::Ptr payload = DdsAcquisitionBridge::instance().latestSnapshot();
        logger().information(
            "JNDM123 WebUI runtime initialized; acquisition bootstrap is handled by the external acquisition agent. "
            "Current state: " + payload->optValue("message", std::string("Waiting for acquisition snapshots.")));
    }
    catch (const Poco::Exception& exc)
    {
        logger().warning("JNDM123 WebUI startup bridge initialization skipped: " + exc.displayText());
    }
    catch (const std::exception& exc)
    {
        logger().warning(std::string("JNDM123 WebUI startup bridge initialization skipped: ") + exc.what());
    }
#endif
}

DividerSnapshot JNDM123Runtime::readDividerStatus(const std::string& devicePath)
{
    Poco::FastMutex::ScopedLock lock(_controlMutex);
    Object::Ptr payload = DdsAcquisitionBridge::instance().sendCommandAndAwait(
        "divider-status",
        std::string(),
        createDividerRequestPayload(devicePath, resolvePreferredReferenceClockHz("")),
        true);
    return dividerSnapshotFromPayload(payload);
}

DividerSnapshot JNDM123Runtime::applyDivider(
    const std::string& devicePath,
    int outputIndex,
    int divider,
    Poco::UInt64 referenceClockHz)
{
    return applyDividers(devicePath, std::vector<int>{outputIndex}, divider, referenceClockHz);
}

DividerSnapshot JNDM123Runtime::applyDividers(
    const std::string& devicePath,
    const std::vector<int>& outputIndices,
    int divider,
    Poco::UInt64 referenceClockHz)
{
    const std::vector<int> normalizedOutputs = normalizeOutputIndicesOrThrow(outputIndices);
    for (const int outputIndex: normalizedOutputs)
    {
        validateDividerOrThrow(outputIndex, divider);
    }
    validateReferenceClockHzOrThrow(referenceClockHz);

    Poco::FastMutex::ScopedLock lock(_controlMutex);
    Object::Ptr payload = createDividerRequestPayload(devicePath, referenceClockHz);
    payload->set("divider", divider);
    Poco::JSON::Array::Ptr outputArray = new Poco::JSON::Array;
    for (const int outputIndex: normalizedOutputs) outputArray->add(outputIndex);
    payload->set("outputIndices", outputArray);

    return dividerSnapshotFromPayload(
        DdsAcquisitionBridge::instance().sendCommandAndAwait("divider-apply", std::string(), payload, true));
}

DividerSnapshot JNDM123Runtime::updateReferenceClock(const std::string& devicePath, Poco::UInt64 referenceClockHz)
{
    validateReferenceClockHzOrThrow(referenceClockHz);

    Poco::FastMutex::ScopedLock lock(_controlMutex);
    return dividerSnapshotFromPayload(
        DdsAcquisitionBridge::instance().sendCommandAndAwait(
            "divider-update-reference",
            std::string(),
            createDividerRequestPayload(devicePath, referenceClockHz),
            true));
}

AcquisitionActionResult JNDM123Runtime::startAcquisition()
{
    Poco::FastMutex::ScopedLock lock(_controlMutex);
    return acquisitionResultFromPayload(
        DdsAcquisitionBridge::instance().sendCommandAndAwait("start", std::string(), true),
        "Acquisition started.");
}

AcquisitionActionResult JNDM123Runtime::stopAcquisition(const std::string& message)
{
    Poco::FastMutex::ScopedLock lock(_controlMutex);
    return acquisitionResultFromPayload(
        DdsAcquisitionBridge::instance().sendCommandAndAwait("stop", message, true),
        message);
}

void JNDM123Runtime::touchPreviewLease()
{
}

Object::Ptr JNDM123Runtime::acquisitionSnapshot(bool includeWaveform)
{
    static std::atomic<Poco::UInt64> sSnapshotCount{0};
    Object::Ptr payload = DdsAcquisitionBridge::instance().latestSnapshot();
    payload->set("authenticated", true);
    if (!includeWaveform)
    {
        stripWaveformSamples(payload);
    }
    const Poco::UInt64 count = ++sSnapshotCount;
    if (count <= 5 || (count % 20) == 0)
    {
        logger().information("HTTP acquisition snapshot: " + httpSnapshotDebugSummary(payload));
    }
    return payload;
}

void JNDM123Runtime::shutdown()
{
}

} } } // namespace MyIoT::WebUI::JNDM123
