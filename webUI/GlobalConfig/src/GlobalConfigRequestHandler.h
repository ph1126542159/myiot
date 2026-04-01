#ifndef MyIoT_WebUI_GlobalConfig_GlobalConfigRequestHandler_INCLUDED
#define MyIoT_WebUI_GlobalConfig_GlobalConfigRequestHandler_INCLUDED

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"

namespace MyIoT {
namespace WebUI {
namespace GlobalConfig {

class GlobalConfigRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
    explicit GlobalConfigRequestHandler(Poco::OSP::BundleContext::Ptr pContext);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
};

class GlobalConfigRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

} } } // namespace MyIoT::WebUI::GlobalConfig

#endif // MyIoT_WebUI_GlobalConfig_GlobalConfigRequestHandler_INCLUDED
