#ifndef MyIoT_Services_ProcessConsole_ConsoleCommandRequestHandler_INCLUDED
#define MyIoT_Services_ProcessConsole_ConsoleCommandRequestHandler_INCLUDED

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"

namespace MyIoT {
namespace Services {
namespace ProcessConsole {

class ConsoleCommandRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
    explicit ConsoleCommandRequestHandler(Poco::OSP::BundleContext::Ptr pContext);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
};

class ConsoleCommandRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

} } } // namespace MyIoT::Services::ProcessConsole

#endif // MyIoT_Services_ProcessConsole_ConsoleCommandRequestHandler_INCLUDED
