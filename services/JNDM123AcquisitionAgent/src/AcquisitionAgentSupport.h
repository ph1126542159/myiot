#ifndef MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionAgentSupport_INCLUDED
#define MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionAgentSupport_INCLUDED

#include "Poco/Logger.h"
#include "Poco/Timestamp.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

constexpr std::size_t kAd7606Count = 6;
constexpr std::size_t kChannelsPerChip = 8;
constexpr std::size_t kFrameWords = 24;
constexpr std::size_t kFrameColumns = kAd7606Count * kChannelsPerChip;
constexpr std::size_t kHistoryLimit = 960;
constexpr std::size_t kMaxPendingFrames = 120000;
constexpr int kWaveformPublishIntervalMs = 1000;
constexpr Poco::Int64 kFrameBatchWindowUs = 1000 * 1000;
constexpr Poco::Int64 kHistorySampleIntervalUs = 100;
constexpr Poco::Int64 kReaderDrainIntervalUs = 5 * 1000;
constexpr int kDefaultDdsDomain = 37;

#if defined(__linux__)
constexpr std::uint32_t kFifoCtrlBase = 0x43C00000UL;
constexpr std::uint32_t kFifoDataPort = 0x83C01000UL;
constexpr std::size_t kMapSize = 4096U;
constexpr std::uint32_t kIsr = 0x00U;
constexpr std::uint32_t kRdfr = 0x18U;
constexpr std::uint32_t kRdfo = 0x1CU;
constexpr std::uint32_t kRlr = 0x24U;
constexpr std::uint32_t kSrr = 0x28U;
constexpr std::uint32_t kIsrRpue = (1u << 29);
constexpr std::uint32_t kIsrRpore = (1u << 30);
constexpr std::uint32_t kIsrRpure = (1u << 31);
constexpr int kGpioReset = 968;
constexpr int kGpioMaskStart = 976;
constexpr bool kMaskStartActiveLow = false;
#endif

Poco::Logger& logger();
std::string isoTimestamp(const Poco::Timestamp& timestamp = Poco::Timestamp());
int parseIntEnv(const char* name, int fallback);

#if defined(__linux__)
void writeSysfsValue(const std::string& path, const std::string& value);
void exportGpio(int gpio);
void setGpioDirection(int gpio, const std::string& direction);
void setGpioValue(int gpio, bool high);
bool activeMaskValue(bool active);
void elevateCurrentThreadPriority();
#endif

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent

#endif // MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionAgentSupport_INCLUDED
