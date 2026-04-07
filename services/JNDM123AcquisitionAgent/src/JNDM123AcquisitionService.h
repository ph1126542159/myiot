#ifndef MyIoT_Services_JNDM123AcquisitionAgent_JNDM123AcquisitionService_INCLUDED
#define MyIoT_Services_JNDM123AcquisitionAgent_JNDM123AcquisitionService_INCLUDED

#include "Poco/AutoPtr.h"
#include "Poco/JSON/Object.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Service.h"

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

class JNDM123AcquisitionService: public Poco::OSP::Service
{
public:
    using Ptr = Poco::AutoPtr<JNDM123AcquisitionService>;

    static const std::string SERVICE_NAME;

    virtual Poco::JSON::Object::Ptr restartProcess() = 0;
    virtual Poco::JSON::Object::Ptr serviceStatus() = 0;
};

JNDM123AcquisitionService::Ptr createJNDM123AcquisitionService(Poco::OSP::BundleContext::Ptr pContext);

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent

#endif // MyIoT_Services_JNDM123AcquisitionAgent_JNDM123AcquisitionService_INCLUDED
