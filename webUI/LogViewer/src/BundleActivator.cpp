#include "IndexRequestHandler.h"
#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"

namespace MyIoT {
namespace WebUI {
namespace LogViewer {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.start");
        pContext->logger().information("MyIoT WebUI Log Viewer bundle started.");
        activity.success("log viewer ready");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.stop");
        pContext->logger().information("MyIoT WebUI Log Viewer bundle stopped.");
        activity.success("log viewer stopped");
    }
};

} } } // namespace MyIoT::WebUI::LogViewer

POCO_BEGIN_NAMED_MANIFEST(WebServer, Poco::OSP::Web::WebRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::LogViewer::IndexRequestHandlerFactory)
POCO_END_MANIFEST

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::WebUI::LogViewer::BundleActivator)
POCO_END_MANIFEST
