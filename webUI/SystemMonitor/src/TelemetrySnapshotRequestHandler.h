#ifndef MyIoT_WebUI_SystemMonitor_TelemetrySnapshotRequestHandler_INCLUDED
#define MyIoT_WebUI_SystemMonitor_TelemetrySnapshotRequestHandler_INCLUDED

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"

namespace MyIoT {
namespace WebUI {
namespace SystemMonitor {

class TelemetrySnapshotRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
    explicit TelemetrySnapshotRequestHandler(Poco::OSP::BundleContext::Ptr pContext);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
};

class TelemetrySnapshotRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

} } } // namespace MyIoT::WebUI::SystemMonitor

#endif // MyIoT_WebUI_SystemMonitor_TelemetrySnapshotRequestHandler_INCLUDED
