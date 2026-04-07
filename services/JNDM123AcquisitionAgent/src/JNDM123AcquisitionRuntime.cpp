#include "JNDM123AcquisitionRuntime.h"

#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/Format.h"
#include "Poco/Logger.h"
#include "Poco/Path.h"
#include "Poco/String.h"
#include "Poco/Thread.h"
#include "Poco/Timestamp.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>

#if defined(__linux__)
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

namespace {

constexpr std::size_t kFrameWords = 24;
constexpr std::size_t kHistoryLimit = 240;
constexpr const char* kDefaultI2CDevice = "/dev/i2c-0";
constexpr const char* kDividerStateFile = "/var/lib/myiot/jndm123-divider-state.properties";
constexpr std::size_t kDividerStartupOutputCount = 6;
constexpr Poco::UInt64 kFallbackReferenceClockHz = 1000000ULL;
constexpr Poco::UInt64 kMaxSafeAcquisitionClockHz = 50000ULL;

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
constexpr std::uint8_t kCdce937RegId = 0x00;
constexpr std::uint8_t kCdce937RegCfg1 = 0x01;
constexpr std::uint8_t kCdce937RegY1Control = 0x02;
constexpr std::uint8_t kCdce937RegMux1 = 0x14;
constexpr std::uint8_t kCdce937RegMux2 = 0x24;
constexpr std::uint8_t kCdce937RegMux3 = 0x34;
constexpr std::uint8_t kProbeAddresses[] = {0x6c, 0x6d, 0x6e, 0x6f};
#endif

struct DividerOutputSpec
{
    int index;
    const char* name;
    const char* pdivName;
    int pin;
};

struct DividerOutputState
{
    int index = 0;
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

struct SavedDividerConfiguration
{
    std::string devicePath = kDefaultI2CDevice;
    Poco::UInt64 referenceClockHz = kFallbackReferenceClockHz;
    std::array<int, kDividerStartupOutputCount> dividers{{1, 1, 1, 1, 1, 1}};
    std::array<bool, kDividerStartupOutputCount> savedOutputs{{false, false, false, false, false, false}};
    bool hasAnyOutput = false;
    bool hasAllOutputs = false;
};

const std::array<DividerOutputSpec, kJndm123DividerOutputCount> kDividerOutputs = {{
    {0, "Y1", "Pdiv1", 17},
    {1, "Y2", "Pdiv1", 15},
    {2, "Y3", "Pdiv1", 14},
    {3, "Y4", "Pdiv4", 7},
    {4, "Y5", "Pdiv4", 8},
    {5, "Y6", "Pdiv4", 12},
    {6, "Y7", "Pdiv7", 11},
}};

Poco::Logger& logger()
{
    static Poco::Logger& instance = Poco::Logger::get("MyIoT.JNDM123.AcquisitionAgent");
    return instance;
}

std::string isoTimestamp(const Poco::Timestamp& timestamp = Poco::Timestamp())
{
    return Poco::DateTimeFormatter::format(timestamp, Poco::DateTimeFormat::ISO8601_FORMAT);
}

void writeSysfsValue(const std::string& path, const std::string& value)
{
#if defined(__linux__)
    const int fd = ::open(path.c_str(), O_WRONLY);
    if (fd < 0) return;
    ::write(fd, value.c_str(), value.size());
    ::close(fd);
#else
    (void)path;
    (void)value;
#endif
}

void exportGpio(int gpio)
{
    writeSysfsValue("/sys/class/gpio/export", std::to_string(gpio));
}

void setGpioDirection(int gpio, const std::string& direction)
{
    writeSysfsValue("/sys/class/gpio/gpio" + std::to_string(gpio) + "/direction", direction);
}

void setGpioValue(int gpio, bool high)
{
    writeSysfsValue("/sys/class/gpio/gpio" + std::to_string(gpio) + "/value", high ? "1" : "0");
}

bool activeMaskValue(bool active)
{
    return kMaskStartActiveLow ? !active : active;
}

int dividerLimitForOutput(int outputIndex)
{
    return outputIndex >= 0 && outputIndex <= 2 ? 1023 : 127;
}

void validateDividerOrThrow(int outputIndex, int divider)
{
    if (outputIndex < 0 || outputIndex >= static_cast<int>(kJndm123DividerOutputCount))
    {
        throw Poco::InvalidArgumentException("Output index must be in 0..6.");
    }

    const int limit = dividerLimitForOutput(outputIndex);
    if (divider <= 0 || divider > limit)
    {
        throw Poco::InvalidArgumentException(
            Poco::format("Divider for %s must be in 1..%d.", kDividerOutputs[outputIndex].name, limit));
    }
}

void validateReferenceClockHzOrThrow(Poco::UInt64 referenceClockHz)
{
    if (referenceClockHz == 0 || referenceClockHz > 1000000000ULL)
    {
        throw Poco::InvalidArgumentException("referenceClockHz must be in 1..1000000000.");
    }
}

bool parseUInt64Strict(const std::string& text, Poco::UInt64& value)
{
    if (text.empty()) return false;
    try
    {
        std::size_t pos = 0;
        const unsigned long long parsed = std::stoull(text, &pos, 10);
        if (pos != text.size()) return false;
        value = static_cast<Poco::UInt64>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

SavedDividerConfiguration loadSavedDividerConfiguration()
{
    SavedDividerConfiguration configuration;
    std::ifstream input(kDividerStateFile);
    if (!input) return configuration;

    std::string line;
    while (std::getline(input, line))
    {
        Poco::trimInPlace(line);
        if (line.empty() || line[0] == '#') continue;

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;

        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        Poco::trimInPlace(key);
        Poco::trimInPlace(value);

        if (key == "devicePath")
        {
            if (!value.empty()) configuration.devicePath = value;
            continue;
        }

        if (key == "referenceClockHz")
        {
            Poco::UInt64 referenceClockHz = 0;
            if (!parseUInt64Strict(value, referenceClockHz)) continue;
            try
            {
                validateReferenceClockHzOrThrow(referenceClockHz);
                configuration.referenceClockHz = referenceClockHz;
            }
            catch (...)
            {
            }
            continue;
        }

        if (key.size() == 2 && key[0] == 'y' && key[1] >= '1' && key[1] <= '6')
        {
            const int outputIndex = key[1] - '1';
            try
            {
                const int divider = std::stoi(value);
                validateDividerOrThrow(outputIndex, divider);
                configuration.dividers[outputIndex] = divider;
                configuration.savedOutputs[outputIndex] = true;
            }
            catch (...)
            {
            }
        }
    }

    configuration.hasAnyOutput = std::any_of(
        configuration.savedOutputs.begin(),
        configuration.savedOutputs.end(),
        [](bool value) { return value; });
    configuration.hasAllOutputs = std::all_of(
        configuration.savedOutputs.begin(),
        configuration.savedOutputs.end(),
        [](bool value) { return value; });
    return configuration;
}

std::string resolvePreferredDevicePath(const std::string& requestedDevicePath)
{
    if (!requestedDevicePath.empty())
    {
        return requestedDevicePath;
    }

    const SavedDividerConfiguration saved = loadSavedDividerConfiguration();
    if (!saved.devicePath.empty())
    {
        return saved.devicePath;
    }

    return kDefaultI2CDevice;
}

Poco::UInt64 resolvePreferredReferenceClockHz(const std::string& requestedReferenceClockHz = std::string())
{
    if (!requestedReferenceClockHz.empty())
    {
        Poco::UInt64 referenceClockHz = 0;
        if (!parseUInt64Strict(requestedReferenceClockHz, referenceClockHz))
        {
            throw Poco::InvalidArgumentException("referenceClockHz must be a numeric value.");
        }
        validateReferenceClockHzOrThrow(referenceClockHz);
        return referenceClockHz;
    }

    const SavedDividerConfiguration saved = loadSavedDividerConfiguration();
    validateReferenceClockHzOrThrow(saved.referenceClockHz);
    return saved.referenceClockHz;
}

void saveDividerConfiguration(const DividerSnapshot& snapshot, const std::string& devicePath)
{
    Poco::Path configPath(kDividerStateFile);
    Poco::File(configPath.parent()).createDirectories();

    std::ofstream output(kDividerStateFile, std::ios::trunc);
    if (!output)
    {
        throw Poco::IOException("Unable to write divider state file " + std::string(kDividerStateFile));
    }

    output << "# Auto-generated by MyIoT JNDM123 acquisition agent\n";
    output << "devicePath=" << (devicePath.empty() ? kDefaultI2CDevice : devicePath) << "\n";
    output << "referenceClockHz=" << snapshot.referenceClockHz << "\n";
    for (std::size_t index = 0; index < kDividerStartupOutputCount; ++index)
    {
        if (index >= snapshot.outputs.size())
        {
            throw Poco::InvalidAccessException("Divider snapshot is missing outputs required for persistence.");
        }
        output << "y" << (index + 1) << "=" << snapshot.outputs[index].divider << "\n";
    }

    if (!output.good())
    {
        throw Poco::IOException("Unable to flush divider state file " + std::string(kDividerStateFile));
    }
}

double maximumStartupOutputFrequencyHz(const DividerSnapshot& snapshot)
{
    double maximum = 0.0;
    const std::size_t count = std::min(snapshot.outputs.size(), kDividerStartupOutputCount);
    for (std::size_t index = 0; index < count; ++index)
    {
        maximum = std::max(maximum, snapshot.outputs[index].frequencyHz);
    }
    return maximum;
}

std::string acquisitionClockSafetyMessage(const DividerSnapshot& snapshot)
{
    const double maximumFrequencyHz = maximumStartupOutputFrequencyHz(snapshot);
    if (maximumFrequencyHz <= static_cast<double>(kMaxSafeAcquisitionClockHz))
    {
        return std::string();
    }

    return Poco::format(
        "Acquisition remains stopped because the configured clock (%.0f Hz) exceeds the software safety limit (%Lu Hz).",
        maximumFrequencyHz,
        static_cast<unsigned long long>(kMaxSafeAcquisitionClockHz));
}

std::vector<int> outputMaskToIndices(std::uint32_t outputMask)
{
    std::vector<int> outputIndices;
    for (std::size_t index = 0; index < kJndm123DividerOutputCount; ++index)
    {
        if ((outputMask & (1u << index)) != 0u)
        {
            outputIndices.push_back(static_cast<int>(index));
        }
    }
    if (outputIndices.empty())
    {
        throw Poco::InvalidArgumentException("At least one output must be selected.");
    }
    return outputIndices;
}

std::string describeOutputs(const std::vector<int>& outputIndices)
{
    std::ostringstream description;
    for (std::size_t index = 0; index < outputIndices.size(); ++index)
    {
        if (index > 0) description << ", ";
        description << kDividerOutputs[outputIndices[index]].name;
    }
    return description.str();
}

#if defined(__linux__)
int openCdce937Device(JNDM123AcquisitionRuntime::Cdce937Device& device)
{
    device.fd = ::open(device.path.c_str(), O_RDWR);
    return device.fd >= 0 ? 0 : -1;
}

void closeCdce937Device(JNDM123AcquisitionRuntime::Cdce937Device& device)
{
    if (device.fd >= 0)
    {
        ::close(device.fd);
        device.fd = -1;
    }
}

int i2cByteRead(int fd, std::uint8_t address, std::uint8_t reg, std::uint8_t& value)
{
    std::uint8_t cmd = static_cast<std::uint8_t>(0x80u | reg);
    struct i2c_msg messages[2];
    struct i2c_rdwr_ioctl_data ioctlData;

    messages[0].addr = address;
    messages[0].flags = 0;
    messages[0].len = 1;
    messages[0].buf = &cmd;

    messages[1].addr = address;
    messages[1].flags = I2C_M_RD;
    messages[1].len = 1;
    messages[1].buf = &value;

    ioctlData.msgs = messages;
    ioctlData.nmsgs = 2;
    return ::ioctl(fd, I2C_RDWR, &ioctlData) < 0 ? -1 : 0;
}

int i2cByteWrite(int fd, std::uint8_t address, std::uint8_t reg, std::uint8_t value)
{
    std::uint8_t buffer[2];
    struct i2c_msg message;
    struct i2c_rdwr_ioctl_data ioctlData;

    buffer[0] = static_cast<std::uint8_t>(0x80u | reg);
    buffer[1] = value;

    message.addr = address;
    message.flags = 0;
    message.len = sizeof(buffer);
    message.buf = buffer;

    ioctlData.msgs = &message;
    ioctlData.nmsgs = 1;
    return ::ioctl(fd, I2C_RDWR, &ioctlData) < 0 ? -1 : 0;
}

bool looksLikeCdce937(int fd, std::uint8_t address)
{
    std::uint8_t id = 0;
    if (i2cByteRead(fd, address, kCdce937RegId, id) != 0) return false;
    return (id & 0x0Fu) == 0x01u;
}

void autodetectCdce937(JNDM123AcquisitionRuntime::Cdce937Device& device)
{
    if (device.addressFixed) return;

    for (const auto address: kProbeAddresses)
    {
        if (looksLikeCdce937(device.fd, address))
        {
            device.address = address;
            return;
        }
    }

    throw Poco::NotFoundException("No CDCE937 device found on the requested I2C bus.");
}

const char* inclkName(std::uint8_t value)
{
    switch (value)
    {
    case 0: return "crystal";
    case 1: return "vcxo";
    case 2: return "lvcmos";
    default: return "reserved";
    }
}

int readDividerRegister(JNDM123AcquisitionRuntime::Cdce937Device& device, int outputIndex)
{
    if (outputIndex >= 0 && outputIndex <= 2)
    {
        std::uint8_t reg02 = 0;
        std::uint8_t reg03 = 0;
        if (i2cByteRead(device.fd, device.address, 0x02, reg02) != 0) return 0;
        if (i2cByteRead(device.fd, device.address, 0x03, reg03) != 0) return 0;
        return static_cast<int>(((reg02 & 0x03u) << 8) | reg03);
    }

    const std::uint8_t registers[] = {0x26, 0x26, 0x26, 0x37};
    std::uint8_t value = 0;
    if (i2cByteRead(device.fd, device.address, registers[outputIndex - 3], value) != 0) return 0;
    return static_cast<int>(value & 0x7Fu);
}

void writeDividerRegister(JNDM123AcquisitionRuntime::Cdce937Device& device, int outputIndex, int divider)
{
    if (outputIndex >= 0 && outputIndex <= 2)
    {
        std::uint8_t reg02 = 0;
        if (i2cByteRead(device.fd, device.address, 0x02, reg02) != 0)
        {
            throw Poco::IOException("Unable to read CDCE937 register 0x02.");
        }

        reg02 = static_cast<std::uint8_t>((reg02 & 0xFCu) | ((divider >> 8) & 0x03u));
        if (i2cByteWrite(device.fd, device.address, 0x02, reg02) != 0 ||
            i2cByteWrite(device.fd, device.address, 0x03, static_cast<std::uint8_t>(divider & 0xFFu)) != 0)
        {
            throw Poco::IOException("Unable to write Y1 divider registers.");
        }
        return;
    }

    const std::uint8_t registers[] = {0x26, 0x26, 0x26, 0x37};
    std::uint8_t oldValue = 0;
    if (i2cByteRead(device.fd, device.address, registers[outputIndex - 3], oldValue) != 0)
    {
        throw Poco::IOException("Unable to read divider register.");
    }

    const std::uint8_t newValue = static_cast<std::uint8_t>((oldValue & 0x80u) | (divider & 0x7Fu));
    if (i2cByteWrite(device.fd, device.address, registers[outputIndex - 3], newValue) != 0)
    {
        throw Poco::IOException("Unable to write divider register.");
    }
}

void writeMaskedCdce937Register(
        JNDM123AcquisitionRuntime::Cdce937Device& device,
        std::uint8_t reg,
        std::uint8_t mask,
        std::uint8_t value,
        const std::string& errorMessage)
{
    std::uint8_t current = 0;
    if (i2cByteRead(device.fd, device.address, reg, current) != 0)
    {
        throw Poco::IOException(errorMessage + " (read failed).");
    }

    const std::uint8_t updated = static_cast<std::uint8_t>((current & ~mask) | (value & mask));
    if (updated == current) return;

    if (i2cByteWrite(device.fd, device.address, reg, updated) != 0)
    {
        throw Poco::IOException(errorMessage + " (write failed).");
    }
}

void forceOutputEnabledLocked(JNDM123AcquisitionRuntime::Cdce937Device& device, int outputIndex)
{
    switch (outputIndex)
    {
    case 0:
        writeMaskedCdce937Register(device, kCdce937RegY1Control, 0x3Cu, 0x3Cu, "Unable to force-enable Y1 output");
        break;
    case 1:
    case 2:
        writeMaskedCdce937Register(device, kCdce937RegMux1, 0x0Fu, 0x0Fu, "Unable to force-enable Y2/Y3 output group");
        break;
    case 3:
    case 4:
        writeMaskedCdce937Register(device, kCdce937RegMux2, 0x0Fu, 0x0Fu, "Unable to force-enable Y4/Y5 output group");
        break;
    case 5:
    case 6:
        writeMaskedCdce937Register(device, kCdce937RegMux3, 0x0Fu, 0x0Fu, "Unable to force-enable Y6/Y7 output group");
        break;
    default:
        throw Poco::InvalidArgumentException("Output index must be in 0..6.");
    }
}

void disablePllModeForOutputLocked(JNDM123AcquisitionRuntime::Cdce937Device& device, int outputIndex)
{
    switch (outputIndex)
    {
    case 0:
        writeMaskedCdce937Register(device, kCdce937RegY1Control, 0x80u, 0x00u, "Unable to route Y1 directly from the input clock");
        break;
    case 1:
        writeMaskedCdce937Register(device, kCdce937RegMux1, 0xC0u, 0x80u, "Unable to route Y2 to shared Pdiv1 in PLL bypass mode");
        break;
    case 2:
        writeMaskedCdce937Register(device, kCdce937RegMux1, 0xB0u, 0x80u, "Unable to route Y3 to shared Pdiv1 in PLL bypass mode");
        break;
    case 3:
        writeMaskedCdce937Register(device, kCdce937RegMux2, 0xC0u, 0xC0u, "Unable to bypass PLL routing for Y4");
        break;
    case 4:
        writeMaskedCdce937Register(device, kCdce937RegMux2, 0xB0u, 0x90u, "Unable to route Y5 to shared Pdiv4 in PLL bypass mode");
        break;
    case 5:
        writeMaskedCdce937Register(device, kCdce937RegMux3, 0xC0u, 0x80u, "Unable to route Y6 to shared Pdiv4 in PLL bypass mode");
        break;
    case 6:
        writeMaskedCdce937Register(device, kCdce937RegMux3, 0xB0u, 0xA0u, "Unable to bypass PLL routing for Y7");
        break;
    default:
        throw Poco::InvalidArgumentException("Output index must be in 0..6.");
    }
}

void disablePllModeLocked(JNDM123AcquisitionRuntime::Cdce937Device& device)
{
    writeMaskedCdce937Register(
        device,
        kCdce937RegCfg1,
        0x1Cu,
        0x08u,
        "Unable to switch CDCE937 input clock to external LVCMOS bypass mode");
}

void writeOutputDividerLocked(JNDM123AcquisitionRuntime::Cdce937Device& device, int outputIndex, int divider)
{
    forceOutputEnabledLocked(device, outputIndex);
    disablePllModeForOutputLocked(device, outputIndex);
    writeDividerRegister(device, outputIndex, divider);
}

DividerSnapshot readDividerStatusLocked(const std::string& devicePath, Poco::UInt64 referenceClockHz)
{
    DividerSnapshot snapshot;
    snapshot.devicePath = devicePath.empty() ? kDefaultI2CDevice : devicePath;
    snapshot.referenceClockHz = referenceClockHz;

    JNDM123AcquisitionRuntime::Cdce937Device device;
    device.path = snapshot.devicePath;

    if (openCdce937Device(device) != 0)
    {
        throw Poco::IOException("Unable to open I2C device " + snapshot.devicePath);
    }

    try
    {
        autodetectCdce937(device);

        std::uint8_t reg0 = 0;
        std::uint8_t reg1 = 0;
        if (i2cByteRead(device.fd, device.address, kCdce937RegId, reg0) != 0 ||
            i2cByteRead(device.fd, device.address, kCdce937RegCfg1, reg1) != 0)
        {
            throw Poco::IOException("Unable to read CDCE937 status registers.");
        }

        snapshot.deviceType = (reg0 & 0x80u) ? "CDCE937" : "CDCEL937";
        snapshot.address = Poco::format("0x%02x", static_cast<unsigned>(device.address));
        snapshot.inputClock = inclkName((reg1 >> 2) & 0x03u);
        snapshot.revisionId = (reg0 >> 4) & 0x07u;
        snapshot.eepBusy = ((reg1 >> 6) & 0x01u) != 0;
        snapshot.eepLock = ((reg1 >> 5) & 0x01u) != 0;
        snapshot.powerDown = ((reg1 >> 4) & 0x01u) != 0;

        for (const auto& spec: kDividerOutputs)
        {
            DividerOutputState output;
            output.index = spec.index;
            output.divider = readDividerRegister(device, spec.index);
            output.frequencyHz = output.divider > 0
                ? (static_cast<double>(referenceClockHz) / static_cast<double>(output.divider))
                : 0.0;
            snapshot.outputs.push_back(output);
        }

        snapshot.message = "Divider status synchronized from hardware.";
    }
    catch (...)
    {
        closeCdce937Device(device);
        throw;
    }

    closeCdce937Device(device);
    return snapshot;
}

DividerSnapshot applyDividersLocked(const std::string& devicePath, const std::vector<int>& outputIndices, int divider, Poco::UInt64 referenceClockHz)
{
    JNDM123AcquisitionRuntime::Cdce937Device device;
    device.path = devicePath.empty() ? kDefaultI2CDevice : devicePath;

    if (openCdce937Device(device) != 0)
    {
        throw Poco::IOException("Unable to open I2C device " + device.path);
    }

    try
    {
        autodetectCdce937(device);
        disablePllModeLocked(device);
        for (const int outputIndex: outputIndices)
        {
            writeOutputDividerLocked(device, outputIndex, divider);
        }
    }
    catch (...)
    {
        closeCdce937Device(device);
        throw;
    }

    closeCdce937Device(device);

    DividerSnapshot snapshot = readDividerStatusLocked(device.path, referenceClockHz);
    saveDividerConfiguration(snapshot, snapshot.devicePath);
    snapshot.message = "Divider applied to " + describeOutputs(outputIndices) + " and read back from hardware.";
    return snapshot;
}
#endif

} // namespace

JNDM123AcquisitionRuntime::JNDM123AcquisitionRuntime(int ddsDomain):
    _ddsDomain(ddsDomain)
{
    _readerRunnablePtr = new ReaderRunnable(*this);
    _publisherRunnablePtr = new PublisherRunnable(*this);
    _publisher.start(_ddsDomain);
    _publisherThread.start(*_publisherRunnablePtr);
    _publisherStarted = true;

#if defined(__linux__)
    try
    {
        Poco::FastMutex::ScopedLock lock(_controlMutex);
        initializeFromSavedConfigurationLocked();
    }
    catch (const Poco::Exception& exc)
    {
        recordError(exc.displayText());
        setStatusMessage("JNDM123 startup initialization skipped: " + exc.displayText());
        logger().warning("JNDM123 startup initialization skipped: " + exc.displayText());
    }
    catch (const std::exception& exc)
    {
        recordError(exc.what());
        setStatusMessage(std::string("JNDM123 startup initialization skipped: ") + exc.what());
        logger().warning(std::string("JNDM123 startup initialization skipped: ") + exc.what());
    }
#endif

    publishSnapshot();
}

JNDM123AcquisitionRuntime::~JNDM123AcquisitionRuntime()
{
    shutdown();
}

AcquisitionDdsCommandResult JNDM123AcquisitionRuntime::handleCommand(const AcquisitionDdsCommand& command)
{
    AcquisitionDdsCommandResult result;
    result.requestId = command.requestId;
    result.clientId = command.clientId;

    Poco::FastMutex::ScopedLock lock(_controlMutex);

    try
    {
        switch (command.kind)
        {
        case AcquisitionCommandKind::Status:
            result.snapshot = buildSnapshotLocked(command.devicePath);
            result.ok = result.snapshot.ok;
            result.message = result.snapshot.message;
            break;
        case AcquisitionCommandKind::Start:
        {
            const auto status = startAcquisitionLocked();
            result.snapshot = buildSnapshotLocked(command.devicePath);
            result.ok = status.first;
            result.message = status.second;
            result.snapshot.ok = status.first;
            result.snapshot.message = status.second;
            break;
        }
        case AcquisitionCommandKind::Stop:
        {
            const auto status = stopAcquisitionLocked(
                command.message.empty() ? "Acquisition stopped by operator." : command.message,
                false);
            result.snapshot = buildSnapshotLocked(command.devicePath);
            result.ok = status.first;
            result.message = status.second;
            result.snapshot.ok = status.first;
            result.snapshot.message = status.second;
            break;
        }
        case AcquisitionCommandKind::ApplyDividers:
        {
#if defined(__linux__)
            const std::vector<int> outputIndices = outputMaskToIndices(command.outputMask);
            for (const int outputIndex: outputIndices)
            {
                validateDividerOrThrow(outputIndex, command.divider);
            }
            validateReferenceClockHzOrThrow(command.referenceClockHz);

            const bool wasRunning = _running.load();
            if (wasRunning)
            {
                stopAcquisitionLocked("Acquisition paused for divider update.", false);
            }

            try
            {
                DividerSnapshot dividerSnapshot = applyDividersLocked(command.devicePath, outputIndices, command.divider, command.referenceClockHz);
                if (wasRunning)
                {
                    const std::string safetyMessage = acquisitionClockSafetyMessage(dividerSnapshot);
                    if (!safetyMessage.empty())
                    {
                        dividerSnapshot.ok = false;
                        dividerSnapshot.message += " " + safetyMessage;
                        setStatusMessage(dividerSnapshot.message);
                    }
                    else
                    {
                        const auto restartStatus = startAcquisitionLocked();
                        if (!restartStatus.first)
                        {
                            dividerSnapshot.ok = false;
                            dividerSnapshot.message += " Divider update completed, but acquisition restart failed: " + restartStatus.second;
                        }
                        else
                        {
                            dividerSnapshot.message += " Acquisition restarted.";
                        }
                    }
                }
                setStatusMessage(dividerSnapshot.message);
                result.snapshot = buildSnapshotLocked(command.devicePath);
                result.ok = dividerSnapshot.ok;
                result.message = dividerSnapshot.message;
                result.snapshot.ok = dividerSnapshot.ok;
                result.snapshot.message = dividerSnapshot.message;
            }
            catch (...)
            {
                if (wasRunning)
                {
                    try
                    {
                        startAcquisitionLocked();
                    }
                    catch (...)
                    {
                    }
                }
                throw;
            }
#else
            throw Poco::NotImplementedException("Divider control is only supported on Linux.");
#endif
            break;
        }
        case AcquisitionCommandKind::UpdateReferenceClock:
        {
#if defined(__linux__)
            validateReferenceClockHzOrThrow(command.referenceClockHz);
            DividerSnapshot dividerSnapshot = readDividerStatusLocked(command.devicePath, command.referenceClockHz);
            saveDividerConfiguration(dividerSnapshot, dividerSnapshot.devicePath);
            dividerSnapshot.message = "External reference clock updated and saved.";
            setStatusMessage(dividerSnapshot.message);
            result.snapshot = buildSnapshotLocked(command.devicePath);
            result.ok = true;
            result.message = dividerSnapshot.message;
            result.snapshot.ok = true;
            result.snapshot.message = dividerSnapshot.message;
#else
            throw Poco::NotImplementedException("Reference clock control is only supported on Linux.");
#endif
            break;
        }
        default:
            throw Poco::InvalidArgumentException("Unsupported acquisition command.");
        }
    }
    catch (const Poco::Exception& exc)
    {
        result.snapshot = buildSnapshotLocked(command.devicePath);
        result.ok = false;
        result.message = exc.displayText();
        result.snapshot.ok = false;
        result.snapshot.message = result.message;
        recordError(result.message);
        setStatusMessage(result.message);
    }
    catch (const std::exception& exc)
    {
        result.snapshot = buildSnapshotLocked(command.devicePath);
        result.ok = false;
        result.message = exc.what();
        result.snapshot.ok = false;
        result.snapshot.message = result.message;
        recordError(result.message);
        setStatusMessage(result.message);
    }

    publishSnapshot();
    return result;
}

void JNDM123AcquisitionRuntime::shutdown()
{
    if (_shutdown.exchange(true)) return;

    {
        Poco::FastMutex::ScopedLock lock(_controlMutex);
        stopAcquisitionLocked("Acquisition stopped during process shutdown.", false);
    }

    _publishWake.set();
    if (_publisherStarted)
    {
        _publisherThread.join();
        _publisherStarted = false;
    }
    _publisher.stop();

#if defined(__linux__)
    releaseMappedHardwareLocked();
#endif

    delete _readerRunnablePtr;
    _readerRunnablePtr = nullptr;
    delete _publisherRunnablePtr;
    _publisherRunnablePtr = nullptr;
}

void JNDM123AcquisitionRuntime::setStatusMessage(const std::string& message)
{
    Poco::FastMutex::ScopedLock lock(_stateMutex);
    _statusMessage = message;
}

void JNDM123AcquisitionRuntime::recordError(const std::string& message)
{
    Poco::FastMutex::ScopedLock lock(_stateMutex);
    _lastError = message;
}

void JNDM123AcquisitionRuntime::clearHistoryLocked()
{
    for (auto& series: _history)
    {
        series.clear();
    }
    _timelineUs.clear();
    _hasLatestFrame = false;
    _lastFrameAt.clear();
}

void JNDM123AcquisitionRuntime::clearHistory()
{
    Poco::FastMutex::ScopedLock lock(_stateMutex);
    clearHistoryLocked();
}

AcquisitionDdsSnapshot JNDM123AcquisitionRuntime::buildSnapshotLocked(const std::string& requestedDevicePath)
{
    AcquisitionDdsSnapshot snapshot;
    snapshot.sequence = _snapshotSequence.fetch_add(1) + 1;
    snapshot.updatedAt = isoTimestamp();
    snapshot.previewActive = true;
    snapshot.historyLimit = static_cast<uint32_t>(kHistoryLimit);
    snapshot.queueDepth = 0;

    DividerSnapshot dividerSnapshot;
    try
    {
#if defined(__linux__)
        dividerSnapshot = readDividerStatusLocked(
            resolvePreferredDevicePath(requestedDevicePath),
            resolvePreferredReferenceClockHz());
#else
        dividerSnapshot.devicePath = resolvePreferredDevicePath(requestedDevicePath);
        dividerSnapshot.referenceClockHz = resolvePreferredReferenceClockHz();
#endif
    }
    catch (const Poco::Exception& exc)
    {
        dividerSnapshot.ok = false;
        dividerSnapshot.message = exc.displayText();
        dividerSnapshot.devicePath = resolvePreferredDevicePath(requestedDevicePath);
        dividerSnapshot.referenceClockHz = loadSavedDividerConfiguration().referenceClockHz;
    }

    std::array<std::int16_t, kJndm123ChannelCount> latestSamples{};
    std::array<std::deque<std::int16_t>, kJndm123ChannelCount> historyCopy;
    std::deque<Poco::Int64> timelineCopy;
    std::string statusMessage;
    std::string lastError;
    std::string lastFrameAt;
    std::string lastPublishedAt;
    bool hasLatestFrame = false;
    bool running = false;

    {
        Poco::FastMutex::ScopedLock lock(_stateMutex);
        latestSamples = _latestSamples;
        historyCopy = _history;
        timelineCopy = _timelineUs;
        statusMessage = _statusMessage.empty()
            ? (_running.load() ? "Acquisition running." : "Acquisition idle.")
            : _statusMessage;
        lastError = _lastError;
        lastFrameAt = _lastFrameAt;
        lastPublishedAt = _lastPublishedAt;
        hasLatestFrame = _hasLatestFrame;
        running = _running.load();
    }

    snapshot.ok = dividerSnapshot.ok;
    snapshot.message = dividerSnapshot.ok ? statusMessage : dividerSnapshot.message;
    snapshot.running = running;
    snapshot.totalFrames = _totalFrames.load();
    snapshot.droppedFrames = _droppedFrames.load();
    snapshot.recoveries = _recoveries.load();
    snapshot.lastFrameSequence = _lastFrameSequence.load();
    snapshot.lastFrameAt = lastFrameAt;
    snapshot.waveformUpdatedAt = lastPublishedAt;
    snapshot.lastError = lastError;
    snapshot.devicePath = dividerSnapshot.devicePath;
    snapshot.referenceClockHz = dividerSnapshot.referenceClockHz;
    snapshot.deviceType = dividerSnapshot.deviceType;
    snapshot.address = dividerSnapshot.address;
    snapshot.inputClock = dividerSnapshot.inputClock;
    snapshot.revisionId = dividerSnapshot.revisionId;
    snapshot.eepBusy = dividerSnapshot.eepBusy;
    snapshot.eepLock = dividerSnapshot.eepLock;
    snapshot.powerDown = dividerSnapshot.powerDown;

    for (std::size_t index = 0; index < dividerSnapshot.outputs.size() && index < kJndm123DividerOutputCount; ++index)
    {
        snapshot.dividerValues[index] = dividerSnapshot.outputs[index].divider;
        snapshot.dividerFrequenciesHz[index] = dividerSnapshot.outputs[index].frequencyHz;
        snapshot.dividerEnabled[index] = dividerSnapshot.outputs[index].divider > 0;
    }

    snapshot.timelineUs.reserve(timelineCopy.size());
    for (const auto value: timelineCopy)
    {
        snapshot.timelineUs.push_back(value);
    }

    const std::size_t sampleCount = historyCopy[0].size();
    snapshot.historySamples.reserve(kJndm123ChannelCount * sampleCount);
    for (std::size_t channelIndex = 0; channelIndex < kJndm123ChannelCount; ++channelIndex)
    {
        snapshot.latestValueValid[channelIndex] = hasLatestFrame;
        snapshot.latestValues[channelIndex] = static_cast<int32_t>(latestSamples[channelIndex]);
        for (const auto sample: historyCopy[channelIndex])
        {
            snapshot.historySamples.push_back(static_cast<int32_t>(sample));
        }
    }

    return snapshot;
}

void JNDM123AcquisitionRuntime::publishSnapshot()
{
    AcquisitionDdsSnapshot snapshot;
    {
        Poco::FastMutex::ScopedLock lock(_controlMutex);
        snapshot = buildSnapshotLocked();
    }
    _publisher.publish(snapshot);
    Poco::FastMutex::ScopedLock stateLock(_stateMutex);
    _lastPublishedAt = snapshot.updatedAt;
}

std::pair<bool, std::string> JNDM123AcquisitionRuntime::startAcquisitionLocked()
{
    if (_running.load())
    {
        const std::string message = "Acquisition is already running.";
        setStatusMessage(message);
        return std::make_pair(true, message);
    }

    try
    {
#if defined(__linux__)
        const DividerSnapshot safetySnapshot = readDividerStatusLocked(resolvePreferredDevicePath(""), resolvePreferredReferenceClockHz());
        const std::string safetyMessage = acquisitionClockSafetyMessage(safetySnapshot);
        if (!safetyMessage.empty())
        {
            setStatusMessage(safetyMessage);
            return std::make_pair(false, safetyMessage);
        }

        ensureMappedHardwareLocked();
        initializeGpioLocked();
        stopAdcLocked();
        resetFifoLocked();
        clearHistoryLocked();
        _readerShouldRun.store(true);
        _readerThread.start(*_readerRunnablePtr);
        _readerStarted = true;
        startAdcLocked();
        _running.store(true);
        _lastError.clear();
        setStatusMessage("Acquisition started.");
        return std::make_pair(true, std::string("Acquisition started."));
#else
        const std::string message = "Acquisition is only supported on Linux.";
        recordError(message);
        setStatusMessage(message);
        return std::make_pair(false, message);
#endif
    }
    catch (const Poco::Exception& exc)
    {
        _readerShouldRun.store(false);
        recordError(exc.displayText());
        setStatusMessage(exc.displayText());
        return std::make_pair(false, exc.displayText());
    }
    catch (const std::exception& exc)
    {
        _readerShouldRun.store(false);
        recordError(exc.what());
        setStatusMessage(exc.what());
        return std::make_pair(false, std::string(exc.what()));
    }
}

std::pair<bool, std::string> JNDM123AcquisitionRuntime::stopAcquisitionLocked(const std::string& message, bool clearHistoryAfterStop)
{
    _readerShouldRun.store(false);
    if (_readerStarted)
    {
        _readerThread.join();
        _readerStarted = false;
    }

#if defined(__linux__)
    if (_hardware.fifoCtrl && _hardware.fifoData)
    {
        stopAdcLocked();
        resetFifoLocked();
    }
#endif

    _running.store(false);
    if (clearHistoryAfterStop)
    {
        clearHistoryLocked();
    }
    setStatusMessage(message);
    return std::make_pair(true, message);
}

void JNDM123AcquisitionRuntime::readerLoop()
{
#if defined(__linux__)
    std::array<std::uint32_t, kFrameWords> words{};
    while (_readerShouldRun.load() && !_shutdown.load())
    {
        const int result = readOneFramePacket(words);
        if (result == 0)
        {
            ::usleep(1000);
            continue;
        }
        if (result < 0)
        {
            ++_recoveries;
            recordError("RX FIFO recovered after a packet error.");
            recoverRxFifo();
            ::usleep(1000);
            continue;
        }

        std::array<std::int16_t, kJndm123ChannelCount> samples{};
        unpackFrame(words, samples);
        const Poco::Timestamp now;
        const Poco::Int64 capturedAtUs = now.epochMicroseconds();

        {
            Poco::FastMutex::ScopedLock lock(_stateMutex);
            _latestSamples = samples;
            _hasLatestFrame = true;
            _timelineUs.push_back(capturedAtUs);
            while (_timelineUs.size() > kHistoryLimit)
            {
                _timelineUs.pop_front();
            }
            for (std::size_t index = 0; index < samples.size(); ++index)
            {
                auto& series = _history[index];
                series.push_back(samples[index]);
                while (series.size() > kHistoryLimit)
                {
                    series.pop_front();
                }
            }
            _lastFrameAt = isoTimestamp(now);
        }

        _totalFrames.fetch_add(1);
        _lastFrameSequence.store(_totalFrames.load());
    }
#endif
}

void JNDM123AcquisitionRuntime::publisherLoop()
{
    while (!_shutdown.load())
    {
        _publishWake.tryWait(1000);
        if (_shutdown.load()) break;
        publishSnapshot();
    }
}

void JNDM123AcquisitionRuntime::unpackFrame(
        const std::array<std::uint32_t, kJndm123ChannelCount / 2>& words,
        std::array<std::int16_t, kJndm123ChannelCount>& samples) const
{
    for (std::size_t index = 0; index < words.size(); ++index)
    {
        const std::uint32_t word = words[index];
        samples[index * 2] = static_cast<std::int16_t>(word & 0xFFFFu);
        samples[index * 2 + 1] = static_cast<std::int16_t>((word >> 16) & 0xFFFFu);
    }
}

#if defined(__linux__)
void JNDM123AcquisitionRuntime::initializeFromSavedConfigurationLocked()
{
    const SavedDividerConfiguration saved = loadSavedDividerConfiguration();
    if (!saved.hasAnyOutput)
    {
        throw Poco::NotFoundException("No valid saved divider configuration was found for Y1~Y6.");
    }

    Cdce937Device device;
    device.path = saved.devicePath.empty() ? kDefaultI2CDevice : saved.devicePath;
    if (openCdce937Device(device) != 0)
    {
        throw Poco::IOException("Unable to open I2C device " + device.path);
    }

    try
    {
        autodetectCdce937(device);
        disablePllModeLocked(device);
        for (std::size_t index = 0; index < saved.dividers.size(); ++index)
        {
            if (!saved.savedOutputs[index]) continue;
            validateDividerOrThrow(static_cast<int>(index), saved.dividers[index]);
            writeOutputDividerLocked(device, static_cast<int>(index), saved.dividers[index]);
        }
    }
    catch (...)
    {
        closeCdce937Device(device);
        throw;
    }
    closeCdce937Device(device);

    DividerSnapshot snapshot = readDividerStatusLocked(device.path, saved.referenceClockHz);
    saveDividerConfiguration(snapshot, snapshot.devicePath);

    const std::string safetyMessage = acquisitionClockSafetyMessage(snapshot);
    if (!safetyMessage.empty())
    {
        setStatusMessage(safetyMessage);
        logger().warning(safetyMessage);
        return;
    }

    const auto startStatus = startAcquisitionLocked();
    if (startStatus.first)
    {
        logger().information(startStatus.second);
    }
    else
    {
        logger().warning(startStatus.second);
    }
}

void JNDM123AcquisitionRuntime::ensureMappedHardwareLocked()
{
    if (_hardware.memFd >= 0 && _hardware.fifoCtrl && _hardware.fifoData) return;

    _hardware.memFd = ::open("/dev/mem", O_RDWR | O_SYNC);
    if (_hardware.memFd < 0)
    {
        throw Poco::IOException("Unable to open /dev/mem");
    }

    _hardware.fifoCtrl = static_cast<volatile std::uint32_t*>(::mmap(
        nullptr, kMapSize, PROT_READ | PROT_WRITE, MAP_SHARED, _hardware.memFd, kFifoCtrlBase));
    _hardware.fifoData = static_cast<volatile std::uint32_t*>(::mmap(
        nullptr, kMapSize, PROT_READ, MAP_SHARED, _hardware.memFd, kFifoDataPort));

    if (_hardware.fifoCtrl == MAP_FAILED || _hardware.fifoData == MAP_FAILED)
    {
        releaseMappedHardwareLocked();
        throw Poco::IOException("Unable to map FIFO registers");
    }
}

void JNDM123AcquisitionRuntime::releaseMappedHardwareLocked()
{
    if (_hardware.fifoCtrl && _hardware.fifoCtrl != MAP_FAILED)
    {
        ::munmap((void*) _hardware.fifoCtrl, kMapSize);
    }
    if (_hardware.fifoData && _hardware.fifoData != MAP_FAILED)
    {
        ::munmap((void*) _hardware.fifoData, kMapSize);
    }
    if (_hardware.memFd >= 0)
    {
        ::close(_hardware.memFd);
    }

    _hardware.memFd = -1;
    _hardware.fifoCtrl = nullptr;
    _hardware.fifoData = nullptr;
    _hardware.gpioInitialized = false;
}

void JNDM123AcquisitionRuntime::initializeGpioLocked()
{
    if (_hardware.gpioInitialized) return;

    exportGpio(kGpioReset);
    for (std::size_t chip = 0; chip < kJndm123Ad7606Count; ++chip)
    {
        exportGpio(kGpioMaskStart + static_cast<int>(chip));
    }

    ::usleep(100000);
    setGpioDirection(kGpioReset, "out");
    for (std::size_t chip = 0; chip < kJndm123Ad7606Count; ++chip)
    {
        setGpioDirection(kGpioMaskStart + static_cast<int>(chip), "out");
    }
    _hardware.gpioInitialized = true;
}

void JNDM123AcquisitionRuntime::stopAdcLocked()
{
    setGpioValue(kGpioReset, true);
    for (std::size_t chip = 0; chip < kJndm123Ad7606Count; ++chip)
    {
        setGpioValue(kGpioMaskStart + static_cast<int>(chip), activeMaskValue(false));
    }
}

void JNDM123AcquisitionRuntime::startAdcLocked()
{
    setGpioValue(kGpioReset, true);
    ::usleep(1000);
    for (std::size_t chip = 0; chip < kJndm123Ad7606Count; ++chip)
    {
        setGpioValue(kGpioMaskStart + static_cast<int>(chip), activeMaskValue(true));
    }
    ::usleep(1000);
    setGpioValue(kGpioReset, false);
}

void JNDM123AcquisitionRuntime::resetFifoLocked()
{
    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
    _hardware.fifoCtrl[kRdfr / 4] = 0xA5u;
    ::usleep(1000);
    while (_hardware.fifoCtrl[kRdfo / 4] > 0)
    {
        (void)_hardware.fifoData[0];
    }
    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
}

void JNDM123AcquisitionRuntime::recoverRxFifo()
{
    if (!_hardware.fifoCtrl || !_hardware.fifoData) return;

    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
    _hardware.fifoCtrl[kRdfr / 4] = 0xA5u;
    ::usleep(1000);
    while (_hardware.fifoCtrl[kRdfo / 4] > 0)
    {
        (void)_hardware.fifoData[0];
    }
    _hardware.fifoCtrl[kSrr / 4] = 0xA5u;
    ::usleep(1000);
    _hardware.fifoCtrl[kIsr / 4] = 0xFFFFFFFFu;
    ::usleep(1000);
}

int JNDM123AcquisitionRuntime::readOneFramePacket(std::array<std::uint32_t, kJndm123ChannelCount / 2>& frame)
{
    const std::uint32_t isr = _hardware.fifoCtrl[kIsr / 4];
    if (isr & (kIsrRpue | kIsrRpore | kIsrRpure))
    {
        return -2;
    }

    const std::uint32_t occupancy = _hardware.fifoCtrl[kRdfo / 4];
    if (occupancy == 0)
    {
        return 0;
    }

    const std::uint32_t rlr = _hardware.fifoCtrl[kRlr / 4];
    if (rlr & 0x80000000u)
    {
        return 0;
    }

    const std::uint32_t bytes = rlr & 0x007FFFFFu;
    if (bytes != frame.size() * sizeof(std::uint32_t))
    {
        return -3;
    }

    for (std::size_t index = 0; index < frame.size(); ++index)
    {
        frame[index] = _hardware.fifoData[0];
    }
    return 1;
}
#endif

void ReaderRunnable::run()
{
    _owner.readerLoop();
}

void PublisherRunnable::run()
{
    _owner.publisherLoop();
}

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
