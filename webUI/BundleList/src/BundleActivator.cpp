#include "BundleCatalogRequestHandler.h"
#include "Poco/ClassLibrary.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"

namespace MyIoT {
namespace WebUI {
namespace BundleList {

class BundleActivator: public Poco::OSP::BundleActivator
{
public:
    void start(Poco::OSP::BundleContext::Ptr pContext) override
    {
        pContext->logger().information("MyIoT WebUI Bundle List bundle started.");
    }

    void stop(Poco::OSP::BundleContext::Ptr pContext) override
    {
        pContext->logger().information("MyIoT WebUI Bundle List bundle stopped.");
    }
};

} } } // namespace MyIoT::WebUI::BundleList

POCO_BEGIN_NAMED_MANIFEST(WebServer, Poco::OSP::Web::WebRequestHandlerFactory)
    POCO_EXPORT_CLASS(MyIoT::WebUI::BundleList::BundleCatalogRequestHandlerFactory)
POCO_END_MANIFEST

POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
    POCO_EXPORT_CLASS(MyIoT::WebUI::BundleList::BundleActivator)
POCO_END_MANIFEST
