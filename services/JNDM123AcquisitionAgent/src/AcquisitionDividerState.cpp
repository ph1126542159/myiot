#include "AcquisitionDivider.h"

#include "AcquisitionAgentSupport.h"

#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/Format.h"
#include "Poco/JSON/Array.h"
#include "Poco/Path.h"
#include "Poco/String.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

using Poco::JSON::Array;
using Poco::JSON::Object;

const std::array<DividerOutputSpec, kDividerOutputCount> kDividerOutputs = {{
    {0, "Y1", "Pdiv1", 17},
    {1, "Y2", "Pdiv1", 15},
    {2, "Y3", "Pdiv1", 14},
    {3, "Y4", "Pdiv4", 7},
    {4, "Y5", "Pdiv4", 8},
    {5, "Y6", "Pdiv4", 12},
    {6, "Y7", "Pdiv7", 11},
}};

namespace {

std::string dividerStateFilePath()
{
#if defined(__linux__)
    const std::array<const char*, 2> persistentMounts = {{
        "/run/media/mmcblk0p2",
        "/run/media/mmcblk1p2",
    }};

    std::ifstream mounts("/proc/mounts");
    if (mounts)
    {
        std::string line;
        while (std::getline(mounts, line))
        {
            std::istringstream parser(line);
            std::string device;
            std::string mountPoint;
            if (!(parser >> device >> mountPoint))
            {
                continue;
            }

            for (const char* candidate: persistentMounts)
            {
                if (mountPoint == candidate)
                {
                    return mountPoint + std::string(kDividerStateFile);
                }
            }
        }
    }
#endif

    return kDividerStateFile;
}

bool parseIntStrict(const std::string& text, int& value)
{
    if (text.empty()) return false;

    try
    {
        std::size_t pos = 0;
        const int parsed = std::stoi(text, &pos, 10);
        if (pos != text.size()) return false;
        value = parsed;
        return true;
    }
    catch (...)
    {
        return false;
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

} // namespace

SavedDividerConfiguration loadSavedDividerConfiguration()
{
    SavedDividerConfiguration configuration;

    std::ifstream input(dividerStateFilePath());
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
            }
            catch (...)
            {
                continue;
            }
            configuration.referenceClockHz = referenceClockHz;
            continue;
        }

        if (key.size() == 2 && key[0] == 'y' && key[1] >= '1' && key[1] <= '7')
        {
            int divider = 0;
            if (!parseIntStrict(value, divider)) continue;

            const int outputIndex = key[1] - '1';
            try
            {
                validateDividerOrThrow(outputIndex, divider);
            }
            catch (...)
            {
                continue;
            }

            configuration.dividers[outputIndex] = divider;
            configuration.savedOutputs[outputIndex] = true;
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
    if (!requestedDevicePath.empty()) return requestedDevicePath;

    const SavedDividerConfiguration saved = loadSavedDividerConfiguration();
    if (!saved.devicePath.empty()) return saved.devicePath;
    return kDefaultI2CDevice;
}

Poco::UInt64 resolvePreferredReferenceClockHz(const std::string& requestedReferenceClockHz)
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
    const std::string stateFile = dividerStateFilePath();
    Poco::Path configPath(stateFile);
    Poco::File(configPath.parent()).createDirectories();

    std::ofstream output(stateFile, std::ios::trunc);
    if (!output)
    {
        throw Poco::IOException("Unable to write divider state file " + stateFile);
    }

    output << "# Auto-generated by MyIoT JNDM123 divider control\n";
    output << "devicePath=" << (devicePath.empty() ? kDefaultI2CDevice : devicePath) << "\n";
    output << "referenceClockHz=" << snapshot.referenceClockHz << "\n";
    for (std::size_t index = 0; index < kDividerPersistedOutputCount; ++index)
    {
        if (index >= snapshot.outputs.size())
        {
            throw Poco::InvalidAccessException("Divider snapshot is missing outputs required for persistence.");
        }
        output << "y" << (index + 1) << "=" << snapshot.outputs[index].divider << "\n";
    }

    if (!output.good())
    {
        throw Poco::IOException("Unable to flush divider state file " + stateFile);
    }
}

int dividerLimitForOutput(int outputIndex)
{
    return outputIndex >= 0 && outputIndex <= 2 ? 1023 : 127;
}

std::vector<int> normalizeOutputIndicesOrThrow(const std::vector<int>& outputIndices)
{
    if (outputIndices.empty())
    {
        throw Poco::InvalidArgumentException("At least one output must be selected.");
    }

    std::vector<int> normalized = outputIndices;
    std::sort(normalized.begin(), normalized.end());
    normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());

    for (const int outputIndex: normalized)
    {
        if (outputIndex < 0 || outputIndex >= static_cast<int>(kDividerOutputCount))
        {
            throw Poco::InvalidArgumentException("Output index must be in 0..6.");
        }
    }

    return normalized;
}

void validateDividerOrThrow(int outputIndex, int divider)
{
    if (outputIndex < 0 || outputIndex >= static_cast<int>(kDividerOutputCount))
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

double maximumStartupOutputFrequencyHz(const DividerSnapshot& snapshot)
{
    double maximum = 0.0;
    const std::size_t count = std::min(snapshot.outputs.size(), kDividerAcquisitionOutputCount);
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

Object::Ptr dividerSnapshotToJson(const DividerSnapshot& snapshot)
{
    Object::Ptr payload = new Object;
    payload->set("ok", snapshot.ok);
    payload->set("message", snapshot.message);
    payload->set("updatedAt", isoTimestamp());
    payload->set("devicePath", snapshot.devicePath);
    payload->set("referenceClockHz", snapshot.referenceClockHz);
    payload->set("deviceType", snapshot.deviceType);
    payload->set("address", snapshot.address);
    payload->set("inputClock", snapshot.inputClock);
    payload->set("revisionId", snapshot.revisionId);
    payload->set("eepBusy", snapshot.eepBusy);
    payload->set("eepLock", snapshot.eepLock);
    payload->set("powerDown", snapshot.powerDown);

    Array::Ptr outputs = new Array;
    for (const auto& output: snapshot.outputs)
    {
        Object::Ptr item = new Object;
        item->set("index", output.index);
        item->set("name", output.name);
        item->set("pdiv", output.pdivName);
        item->set("pin", output.pin);
        item->set("divider", output.divider);
        item->set("frequencyHz", output.frequencyHz);
        outputs->add(item);
    }
    payload->set("outputs", outputs);
    return payload;
}

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
