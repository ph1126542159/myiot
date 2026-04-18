#include "ConsoleCommandRequestHandler.h"
#include "ProcessConsoleService.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/NumberParser.h"
#include "Poco/String.h"
#include "Poco/Timestamp.h"

namespace {

const std::string PROCESS_CONSOLE_CWD_KEY("processConsole.cwd");

Poco::OSP::Web::WebSession::Ptr findSession(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request)
{
    Poco::OSP::Web::WebSessionManager::Ptr pSessionManager =
        Poco::OSP::ServiceFinder::find<Poco::OSP::Web::WebSessionManager>(pContext);
    return pSessionManager->find(pContext->thisBundle()->properties().getString("websession.id", "myiot.webui"), request);
}

std::string currentWorkingDirectory(Poco::OSP::Web::WebSession::Ptr pSession)
{
    return pSession ? pSession->getValue<std::string>(PROCESS_CONSOLE_CWD_KEY, "") : "";
}

void updateWorkingDirectory(Poco::OSP::Web::WebSession::Ptr pSession, Poco::JSON::Object::Ptr payload)
{
    if (!pSession || !payload) return;

    const std::string workingDirectory =
        payload->has("workingDirectory") ? payload->getValue<std::string>("workingDirectory") : "";
    if (!workingDirectory.empty())
    {
        pSession->set(PROCESS_CONSOLE_CWD_KEY, workingDirectory);
    }
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

void sendJSON(Poco::Net::HTTPServerResponse& response, Poco::JSON::Object::Ptr payload, Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_OK)
{
    response.setStatus(status);
    response.setChunkedTransferEncoding(true);
    response.setContentType("application/json; charset=utf-8");
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

Poco::JSON::Object::Ptr createUnauthorizedPayload()
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", false);
    payload->set("ok", false);
    payload->set("message", "未登录，无法执行诊断控制台命令。");
    payload->set("output", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    return payload;
}

int parseLimit(const Poco::Net::HTMLForm& form)
{
    try
    {
        if (!form.has("limit")) return -1;

        int limit = Poco::NumberParser::parse(form.get("limit"));
        if (limit < 1) return 1;
        if (limit > 200) return 200;
        return limit;
    }
    catch (...)
    {
        return -1;
    }
}

} // namespace

namespace MyIoT {
namespace Services {
namespace ProcessConsole {

ConsoleCommandRequestHandler::ConsoleCommandRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
    _pContext(pContext)
{
}

void ConsoleCommandRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    if (!isAuthenticated(_pContext, request))
    {
        sendJSON(response, createUnauthorizedPayload(), Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        return;
    }

    if (Poco::icompare(request.getMethod(), std::string("GET")) != 0 &&
        Poco::icompare(request.getMethod(), std::string("POST")) != 0)
    {
        Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
        payload->set("authenticated", true);
        payload->set("ok", false);
        payload->set("message", "仅支持 GET 或 POST 请求。");
        payload->set("output", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
        sendJSON(response, payload, Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
        return;
    }

    Poco::Net::HTMLForm form(request, request.stream());
    const std::string command = Poco::trim(form.get("command", "help"));
    const int limit = parseLimit(form);
    Poco::OSP::Web::WebSession::Ptr pSession;
    try
    {
        pSession = findSession(_pContext, request);
    }
    catch (...)
    {
    }

    ProcessConsoleService::Ptr pConsoleService;
    try
    {
        pConsoleService = Poco::OSP::ServiceFinder::findByName<ProcessConsoleService>(_pContext, ProcessConsoleService::SERVICE_NAME);
    }
    catch (const Poco::Exception& exc)
    {
        Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
        payload->set("authenticated", true);
        payload->set("ok", false);
        payload->set("message", "诊断控制台服务当前不可用。");
        payload->set("detail", exc.displayText());
        payload->set("output", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
        sendJSON(response, payload, Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
        return;
    }

    Poco::JSON::Object::Ptr payload = pConsoleService->execute(command, limit, currentWorkingDirectory(pSession));
    payload->set("authenticated", true);
    payload->set("updatedAt", Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::ISO8601_FORMAT));
    updateWorkingDirectory(pSession, payload);
    sendJSON(response, payload);
}

Poco::Net::HTTPRequestHandler* ConsoleCommandRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new ConsoleCommandRequestHandler(context());
}

} } } // namespace MyIoT::Services::ProcessConsole
