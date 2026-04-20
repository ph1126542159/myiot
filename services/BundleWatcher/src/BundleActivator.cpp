#include "BundleWatcherService.h"
#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"

namespace MyIoT {
namespace Services {
namespace BundleWatcher {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.start");
        _pServiceInstance = createBundleWatcherService(pContext);
        _pServiceInstance->startWatching();
        _pServiceRef = pContext->registry().registerService(BundleWatcherService::SERVICE_NAME, _pServiceInstance, Poco::OSP::Properties());
        pContext->logger().information("MyIoT Bundle Watcher bundle started.");
        activity.success("bundle watcher ready");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.stop");
        if (_pServiceInstance)
        {
            _pServiceInstance->stopWatching();
        }

        if (_pServiceRef)
        {
            pContext->registry().unregisterService(_pServiceRef);
            _pServiceRef = 0;
        }

        _pServiceInstance = 0;
        pContext->logger().information("MyIoT Bundle Watcher bundle stopped.");
        activity.success("bundle watcher stopped");
    }

private:
    Poco::OSP::ServiceRef::Ptr _pServiceRef;
    BundleWatcherService::Ptr _pServiceInstance;
};

} } } // namespace MyIoT::Services::BundleWatcher

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::Services::BundleWatcher::BundleActivator)
POCO_END_MANIFEST
