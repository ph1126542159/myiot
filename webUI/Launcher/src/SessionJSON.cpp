#include "SessionJSON.h"
#include "Poco/Exception.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"
#include "Poco/String.h"
#include "Poco/URI.h"

namespace {

Poco::OSP::Web::WebSessionManager::Ptr getSessionManager(Poco::OSP::BundleContext::Ptr pContext)
{
    return Poco::OSP::ServiceFinder::find<Poco::OSP::Web::WebSessionManager>(pContext);
}

bool isMissingCSRFTToken(const Poco::Exception& exc)
{
    return exc.displayText().find("#csrfToken") != std::string::npos;
}

Poco::OSP::Web::WebSession::Ptr findSession(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request)
{
    Poco::OSP::Web::WebSessionManager::Ptr pSessionManager = getSessionManager(pContext);
    const std::string sessionId = pContext->thisBundle()->properties().getString("websession.id");
    const int sessionTimeout = pContext->thisBundle()->properties().getInt("websession.timeout", 7200);

    try
    {
        return pSessionManager->find(sessionId, request);
    }
    catch (const Poco::NotFoundException& exc)
    {
        if (!isMissingCSRFTToken(exc)) throw;

        pContext->logger().warning(
            "WEB-AUDIT action=session_recover result=recreated user=- client=" +
            request.clientAddress().host().toString() +
            " endpoint=" + request.getURI() +
            " detail=missing_#csrfToken");

        return pSessionManager->create(sessionId, request, sessionTimeout, pContext);
    }
}

std::string normalizeLocale(std::string value)
{
    Poco::toLowerInPlace(value);
    return Poco::startsWith(value, std::string("en")) ? "en" : "zh";
}

std::string localized(Poco::Net::HTTPServerRequest& request, const std::string& zh, const std::string& en)
{
    const std::string explicitLocale = request.get("X-MyIoT-Locale", "");
    if (!explicitLocale.empty())
    {
        return normalizeLocale(explicitLocale) == "en" ? en : zh;
    }

    const std::string acceptLanguage = request.get("Accept-Language", "");
    if (!acceptLanguage.empty())
    {
        return normalizeLocale(acceptLanguage) == "en" ? en : zh;
    }

    return zh;
}

std::string requestPath(Poco::Net::HTTPServerRequest& request)
{
    try
    {
        return Poco::URI(request.getURI()).getPathEtc();
    }
    catch (...)
    {
        return request.getURI();
    }
}

std::string normalizeValue(const std::string& value)
{
    return value.empty() ? "-" : value;
}

std::string refererPath(Poco::Net::HTTPServerRequest& request)
{
    const std::string referer = request.get("Referer", "");
    if (referer.empty()) return "-";

    try
    {
        Poco::URI uri(referer);
        const std::string path = uri.getPathEtc();
        return path.empty() ? referer : path;
    }
    catch (...)
    {
        return referer;
    }
}

void logSessionProbe(
    Poco::OSP::BundleContext::Ptr pContext,
    Poco::Net::HTTPServerRequest& request,
    const std::string& username,
    bool authenticated)
{
    const std::string message =
        "WEB-AUDIT action=page_enter" +
        std::string(" result=") + (authenticated ? "authenticated" : "anonymous") +
        " user=" + normalizeValue(username) +
        " client=" + request.clientAddress().host().toString() +
        " endpoint=" + requestPath(request) +
        " detail=referer:" + refererPath(request);

    pContext->logger().information(message);
}

void sendPayload(Poco::Net::HTTPServerResponse& response, Poco::JSON::Object::Ptr payload)
{
    response.setChunkedTransferEncoding(true);
    response.setContentType("application/json");
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
        const std::string message = exc.displayText();
        if (message.find("broken pipe") == std::string::npos &&
            message.find("Broken pipe") == std::string::npos)
        {
            throw;
        }
    }
}

std::string buildAccessURL(Poco::Net::HTTPServerRequest& request)
{
    const Poco::Net::SocketAddress& serverAddress = request.serverAddress();
    const std::string protocol = request.secure() ? "https" : "http";
    const std::string deviceIP = serverAddress.host().toString();
    const std::string host = serverAddress.host().family() == Poco::Net::IPAddress::IPv6
        ? "[" + deviceIP + "]"
        : deviceIP;
    return protocol + "://" + host + ":" + std::to_string(static_cast<unsigned>(serverAddress.port()));
}

} // namespace

namespace MyIoT {
namespace WebUI {
namespace Launcher {

SessionJSON::SessionJSON(Poco::OSP::BundleContext::Ptr pContext):
    _pContext(pContext)
{
}

void SessionJSON::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    auto activity = Poco::OpenTelemetry::beginRequestActivity(_pContext, request, "launcher.session");
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    Poco::OSP::Web::WebSession::Ptr pSession;

    try
    {
        pSession = findSession(_pContext, request);
    }
    catch (...)
    {
    }

    const std::string username = pSession ? pSession->getValue<std::string>("username", "") : "";
    activity.tag("auth.username", username.empty() ? "-" : username);
    activity.tag("auth.authenticated", username.empty() ? "false" : "true");
    payload->set("authenticated", !username.empty());
    payload->set("username", username);
    const std::string defaultMessage = localized(request, "登录网关已就绪，请先完成身份验证。", "The session gateway is ready. Please authenticate first.");
    payload->set("message", pSession ? pSession->getValue<std::string>("message", defaultMessage) : defaultMessage);
    payload->set("lastError", pSession ? pSession->getValue<std::string>("lastError", "") : "");
    payload->set("deviceIp", request.serverAddress().host().toString());
    payload->set("devicePort", static_cast<unsigned>(request.serverAddress().port()));
    payload->set("serverAddress", request.serverAddress().toString());
    payload->set("accessProtocol", request.secure() ? "https" : "http");
    payload->set("accessUrl", buildAccessURL(request));
    payload->set("clientIp", request.clientAddress().host().toString());
    logSessionProbe(_pContext, request, username, !username.empty());
    sendPayload(response, payload);
    Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, username.empty() ? "anonymous session" : "authenticated session");
}

Poco::Net::HTTPRequestHandler* SessionJSONFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new SessionJSON(context());
}

} } } // namespace MyIoT::WebUI::Launcher
