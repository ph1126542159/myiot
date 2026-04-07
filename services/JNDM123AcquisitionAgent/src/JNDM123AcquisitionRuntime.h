#ifndef MyIoT_Services_JNDM123AcquisitionAgent_JNDM123AcquisitionRuntime_INCLUDED
#define MyIoT_Services_JNDM123AcquisitionAgent_JNDM123AcquisitionRuntime_INCLUDED

#include "JNDM123AcquisitionDds.h"

#include "Poco/Event.h"
#include "Poco/Mutex.h"
#include "Poco/Runnable.h"
#include "Poco/Thread.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <utility>
#include <string>
#include <vector>

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

class ReaderRunnable;
class PublisherRunnable;

class JNDM123AcquisitionRuntime
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

    struct Cdce937Device
    {
        std::string path;
        int fd = -1;
        std::uint8_t address = 0x6c;
        bool addressFixed = false;
    };
#endif

    explicit JNDM123AcquisitionRuntime(int ddsDomain);
    ~JNDM123AcquisitionRuntime();

    AcquisitionDdsCommandResult handleCommand(const AcquisitionDdsCommand& command);
    void shutdown();

    friend class ReaderRunnable;
    friend class PublisherRunnable;

private:
    AcquisitionDdsSnapshot buildSnapshotLocked(const std::string& requestedDevicePath = std::string());
    void publishSnapshot();
    void readerLoop();
    void publisherLoop();
    std::pair<bool, std::string> startAcquisitionLocked();
    std::pair<bool, std::string> stopAcquisitionLocked(const std::string& message, bool clearHistoryAfterStop);

    void setStatusMessage(const std::string& message);
    void recordError(const std::string& message);
    void clearHistoryLocked();
    void clearHistory();
    void unpackFrame(const std::array<std::uint32_t, kJndm123ChannelCount / 2>& words, std::array<std::int16_t, kJndm123ChannelCount>& samples) const;

#if defined(__linux__)
    void initializeFromSavedConfigurationLocked();
    void ensureMappedHardwareLocked();
    void releaseMappedHardwareLocked();
    void initializeGpioLocked();
    void stopAdcLocked();
    void startAdcLocked();
    void resetFifoLocked();
    void recoverRxFifo();
    int readOneFramePacket(std::array<std::uint32_t, kJndm123ChannelCount / 2>& frame);
#endif

    ReaderRunnable* _readerRunnablePtr = nullptr;
    PublisherRunnable* _publisherRunnablePtr = nullptr;
    Poco::Thread _readerThread;
    Poco::Thread _publisherThread;
    bool _readerStarted = false;
    bool _publisherStarted = false;
    std::atomic<bool> _readerShouldRun{false};
    std::atomic<bool> _running{false};
    std::atomic<bool> _shutdown{false};
    std::atomic<std::uint64_t> _snapshotSequence{0};
    std::atomic<std::uint64_t> _totalFrames{0};
    std::atomic<std::uint64_t> _droppedFrames{0};
    std::atomic<std::uint64_t> _recoveries{0};
    std::atomic<std::uint64_t> _lastFrameSequence{0};
    Poco::FastMutex _controlMutex;
    Poco::FastMutex _stateMutex;
    Poco::Event _publishWake;
    std::array<std::deque<std::int16_t>, kJndm123ChannelCount> _history;
    std::deque<Poco::Int64> _timelineUs;
    std::array<std::int16_t, kJndm123ChannelCount> _latestSamples{};
    bool _hasLatestFrame = false;
    std::string _statusMessage = "Acquisition agent ready.";
    std::string _lastError;
    std::string _lastFrameAt;
    std::string _lastPublishedAt;
    int _ddsDomain = 37;
    AcquisitionSnapshotPublisher _publisher;

#if defined(__linux__)
    MappedHardware _hardware;
#endif
};

class ReaderRunnable: public Poco::Runnable
{
public:
    explicit ReaderRunnable(JNDM123AcquisitionRuntime& owner):
        _owner(owner)
    {
    }

    void run() override;

private:
    JNDM123AcquisitionRuntime& _owner;
};

class PublisherRunnable: public Poco::Runnable
{
public:
    explicit PublisherRunnable(JNDM123AcquisitionRuntime& owner):
        _owner(owner)
    {
    }

    void run() override;

private:
    JNDM123AcquisitionRuntime& _owner;
};

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent

#endif // MyIoT_Services_JNDM123AcquisitionAgent_JNDM123AcquisitionRuntime_INCLUDED
