#include "JNDM123RequestHandler.h"
#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        pContext->logger().information("MyIoT WebUI JNDM123 bundle started.");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        stopJNDM123Runtime();
        pContext->logger().information("MyIoT WebUI JNDM123 bundle stopped.");
    }
};

} } } // namespace MyIoT::WebUI::JNDM123

POCO_BEGIN_NAMED_MANIFEST(WebServer, Poco::OSP::Web::WebRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::JNDM123::DividerRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::JNDM123::AcquisitionRequestHandlerFactory)
POCO_END_MANIFEST

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::WebUI::JNDM123::BundleActivator)
POCO_END_MANIFEST
