#ifndef MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionRuntime_INCLUDED
#define MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionRuntime_INCLUDED

#include "AcquisitionAgentSupport.h"
#include "AcquisitionDivider.h"
#include "AcquisitionDdsJsonMessage.h"

#include "Poco/Event.h"
#include "Poco/JSON/Object.h"
#include "Poco/Mutex.h"
#include "Poco/Runnable.h"
#include "Poco/Thread.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <string>

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

class AcquisitionRuntime;

class ReaderRunnable: public Poco::Runnable
{
public:
    explicit ReaderRunnable(AcquisitionRuntime& owner);
    void run() override;

private:
    AcquisitionRuntime& _owner;
};

class PublisherRunnable: public Poco::Runnable
{
public:
    explicit PublisherRunnable(AcquisitionRuntime& owner);
    void run() override;

private:
    AcquisitionRuntime& _owner;
};

class ControlListener: public AcquisitionDdsSubscriberListener
{
public:
    explicit ControlListener(AcquisitionRuntime& runtime);
    void onMessage(const AcquisitionDdsJsonMessage& message) override;

private:
    AcquisitionRuntime& _runtime;
};

class AcquisitionRuntime
{
public:
#if defined(__linux__)
    struct MappedHardware
    {
        int memFd = -1;
        volatile std::uint32_t* fifoCtrl = nullptr;
        volatile std::uint32_t* fifoData = nullptr;
        bool gpioInitialized = false;
    };
#endif

    explicit AcquisitionRuntime(int ddsDomain);
    ~AcquisitionRuntime();

    Poco::JSON::Object::Ptr startAcquisition();
    Poco::JSON::Object::Ptr stopAcquisition(const std::string& message);
    Poco::JSON::Object::Ptr snapshot();

    void readerLoop();
    void publisherLoop();
    void shutdown();
    void handleControlMessage(const AcquisitionDdsJsonMessage& message);

private:
    void publishSnapshot();
    Poco::JSON::Object::Ptr startAcquisitionLocked();
    void stopAcquisitionLocked(const std::string& message);
    void appendHistoryLocked(const std::array<std::int16_t, kFrameColumns>& samples, Poco::Int64 capturedAtUs);
    void clearHistoryLocked();
    void updateDividerSnapshotLocked(const DividerSnapshot& snapshot);
    void unpackFrame(
        const std::array<std::uint32_t, kFrameWords>& words,
        std::array<std::int16_t, kFrameColumns>& samples) const;

#if defined(__linux__)
    void ensureMappedHardwareLocked();
    void releaseMappedHardwareLocked();
    void initializeGpioLocked();
    void stopAdcLocked();
    void startAdcLocked();
    void resetFifoLocked();
    void recoverRxFifo();
    int readOneFramePacket(std::array<std::uint32_t, kFrameWords>& frame);
#endif

    ReaderRunnable _readerRunnable;
    PublisherRunnable _publisherRunnable;
    Poco::Thread _readerThread;
    Poco::Thread _publisherThread;
    bool _readerStarted = false;
    bool _publisherStarted = false;
    std::atomic<bool> _readerShouldRun{false};
    std::atomic<bool> _running{false};
    std::atomic<bool> _shutdown{false};
    std::atomic<Poco::UInt64> _totalFrames{0};
    std::atomic<Poco::UInt64> _droppedFrames{0};
    std::atomic<Poco::UInt64> _recoveries{0};
    std::atomic<Poco::UInt64> _lastFrameSequence{0};
    Poco::FastMutex _controlMutex;
    Poco::FastMutex _stateMutex;
    Poco::Event _publishWake;
    std::array<std::deque<std::int16_t>, kFrameColumns> _history;
    std::deque<Poco::Int64> _timelineUs;
    std::array<std::int16_t, kFrameColumns> _latestSamples{};
    bool _hasLatestFrame = false;
    std::string _statusMessage = "Acquisition agent ready.";
    std::string _lastError;
    bool _lastCommandOk = true;
    std::string _lastFrameAt;
    std::string _lastPublishedAt;
    std::string _lastCommandAction = "status";
    std::string _lastCommandUpdatedAt;
    std::string _latestDividerPayloadText;
    int _ddsDomain = kDefaultDdsDomain;
    AcquisitionDdsPublisher _publisher;
    AcquisitionDdsSubscriber _commandSubscriber;
    std::atomic<Poco::UInt64> _lastCommandSequence{0};

#if defined(__linux__)
    MappedHardware _hardware;
#endif

    ControlListener _controlListener;
};

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent

#endif // MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionRuntime_INCLUDED
