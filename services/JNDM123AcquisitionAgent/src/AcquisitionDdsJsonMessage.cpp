#include "AcquisitionDdsJsonMessage.h"

#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

namespace {

constexpr const char* kAcquisitionSnapshotTopicName = "MyIoT.JNDM123.AcquisitionSnapshot";
constexpr const char* kAcquisitionSnapshotTypeName = "MyIoT.JNDM123.AcquisitionSnapshot.Json";

} // namespace

AcquisitionDdsJsonMessagePubSubType::AcquisitionDdsJsonMessagePubSubType()
{
    set_name(kAcquisitionSnapshotTypeName);
    max_serialized_type_size = 1024 * 1024;
    is_compute_key_provided = false;
}

bool AcquisitionDdsJsonMessagePubSubType::serialize(
        const void* const data,
        eprosima::fastdds::rtps::SerializedPayload_t& payload,
        eprosima::fastdds::dds::DataRepresentationId_t)
{
    const auto* message = static_cast<const AcquisitionDdsJsonMessage*>(data);
    eprosima::fastcdr::FastBuffer buffer(reinterpret_cast<char*>(payload.data), payload.max_size);
    eprosima::fastcdr::Cdr serializer(buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::CdrVersion::XCDRv1);

    payload.encapsulation = serializer.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS
        ? CDR_BE
        : CDR_LE;

    serializer.serialize_encapsulation();
    serializer << message->sequence;
    serializer << message->updatedAt;
    serializer << message->payload;
    payload.length = static_cast<uint32_t>(serializer.get_serialized_data_length());
    return true;
}

bool AcquisitionDdsJsonMessagePubSubType::deserialize(
        eprosima::fastdds::rtps::SerializedPayload_t& payload,
        void* data)
{
    auto* message = static_cast<AcquisitionDdsJsonMessage*>(data);
    eprosima::fastcdr::FastBuffer buffer(reinterpret_cast<char*>(payload.data), payload.length);
    eprosima::fastcdr::Cdr deserializer(buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::CdrVersion::XCDRv1);

    deserializer.read_encapsulation();
    payload.encapsulation = deserializer.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS
        ? CDR_BE
        : CDR_LE;

    deserializer >> message->sequence;
    deserializer >> message->updatedAt;
    deserializer >> message->payload;
    return true;
}

uint32_t AcquisitionDdsJsonMessagePubSubType::calculate_serialized_size(
        const void* const data,
        eprosima::fastdds::dds::DataRepresentationId_t)
{
    const auto* message = static_cast<const AcquisitionDdsJsonMessage*>(data);
    return static_cast<uint32_t>(64 + message->updatedAt.size() + message->payload.size());
}

void* AcquisitionDdsJsonMessagePubSubType::create_data()
{
    return new AcquisitionDdsJsonMessage;
}

void AcquisitionDdsJsonMessagePubSubType::delete_data(void* data)
{
    delete static_cast<AcquisitionDdsJsonMessage*>(data);
}

bool AcquisitionDdsJsonMessagePubSubType::compute_key(
        eprosima::fastdds::rtps::SerializedPayload_t&,
        eprosima::fastdds::rtps::InstanceHandle_t&,
        bool)
{
    return false;
}

bool AcquisitionDdsJsonMessagePubSubType::compute_key(
        const void* const,
        eprosima::fastdds::rtps::InstanceHandle_t&,
        bool)
{
    return false;
}

bool AcquisitionDdsJsonMessagePubSubType::is_bounded() const
{
    return false;
}

bool AcquisitionDdsJsonMessagePubSubType::is_plain(eprosima::fastdds::dds::DataRepresentationId_t) const
{
    return false;
}

AcquisitionDdsPublisher::AcquisitionDdsPublisher():
    _type(new AcquisitionDdsJsonMessagePubSubType)
{
}

AcquisitionDdsPublisher::~AcquisitionDdsPublisher()
{
    stop();
}

void AcquisitionDdsPublisher::start(int domainId)
{
    if (_participant) return;

    auto participantQos = eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT;
    _participant = eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->create_participant(domainId, participantQos);
    if (!_participant)
    {
        throw std::runtime_error("Unable to create Fast-DDS participant for acquisition publisher.");
    }

    _type.register_type(_participant);
    _topic = _participant->create_topic(
        kAcquisitionSnapshotTopicName,
        kAcquisitionSnapshotTypeName,
        eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
    if (!_topic)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS topic for acquisition publisher.");
    }

    _publisher = _participant->create_publisher(eprosima::fastdds::dds::PUBLISHER_QOS_DEFAULT, nullptr);
    if (!_publisher)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS publisher.");
    }

    auto writerQos = eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT;
    writerQos.reliability().kind = eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
    writerQos.durability().kind = eprosima::fastdds::dds::TRANSIENT_LOCAL_DURABILITY_QOS;
    writerQos.history().kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
    writerQos.history().depth = 1;
    _writer = _publisher->create_datawriter(_topic, writerQos, nullptr);
    if (!_writer)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS writer.");
    }
}

void AcquisitionDdsPublisher::stop()
{
    if (_publisher && _writer)
    {
        _publisher->delete_datawriter(_writer);
        _writer = nullptr;
    }
    if (_participant && _publisher)
    {
        _participant->delete_publisher(_publisher);
        _publisher = nullptr;
    }
    if (_participant && _topic)
    {
        _participant->delete_topic(_topic);
        _topic = nullptr;
    }
    if (_participant)
    {
        eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->delete_participant(_participant);
        _participant = nullptr;
    }
}

bool AcquisitionDdsPublisher::publish(const AcquisitionDdsJsonMessage& message)
{
    if (!_writer) return false;
    return _writer->write(const_cast<AcquisitionDdsJsonMessage*>(&message)) == eprosima::fastdds::dds::RETCODE_OK;
}

class AcquisitionDdsSubscriber::ReaderListener: public eprosima::fastdds::dds::DataReaderListener
{
public:
    explicit ReaderListener(AcquisitionDdsSubscriberListener* listener):
        _listener(listener)
    {
    }

    void on_data_available(eprosima::fastdds::dds::DataReader* reader) override
    {
        if (!_listener) return;

        AcquisitionDdsJsonMessage message;
        eprosima::fastdds::dds::SampleInfo sampleInfo;
        while (reader->take_next_sample(&message, &sampleInfo) == eprosima::fastdds::dds::RETCODE_OK)
        {
            if (sampleInfo.valid_data)
            {
                _listener->onSnapshot(message);
            }
        }
    }

private:
    AcquisitionDdsSubscriberListener* _listener = nullptr;
};

AcquisitionDdsSubscriber::AcquisitionDdsSubscriber():
    _type(new AcquisitionDdsJsonMessagePubSubType)
{
}

AcquisitionDdsSubscriber::~AcquisitionDdsSubscriber()
{
    stop();
}

void AcquisitionDdsSubscriber::start(int domainId, AcquisitionDdsSubscriberListener* listener)
{
    if (_participant) return;

    auto participantQos = eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT;
    _participant = eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->create_participant(domainId, participantQos);
    if (!_participant)
    {
        throw std::runtime_error("Unable to create Fast-DDS participant for acquisition subscriber.");
    }

    _type.register_type(_participant);
    _topic = _participant->create_topic(
        kAcquisitionSnapshotTopicName,
        kAcquisitionSnapshotTypeName,
        eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
    if (!_topic)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS topic for acquisition subscriber.");
    }

    _subscriber = _participant->create_subscriber(eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT, nullptr);
    if (!_subscriber)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS subscriber.");
    }

    auto readerQos = eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT;
    readerQos.reliability().kind = eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
    readerQos.durability().kind = eprosima::fastdds::dds::TRANSIENT_LOCAL_DURABILITY_QOS;
    readerQos.history().kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
    readerQos.history().depth = 1;

    _listener.reset(new ReaderListener(listener));
    _reader = _subscriber->create_datareader(_topic, readerQos, _listener.get());
    if (!_reader)
    {
        stop();
        throw std::runtime_error("Unable to create Fast-DDS reader.");
    }
}

void AcquisitionDdsSubscriber::stop()
{
    if (_subscriber && _reader)
    {
        _subscriber->delete_datareader(_reader);
        _reader = nullptr;
    }
    if (_participant && _subscriber)
    {
        _participant->delete_subscriber(_subscriber);
        _subscriber = nullptr;
    }
    if (_participant && _topic)
    {
        _participant->delete_topic(_topic);
        _topic = nullptr;
    }
    if (_participant)
    {
        eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->delete_participant(_participant);
        _participant = nullptr;
    }
    _listener.reset();
}

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
