#include "RootRedirectRequestHandler.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"
#include <utility>

namespace {

std::string resolveRedirectTarget(
    Poco::OSP::BundleContext::Ptr pContext,
    Poco::Net::HTTPServerRequest& request,
    const std::string& anonymousTarget,
    const std::string& authenticatedTarget)
{
    try
    {
        Poco::OSP::Web::WebSessionManager::Ptr pSessionManager =
            Poco::OSP::ServiceFinder::find<Poco::OSP::Web::WebSessionManager>(pContext);
        Poco::OSP::Web::WebSession::Ptr pSession = pSessionManager->find(
            pContext->thisBundle()->properties().getString("websession.id"),
            request);

        if (pSession && !pSession->getValue<std::string>("username", "").empty())
        {
            return authenticatedTarget;
        }
    }
    catch (...)
    {
    }

    return anonymousTarget;
}

} // namespace

namespace MyIoT {
namespace WebUI {
namespace Launcher {

RootRedirectRequestHandler::RootRedirectRequestHandler(
    Poco::OSP::BundleContext::Ptr pContext,
    std::string anonymousTargetURI,
    std::string authenticatedTargetURI):
    _pContext(std::move(pContext)),
    _anonymousTargetURI(std::move(anonymousTargetURI)),
    _authenticatedTargetURI(std::move(authenticatedTargetURI))
{
}

void RootRedirectRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    auto activity = Poco::OpenTelemetry::beginRequestActivity(_pContext, request, "launcher.redirect");
    const std::string target =
        resolveRedirectTarget(_pContext, request, _anonymousTargetURI, _authenticatedTargetURI);
    activity.tag("redirect.target", target);
    activity.step("redirect.resolve", target);
    response.redirect(target, Poco::Net::HTTPResponse::HTTP_SEE_OTHER);
    Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_SEE_OTHER, target);
}

Poco::Net::HTTPRequestHandler* RootRedirectRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new RootRedirectRequestHandler(context(), "/myiot/login/index.html", "/myiot/home/index.html");
}

Poco::Net::HTTPRequestHandler* FaviconRedirectRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new RootRedirectRequestHandler(context(), "/myiot/login/favicon.svg", "/myiot/home/favicon.svg");
}

} } } // namespace MyIoT::WebUI::Launcher
