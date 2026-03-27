#ifndef MyIoT_WebUI_SystemMonitor_SystemMetricsRequestHandler_INCLUDED
#define MyIoT_WebUI_SystemMonitor_SystemMetricsRequestHandler_INCLUDED

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"

namespace MyIoT {
namespace WebUI {
namespace SystemMonitor {

class SystemMetricsRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
    explicit SystemMetricsRequestHandler(Poco::OSP::BundleContext::Ptr pContext);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
};

class SystemMetricsRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

} } } // namespace MyIoT::WebUI::SystemMonitor

#endif // MyIoT_WebUI_SystemMonitor_SystemMetricsRequestHandler_INCLUDED
