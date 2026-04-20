#include "IndexRequestHandler.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OpenTelemetry/TelemetryHelpers.h"
#include "Poco/StreamCopier.h"
#include <memory>

namespace MyIoT {
namespace WebUI {
namespace LogViewer {

namespace {

void applyNoCacheHeaders(Poco::Net::HTTPServerResponse& response)
{
    response.set("Cache-Control", "no-cache, no-store, must-revalidate");
    response.set("Pragma", "no-cache");
    response.set("Expires", "0");
}

} // namespace

IndexRequestHandler::IndexRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
    _pContext(pContext)
{
}

void IndexRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    auto activity = Poco::OpenTelemetry::beginRequestActivity(_pContext, request, "logviewer.index");
    std::unique_ptr<std::istream> pResource(_pContext->thisBundle()->getResource("webapp/index.html"));
    if (!pResource)
    {
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
        response.setContentType("text/plain");
        applyNoCacheHeaders(response);
        response.send() << "Not Found";
        Poco::OpenTelemetry::failRequest(activity, Poco::Net::HTTPResponse::HTTP_NOT_FOUND, "index resource not found");
        return;
    }

    response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_OK);
    response.setChunkedTransferEncoding(true);
    response.setContentType("text/html");
    applyNoCacheHeaders(response);

    std::ostream& out = response.send();
    Poco::StreamCopier::copyStream(*pResource, out);
    Poco::OpenTelemetry::succeedRequest(activity, Poco::Net::HTTPResponse::HTTP_OK, "index served");
}

Poco::Net::HTTPRequestHandler* IndexRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest& /*request*/)
{
    return new IndexRequestHandler(context());
}

} } } // namespace MyIoT::WebUI::LogViewer
