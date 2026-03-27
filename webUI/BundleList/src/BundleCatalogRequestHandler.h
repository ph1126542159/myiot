#ifndef MyIoT_WebUI_BundleList_BundleCatalogRequestHandler_INCLUDED
#define MyIoT_WebUI_BundleList_BundleCatalogRequestHandler_INCLUDED

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"

namespace MyIoT {
namespace WebUI {
namespace BundleList {

class BundleCatalogRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
    explicit BundleCatalogRequestHandler(Poco::OSP::BundleContext::Ptr pContext);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
};

class BundleCatalogRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

} } } // namespace MyIoT::WebUI::BundleList

#endif // MyIoT_WebUI_BundleList_BundleCatalogRequestHandler_INCLUDED
