#ifndef MyIoT_WebUI_Home_LogStreamRequestHandler_INCLUDED
#define MyIoT_WebUI_Home_LogStreamRequestHandler_INCLUDED

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"

namespace MyIoT {
namespace WebUI {
namespace Home {

class LogStreamRequestLifecycle
{
public:
    static void beginStartup();
    static void beginShutdown(Poco::OSP::BundleContext::Ptr pContext);
    static bool isShuttingDown();
    static bool tryAcquire();
    static void release();
};

class LogStreamRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
    enum class Mode
    {
        snapshot,
        stream
    };

    LogStreamRequestHandler(Poco::OSP::BundleContext::Ptr pContext, Mode mode);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
    Mode _mode;
};

class LogStreamRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

class LogStreamEventsRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

} } } // namespace MyIoT::WebUI::Home

#endif // MyIoT_WebUI_Home_LogStreamRequestHandler_INCLUDED
