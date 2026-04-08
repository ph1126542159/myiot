#include "JNDM123Support.h"

#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/Format.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Logger.h"
#include "Poco/Net/NetException.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/Path.h"
#include "Poco/String.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

using Poco::JSON::Array;

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

Poco::UInt64& configuredReferenceClockHzStorage()
{
    static Poco::UInt64 value = kFallbackReferenceClockHz;
    return value;
}

}

Poco::UInt64 configuredReferenceClockHz()
{
    return configuredReferenceClockHzStorage();
}

void setConfiguredReferenceClockHz(Poco::UInt64 referenceClockHz)
{
    configuredReferenceClockHzStorage() = referenceClockHz;
}

Poco::OSP::BundleContext::Ptr& runtimeBundleContextStorage()
{
    static Poco::OSP::BundleContext::Ptr context;
    return context;
}

Poco::Logger& logger()
{
    static Poco::Logger& instance = Poco::Logger::get("MyIoT.WebUI.JNDM123");
    return instance;
}

std::string isoTimestamp(const Poco::Timestamp& timestamp)
{
    return Poco::DateTimeFormatter::format(timestamp, Poco::DateTimeFormat::ISO8601_FORMAT);
}

Poco::OSP::Web::WebSession::Ptr findSession(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request)
{
    Poco::OSP::Web::WebSessionManager::Ptr pSessionManager =
        Poco::OSP::ServiceFinder::find<Poco::OSP::Web::WebSessionManager>(pContext);
    return pSessionManager->find(pContext->thisBundle()->properties().getString("websession.id", "myiot.webui"), request);
}

bool isAuthenticated(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request)
{
    try
    {
        Poco::OSP::Web::WebSession::Ptr pSession = findSession(pContext, request);
        return pSession && !pSession->getValue<std::string>("username", "").empty();
    }
    catch (...)
    {
        return false;
    }
}

void sendJSON(Poco::Net::HTTPServerResponse& response, Object::Ptr payload, Poco::Net::HTTPResponse::HTTPStatus status)
{
    response.setStatus(status);
    response.setChunkedTransferEncoding(true);
    response.setContentType("application/json");
    response.set("Cache-Control", "no-cache");

    try
    {
        std::ostream& out = response.send();
        Poco::JSON::Stringifier::stringify(payload, out);
    }
    catch (const Poco::Net::ConnectionResetException&)
    {
    }
    catch (const Poco::Net::ConnectionAbortedException&)
    {
    }
    catch (const Poco::IOException& exc)
    {
        const std::string text = exc.displayText();
        if (text.find("broken pipe") == std::string::npos && text.find("Broken pipe") == std::string::npos)
        {
            throw;
        }
    }
}

Object::Ptr createUnauthorizedPayload(const std::string& message)
{
    Object::Ptr payload = new Object;
    payload->set("authenticated", false);
    payload->set("ok", false);
    payload->set("message", message);
    payload->set("updatedAt", isoTimestamp());
    return payload;
}

Object::Ptr createErrorPayload(const std::string& message)
{
    Object::Ptr payload = new Object;
    payload->set("authenticated", true);
    payload->set("ok", false);
    payload->set("message", message);
    payload->set("updatedAt", isoTimestamp());
    return payload;
}

void stripWaveformSamples(Object::Ptr payload)
{
    if (!payload) return;

    payload->set("includeWaveform", false);
    payload->set("timelineUs", Array::Ptr(new Array));

    Array::Ptr chips = payload->getArray("chips");
    if (!chips) return;

    for (std::size_t chipIndex = 0; chipIndex < chips->size(); ++chipIndex)
    {
        Object::Ptr chip = chips->getObject(chipIndex);
        if (!chip) continue;

        Array::Ptr channels = chip->getArray("channels");
        if (!channels) continue;

        for (std::size_t channelIndex = 0; channelIndex < channels->size(); ++channelIndex)
        {
            Object::Ptr channel = channels->getObject(channelIndex);
            if (!channel) continue;
            channel->set("samples", Array::Ptr(new Array));
        }
    }
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

bool parseBoolValue(const std::string& text, bool defaultValue)
{
    if (text.empty()) return defaultValue;
    if (Poco::icompare(text, std::string("1")) == 0) return true;
    if (Poco::icompare(text, std::string("true")) == 0) return true;
    if (Poco::icompare(text, std::string("yes")) == 0) return true;
    if (Poco::icompare(text, std::string("on")) == 0) return true;
    if (Poco::icompare(text, std::string("0")) == 0) return false;
    if (Poco::icompare(text, std::string("false")) == 0) return false;
    if (Poco::icompare(text, std::string("no")) == 0) return false;
    if (Poco::icompare(text, std::string("off")) == 0) return false;
    return defaultValue;
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

std::vector<int> parseOutputIndexListOrThrow(const std::string& text)
{
    std::vector<int> outputIndices;
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        Poco::trimInPlace(token);
        if (token.empty()) continue;

        int outputIndex = 0;
        if (!parseIntStrict(token, outputIndex))
        {
            throw Poco::InvalidArgumentException("outputIndices must contain numeric output indexes.");
        }
        outputIndices.push_back(outputIndex);
    }

    return normalizeOutputIndicesOrThrow(outputIndices);
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

std::string describeOutputs(const std::vector<int>& outputIndices)
{
    const std::vector<int> normalized = normalizeOutputIndicesOrThrow(outputIndices);

    std::ostringstream description;
    for (std::size_t index = 0; index < normalized.size(); ++index)
    {
        if (index > 0) description << ", ";
        description << kDividerOutputs[normalized[index]].name;
    }
    return description.str();
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

SavedDividerConfiguration loadSavedDividerConfiguration()
{
    SavedDividerConfiguration configuration;
    configuration.referenceClockHz = configuredReferenceClockHz();

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
            }
            catch (...)
            {
                continue;
            }
            configuration.referenceClockHz = referenceClockHz;
            continue;
        }

        if (key.size() == 2 && key[0] == 'y' && key[1] >= '1' && key[1] <= '6')
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
    Poco::Path configPath(kDividerStateFile);
    Poco::File(configPath.parent()).createDirectories();

    std::ofstream output(kDividerStateFile, std::ios::trunc);
    if (!output)
    {
        throw Poco::IOException("Unable to write divider state file " + std::string(kDividerStateFile));
    }

    output << "# Auto-generated by MyIoT WebUI JNDM123\n";
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

AcquisitionActionResult acquisitionResultFromPayload(Object::Ptr payload, const std::string& defaultMessage)
{
    AcquisitionActionResult result;
    result.ok = !payload || payload->optValue("ok", true);
    result.message = payload ? payload->optValue("message", defaultMessage) : defaultMessage;
    return result;
}

Object::Ptr cloneFromText(const std::string& text)
{
    if (text.empty()) return nullptr;

    Poco::JSON::Parser parser;
    Poco::Dynamic::Var parsed = parser.parse(text);
    return parsed.extract<Object::Ptr>();
}

Object::Ptr createWaitingAcquisitionPayload(const std::string& message)
{
    Object::Ptr payload = new Object;
    payload->set("authenticated", true);
    payload->set("ok", true);
    payload->set("updatedAt", isoTimestamp());
    payload->set("message", message);
    payload->set("running", false);
    payload->set("previewActive", false);
    payload->set("historyLimit", static_cast<int>(kHistoryLimit));
    payload->set("queueDepth", 0);
    payload->set("totalFrames", static_cast<Poco::UInt64>(0));
    payload->set("droppedFrames", static_cast<Poco::UInt64>(0));
    payload->set("recoveries", static_cast<Poco::UInt64>(0));
    payload->set("lastFrameSequence", static_cast<Poco::UInt64>(0));
    payload->set("lastFrameAt", std::string());
    payload->set("waveformUpdatedAt", std::string());
    payload->set("lastError", std::string());
    payload->set("includeWaveform", true);
    payload->set("commandSequence", static_cast<Poco::UInt64>(0));
    payload->set("commandAction", std::string("status"));
    payload->set("commandUpdatedAt", std::string());
    payload->set("timelineUs", Array::Ptr(new Array));

    Array::Ptr chips = new Array;
    for (std::size_t chipIndex = 0; chipIndex < kAd7606Count; ++chipIndex)
    {
        Object::Ptr chip = new Object;
        chip->set("index", static_cast<int>(chipIndex));
        chip->set("name", "ADC" + std::to_string(chipIndex + 1));

        Array::Ptr channels = new Array;
        for (std::size_t channelIndex = 0; channelIndex < kChannelsPerChip; ++channelIndex)
        {
            Object::Ptr channel = new Object;
            channel->set("index", static_cast<int>(channelIndex));
            channel->set("name", "CH" + std::to_string(channelIndex + 1));
            channel->set("hasValue", false);
            channel->set("value", 0);
            channel->set("samples", Array::Ptr(new Array));
            channels->add(channel);
        }

        chip->set("channels", channels);
        chips->add(chip);
    }

    payload->set("chips", chips);
    return payload;
}

Object::Ptr dividerSnapshotToJson(const DividerSnapshot& snapshot)
{
    Object::Ptr payload = new Object;
    payload->set("authenticated", true);
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

DividerSnapshot dividerSnapshotFromPayload(Object::Ptr payload)
{
    if (!payload)
    {
        throw Poco::InvalidArgumentException("Missing DDS response payload for divider operation.");
    }

    Object::Ptr divider = payload->getObject("divider");
    if (!divider) divider = payload;
    if (!divider)
    {
        throw Poco::InvalidArgumentException("Missing divider snapshot in DDS response.");
    }

    DividerSnapshot snapshot;
    snapshot.ok = divider->optValue("ok", true);
    snapshot.message = divider->optValue("message", std::string("Divider status synchronized."));
    snapshot.devicePath = divider->optValue("devicePath", std::string(kDefaultI2CDevice));
    snapshot.referenceClockHz = divider->optValue<Poco::UInt64>("referenceClockHz", kFallbackReferenceClockHz);
    snapshot.deviceType = divider->optValue("deviceType", std::string("unsupported"));
    snapshot.address = divider->optValue("address", std::string("--"));
    snapshot.inputClock = divider->optValue("inputClock", std::string("--"));
    snapshot.revisionId = divider->optValue("revisionId", 0);
    snapshot.eepBusy = divider->optValue("eepBusy", false);
    snapshot.eepLock = divider->optValue("eepLock", false);
    snapshot.powerDown = divider->optValue("powerDown", false);

    Array::Ptr outputs = divider->getArray("outputs");
    if (!outputs)
    {
        std::string message = divider->optValue("message", payload->optValue("message", std::string()));
        if (message.empty())
        {
            message = divider->optValue("lastError", payload->optValue("lastError", std::string()));
        }
        if (message.empty())
        {
            message = "Divider snapshot is missing outputs.";
        }
        throw Poco::InvalidArgumentException(message);
    }

    snapshot.outputs.reserve(outputs->size());
    for (std::size_t index = 0; index < outputs->size(); ++index)
    {
        Object::Ptr item = outputs->getObject(index);
        if (!item) continue;

        DividerOutputState output;
        output.index = item->optValue("index", 0);
        output.name = item->optValue("name", std::string());
        output.pdivName = item->optValue("pdiv", std::string());
        output.pin = item->optValue("pin", 0);
        output.divider = item->optValue("divider", 0);
        output.frequencyHz = item->optValue("frequencyHz", 0.0);
        snapshot.outputs.push_back(output);
    }

    return snapshot;
}

} } } // namespace MyIoT::WebUI::JNDM123
