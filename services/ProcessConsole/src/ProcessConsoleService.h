#ifndef MyIoT_Services_ProcessConsole_ProcessConsoleService_INCLUDED
#define MyIoT_Services_ProcessConsole_ProcessConsoleService_INCLUDED

#include "Poco/AutoPtr.h"
#include "Poco/JSON/Object.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Service.h"

namespace MyIoT {
namespace Services {
namespace ProcessConsole {

class ProcessConsoleService: public Poco::OSP::Service
{
public:
    using Ptr = Poco::AutoPtr<ProcessConsoleService>;

    static const std::string SERVICE_NAME;

    virtual Poco::JSON::Object::Ptr execute(const std::string& commandLine, int limit = -1) = 0;
};

ProcessConsoleService::Ptr createProcessConsoleService(Poco::OSP::BundleContext::Ptr pContext);

} } } // namespace MyIoT::Services::ProcessConsole

#endif // MyIoT_Services_ProcessConsole_ProcessConsoleService_INCLUDED
