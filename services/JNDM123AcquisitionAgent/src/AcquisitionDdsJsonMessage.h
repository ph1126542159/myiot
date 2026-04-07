#ifndef MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionDdsJsonMessage_INCLUDED
#define MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionDdsJsonMessage_INCLUDED

#include <cstdint>
#include <memory>
#include <string>

#include <fastdds/dds/core/policy/QosPolicies.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TopicDataType.hpp>

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

struct AcquisitionDdsJsonMessage
{
    std::uint64_t sequence = 0;
    std::string updatedAt;
    std::string payload;
};

const char* acquisitionSnapshotTopicName();
const char* acquisitionControlTopicName();

class AcquisitionDdsJsonMessagePubSubType: public eprosima::fastdds::dds::TopicDataType
{
public:
    AcquisitionDdsJsonMessagePubSubType();

    bool serialize(
            const void* const data,
            eprosima::fastdds::rtps::SerializedPayload_t& payload,
            eprosima::fastdds::dds::DataRepresentationId_t data_representation) override;

    bool deserialize(
            eprosima::fastdds::rtps::SerializedPayload_t& payload,
            void* data) override;

    uint32_t calculate_serialized_size(
            const void* const data,
            eprosima::fastdds::dds::DataRepresentationId_t data_representation) override;

    void* create_data() override;
    void delete_data(void* data) override;

    bool compute_key(
            eprosima::fastdds::rtps::SerializedPayload_t& payload,
            eprosima::fastdds::rtps::InstanceHandle_t& ihandle,
            bool force_md5 = false) override;

    bool compute_key(
            const void* const data,
            eprosima::fastdds::rtps::InstanceHandle_t& ihandle,
            bool force_md5 = false) override;

    bool is_bounded() const override;
    bool is_plain(eprosima::fastdds::dds::DataRepresentationId_t data_representation) const override;
};

class AcquisitionDdsPublisher
{
public:
    AcquisitionDdsPublisher();
    ~AcquisitionDdsPublisher();

    void start(int domainId, const std::string& topicName = acquisitionSnapshotTopicName());
    void stop();
    bool publish(const AcquisitionDdsJsonMessage& message);

private:
    eprosima::fastdds::dds::DomainParticipant* _participant = nullptr;
    eprosima::fastdds::dds::Publisher* _publisher = nullptr;
    eprosima::fastdds::dds::Topic* _topic = nullptr;
    eprosima::fastdds::dds::DataWriter* _writer = nullptr;
    eprosima::fastdds::dds::TypeSupport _type;
    std::string _topicName;
};

class AcquisitionDdsSubscriberListener
{
public:
    virtual ~AcquisitionDdsSubscriberListener() = default;
    virtual void onMessage(const AcquisitionDdsJsonMessage& message) = 0;
};

class AcquisitionDdsSubscriber
{
public:
    AcquisitionDdsSubscriber();
    ~AcquisitionDdsSubscriber();

    void start(
        int domainId,
        const std::string& topicName,
        AcquisitionDdsSubscriberListener* listener);
    void stop();

private:
    class ReaderListener;

    eprosima::fastdds::dds::DomainParticipant* _participant = nullptr;
    eprosima::fastdds::dds::Subscriber* _subscriber = nullptr;
    eprosima::fastdds::dds::Topic* _topic = nullptr;
    eprosima::fastdds::dds::DataReader* _reader = nullptr;
    eprosima::fastdds::dds::TypeSupport _type;
    std::unique_ptr<ReaderListener> _listener;
    std::string _topicName;
};

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent

#endif // MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionDdsJsonMessage_INCLUDED
