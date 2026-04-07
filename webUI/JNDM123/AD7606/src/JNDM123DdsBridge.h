#ifndef MyIoT_WebUI_JNDM123_JNDM123DdsBridge_INCLUDED
#define MyIoT_WebUI_JNDM123_JNDM123DdsBridge_INCLUDED

#include "JNDM123Support.h"

#include "Poco/Mutex.h"

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

class DdsAcquisitionBridge: public MyIoT::Services::JNDM123AcquisitionAgent::AcquisitionDdsSubscriberListener
{
public:
    static DdsAcquisitionBridge& instance();

    void onMessage(const MyIoT::Services::JNDM123AcquisitionAgent::AcquisitionDdsJsonMessage& message) override;

    Object::Ptr latestSnapshot();
    bool isRunning();
    Object::Ptr sendCommandAndAwait(
        const std::string& action,
        const std::string& message,
        Object::Ptr arguments,
        bool restartOnTimeout = true);
    Object::Ptr sendCommandAndAwait(
        const std::string& action,
        const std::string& message,
        bool restartOnTimeout = true);
    void resetSnapshotCache();

private:
    DdsAcquisitionBridge();
    void ensureStarted();

    Poco::FastMutex _mutex;
    MyIoT::Services::JNDM123AcquisitionAgent::AcquisitionDdsPublisher _publisher;
    MyIoT::Services::JNDM123AcquisitionAgent::AcquisitionDdsSubscriber _subscriber;
    bool _started = false;
    std::string _latestPayloadText;
    std::string _lastSnapshotUpdatedAt;
    Poco::UInt64 _lastCommandSequenceSeen = 0;
    std::atomic<Poco::UInt64> _nextCommandSequence{0};
};

bool remoteAcquisitionRunning();

} } } // namespace MyIoT::WebUI::JNDM123

#endif // MyIoT_WebUI_JNDM123_JNDM123DdsBridge_INCLUDED
