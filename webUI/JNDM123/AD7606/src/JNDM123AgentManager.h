#ifndef MyIoT_WebUI_JNDM123_JNDM123AgentManager_INCLUDED
#define MyIoT_WebUI_JNDM123_JNDM123AgentManager_INCLUDED

#include "Poco/JSON/Object.h"

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

Poco::JSON::Object::Ptr acquisitionProcessStatusOrThrow();
Poco::JSON::Object::Ptr restartAcquisitionProcessOrThrow();

} } } // namespace MyIoT::WebUI::JNDM123

#endif // MyIoT_WebUI_JNDM123_JNDM123AgentManager_INCLUDED
