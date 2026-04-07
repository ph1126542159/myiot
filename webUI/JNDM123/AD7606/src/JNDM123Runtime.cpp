#include "JNDM123Runtime.h"

#include "JNDM123DdsBridge.h"

#include "Poco/Exception.h"
#include "Poco/JSON/Array.h"

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

namespace {

Object::Ptr createDividerRequestPayload(const std::string& devicePath, Poco::UInt64 referenceClockHz)
{
    Object::Ptr payload = new Object;
    payload->set("devicePath", devicePath);
    payload->set("referenceClockHz", referenceClockHz);
    return payload;
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
        Poco::FastMutex::ScopedLock lock(_controlMutex);
        Object::Ptr payload = DdsAcquisitionBridge::instance().sendCommandAndAwait("divider-init", std::string(), true);
        DividerSnapshot snapshot = dividerSnapshotFromPayload(payload);

        const std::string safetyMessage = acquisitionClockSafetyMessage(snapshot);
        if (!safetyMessage.empty())
        {
            logger().warning(safetyMessage);
            return;
        }

        try
        {
            Object::Ptr payload = DdsAcquisitionBridge::instance().sendCommandAndAwait("start", std::string(), true);
            const std::string message = payload->optValue(
                "message",
                std::string("CDCE937 PLL bypass enabled, saved divider state restored for Y1~Y6, acquisition started."));
            logger().information(message);
        }
        catch (const Poco::Exception& exc)
        {
            logger().warning(
                "Saved divider state restored, but the external acquisition process did not start: " + exc.displayText());
        }
    }
    catch (const Poco::Exception& exc)
    {
        logger().warning("JNDM123 startup initialization skipped: " + exc.displayText());
    }
    catch (const std::exception& exc)
    {
        logger().warning(std::string("JNDM123 startup initialization skipped: ") + exc.what());
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
    Object::Ptr payload = DdsAcquisitionBridge::instance().latestSnapshot();
    payload->set("authenticated", true);
    if (!includeWaveform)
    {
        stripWaveformSamples(payload);
    }
    return payload;
}

void JNDM123Runtime::shutdown()
{
}

} } } // namespace MyIoT::WebUI::JNDM123
