#include "ConsoleCommandRequestHandler.h"
#include "ProcessConsoleService.h"
#include "TerminalWebSocketRequestHandler.h"
#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"

namespace MyIoT {
namespace Services {
namespace ProcessConsole {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.start");
        _pServiceInstance = createProcessConsoleService(pContext);
        _pServiceRef = pContext->registry().registerService(ProcessConsoleService::SERVICE_NAME, _pServiceInstance, Poco::OSP::Properties());
        pContext->logger().information("MyIoT Process Console bundle started.");
        activity.success("process console ready");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        auto activity = Poco::OpenTelemetry::beginBundleActivity(pContext, "bundle.stop");
        if (_pServiceRef)
        {
            pContext->registry().unregisterService(_pServiceRef);
            _pServiceRef = 0;
        }

        _pServiceInstance = 0;
        pContext->logger().information("MyIoT Process Console bundle stopped.");
        activity.success("process console stopped");
    }

private:
    Poco::OSP::ServiceRef::Ptr _pServiceRef;
    ProcessConsoleService::Ptr _pServiceInstance;
};

} } } // namespace MyIoT::Services::ProcessConsole

POCO_BEGIN_NAMED_MANIFEST(WebServer, Poco::OSP::Web::WebRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::Services::ProcessConsole::ConsoleCommandRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::Services::ProcessConsole::TerminalWebSocketRequestHandlerFactory)
POCO_END_MANIFEST

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::Services::ProcessConsole::BundleActivator)
POCO_END_MANIFEST
