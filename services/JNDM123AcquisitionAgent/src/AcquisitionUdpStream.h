#ifndef MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionUdpStream_INCLUDED
#define MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionUdpStream_INCLUDED

#include "AcquisitionAgentSupport.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

struct AcquisitionUdpStreamSettings
{
    bool enabled = true;
    std::string host = "192.168.16.255";
    int port = 50000;
    int maxFramesPerPacket = 10;
};

struct AcquisitionUdpStreamStats
{
    Poco::UInt64 packetsSent = 0;
    Poco::UInt64 framesSent = 0;
    Poco::UInt64 sendErrors = 0;
    std::string lastError;
};

class AcquisitionUdpStreamPublisher
{
public:
    explicit AcquisitionUdpStreamPublisher(const AcquisitionUdpStreamSettings& settings);
    ~AcquisitionUdpStreamPublisher();

    bool enabled() const;
    const AcquisitionUdpStreamSettings& settings() const;
    const AcquisitionUdpStreamStats& stats() const;

    void pushFrame(
        Poco::UInt64 sequence,
        Poco::Int64 capturedAtUs,
        const std::array<std::int16_t, kFrameColumns>& samples);
    void flush();

private:
    struct Impl;
    struct PendingFrame
    {
        Poco::UInt64 sequence = 0;
        Poco::Int64 capturedAtUs = 0;
        std::array<std::int16_t, kFrameColumns> samples{};
    };

    void flushPendingFrames();
    void sendPacket(const std::vector<PendingFrame>& frames);
    void recordError(const std::string& message);

    AcquisitionUdpStreamSettings _settings;
    AcquisitionUdpStreamStats _stats;
    bool _active = false;
    std::vector<PendingFrame> _pendingFrames;
    std::unique_ptr<Impl> _impl;
};

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent

#endif // MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionUdpStream_INCLUDED
