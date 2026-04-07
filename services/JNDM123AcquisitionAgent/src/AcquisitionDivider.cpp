#include "AcquisitionDivider.h"

#include "AcquisitionAgentSupport.h"

#include "Poco/Exception.h"
#include "Poco/Format.h"

#include <sstream>

#if defined(__linux__)
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

#if defined(__linux__)
const std::uint8_t kProbeAddresses[4] = {0x6c, 0x6d, 0x6e, 0x6f};
#endif

namespace {

#if defined(__linux__)
struct Cdce937Device
{
    std::string path;
    int fd = -1;
    std::uint8_t address = 0x6c;
    bool addressFixed = false;
};

int openCdce937Device(Cdce937Device& device)
{
    device.fd = ::open(device.path.c_str(), O_RDWR);
    return device.fd >= 0 ? 0 : -1;
}

void closeCdce937Device(Cdce937Device& device)
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

void autodetectCdce937(Cdce937Device& device)
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

int readDividerRegister(Cdce937Device& device, int outputIndex)
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

void writeMaskedCdce937Register(
    Cdce937Device& device,
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

void writeDividerRegister(Cdce937Device& device, int outputIndex, int divider)
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

void forceOutputEnabledLocked(Cdce937Device& device, int outputIndex)
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

void disablePllModeForOutputLocked(Cdce937Device& device, int outputIndex)
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

void writeOutputDividerLocked(Cdce937Device& device, int outputIndex, int divider)
{
    forceOutputEnabledLocked(device, outputIndex);
    disablePllModeForOutputLocked(device, outputIndex);
    writeDividerRegister(device, outputIndex, divider);
}

void disablePllModeLocked(Cdce937Device& device)
{
    writeMaskedCdce937Register(
        device,
        kCdce937RegCfg1,
        0x1Cu,
        0x08u,
        "Unable to switch CDCE937 input clock to external LVCMOS bypass mode");
}

template <typename Fn>
DividerSnapshot withCdce937(
    const std::string& devicePath,
    Poco::UInt64 referenceClockHz,
    const Fn& fn)
{
    Cdce937Device device;
    device.path = devicePath.empty() ? kDefaultI2CDevice : devicePath;

    if (openCdce937Device(device) != 0)
    {
        throw Poco::IOException("Unable to open I2C device " + device.path);
    }

    try
    {
        autodetectCdce937(device);
        DividerSnapshot snapshot = fn(device, referenceClockHz);
        closeCdce937Device(device);
        return snapshot;
    }
    catch (...)
    {
        closeCdce937Device(device);
        throw;
    }
}
#endif

} // namespace

#if defined(__linux__)
DividerSnapshot initializeHardwareFromSavedConfiguration()
{
    const SavedDividerConfiguration saved = loadSavedDividerConfiguration();
    if (!saved.hasAnyOutput)
    {
        throw Poco::NotFoundException("No valid saved divider configuration was found for Y1~Y6.");
    }

    DividerSnapshot snapshot = withCdce937(
        saved.devicePath,
        saved.referenceClockHz,
        [&](Cdce937Device& device, Poco::UInt64 referenceClockHz) {
            disablePllModeLocked(device);
            for (std::size_t index = 0; index < saved.dividers.size(); ++index)
            {
                if (!saved.savedOutputs[index]) continue;
                validateDividerOrThrow(static_cast<int>(index), saved.dividers[index]);
                writeOutputDividerLocked(device, static_cast<int>(index), saved.dividers[index]);
            }
            return readDividerStatus(device.path, referenceClockHz);
        });

    snapshot.message = saved.hasAllOutputs
        ? "CDCE937 PLL bypass enabled and saved divider state restored for Y1~Y6."
        : "CDCE937 PLL bypass enabled and saved divider state restored for available outputs.";
    return snapshot;
}

DividerSnapshot readDividerStatus(const std::string& devicePath, Poco::UInt64 referenceClockHz)
{
    validateReferenceClockHzOrThrow(referenceClockHz);

    return withCdce937(
        devicePath,
        referenceClockHz,
        [&](Cdce937Device& device, Poco::UInt64 clockHz) {
            DividerSnapshot snapshot;
            snapshot.devicePath = device.path;
            snapshot.referenceClockHz = clockHz;

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
                output.name = spec.name;
                output.pdivName = spec.pdivName;
                output.pin = spec.pin;
                output.divider = readDividerRegister(device, spec.index);
                output.frequencyHz =
                    output.divider > 0
                        ? (static_cast<double>(clockHz) / static_cast<double>(output.divider))
                        : 0.0;
                snapshot.outputs.push_back(output);
            }

            snapshot.message = "Divider status synchronized from hardware.";
            return snapshot;
        });
}

DividerSnapshot applyDividers(
    const std::string& devicePath,
    const std::vector<int>& outputIndices,
    int divider,
    Poco::UInt64 referenceClockHz)
{
    const std::vector<int> normalizedOutputs = normalizeOutputIndicesOrThrow(outputIndices);
    for (const int outputIndex: normalizedOutputs)
    {
        validateDividerOrThrow(outputIndex, divider);
    }
    validateReferenceClockHzOrThrow(referenceClockHz);

    DividerSnapshot snapshot = withCdce937(
        devicePath,
        referenceClockHz,
        [&](Cdce937Device& device, Poco::UInt64 clockHz) {
            disablePllModeLocked(device);
            for (const int outputIndex: normalizedOutputs)
            {
                writeOutputDividerLocked(device, outputIndex, divider);
            }
            return readDividerStatus(device.path, clockHz);
        });

    saveDividerConfiguration(snapshot, snapshot.devicePath);
    std::ostringstream description;
    for (std::size_t index = 0; index < normalizedOutputs.size(); ++index)
    {
        if (index > 0) description << ", ";
        description << kDividerOutputs[normalizedOutputs[index]].name;
    }
    snapshot.message = "Divider applied to " + description.str() + " and read back from hardware.";
    return snapshot;
}
#endif

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
