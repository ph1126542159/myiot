#include "TelemetrySnapshotRequestHandler.h"

#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"
#include "Poco/Net/NetException.h"
#include "Poco/NumberParser.h"
#include "Poco/OpenTelemetry/TelemetryClient.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/String.h"
#include "Poco/URI.h"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string normalizeLocale(std::string value)
{
    Poco::toLowerInPlace(value);
    return Poco::startsWith(value, std::string("en")) ? "en" : "zh";
}

std::string requestLocale(Poco::Net::HTTPServerRequest& request)
{
    const std::string explicitLocale = request.get("X-MyIoT-Locale", "");
    if (!explicitLocale.empty()) return normalizeLocale(explicitLocale);

    const std::string acceptLanguage = request.get("Accept-Language", "");
    if (!acceptLanguage.empty()) return normalizeLocale(acceptLanguage);

    return "zh";
}

std::string localized(Poco::Net::HTTPServerRequest& request, const std::string& zh, const std::string& en)
{
    return requestLocale(request) == "en" ? en : zh;
}

Poco::OSP::Web::WebSession::Ptr findSession(
    Poco::OSP::BundleContext::Ptr pContext,
    Poco::Net::HTTPServerRequest& request)
{
    Poco::OSP::Web::WebSessionManager::Ptr pSessionManager =
        Poco::OSP::ServiceFinder::find<Poco::OSP::Web::WebSessionManager>(pContext);
    return pSessionManager->find(
        pContext->thisBundle()->properties().getString("websession.id", "myiot.webui"),
        request);
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

void sendJSON(
    Poco::Net::HTTPServerResponse& response,
    Poco::JSON::Object::Ptr payload,
    Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_OK)
{
    response.setStatus(status);
    response.setChunkedTransferEncoding(false);
    response.setContentType("application/json");
    response.set("Cache-Control", "no-cache, no-store, must-revalidate");
    response.set("Pragma", "no-cache");
    response.set("Expires", "0");

    try
    {
        std::ostringstream buffer;
        Poco::JSON::Stringifier::stringify(payload, buffer);
        const std::string body = buffer.str();
        response.setContentLength(static_cast<int>(body.size()));
        response.sendBuffer(body.data(), static_cast<int>(body.size()));
    }
    catch (const Poco::Net::ConnectionResetException&)
    {
    }
    catch (const Poco::Net::ConnectionAbortedException&)
    {
    }
    catch (const Poco::IOException& exc)
    {
        const std::string message = exc.displayText();
        if (message.find("broken pipe") == std::string::npos &&
            message.find("Broken pipe") == std::string::npos)
        {
            throw;
        }
    }
}

int parseLimit(Poco::Net::HTTPServerRequest& request, const std::string& key, int defaultValue)
{
    try
    {
        Poco::URI uri(request.getURI());
        for (const auto& parameter: uri.getQueryParameters())
        {
            if (parameter.first == key)
            {
                const int value = Poco::NumberParser::parse(parameter.second);
                if (value < 10) return 10;
                if (value > 500) return 500;
                return value;
            }
        }
    }
    catch (...)
    {
    }

    return defaultValue;
}

Poco::JSON::Array::Ptr attributesToJSON(const Poco::OpenTelemetry::TelemetryAttributes& attributes)
{
    Poco::JSON::Array::Ptr values = new Poco::JSON::Array;
    for (const auto& attribute: attributes)
    {
        Poco::JSON::Object::Ptr item = new Poco::JSON::Object;
        item->set("key", attribute.key);
        item->set("value", attribute.value);
        values->add(item);
    }
    return values;
}

Poco::JSON::Array::Ptr logsToJSON(const std::vector<Poco::OpenTelemetry::TelemetryLogEntry>& logs)
{
    Poco::JSON::Array::Ptr values = new Poco::JSON::Array;
    for (const auto& log: logs)
    {
        Poco::JSON::Object::Ptr item = new Poco::JSON::Object;
        item->set("timestamp", log.timestamp);
        item->set("level", log.level);
        item->set("source", log.source);
        item->set("message", log.message);
        item->set("attributes", attributesToJSON(log.attributes));
        values->add(item);
    }
    return values;
}

Poco::JSON::Array::Ptr tracesToJSON(const std::vector<Poco::OpenTelemetry::TelemetryTraceEntry>& traces)
{
    Poco::JSON::Array::Ptr values = new Poco::JSON::Array;
    for (const auto& trace: traces)
    {
        Poco::JSON::Object::Ptr item = new Poco::JSON::Object;
        item->set("activityId", trace.activityId);
        item->set("traceId", trace.traceId);
        item->set("parentActivityId", trace.parentActivityId);
        item->set("name", trace.name);
        item->set("category", trace.category);
        item->set("status", trace.status);
        item->set("input", trace.input);
        item->set("output", trace.output);
        item->set("error", trace.error);
        item->set("startedAt", trace.startedAt);
        item->set("endedAt", trace.endedAt);
        item->set("durationMs", trace.durationMs);
        item->set("attributes", attributesToJSON(trace.attributes));

        Poco::JSON::Array::Ptr steps = new Poco::JSON::Array;
        for (const auto& step: trace.steps)
        {
            Poco::JSON::Object::Ptr stepItem = new Poco::JSON::Object;
            stepItem->set("timestamp", step.timestamp);
            stepItem->set("name", step.name);
            stepItem->set("detail", step.detail);
            stepItem->set("status", step.status);
            stepItem->set("attributes", attributesToJSON(step.attributes));
            steps->add(stepItem);
        }

        item->set("steps", steps);
        values->add(item);
    }
    return values;
}

Poco::JSON::Array::Ptr metricsToJSON(const std::vector<Poco::OpenTelemetry::TelemetryMetricSample>& metrics)
{
    Poco::JSON::Array::Ptr values = new Poco::JSON::Array;
    for (const auto& metric: metrics)
    {
        Poco::JSON::Object::Ptr item = new Poco::JSON::Object;
        item->set("timestamp", metric.timestamp);
        item->set("name", metric.name);
        item->set("description", metric.description);
        item->set("unit", metric.unit);
        item->set("value", metric.value);
        item->set("attributes", attributesToJSON(metric.attributes));
        values->add(item);
    }
    return values;
}

Poco::JSON::Object::Ptr createUnauthorizedPayload(Poco::Net::HTTPServerRequest& request)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", false);
    payload->set("available", false);
    payload->set("message", localized(
        request,
        "未登录，无法读取遥测数据。",
        "You are not signed in, so telemetry data is unavailable."));
    payload->set("logs", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    payload->set("traces", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    payload->set("metrics", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    return payload;
}

Poco::JSON::Object::Ptr createUnavailablePayload(Poco::Net::HTTPServerRequest& request)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", true);
    payload->set("available", false);
    payload->set("message", localized(
        request,
        "遥测服务尚未就绪。",
        "The telemetry service is not ready yet."));
    payload->set("logs", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    payload->set("traces", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    payload->set("metrics", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    return payload;
}

Poco::JSON::Object::Ptr createFailurePayload(
    Poco::Net::HTTPServerRequest& request,
    const std::string& detail)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", true);
    payload->set("available", false);
    payload->set("message", localized(
        request,
        "遥测快照生成失败。",
        "Failed to build the telemetry snapshot."));
    payload->set("detail", detail);
    payload->set("logs", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    payload->set("traces", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    payload->set("metrics", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    return payload;
}

} // namespace

namespace MyIoT {
namespace WebUI {
namespace SystemMonitor {

TelemetrySnapshotRequestHandler::TelemetrySnapshotRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
    _pContext(std::move(pContext))
{
}

void TelemetrySnapshotRequestHandler::handleRequest(
    Poco::Net::HTTPServerRequest& request,
    Poco::Net::HTTPServerResponse& response)
{
    auto activity = Poco::OpenTelemetry::beginRequestActivity(_pContext, request, "system.monitor.telemetry.snapshot");
    try
    {
        if (!isAuthenticated(_pContext, request))
        {
            Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED, "user is not authenticated");
            sendJSON(response, createUnauthorizedPayload(request), Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
            return;
        }

        Poco::OpenTelemetry::TelemetryClient telemetry(_pContext);
        if (!telemetry.available())
        {
            Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "telemetry service unavailable");
            sendJSON(response, createUnavailablePayload(request), Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
            return;
        }

        Poco::OpenTelemetry::TelemetrySnapshotOptions options;
        options.logLimit = static_cast<std::size_t>(parseLimit(request, "logs", 120));
        options.traceLimit = static_cast<std::size_t>(parseLimit(request, "traces", 40));
        options.metricLimit = static_cast<std::size_t>(parseLimit(request, "metrics", 40));
        activity.tag("telemetry.logs.limit", std::to_string(options.logLimit));
        activity.tag("telemetry.traces.limit", std::to_string(options.traceLimit));
        activity.tag("telemetry.metrics.limit", std::to_string(options.metricLimit));
        activity.step("snapshot.collect");

        const auto snapshot = telemetry.snapshot(options);
        activity.step("snapshot.ready", snapshot.generatedAt);

        Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
        payload->set("authenticated", true);
        payload->set("available", true);
        payload->set("message", localized(
            request,
            "遥测快照同步正常。",
            "Telemetry snapshot synchronized."));
        payload->set("updatedAt", snapshot.generatedAt);
        payload->set("activeTraceCount", static_cast<unsigned>(snapshot.activeTraceCount));
        payload->set("logs", logsToJSON(snapshot.logs));
        payload->set("traces", tracesToJSON(snapshot.traces));
        payload->set("metrics", metricsToJSON(snapshot.metrics));
        sendJSON(response, payload);
        Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, "snapshot served");
    }
    catch (const Poco::Exception& exc)
    {
        Poco::OpenTelemetry::failException(activity, exc);
        _pContext->logger().error("Failed to build telemetry snapshot: " + exc.displayText());
        sendJSON(response, createFailurePayload(request, exc.displayText()), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (const std::exception& exc)
    {
        Poco::OpenTelemetry::failException(activity, exc);
        _pContext->logger().error("Failed to build telemetry snapshot: " + std::string(exc.what()));
        sendJSON(response, createFailurePayload(request, exc.what()), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (...)
    {
        activity.fail("unknown exception");
        _pContext->logger().error("Failed to build telemetry snapshot: unknown exception");
        sendJSON(response, createFailurePayload(request, "unknown exception"), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

Poco::Net::HTTPRequestHandler* TelemetrySnapshotRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new TelemetrySnapshotRequestHandler(context());
}

} } } // namespace MyIoT::WebUI::SystemMonitor
