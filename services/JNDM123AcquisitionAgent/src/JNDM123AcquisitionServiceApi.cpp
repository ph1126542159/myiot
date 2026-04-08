#include "JNDM123AcquisitionService.h"

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

const std::string JNDM123AcquisitionService::SERVICE_NAME("io.myiot.services.jndm123acquisition");

JNDM123AcquisitionService::~JNDM123AcquisitionService() = default;

const std::type_info& JNDM123AcquisitionService::type() const
{
    return typeid(JNDM123AcquisitionService);
}

bool JNDM123AcquisitionService::isA(const std::type_info& otherType) const
{
    return std::string(typeid(JNDM123AcquisitionService).name()) == otherType.name()
        || Poco::OSP::Service::isA(otherType);
}

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
