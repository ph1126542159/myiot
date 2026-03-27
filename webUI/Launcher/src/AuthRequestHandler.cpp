#include "AuthRequestHandler.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"

namespace {

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
    std::ostream& out = response.send();
    Poco::JSON::Stringifier::stringify(payload, out);
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
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
        sendJSON(response, createPayload(false, "", "请求方式错误，必须使用 POST。", "请求方式错误，必须使用 POST。"));
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
                const std::string errorMessage = "请输入用户名和密码后再登录。";
                pSession->erase("username");
                pSession->set("message", errorMessage);
                pSession->set("lastError", errorMessage);
                sendJSON(response, createPayload(false, "", errorMessage, errorMessage));
                return;
            }

            Poco::OSP::Auth::AuthService::Ptr pAuthService = getAuthService(_pContext);
            if (pAuthService->authenticate(username, password))
            {
                const std::string successMessage = "账号 " + username + " 验证通过，登录会话已建立。";
                pSession->set("username", username);
                pSession->set("message", successMessage);
                pSession->erase("lastError");
                sendJSON(response, createPayload(true, username, successMessage, ""));
            }
            else
            {
                const std::string errorMessage = "用户名或密码错误，或该账号已被禁用。";
                pSession->erase("username");
                pSession->set("message", errorMessage);
                pSession->set("lastError", errorMessage);
                sendJSON(response, createPayload(false, "", errorMessage, errorMessage));
            }
        }
        else
        {
            Poco::OSP::Web::WebSessionManager::Ptr pSessionManager = getSessionManager(_pContext);
            if (pSession)
            {
                pSessionManager->remove(pSession);
            }

            sendJSON(response, createPayload(false, "", "已退出登录，请重新验证身份。", ""));
        }
    }
    catch (const Poco::Exception& exc)
    {
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        sendJSON(response, createPayload(false, "", "认证服务当前不可用。", exc.displayText()));
    }
    catch (const std::exception& exc)
    {
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        sendJSON(response, createPayload(false, "", "认证服务当前不可用。", exc.what()));
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
