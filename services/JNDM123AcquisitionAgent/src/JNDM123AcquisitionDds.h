#ifndef MyIoT_Services_JNDM123AcquisitionAgent_JNDM123AcquisitionDds_INCLUDED
#define MyIoT_Services_JNDM123AcquisitionAgent_JNDM123AcquisitionDds_INCLUDED

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <fastdds/dds/core/policy/QosPolicies.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TopicDataType.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

constexpr std::size_t kJndm123DividerOutputCount = 7;
constexpr std::size_t kJndm123Ad7606Count = 6;
constexpr std::size_t kJndm123ChannelsPerChip = 8;
constexpr std::size_t kJndm123ChannelCount = kJndm123Ad7606Count * kJndm123ChannelsPerChip;

enum class AcquisitionCommandKind: std::uint32_t
{
    None = 0,
    Status = 1,
    Start = 2,
    Stop = 3,
    ApplyDividers = 4,
    UpdateReferenceClock = 5
};

struct AcquisitionDdsCommand
{
    std::string clientId;
    std::uint64_t requestId = 0;
    AcquisitionCommandKind kind = AcquisitionCommandKind::None;
    std::string devicePath;
    std::uint64_t referenceClockHz = 0;
    std::uint32_t outputMask = 0;
    std::int32_t divider = 0;
    std::string message;
};

struct AcquisitionDdsSnapshot
{
    std::uint64_t sequence = 0;
    std::string updatedAt;
    bool ok = true;
    std::string message;
    bool running = false;
    bool previewActive = true;
    std::uint32_t historyLimit = 0;
    std::uint64_t queueDepth = 0;
    std::uint64_t totalFrames = 0;
    std::uint64_t droppedFrames = 0;
    std::uint64_t recoveries = 0;
    std::uint64_t lastFrameSequence = 0;
    std::string lastFrameAt;
    std::string waveformUpdatedAt;
    std::string lastError;
    std::string devicePath;
    std::uint64_t referenceClockHz = 0;
    std::string deviceType;
    std::string address;
    std::string inputClock;
    std::int32_t revisionId = 0;
    bool eepBusy = false;
    bool eepLock = false;
    bool powerDown = false;
    std::array<std::int32_t, kJndm123DividerOutputCount> dividerValues{};
    std::array<double, kJndm123DividerOutputCount> dividerFrequenciesHz{};
    std::array<bool, kJndm123DividerOutputCount> dividerEnabled{};
    std::array<bool, kJndm123ChannelCount> latestValueValid{};
    std::array<std::int32_t, kJndm123ChannelCount> latestValues{};
    std::vector<std::int64_t> timelineUs;
    std::vector<std::int32_t> historySamples;
};

struct AcquisitionDdsCommandResult
{
    std::string clientId;
    std::uint64_t requestId = 0;
    bool ok = true;
    std::string message;
    AcquisitionDdsSnapshot snapshot;
};

using AcquisitionSnapshotHandler = std::function<void(const AcquisitionDdsSnapshot&)>;
using AcquisitionCommandHandler = std::function<AcquisitionDdsCommandResult(const AcquisitionDdsCommand&)>;

class AcquisitionSnapshotPublisher
{
public:
    AcquisitionSnapshotPublisher();
    ~AcquisitionSnapshotPublisher();

    void start(int domainId);
    void stop();
    bool publish(const AcquisitionDdsSnapshot& snapshot);

private:
    eprosima::fastdds::dds::DomainParticipant* _participant = nullptr;
    eprosima::fastdds::dds::Publisher* _publisher = nullptr;
    eprosima::fastdds::dds::Topic* _topic = nullptr;
    eprosima::fastdds::dds::DataWriter* _writer = nullptr;
    eprosima::fastdds::dds::TypeSupport _type;
};

class AcquisitionSnapshotSubscriber
{
public:
    AcquisitionSnapshotSubscriber();
    ~AcquisitionSnapshotSubscriber();

    void start(int domainId, AcquisitionSnapshotHandler handler);
    void stop();

private:
    class ReaderListener;

    eprosima::fastdds::dds::DomainParticipant* _participant = nullptr;
    eprosima::fastdds::dds::Subscriber* _subscriber = nullptr;
    eprosima::fastdds::dds::Topic* _topic = nullptr;
    eprosima::fastdds::dds::DataReader* _reader = nullptr;
    eprosima::fastdds::dds::TypeSupport _type;
    std::unique_ptr<ReaderListener> _listener;
};

class AcquisitionCommandServer
{
public:
    AcquisitionCommandServer();
    ~AcquisitionCommandServer();

    void start(int domainId, AcquisitionCommandHandler handler);
    void stop();

private:
    class ReaderListener;

    bool publishResult(const AcquisitionDdsCommandResult& result);

    eprosima::fastdds::dds::DomainParticipant* _participant = nullptr;
    eprosima::fastdds::dds::Publisher* _publisher = nullptr;
    eprosima::fastdds::dds::Subscriber* _subscriber = nullptr;
    eprosima::fastdds::dds::Topic* _commandTopic = nullptr;
    eprosima::fastdds::dds::Topic* _resultTopic = nullptr;
    eprosima::fastdds::dds::DataWriter* _resultWriter = nullptr;
    eprosima::fastdds::dds::DataReader* _commandReader = nullptr;
    eprosima::fastdds::dds::TypeSupport _commandType;
    eprosima::fastdds::dds::TypeSupport _resultType;
    std::unique_ptr<ReaderListener> _listener;
    AcquisitionCommandHandler _handler;
};

class AcquisitionCommandClient
{
public:
    AcquisitionCommandClient();
    ~AcquisitionCommandClient();

    void start(int domainId, const std::string& clientId = std::string());
    void stop();
    AcquisitionDdsCommandResult sendCommand(const AcquisitionDdsCommand& command, long timeoutMs = 3000);
    const std::string& clientId() const;

private:
    class ReaderListener;

    struct PendingRequest
    {
        bool completed = false;
        AcquisitionDdsCommandResult result;
    };

    void deliverResult(const AcquisitionDdsCommandResult& result);

    eprosima::fastdds::dds::DomainParticipant* _participant = nullptr;
    eprosima::fastdds::dds::Publisher* _publisher = nullptr;
    eprosima::fastdds::dds::Subscriber* _subscriber = nullptr;
    eprosima::fastdds::dds::Topic* _commandTopic = nullptr;
    eprosima::fastdds::dds::Topic* _resultTopic = nullptr;
    eprosima::fastdds::dds::DataWriter* _commandWriter = nullptr;
    eprosima::fastdds::dds::DataReader* _resultReader = nullptr;
    eprosima::fastdds::dds::TypeSupport _commandType;
    eprosima::fastdds::dds::TypeSupport _resultType;
    std::unique_ptr<ReaderListener> _listener;
    std::string _clientId;
    std::atomic<std::uint64_t> _nextRequestId{1};
    mutable std::mutex _mutex;
    std::condition_variable _condition;
    std::unordered_map<std::uint64_t, PendingRequest> _pending;
};

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent

#endif // MyIoT_Services_JNDM123AcquisitionAgent_JNDM123AcquisitionDds_INCLUDED
