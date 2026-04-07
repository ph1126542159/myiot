#include "JNDM123AcquisitionDds.h"

#include <chrono>
#include <stdexcept>

#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

namespace {

using namespace eprosima::fastdds::dds;

constexpr const char* kSnapshotTopicName = "MyIoT.JNDM123.Acquisition.Snapshot";
constexpr const char* kSnapshotTypeName = "MyIoT.JNDM123.Acquisition.Snapshot.v1";
constexpr const char* kCommandTopicName = "MyIoT.JNDM123.Acquisition.Command";
constexpr const char* kCommandTypeName = "MyIoT.JNDM123.Acquisition.Command.v1";
constexpr const char* kResultTopicName = "MyIoT.JNDM123.Acquisition.CommandResult";
constexpr const char* kResultTypeName = "MyIoT.JNDM123.Acquisition.CommandResult.v1";
constexpr uint32_t kMaxCommandSize = 64 * 1024;
constexpr uint32_t kMaxSnapshotSize = 8 * 1024 * 1024;
constexpr uint32_t kMaxResultSize = 8 * 1024 * 1024;

template <typename T, std::size_t N>
void write_array(eprosima::fastcdr::Cdr& serializer, const std::array<T, N>& values)
{
    for (const auto& value: values)
    {
        serializer << value;
    }
}

template <typename T, std::size_t N>
void read_array(eprosima::fastcdr::Cdr& deserializer, std::array<T, N>& values)
{
    for (auto& value: values)
    {
        deserializer >> value;
    }
}

template <typename T>
void write_vector(eprosima::fastcdr::Cdr& serializer, const std::vector<T>& values)
{
    serializer << static_cast<uint32_t>(values.size());
    for (const auto& value: values)
    {
        serializer << value;
    }
}

template <typename T>
void read_vector(eprosima::fastcdr::Cdr& deserializer, std::vector<T>& values)
{
    uint32_t size = 0;
    deserializer >> size;
    values.resize(size);
    for (auto& value: values)
    {
        deserializer >> value;
    }
}

void serialize_snapshot(eprosima::fastcdr::Cdr& serializer, const AcquisitionDdsSnapshot& snapshot)
{
    serializer << snapshot.sequence;
    serializer << snapshot.updatedAt;
    serializer << snapshot.ok;
    serializer << snapshot.message;
    serializer << snapshot.running;
    serializer << snapshot.previewActive;
    serializer << snapshot.historyLimit;
    serializer << snapshot.queueDepth;
    serializer << snapshot.totalFrames;
    serializer << snapshot.droppedFrames;
    serializer << snapshot.recoveries;
    serializer << snapshot.lastFrameSequence;
    serializer << snapshot.lastFrameAt;
    serializer << snapshot.waveformUpdatedAt;
    serializer << snapshot.lastError;
    serializer << snapshot.devicePath;
    serializer << snapshot.referenceClockHz;
    serializer << snapshot.deviceType;
    serializer << snapshot.address;
    serializer << snapshot.inputClock;
    serializer << snapshot.revisionId;
    serializer << snapshot.eepBusy;
    serializer << snapshot.eepLock;
    serializer << snapshot.powerDown;
    write_array(serializer, snapshot.dividerValues);
    write_array(serializer, snapshot.dividerFrequenciesHz);
    write_array(serializer, snapshot.dividerEnabled);
    write_array(serializer, snapshot.latestValueValid);
    write_array(serializer, snapshot.latestValues);
    write_vector(serializer, snapshot.timelineUs);
    write_vector(serializer, snapshot.historySamples);
}

void deserialize_snapshot(eprosima::fastcdr::Cdr& deserializer, AcquisitionDdsSnapshot& snapshot)
{
    deserializer >> snapshot.sequence;
    deserializer >> snapshot.updatedAt;
    deserializer >> snapshot.ok;
    deserializer >> snapshot.message;
    deserializer >> snapshot.running;
    deserializer >> snapshot.previewActive;
    deserializer >> snapshot.historyLimit;
    deserializer >> snapshot.queueDepth;
    deserializer >> snapshot.totalFrames;
    deserializer >> snapshot.droppedFrames;
    deserializer >> snapshot.recoveries;
    deserializer >> snapshot.lastFrameSequence;
    deserializer >> snapshot.lastFrameAt;
    deserializer >> snapshot.waveformUpdatedAt;
    deserializer >> snapshot.lastError;
    deserializer >> snapshot.devicePath;
    deserializer >> snapshot.referenceClockHz;
    deserializer >> snapshot.deviceType;
    deserializer >> snapshot.address;
    deserializer >> snapshot.inputClock;
    deserializer >> snapshot.revisionId;
    deserializer >> snapshot.eepBusy;
    deserializer >> snapshot.eepLock;
    deserializer >> snapshot.powerDown;
    read_array(deserializer, snapshot.dividerValues);
    read_array(deserializer, snapshot.dividerFrequenciesHz);
    read_array(deserializer, snapshot.dividerEnabled);
    read_array(deserializer, snapshot.latestValueValid);
    read_array(deserializer, snapshot.latestValues);
    read_vector(deserializer, snapshot.timelineUs);
    read_vector(deserializer, snapshot.historySamples);
}

uint32_t serialized_size(const AcquisitionDdsSnapshot& snapshot)
{
    return static_cast<uint32_t>(
        2048u
        + snapshot.updatedAt.size()
        + snapshot.message.size()
        + snapshot.lastFrameAt.size()
        + snapshot.waveformUpdatedAt.size()
        + snapshot.lastError.size()
        + snapshot.devicePath.size()
        + snapshot.deviceType.size()
        + snapshot.address.size()
        + snapshot.inputClock.size()
        + snapshot.timelineUs.size() * sizeof(std::int64_t)
        + snapshot.historySamples.size() * sizeof(std::int32_t));
}

void serialize_command(eprosima::fastcdr::Cdr& serializer, const AcquisitionDdsCommand& command)
{
    serializer << command.clientId;
    serializer << command.requestId;
    serializer << static_cast<uint32_t>(command.kind);
    serializer << command.devicePath;
    serializer << command.referenceClockHz;
    serializer << command.outputMask;
    serializer << command.divider;
    serializer << command.message;
}

void deserialize_command(eprosima::fastcdr::Cdr& deserializer, AcquisitionDdsCommand& command)
{
    uint32_t kind = 0;

    deserializer >> command.clientId;
    deserializer >> command.requestId;
    deserializer >> kind;
    deserializer >> command.devicePath;
    deserializer >> command.referenceClockHz;
    deserializer >> command.outputMask;
    deserializer >> command.divider;
    deserializer >> command.message;
    command.kind = static_cast<AcquisitionCommandKind>(kind);
}

uint32_t serialized_size(const AcquisitionDdsCommand& command)
{
    return static_cast<uint32_t>(
        256u
        + command.clientId.size()
        + command.devicePath.size()
        + command.message.size());
}

void serialize_result(eprosima::fastcdr::Cdr& serializer, const AcquisitionDdsCommandResult& result)
{
    serializer << result.clientId;
    serializer << result.requestId;
    serializer << result.ok;
    serializer << result.message;
    serialize_snapshot(serializer, result.snapshot);
}

void deserialize_result(eprosima::fastcdr::Cdr& deserializer, AcquisitionDdsCommandResult& result)
{
    deserializer >> result.clientId;
    deserializer >> result.requestId;
    deserializer >> result.ok;
    deserializer >> result.message;
    deserialize_snapshot(deserializer, result.snapshot);
}

uint32_t serialized_size(const AcquisitionDdsCommandResult& result)
{
    return static_cast<uint32_t>(256u + result.clientId.size() + result.message.size() + serialized_size(result.snapshot));
}

template <typename Message,
        void (*SerializeFn)(eprosima::fastcdr::Cdr&, const Message&),
        void (*DeserializeFn)(eprosima::fastcdr::Cdr&, Message&),
        uint32_t (*SizeFn)(const Message&)>
class BasicTopicDataType: public TopicDataType
{
public:
    BasicTopicDataType(const char* typeName, uint32_t maxSerializedSize)
    {
        set_name(typeName);
        max_serialized_type_size = maxSerializedSize;
        is_compute_key_provided = false;
    }

    bool serialize(
            const void* const data,
            eprosima::fastdds::rtps::SerializedPayload_t& payload,
            DataRepresentationId_t) override
    {
        const auto* message = static_cast<const Message*>(data);
        eprosima::fastcdr::FastBuffer buffer(reinterpret_cast<char*>(payload.data), payload.max_size);
        eprosima::fastcdr::Cdr serializer(
            buffer,
            eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
            eprosima::fastcdr::CdrVersion::XCDRv1);

        payload.encapsulation = serializer.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS ? CDR_BE : CDR_LE;
        serializer.serialize_encapsulation();
        SerializeFn(serializer, *message);
        payload.length = static_cast<uint32_t>(serializer.get_serialized_data_length());
        return true;
    }

    bool deserialize(
            eprosima::fastdds::rtps::SerializedPayload_t& payload,
            void* data) override
    {
        auto* message = static_cast<Message*>(data);
        eprosima::fastcdr::FastBuffer buffer(reinterpret_cast<char*>(payload.data), payload.length);
        eprosima::fastcdr::Cdr deserializer(
            buffer,
            eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
            eprosima::fastcdr::CdrVersion::XCDRv1);

        deserializer.read_encapsulation();
        payload.encapsulation = deserializer.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS ? CDR_BE : CDR_LE;
        DeserializeFn(deserializer, *message);
        return true;
    }

    uint32_t calculate_serialized_size(const void* const data, DataRepresentationId_t) override
    {
        return SizeFn(*static_cast<const Message*>(data));
    }

    void* create_data() override
    {
        return new Message;
    }

    void delete_data(void* data) override
    {
        delete static_cast<Message*>(data);
    }

    bool compute_key(eprosima::fastdds::rtps::SerializedPayload_t&, eprosima::fastdds::rtps::InstanceHandle_t&, bool) override
    {
        return false;
    }

    bool compute_key(const void*, eprosima::fastdds::rtps::InstanceHandle_t&, bool) override
    {
        return false;
    }

    bool is_bounded() const override
    {
        return false;
    }

    bool is_plain(DataRepresentationId_t) const override
    {
        return false;
    }
};

using SnapshotTopicDataType = BasicTopicDataType<AcquisitionDdsSnapshot, serialize_snapshot, deserialize_snapshot, serialized_size>;
using CommandTopicDataType = BasicTopicDataType<AcquisitionDdsCommand, serialize_command, deserialize_command, serialized_size>;
using ResultTopicDataType = BasicTopicDataType<AcquisitionDdsCommandResult, serialize_result, deserialize_result, serialized_size>;

void enable_data_sharing(DataWriterQos& qos)
{
    qos.data_sharing().automatic();
}

void enable_data_sharing(DataReaderQos& qos)
{
    qos.data_sharing().automatic();
}

std::string default_client_id()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return "client-" + std::to_string(static_cast<unsigned long long>(
        std::chrono::duration_cast<std::chrono::microseconds>(now).count()));
}

template <typename Listener>
void destroy_reader(DomainParticipant* participant, Subscriber*& subscriber, DataReader*& reader)
{
    if (subscriber && reader)
    {
        subscriber->delete_datareader(reader);
        reader = nullptr;
    }
    if (participant && subscriber)
    {
        participant->delete_subscriber(subscriber);
        subscriber = nullptr;
    }
}

void destroy_writer(DomainParticipant* participant, Publisher*& publisher, DataWriter*& writer)
{
    if (publisher && writer)
    {
        publisher->delete_datawriter(writer);
        writer = nullptr;
    }
    if (participant && publisher)
    {
        participant->delete_publisher(publisher);
        publisher = nullptr;
    }
}

void destroy_topic(DomainParticipant* participant, Topic*& topic)
{
    if (participant && topic)
    {
        participant->delete_topic(topic);
        topic = nullptr;
    }
}

void destroy_participant(DomainParticipant*& participant)
{
    if (participant)
    {
        DomainParticipantFactory::get_instance()->delete_participant(participant);
        participant = nullptr;
    }
}

} // namespace

class AcquisitionSnapshotSubscriber::ReaderListener: public DataReaderListener
{
public:
    explicit ReaderListener(AcquisitionSnapshotHandler handler):
        _handler(std::move(handler))
    {
    }

    void on_data_available(DataReader* reader) override
    {
        if (!_handler) return;

        AcquisitionDdsSnapshot snapshot;
        SampleInfo info;
        while (reader->take_next_sample(&snapshot, &info) == RETCODE_OK)
        {
            if (info.valid_data)
            {
                _handler(snapshot);
            }
        }
    }

private:
    AcquisitionSnapshotHandler _handler;
};

class AcquisitionCommandServer::ReaderListener: public DataReaderListener
{
public:
    explicit ReaderListener(AcquisitionCommandServer& owner):
        _owner(owner)
    {
    }

    void on_data_available(DataReader* reader) override
    {
        AcquisitionDdsCommand command;
        SampleInfo info;
        while (reader->take_next_sample(&command, &info) == RETCODE_OK)
        {
            if (!info.valid_data) continue;
            if (!_owner._handler) continue;

            AcquisitionDdsCommandResult result = _owner._handler(command);
            result.clientId = command.clientId;
            result.requestId = command.requestId;
            if (result.message.empty())
            {
                result.message = result.snapshot.message;
            }
            if (!_owner.publishResult(result))
            {
                throw std::runtime_error("Unable to publish DDS command result.");
            }
        }
    }

private:
    AcquisitionCommandServer& _owner;
};

class AcquisitionCommandClient::ReaderListener: public DataReaderListener
{
public:
    explicit ReaderListener(AcquisitionCommandClient& owner):
        _owner(owner)
    {
    }

    void on_data_available(DataReader* reader) override
    {
        AcquisitionDdsCommandResult result;
        SampleInfo info;
        while (reader->take_next_sample(&result, &info) == RETCODE_OK)
        {
            if (info.valid_data)
            {
                _owner.deliverResult(result);
            }
        }
    }

private:
    AcquisitionCommandClient& _owner;
};

AcquisitionSnapshotPublisher::AcquisitionSnapshotPublisher():
    _type(new SnapshotTopicDataType(kSnapshotTypeName, kMaxSnapshotSize))
{
}

AcquisitionSnapshotPublisher::~AcquisitionSnapshotPublisher()
{
    stop();
}

void AcquisitionSnapshotPublisher::start(int domainId)
{
    if (_participant) return;

    _participant = DomainParticipantFactory::get_instance()->create_participant(domainId, PARTICIPANT_QOS_DEFAULT);
    if (!_participant)
    {
        throw std::runtime_error("Unable to create Fast-DDS participant for acquisition snapshot publisher.");
    }

    _type.register_type(_participant);
    _topic = _participant->create_topic(kSnapshotTopicName, kSnapshotTypeName, TOPIC_QOS_DEFAULT);
    if (!_topic)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS snapshot topic.");
    }

    _publisher = _participant->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);
    if (!_publisher)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS snapshot publisher.");
    }

    auto writerQos = DATAWRITER_QOS_DEFAULT;
    writerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    writerQos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
    writerQos.history().kind = KEEP_LAST_HISTORY_QOS;
    writerQos.history().depth = 1;
    enable_data_sharing(writerQos);
    _writer = _publisher->create_datawriter(_topic, writerQos, nullptr);
    if (!_writer)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS snapshot writer.");
    }
}

void AcquisitionSnapshotPublisher::stop()
{
    destroy_writer(_participant, _publisher, _writer);
    destroy_topic(_participant, _topic);
    destroy_participant(_participant);
}

bool AcquisitionSnapshotPublisher::publish(const AcquisitionDdsSnapshot& snapshot)
{
    if (!_writer) return false;
    return _writer->write(const_cast<AcquisitionDdsSnapshot*>(&snapshot)) == RETCODE_OK;
}

AcquisitionSnapshotSubscriber::AcquisitionSnapshotSubscriber():
    _type(new SnapshotTopicDataType(kSnapshotTypeName, kMaxSnapshotSize))
{
}

AcquisitionSnapshotSubscriber::~AcquisitionSnapshotSubscriber()
{
    stop();
}

void AcquisitionSnapshotSubscriber::start(int domainId, AcquisitionSnapshotHandler handler)
{
    if (_participant) return;

    _participant = DomainParticipantFactory::get_instance()->create_participant(domainId, PARTICIPANT_QOS_DEFAULT);
    if (!_participant)
    {
        throw std::runtime_error("Unable to create Fast-DDS participant for acquisition snapshot subscriber.");
    }

    _type.register_type(_participant);
    _topic = _participant->create_topic(kSnapshotTopicName, kSnapshotTypeName, TOPIC_QOS_DEFAULT);
    if (!_topic)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS snapshot topic.");
    }

    _subscriber = _participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr);
    if (!_subscriber)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS snapshot subscriber.");
    }

    auto readerQos = DATAREADER_QOS_DEFAULT;
    readerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    readerQos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
    readerQos.history().kind = KEEP_LAST_HISTORY_QOS;
    readerQos.history().depth = 1;
    enable_data_sharing(readerQos);
    _listener.reset(new ReaderListener(std::move(handler)));
    _reader = _subscriber->create_datareader(_topic, readerQos, _listener.get());
    if (!_reader)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS snapshot reader.");
    }
}

void AcquisitionSnapshotSubscriber::stop()
{
    destroy_reader<ReaderListener>(_participant, _subscriber, _reader);
    destroy_topic(_participant, _topic);
    destroy_participant(_participant);
    _listener.reset();
}

AcquisitionCommandServer::AcquisitionCommandServer():
    _commandType(new CommandTopicDataType(kCommandTypeName, kMaxCommandSize)),
    _resultType(new ResultTopicDataType(kResultTypeName, kMaxResultSize))
{
}

AcquisitionCommandServer::~AcquisitionCommandServer()
{
    stop();
}

void AcquisitionCommandServer::start(int domainId, AcquisitionCommandHandler handler)
{
    if (_participant) return;

    _handler = std::move(handler);
    _participant = DomainParticipantFactory::get_instance()->create_participant(domainId, PARTICIPANT_QOS_DEFAULT);
    if (!_participant)
    {
        throw std::runtime_error("Unable to create Fast-DDS participant for acquisition command server.");
    }

    _commandType.register_type(_participant);
    _resultType.register_type(_participant);

    _commandTopic = _participant->create_topic(kCommandTopicName, kCommandTypeName, TOPIC_QOS_DEFAULT);
    _resultTopic = _participant->create_topic(kResultTopicName, kResultTypeName, TOPIC_QOS_DEFAULT);
    if (!_commandTopic || !_resultTopic)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS command server topics.");
    }

    _publisher = _participant->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);
    _subscriber = _participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr);
    if (!_publisher || !_subscriber)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS command server endpoints.");
    }

    auto writerQos = DATAWRITER_QOS_DEFAULT;
    writerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    writerQos.history().kind = KEEP_LAST_HISTORY_QOS;
    writerQos.history().depth = 32;
    enable_data_sharing(writerQos);
    _resultWriter = _publisher->create_datawriter(_resultTopic, writerQos, nullptr);

    auto readerQos = DATAREADER_QOS_DEFAULT;
    readerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    readerQos.history().kind = KEEP_LAST_HISTORY_QOS;
    readerQos.history().depth = 32;
    enable_data_sharing(readerQos);
    _listener.reset(new ReaderListener(*this));
    _commandReader = _subscriber->create_datareader(_commandTopic, readerQos, _listener.get());

    if (!_resultWriter || !_commandReader)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS command server writer/reader.");
    }
}

void AcquisitionCommandServer::stop()
{
    if (_subscriber && _commandReader)
    {
        _subscriber->delete_datareader(_commandReader);
        _commandReader = nullptr;
    }
    if (_participant && _subscriber)
    {
        _participant->delete_subscriber(_subscriber);
        _subscriber = nullptr;
    }
    destroy_writer(_participant, _publisher, _resultWriter);
    destroy_topic(_participant, _commandTopic);
    destroy_topic(_participant, _resultTopic);
    destroy_participant(_participant);
    _listener.reset();
    _handler = nullptr;
}

bool AcquisitionCommandServer::publishResult(const AcquisitionDdsCommandResult& result)
{
    if (!_resultWriter) return false;
    return _resultWriter->write(const_cast<AcquisitionDdsCommandResult*>(&result)) == RETCODE_OK;
}

AcquisitionCommandClient::AcquisitionCommandClient():
    _commandType(new CommandTopicDataType(kCommandTypeName, kMaxCommandSize)),
    _resultType(new ResultTopicDataType(kResultTypeName, kMaxResultSize))
{
}

AcquisitionCommandClient::~AcquisitionCommandClient()
{
    stop();
}

void AcquisitionCommandClient::start(int domainId, const std::string& clientId)
{
    if (_participant) return;

    _clientId = clientId.empty() ? default_client_id() : clientId;
    _participant = DomainParticipantFactory::get_instance()->create_participant(domainId, PARTICIPANT_QOS_DEFAULT);
    if (!_participant)
    {
        throw std::runtime_error("Unable to create Fast-DDS participant for acquisition command client.");
    }

    _commandType.register_type(_participant);
    _resultType.register_type(_participant);
    _commandTopic = _participant->create_topic(kCommandTopicName, kCommandTypeName, TOPIC_QOS_DEFAULT);
    _resultTopic = _participant->create_topic(kResultTopicName, kResultTypeName, TOPIC_QOS_DEFAULT);
    if (!_commandTopic || !_resultTopic)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS command client topics.");
    }

    _publisher = _participant->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);
    _subscriber = _participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr);
    if (!_publisher || !_subscriber)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS command client endpoints.");
    }

    auto writerQos = DATAWRITER_QOS_DEFAULT;
    writerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    writerQos.history().kind = KEEP_LAST_HISTORY_QOS;
    writerQos.history().depth = 32;
    enable_data_sharing(writerQos);
    _commandWriter = _publisher->create_datawriter(_commandTopic, writerQos, nullptr);

    auto readerQos = DATAREADER_QOS_DEFAULT;
    readerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    readerQos.history().kind = KEEP_LAST_HISTORY_QOS;
    readerQos.history().depth = 32;
    enable_data_sharing(readerQos);
    _listener.reset(new ReaderListener(*this));
    _resultReader = _subscriber->create_datareader(_resultTopic, readerQos, _listener.get());

    if (!_commandWriter || !_resultReader)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS command client writer/reader.");
    }
}

void AcquisitionCommandClient::stop()
{
    if (_subscriber && _resultReader)
    {
        _subscriber->delete_datareader(_resultReader);
        _resultReader = nullptr;
    }
    if (_participant && _subscriber)
    {
        _participant->delete_subscriber(_subscriber);
        _subscriber = nullptr;
    }
    destroy_writer(_participant, _publisher, _commandWriter);
    destroy_topic(_participant, _commandTopic);
    destroy_topic(_participant, _resultTopic);
    destroy_participant(_participant);
    _listener.reset();
    _clientId.clear();

    std::lock_guard<std::mutex> lock(_mutex);
    _pending.clear();
}

AcquisitionDdsCommandResult AcquisitionCommandClient::sendCommand(const AcquisitionDdsCommand& inputCommand, long timeoutMs)
{
    if (!_commandWriter)
    {
        throw std::runtime_error("DDS command client is not started.");
    }

    AcquisitionDdsCommand command = inputCommand;
    if (command.clientId.empty())
    {
        command.clientId = _clientId;
    }
    if (command.requestId == 0)
    {
        command.requestId = _nextRequestId.fetch_add(1);
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _pending[command.requestId] = PendingRequest{};
    }

    if (_commandWriter->write(&command) != RETCODE_OK)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _pending.erase(command.requestId);
        throw std::runtime_error("Unable to publish DDS command.");
    }

    std::unique_lock<std::mutex> lock(_mutex);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    _condition.wait_until(lock, deadline, [&]() {
        const auto it = _pending.find(command.requestId);
        return it == _pending.end() || it->second.completed;
    });

    const auto it = _pending.find(command.requestId);
    if (it == _pending.end())
    {
        throw std::runtime_error("DDS command request state disappeared.");
    }
    if (!it->second.completed)
    {
        _pending.erase(it);
        throw std::runtime_error("Timed out waiting for DDS command result.");
    }

    AcquisitionDdsCommandResult result = it->second.result;
    _pending.erase(it);
    return result;
}

const std::string& AcquisitionCommandClient::clientId() const
{
    return _clientId;
}

void AcquisitionCommandClient::deliverResult(const AcquisitionDdsCommandResult& result)
{
    if (result.clientId != _clientId) return;

    std::lock_guard<std::mutex> lock(_mutex);
    const auto it = _pending.find(result.requestId);
    if (it == _pending.end()) return;
    it->second.completed = true;
    it->second.result = result;
    _condition.notify_all();
}

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
