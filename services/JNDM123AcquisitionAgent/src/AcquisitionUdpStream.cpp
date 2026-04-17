#include "AcquisitionUdpStream.h"

#include "Poco/Exception.h"
#include "Poco/Net/DatagramSocket.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Timespan.h"

#include <memory>
#include <vector>

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

namespace {

constexpr std::uint16_t kPacketVersion = 1;
constexpr std::uint16_t kPacketHeaderSize = 40;
constexpr std::uint32_t kPacketFlagsLittleEndian = 0x00000001u;
constexpr char kPacketMagic[4] = {'J', 'N', 'D', 'A'};

void appendBytes(std::vector<std::uint8_t>& buffer, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    buffer.insert(buffer.end(), bytes, bytes + size);
}

void appendU16Le(std::vector<std::uint8_t>& buffer, std::uint16_t value)
{
    buffer.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

void appendU32Le(std::vector<std::uint8_t>& buffer, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
    {
        buffer.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

void appendU64Le(std::vector<std::uint8_t>& buffer, std::uint64_t value)
{
    for (int shift = 0; shift < 64; shift += 8)
    {
        buffer.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

void appendI64Le(std::vector<std::uint8_t>& buffer, Poco::Int64 value)
{
    appendU64Le(buffer, static_cast<std::uint64_t>(value));
}

void appendI16Le(std::vector<std::uint8_t>& buffer, std::int16_t value)
{
    appendU16Le(buffer, static_cast<std::uint16_t>(value));
}

} // namespace

struct AcquisitionUdpStreamPublisher::Impl
{
    explicit Impl(const AcquisitionUdpStreamSettings& settings):
        destination(settings.host, static_cast<Poco::UInt16>(settings.port)),
        socket(Poco::Net::SocketAddress(Poco::Net::IPAddress(), 0))
    {
        socket.setBroadcast(true);
        socket.setSendTimeout(Poco::Timespan(0, 200 * 1000));
    }

    Poco::Net::SocketAddress destination;
    Poco::Net::DatagramSocket socket;
};

AcquisitionUdpStreamPublisher::AcquisitionUdpStreamPublisher(const AcquisitionUdpStreamSettings& settings):
    _settings(settings)
{
    if (!_settings.enabled || _settings.host.empty() || _settings.port <= 0)
    {
        _active = false;
        return;
    }

    if (_settings.maxFramesPerPacket <= 0)
    {
        _settings.maxFramesPerPacket = 10;
    }

    try
    {
        _impl.reset(new Impl(_settings));
        _active = true;
    }
    catch (const Poco::Exception& exc)
    {
        recordError(exc.displayText());
    }
    catch (const std::exception& exc)
    {
        recordError(exc.what());
    }
}

AcquisitionUdpStreamPublisher::~AcquisitionUdpStreamPublisher()
{
    try
    {
        flush();
    }
    catch (...)
    {
    }
}

bool AcquisitionUdpStreamPublisher::enabled() const
{
    return _active;
}

const AcquisitionUdpStreamSettings& AcquisitionUdpStreamPublisher::settings() const
{
    return _settings;
}

const AcquisitionUdpStreamStats& AcquisitionUdpStreamPublisher::stats() const
{
    return _stats;
}

void AcquisitionUdpStreamPublisher::pushFrame(
    Poco::UInt64 sequence,
    Poco::Int64 capturedAtUs,
    const std::array<std::int16_t, kFrameColumns>& samples)
{
    if (!_active) return;

    PendingFrame frame;
    frame.sequence = sequence;
    frame.capturedAtUs = capturedAtUs;
    frame.samples = samples;
    _pendingFrames.push_back(std::move(frame));

    if (static_cast<int>(_pendingFrames.size()) >= _settings.maxFramesPerPacket)
    {
        flushPendingFrames();
    }
}

void AcquisitionUdpStreamPublisher::flush()
{
    if (!_active) return;
    flushPendingFrames();
}

void AcquisitionUdpStreamPublisher::flushPendingFrames()
{
    if (_pendingFrames.empty()) return;

    try
    {
        sendPacket(_pendingFrames);
    }
    catch (const Poco::Exception& exc)
    {
        recordError(exc.displayText());
    }
    catch (const std::exception& exc)
    {
        recordError(exc.what());
    }

    _pendingFrames.clear();
}

void AcquisitionUdpStreamPublisher::sendPacket(const std::vector<PendingFrame>& frames)
{
    if (!_active || !_impl) return;

    std::vector<std::uint8_t> buffer;
    buffer.reserve(
        kPacketHeaderSize +
        frames.size() * (sizeof(std::uint64_t) + sizeof(Poco::Int64) + kFrameColumns * sizeof(std::int16_t)));

    appendBytes(buffer, kPacketMagic, sizeof(kPacketMagic));
    appendU16Le(buffer, kPacketVersion);
    appendU16Le(buffer, kPacketHeaderSize);
    appendU16Le(buffer, static_cast<std::uint16_t>(kFrameColumns));
    appendU16Le(buffer, static_cast<std::uint16_t>(frames.size()));
    appendU32Le(buffer, kPacketFlagsLittleEndian);
    appendU64Le(buffer, static_cast<std::uint64_t>(frames.front().sequence));
    appendU64Le(buffer, static_cast<std::uint64_t>(frames.back().sequence));
    appendI64Le(buffer, frames.back().capturedAtUs);

    for (const auto& frame: frames)
    {
        appendU64Le(buffer, static_cast<std::uint64_t>(frame.sequence));
        appendI64Le(buffer, frame.capturedAtUs);
        for (const auto sample: frame.samples)
        {
            appendI16Le(buffer, sample);
        }
    }

    _impl->socket.sendTo(
        reinterpret_cast<const void*>(buffer.data()),
        static_cast<int>(buffer.size()),
        _impl->destination);

    _stats.packetsSent += 1;
    _stats.framesSent += static_cast<Poco::UInt64>(frames.size());
    _stats.lastError.clear();
}

void AcquisitionUdpStreamPublisher::recordError(const std::string& message)
{
    _stats.sendErrors += 1;
    _stats.lastError = message;
}

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
