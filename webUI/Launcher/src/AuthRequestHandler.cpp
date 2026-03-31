#include "AuthRequestHandler.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/String.h"
#include "Poco/URI.h"

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

Poco::OSP::Web::WebSessionManager::Ptr getSessionManager(Poco::OSP::BundleContext::Ptr pContext)
{
    return Poco::OSP::ServiceFinder::find<Poco::OSP::Web::WebSessionManager>(pContext);
}

Poco::OSP::Auth::AuthService::Ptr getAuthService(Poco::OSP::BundleContext::Ptr pContext)
{
    const std::string authServiceName = pContext->thisBundle()->properties().getString("auth.serviceName", "osp.auth");
    return Poco::OSP::ServiceFinder::findByName<Poco::OSP::Auth::AuthService>(pContext, authServiceName);
}

Poco::OSP::Web::WebSession::Ptr getSession(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request)
{
    Poco::OSP::Web::WebSessionManager::Ptr pSessionManager = getSessionManager(pContext);
    const std::string sessionId = pContext->thisBundle()->properties().getString("websession.id");
    const int sessionTimeout = pContext->thisBundle()->properties().getInt("websession.timeout", 7200);
    return pSessionManager->get(sessionId, request, sessionTimeout, pContext);
}

void sendJSON(Poco::Net::HTTPServerResponse& response, Poco::JSON::Object::Ptr payload)
{
    response.setChunkedTransferEncoding(true);
    response.setContentType("application/json");
    response.set("Cache-Control", "no-cache");
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

Poco::JSON::Object::Ptr createPayload(bool authenticated, const std::string& username, const std::string& message, const std::string& lastError)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", authenticated);
    payload->set("username", username);
    payload->set("message", message);
    payload->set("lastError", lastError);
    return payload;
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

void logAudit(Poco::OSP::BundleContext::Ptr pContext,
    Poco::Net::HTTPServerRequest& request,
    const std::string& action,
    const std::string& result,
    const std::string& username,
    const std::string& detail = std::string(),
    bool isError = false)
{
    std::string message =
        "WEB-AUDIT action=" + action +
        " result=" + result +
        " user=" + normalizeValue(username) +
        " client=" + request.clientAddress().host().toString() +
        " endpoint=" + requestPath(request);

    if (!detail.empty())
    {
        message += " detail=" + detail;
    }

    if (isError)
    {
        pContext->logger().error(message);
    }
    else if (result == "success")
    {
        pContext->logger().information(message);
    }
    else
    {
        pContext->logger().warning(message);
    }
}

} // namespace

namespace MyIoT {
namespace WebUI {
namespace Launcher {

AuthRequestHandler::AuthRequestHandler(Poco::OSP::BundleContext::Ptr pContext, Mode mode):
    _pContext(pContext),
    _mode(mode)
{
}

void AuthRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    if (request.getMethod() != Poco::Net::HTTPRequest::HTTP_POST)
    {
        logAudit(_pContext, request, _mode == Mode::login ? "login" : "logout", "method_not_allowed", "", request.getMethod());
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
        const std::string message = localized(request, "请求方式错误，必须使用 POST。", "Invalid request method. POST is required.");
        sendJSON(response, createPayload(false, "", message, message));
        return;
    }

    try
    {
        Poco::Net::HTMLForm form(request, request.stream());
        Poco::OSP::Web::WebSession::Ptr pSession = getSession(_pContext, request);

        if (_mode == Mode::login)
        {
            const std::string username = form.get("username", "");
            const std::string password = form.get("password", "");

            if (username.empty() || password.empty())
            {
                const std::string errorMessage = localized(request, "请输入用户名和密码后再登录。", "Please enter both username and password before signing in.");
                pSession->erase("username");
                pSession->set("message", errorMessage);
                pSession->set("lastError", errorMessage);
                logAudit(_pContext, request, "login", "invalid_request", username, "missing_username_or_password");
                sendJSON(response, createPayload(false, "", errorMessage, errorMessage));
                return;
            }

            Poco::OSP::Auth::AuthService::Ptr pAuthService = getAuthService(_pContext);
            if (pAuthService->authenticate(username, password))
            {
                const std::string successMessage = requestLocale(request) == "en"
                    ? "Account " + username + " authenticated successfully."
                    : "账号 " + username + " 验证通过，登录会话已建立。";
                pSession->set("username", username);
                pSession->set("message", successMessage);
                pSession->erase("lastError");
                logAudit(_pContext, request, "login", "success", username, "session_established");
                sendJSON(response, createPayload(true, username, successMessage, ""));
            }
            else
            {
                const std::string errorMessage = localized(request, "用户名或密码错误，或该账号已被禁用。", "Incorrect username or password, or the account has been disabled.");
                pSession->erase("username");
                pSession->set("message", errorMessage);
                pSession->set("lastError", errorMessage);
                logAudit(_pContext, request, "login", "auth_failed", username, "credential_rejected");
                sendJSON(response, createPayload(false, "", errorMessage, errorMessage));
            }
        }
        else
        {
            Poco::OSP::Web::WebSessionManager::Ptr pSessionManager = getSessionManager(_pContext);
            const std::string username = pSession ? pSession->getValue<std::string>("username", "") : "";
            if (pSession)
            {
                pSessionManager->remove(pSession);
            }

            logAudit(_pContext, request, "logout", "success", username, "session_removed");
            sendJSON(response, createPayload(false, "", localized(request, "已退出登录，请重新验证身份。", "Signed out. Please authenticate again."), ""));
        }
    }
    catch (const Poco::Exception& exc)
    {
        logAudit(_pContext, request, _mode == Mode::login ? "login" : "logout", "backend_error", "", exc.displayText(), true);
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        sendJSON(response, createPayload(false, "", localized(request, "认证服务当前不可用。", "The authentication service is currently unavailable."), exc.displayText()));
    }
    catch (const std::exception& exc)
    {
        logAudit(_pContext, request, _mode == Mode::login ? "login" : "logout", "backend_error", "", exc.what(), true);
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        sendJSON(response, createPayload(false, "", localized(request, "认证服务当前不可用。", "The authentication service is currently unavailable."), exc.what()));
    }
}

Poco::Net::HTTPRequestHandler* LoginRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new AuthRequestHandler(context(), AuthRequestHandler::Mode::login);
}

Poco::Net::HTTPRequestHandler* LogoutRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new AuthRequestHandler(context(), AuthRequestHandler::Mode::logout);
}

} } } // namespace MyIoT::WebUI::Launcher
