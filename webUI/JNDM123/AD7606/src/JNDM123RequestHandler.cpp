#include "JNDM123RequestHandler.h"

#include "JNDM123DdsBridge.h"
#include "JNDM123AgentManager.h"
#include "JNDM123Runtime.h"

#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"
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
    auto activity = Poco::OpenTelemetry::beginRequestActivity(_pContext, request, "jndm123.divider");
    if (!isAuthenticated(_pContext, request))
    {
        Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED, "user is not authenticated");
        sendJSON(response, createUnauthorizedPayload("Authentication required for JNDM123 divider control."), Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        return;
    }

    try
    {
        Poco::Net::HTMLForm form(request, request.stream());
        const std::string requestedDevicePath = form.get("devicePath", "");
        const std::string devicePath = resolvePreferredDevicePath(requestedDevicePath);
        const Poco::UInt64 referenceClockHz = resolvePreferredReferenceClockHz(form.get("referenceClockHz", ""));
        activity.tag("jndm123.device_path", devicePath.empty() ? "-" : devicePath);
        activity.tag("jndm123.reference_clock_hz", std::to_string(referenceClockHz));

        if (Poco::icompare(request.getMethod(), std::string("POST")) == 0)
        {
            if (!form.has("divider"))
            {
                activity.step("reference_clock.update", devicePath);
                sendJSON(response, dividerSnapshotToJson(JNDM123Runtime::instance().updateReferenceClock(devicePath, referenceClockHz)));
                Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, "reference clock updated");
                return;
            }

            int divider = 0;
            if (!parseIntStrict(form.get("divider", ""), divider))
            {
                Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_BAD_REQUEST, "divider is missing or invalid");
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
                    Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_BAD_REQUEST, "output index is missing or invalid");
                    sendJSON(response, createErrorPayload("outputIndex or outputIndices is required."), Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                    return;
                }
                outputIndices.push_back(outputIndex);
            }

            activity.step("divider.apply", std::to_string(divider));
            sendJSON(
                response,
                dividerSnapshotToJson(JNDM123Runtime::instance().applyDividers(
                    devicePath,
                    outputIndices,
                    divider,
                    referenceClockHz)));
            Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, "divider applied");
            return;
        }

        sendJSON(response, dividerSnapshotToJson(JNDM123Runtime::instance().readDividerStatus(devicePath)));
        Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, "divider status read");
    }
    catch (const Poco::InvalidArgumentException& exc)
    {
        Poco::OpenTelemetry::failException(activity, exc);
        logger().warning("Divider request rejected: " + exc.displayText());
        sendJSON(response, createErrorPayload(exc.displayText()), Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
    catch (const Poco::Exception& exc)
    {
        Poco::OpenTelemetry::failException(activity, exc);
        logger().error("Divider request failed: " + exc.displayText());
        sendJSON(response, createErrorPayload(exc.displayText()), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (const std::exception& exc)
    {
        Poco::OpenTelemetry::failException(activity, exc);
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
    auto activity = Poco::OpenTelemetry::beginRequestActivity(_pContext, request, "jndm123.acquisition");
    if (!isAuthenticated(_pContext, request))
    {
        Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED, "user is not authenticated");
        sendJSON(response, createUnauthorizedPayload("Authentication required for JNDM123 acquisition."), Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        return;
    }

    try
    {
        Poco::Net::HTMLForm form(request, request.stream());
        const bool includeWaveform = parseBoolValue(form.get("includeWaveform", "0"));
        activity.tag("jndm123.include_waveform", includeWaveform ? "true" : "false");
        if (includeWaveform)
        {
            JNDM123Runtime::instance().touchPreviewLease();
            activity.step("preview.lease.touch");
        }

        if (Poco::icompare(request.getMethod(), std::string("POST")) == 0)
        {
            const std::string action = form.get("action", "");
            activity.tag("jndm123.action", action.empty() ? "-" : action);
            if (action == "start")
            {
                activity.step("acquisition.start");
                const AcquisitionActionResult result = JNDM123Runtime::instance().startAcquisition();
                Object::Ptr payload = JNDM123Runtime::instance().acquisitionSnapshot(includeWaveform);
                payload->set("ok", result.ok);
                payload->set("message", result.message);
                sendJSON(response, payload, result.ok ? Poco::Net::HTTPResponse::HTTP_OK : Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                if (result.ok) Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, result.message);
                else Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, result.message);
                return;
            }

            if (action == "stop")
            {
                activity.step("acquisition.stop");
                const AcquisitionActionResult result = JNDM123Runtime::instance().stopAcquisition();
                Object::Ptr payload = JNDM123Runtime::instance().acquisitionSnapshot(includeWaveform);
                payload->set("ok", result.ok);
                payload->set("message", result.message);
                sendJSON(response, payload);
                if (result.ok) Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, result.message);
                else Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, result.message);
                return;
            }

            if (action == "restart-process")
            {
                activity.step("acquisition.restart_process");
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
                if (payload->optValue("ok", true)) Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, payload->optValue("message", std::string("acquisition process restarted")));
                else Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, payload->optValue("message", std::string("acquisition process restart failed")));
                return;
            }
        }

        sendJSON(response, JNDM123Runtime::instance().acquisitionSnapshot(includeWaveform));
        Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, "acquisition snapshot read");
    }
    catch (const Poco::Exception& exc)
    {
        Poco::OpenTelemetry::failException(activity, exc);
        logger().error("Acquisition request failed: " + exc.displayText());
        sendJSON(response, createErrorPayload(exc.displayText()), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (const std::exception& exc)
    {
        Poco::OpenTelemetry::failException(activity, exc);
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
