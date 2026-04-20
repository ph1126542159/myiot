#include "SystemMetricsRequestHandler.h"
#include "TelemetrySnapshotRequestHandler.h"
#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"

namespace MyIoT {
namespace WebUI {
namespace SystemMonitor {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.start");
        pContext->logger().information("MyIoT WebUI System Monitor bundle started.");
        activity.success("system monitor ready");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.stop");
        pContext->logger().information("MyIoT WebUI System Monitor bundle stopped.");
        activity.success("system monitor stopped");
    }
};

} } } // namespace MyIoT::WebUI::SystemMonitor

POCO_BEGIN_NAMED_MANIFEST(WebServer, Poco::OSP::Web::WebRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::SystemMonitor::SystemMetricsRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::SystemMonitor::TelemetrySnapshotRequestHandlerFactory)
POCO_END_MANIFEST

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::WebUI::SystemMonitor::BundleActivator)
POCO_END_MANIFEST
