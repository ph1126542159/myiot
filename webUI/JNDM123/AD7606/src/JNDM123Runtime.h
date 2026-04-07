#ifndef MyIoT_WebUI_JNDM123_JNDM123Runtime_INCLUDED
#define MyIoT_WebUI_JNDM123_JNDM123Runtime_INCLUDED

#include "JNDM123Support.h"

#include "Poco/Mutex.h"

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

class JNDM123Runtime
{
public:
    static JNDM123Runtime& instance();

    void initializeFromSavedConfiguration();
    DividerSnapshot readDividerStatus(const std::string& devicePath);
    DividerSnapshot applyDivider(const std::string& devicePath, int outputIndex, int divider, Poco::UInt64 referenceClockHz);
    DividerSnapshot applyDividers(const std::string& devicePath, const std::vector<int>& outputIndices, int divider, Poco::UInt64 referenceClockHz);
    DividerSnapshot updateReferenceClock(const std::string& devicePath, Poco::UInt64 referenceClockHz);
    AcquisitionActionResult startAcquisition();
    AcquisitionActionResult stopAcquisition(const std::string& message = "Acquisition stopped by operator.");
    void touchPreviewLease();
    Object::Ptr acquisitionSnapshot(bool includeWaveform);
    void shutdown();

private:
    JNDM123Runtime() = default;
    ~JNDM123Runtime() = default;
    JNDM123Runtime(const JNDM123Runtime&) = delete;
    JNDM123Runtime& operator=(const JNDM123Runtime&) = delete;

    Poco::FastMutex _controlMutex;
};

} } } // namespace MyIoT::WebUI::JNDM123

#endif // MyIoT_WebUI_JNDM123_JNDM123Runtime_INCLUDED
