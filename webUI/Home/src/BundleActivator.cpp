#include "LogStreamRequestHandler.h"
#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"

namespace MyIoT {
namespace WebUI {
namespace Home {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.start");
        LogStreamRequestLifecycle::beginStartup();
        pContext->logger().information("MyIoT WebUI Home bundle started.");
        activity.success("home bundle ready");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.stop");
        LogStreamRequestLifecycle::beginShutdown(pContext);
        pContext->logger().information("MyIoT WebUI Home bundle stopped.");
        activity.success("home bundle stopped");
    }
};

} } } // namespace MyIoT::WebUI::Home

POCO_BEGIN_NAMED_MANIFEST(WebServer, Poco::OSP::Web::WebRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::Home::LogStreamRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::Home::LogStreamEventsRequestHandlerFactory)
POCO_END_MANIFEST

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::WebUI::Home::BundleActivator)
POCO_END_MANIFEST
