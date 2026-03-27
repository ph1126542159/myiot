#include "AuthRequestHandler.h"
#include "RootRedirectRequestHandler.h"
#include "SessionJSON.h"
#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"

namespace MyIoT {
namespace WebUI {
namespace Launcher {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        pContext->logger().information("MyIoT WebUI Launcher bundle started.");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        pContext->logger().information("MyIoT WebUI Launcher bundle stopped.");
    }
};

} } } // namespace MyIoT::WebUI::Launcher

POCO_BEGIN_NAMED_MANIFEST(WebServer, Poco::OSP::Web::WebRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::Launcher::RootRedirectRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::Launcher::FaviconRedirectRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::Launcher::LoginRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::Launcher::LogoutRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::Launcher::SessionJSONFactory)
POCO_END_MANIFEST

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::WebUI::Launcher::BundleActivator)
POCO_END_MANIFEST
