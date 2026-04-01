#include "GlobalConfigRequestHandler.h"
#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"

namespace MyIoT {
namespace WebUI {
namespace GlobalConfig {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        pContext->logger().information("MyIoT WebUI Global Config bundle started.");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        pContext->logger().information("MyIoT WebUI Global Config bundle stopped.");
    }
};

} } } // namespace MyIoT::WebUI::GlobalConfig

POCO_BEGIN_NAMED_MANIFEST(WebServer, Poco::OSP::Web::WebRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::GlobalConfig::GlobalConfigRequestHandlerFactory)
POCO_END_MANIFEST

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::WebUI::GlobalConfig::BundleActivator)
POCO_END_MANIFEST
