#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"

namespace MyIoT {
namespace WebUI {
namespace LogViewer {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        pContext->logger().information("MyIoT WebUI Log Viewer bundle started.");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        pContext->logger().information("MyIoT WebUI Log Viewer bundle stopped.");
    }
};

} } } // namespace MyIoT::WebUI::LogViewer

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::WebUI::LogViewer::BundleActivator)
POCO_END_MANIFEST
