#include "JNDM123AcquisitionService.h"

#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.start");
        _pService = createJNDM123AcquisitionService(pContext);
        _pServiceRef = pContext->registry().registerService(
            JNDM123AcquisitionService::SERVICE_NAME,
            _pService,
            Poco::OSP::Properties());
        pContext->logger().information("MyIoT JNDM123 acquisition service bundle started.");
        activity.success("jndm123 acquisition service ready");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.stop");
        if (_pServiceRef)
        {
            pContext->registry().unregisterService(_pServiceRef);
            _pServiceRef = nullptr;
        }
        _pService = nullptr;
        pContext->logger().information("MyIoT JNDM123 acquisition service bundle stopped.");
        activity.success("jndm123 acquisition service stopped");
    }

private:
    Poco::OSP::ServiceRef::Ptr _pServiceRef;
    JNDM123AcquisitionService::Ptr _pService;
};

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::Services::JNDM123AcquisitionAgent::BundleActivator)
POCO_END_MANIFEST
