#include "LogStreamRequestHandler.h"
#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"

namespace MyIoT {
namespace WebUI {
namespace Home {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        pContext->logger().information("MyIoT WebUI Home bundle started.");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        pContext->logger().information("MyIoT WebUI Home bundle stopped.");
    }
};

} } } // namespace MyIoT::WebUI::Home

POCO_BEGIN_NAMED_MANIFEST(WebServer, Poco::OSP::Web::WebRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::Home::LogStreamRequestHandlerFactory)
POCO_END_MANIFEST

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::WebUI::Home::BundleActivator)
POCO_END_MANIFEST
