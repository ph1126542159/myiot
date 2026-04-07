#ifndef MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionDivider_INCLUDED
#define MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionDivider_INCLUDED

#include "Poco/JSON/Object.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

constexpr const char* kDefaultI2CDevice = "/dev/i2c-0";
constexpr const char* kDividerStateFile = "/var/lib/myiot/jndm123-divider-state.properties";
constexpr std::size_t kDividerOutputCount = 7;
constexpr std::size_t kDividerStartupOutputCount = 6;
constexpr Poco::UInt64 kFallbackReferenceClockHz = 1000000ULL;
constexpr Poco::UInt64 kMaxSafeAcquisitionClockHz = 50000ULL;

#if defined(__linux__)
constexpr std::uint8_t kCdce937RegId = 0x00;
constexpr std::uint8_t kCdce937RegCfg1 = 0x01;
constexpr std::uint8_t kCdce937RegY1Control = 0x02;
constexpr std::uint8_t kCdce937RegMux1 = 0x14;
constexpr std::uint8_t kCdce937RegMux2 = 0x24;
constexpr std::uint8_t kCdce937RegMux3 = 0x34;
extern const std::uint8_t kProbeAddresses[4];
#endif

struct DividerOutputSpec
{
    int index;
    const char* name;
    const char* pdivName;
    int pin;
};

extern const std::array<DividerOutputSpec, kDividerOutputCount> kDividerOutputs;

struct SavedDividerConfiguration
{
    std::string devicePath = kDefaultI2CDevice;
    Poco::UInt64 referenceClockHz = kFallbackReferenceClockHz;
    std::array<int, kDividerStartupOutputCount> dividers{{1, 1, 1, 1, 1, 1}};
    std::array<bool, kDividerStartupOutputCount> savedOutputs{{false, false, false, false, false, false}};
    bool hasAnyOutput = false;
    bool hasAllOutputs = false;
};

struct DividerOutputState
{
    int index = 0;
    std::string name;
    std::string pdivName;
    int pin = 0;
    int divider = 0;
    double frequencyHz = 0.0;
};

struct DividerSnapshot
{
    bool ok = true;
    std::string message = "Divider status synchronized.";
    std::string devicePath = kDefaultI2CDevice;
    Poco::UInt64 referenceClockHz = kFallbackReferenceClockHz;
    std::string deviceType = "unsupported";
    std::string address = "--";
    std::string inputClock = "--";
    int revisionId = 0;
    bool eepBusy = false;
    bool eepLock = false;
    bool powerDown = false;
    std::vector<DividerOutputState> outputs;
};

SavedDividerConfiguration loadSavedDividerConfiguration();
std::string resolvePreferredDevicePath(const std::string& requestedDevicePath);
Poco::UInt64 resolvePreferredReferenceClockHz(const std::string& requestedReferenceClockHz);
void saveDividerConfiguration(const DividerSnapshot& snapshot, const std::string& devicePath);
int dividerLimitForOutput(int outputIndex);
std::vector<int> normalizeOutputIndicesOrThrow(const std::vector<int>& outputIndices);
void validateDividerOrThrow(int outputIndex, int divider);
void validateReferenceClockHzOrThrow(Poco::UInt64 referenceClockHz);
double maximumStartupOutputFrequencyHz(const DividerSnapshot& snapshot);
std::string acquisitionClockSafetyMessage(const DividerSnapshot& snapshot);
Poco::JSON::Object::Ptr dividerSnapshotToJson(const DividerSnapshot& snapshot);

#if defined(__linux__)
DividerSnapshot initializeHardwareFromSavedConfiguration();
DividerSnapshot readDividerStatus(const std::string& devicePath, Poco::UInt64 referenceClockHz);
DividerSnapshot applyDividers(
    const std::string& devicePath,
    const std::vector<int>& outputIndices,
    int divider,
    Poco::UInt64 referenceClockHz);
#endif

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent

#endif // MyIoT_Services_JNDM123AcquisitionAgent_AcquisitionDivider_INCLUDED
