#ifndef MyIoT_WebUI_JNDM123_JNDM123RequestHandler_INCLUDED
#define MyIoT_WebUI_JNDM123_JNDM123RequestHandler_INCLUDED

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

void stopJNDM123Runtime();

class DividerRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
    explicit DividerRequestHandler(Poco::OSP::BundleContext::Ptr pContext);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
};

class AcquisitionRequestHandler: public Poco::Net::HTTPRequestHandler
{
public:
    explicit AcquisitionRequestHandler(Poco::OSP::BundleContext::Ptr pContext);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    Poco::OSP::BundleContext::Ptr _pContext;
};

class DividerRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

class AcquisitionRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
{
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};

} } } // namespace MyIoT::WebUI::JNDM123

#endif // MyIoT_WebUI_JNDM123_JNDM123RequestHandler_INCLUDED
