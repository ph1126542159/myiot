#include "JNDM123AcquisitionDds.h"
#include "JNDM123AcquisitionRuntime.h"

#include "Poco/Environment.h"
#include "Poco/Logger.h"
#include "Poco/NumberParser.h"
#include "Poco/Util/ServerApplication.h"

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

namespace {

constexpr int kDefaultDdsDomain = 37;

Poco::Logger& logger()
{
    static Poco::Logger& instance = Poco::Logger::get("MyIoT.JNDM123.AcquisitionAgent");
    return instance;
}

int parseIntEnv(const char* name, int fallback)
{
    try
    {
        if (!Poco::Environment::has(name)) return fallback;
        return Poco::NumberParser::parse(Poco::Environment::get(name));
    }
    catch (...)
    {
        return fallback;
    }
}

class AcquisitionAgentApp: public Poco::Util::ServerApplication
{
protected:
    int main(const std::vector<std::string>&) override
    {
        const int ddsDomain = parseIntEnv("MYIOT_JNDM123_AGENT_DDS_DOMAIN", kDefaultDdsDomain);

        JNDM123AcquisitionRuntime runtime(ddsDomain);
        AcquisitionCommandServer commandServer;
        commandServer.start(ddsDomain, [&](const AcquisitionDdsCommand& command) {
            return runtime.handleCommand(command);
        });

        logger().information("JNDM123 acquisition agent started with DDS command transport.");
        waitForTerminationRequest();
        commandServer.stop();
        runtime.shutdown();
        return Application::EXIT_OK;
    }
};

} // namespace

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent

POCO_SERVER_MAIN(MyIoT::Services::JNDM123AcquisitionAgent::AcquisitionAgentApp)
