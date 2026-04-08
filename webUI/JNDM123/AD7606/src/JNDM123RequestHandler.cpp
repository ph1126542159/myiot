#include "JNDM123RequestHandler.h"

#include "JNDM123DdsBridge.h"
#include "JNDM123AgentManager.h"
#include "JNDM123Runtime.h"

#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/String.h"

#include <exception>

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

void initializeJNDM123Runtime(Poco::OSP::BundleContext::Ptr pContext)
{
    Poco::UInt64 referenceClockHz = kFallbackReferenceClockHz;
    runtimeBundleContextStorage() = pContext;

    if (pContext)
    {
        const std::string configuredReferenceClockHzText =
            pContext->thisBundle()->properties().getString("referenceClockHz", "");
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
                        ": " + exc.displayText() +
                        ". Using fallback " + std::to_string(kFallbackReferenceClockHz) + ".");
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
            if (!form.has("divider"))
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

            sendJSON(
                response,
                dividerSnapshotToJson(JNDM123Runtime::instance().applyDividers(
                    devicePath,
                    outputIndices,
                    divider,
                    referenceClockHz)));
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
                Object::Ptr payload = JNDM123Runtime::instance().acquisitionSnapshot(includeWaveform);
                payload->set("ok", result.ok);
                payload->set("message", result.message);
                sendJSON(response, payload, result.ok ? Poco::Net::HTTPResponse::HTTP_OK : Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            if (action == "stop")
            {
                const AcquisitionActionResult result = JNDM123Runtime::instance().stopAcquisition();
                Object::Ptr payload = JNDM123Runtime::instance().acquisitionSnapshot(includeWaveform);
                payload->set("ok", result.ok);
                payload->set("message", result.message);
                sendJSON(response, payload);
                return;
            }

            if (action == "restart-process")
            {
                DdsAcquisitionBridge::instance().resetSnapshotCache();
                Object::Ptr payload = restartAcquisitionProcessOrThrow();
                try
                {
                    Object::Ptr snapshotPayload = DdsAcquisitionBridge::instance().latestSnapshot();
                    snapshotPayload->set("message", payload->optValue("message", std::string("Acquisition process restarted.")));
                    payload = snapshotPayload;
                }
                catch (...)
                {
                }

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
