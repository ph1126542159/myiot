#include "SessionJSON.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/String.h"

namespace {

Poco::OSP::Web::WebSession::Ptr findSession(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request)
{
    Poco::OSP::Web::WebSessionManager::Ptr pSessionManager =
        Poco::OSP::ServiceFinder::find<Poco::OSP::Web::WebSessionManager>(pContext);
    return pSessionManager->find(pContext->thisBundle()->properties().getString("websession.id"), request);
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
    sendPayload(response, payload);
}

Poco::Net::HTTPRequestHandler* SessionJSONFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new SessionJSON(context());
}

} } } // namespace MyIoT::WebUI::Launcher
