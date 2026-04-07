#include "AcquisitionAgentSupport.h"
#include "AcquisitionRuntime.h"

#include "Poco/Util/ServerApplication.h"

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

class AcquisitionAgentApp: public Poco::Util::ServerApplication
{
protected:
    int main(const std::vector<std::string>&) override
    {
        const int ddsDomain = parseIntEnv("MYIOT_JNDM123_AGENT_DDS_DOMAIN", kDefaultDdsDomain);

        AcquisitionRuntime runtime(ddsDomain);
        logger().information("JNDM123 acquisition agent started.");
        waitForTerminationRequest();
        return Application::EXIT_OK;
    }
};

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent

POCO_SERVER_MAIN(MyIoT::Services::JNDM123AcquisitionAgent::AcquisitionAgentApp)
