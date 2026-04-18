#ifndef MyIoT_Services_ProcessConsole_TerminalWebSocketRequestHandler_INCLUDED
#define MyIoT_Services_ProcessConsole_TerminalWebSocketRequestHandler_INCLUDED

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"

namespace MyIoT {
namespace Services {
namespace ProcessConsole {

class TerminalWebSocketRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
    explicit TerminalWebSocketRequestHandler(Poco::OSP::BundleContext::Ptr pContext);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
};

class TerminalWebSocketRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

} } } // namespace MyIoT::Services::ProcessConsole

#endif // MyIoT_Services_ProcessConsole_TerminalWebSocketRequestHandler_INCLUDED
