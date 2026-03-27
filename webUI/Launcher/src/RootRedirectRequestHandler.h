#ifndef MyIoT_WebUI_Launcher_RootRedirectRequestHandler_INCLUDED
#define MyIoT_WebUI_Launcher_RootRedirectRequestHandler_INCLUDED

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include <string>

namespace MyIoT {
namespace WebUI {
namespace Launcher {

class RootRedirectRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
    RootRedirectRequestHandler(
        Poco::OSP::BundleContext::Ptr pContext,
        std::string anonymousTargetURI,
        std::string authenticatedTargetURI);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
    std::string _anonymousTargetURI;
    std::string _authenticatedTargetURI;
};

class RootRedirectRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

class FaviconRedirectRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

} } } // namespace MyIoT::WebUI::Launcher

#endif // MyIoT_WebUI_Launcher_RootRedirectRequestHandler_INCLUDED
