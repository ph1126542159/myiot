#ifndef MyIoT_WebUI_Launcher_SessionJSON_INCLUDED
#define MyIoT_WebUI_Launcher_SessionJSON_INCLUDED

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"

namespace MyIoT {
namespace WebUI {
namespace Launcher {

class SessionJSON: public Poco::Net::HTTPRequestHandler
{
public:
    explicit SessionJSON(Poco::OSP::BundleContext::Ptr pContext);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
};

class SessionJSONFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

} } } // namespace MyIoT::WebUI::Launcher

#endif // MyIoT_WebUI_Launcher_SessionJSON_INCLUDED
