#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"

namespace MyIoT {
namespace WebUI {
namespace ProcessConsole {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.start");
        pContext->logger().information("MyIoT WebUI Process Console bundle started.");
        activity.success("web process console ready");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.stop");
        pContext->logger().information("MyIoT WebUI Process Console bundle stopped.");
        activity.success("web process console stopped");
    }
};

} } } // namespace MyIoT::WebUI::ProcessConsole

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::WebUI::ProcessConsole::BundleActivator)
POCO_END_MANIFEST
