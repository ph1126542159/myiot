#ifndef MyIoT_WebUI_Launcher_AuthRequestHandler_INCLUDED
#define MyIoT_WebUI_Launcher_AuthRequestHandler_INCLUDED

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"

namespace MyIoT {
namespace WebUI {
namespace Launcher {

class AuthRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
    enum class Mode
    {
        login,
        logout
    };

    AuthRequestHandler(Poco::OSP::BundleContext::Ptr pContext, Mode mode);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
    Mode _mode;
};

class LoginRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

class LogoutRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

} } } // namespace MyIoT::WebUI::Launcher

#endif // MyIoT_WebUI_Launcher_AuthRequestHandler_INCLUDED
