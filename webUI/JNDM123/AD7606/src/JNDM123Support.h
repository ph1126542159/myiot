#ifndef MyIoT_WebUI_JNDM123_JNDM123Support_INCLUDED
#define MyIoT_WebUI_JNDM123_JNDM123Support_INCLUDED

#include "AcquisitionDdsJsonMessage.h"
#include "JNDM123AcquisitionService.h"

#include "Poco/JSON/Object.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/Timestamp.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MyIoT {
namespace WebUI {
namespace JNDM123 {

using Poco::JSON::Object;

constexpr const char* kDefaultI2CDevice = "/dev/i2c-0";
constexpr const char* kDividerStateFile = "/var/lib/myiot/jndm123-divider-state.properties";
constexpr std::size_t kDividerOutputCount = 7;
constexpr std::size_t kDividerStartupOutputCount = 6;
constexpr std::size_t kAd7606Count = 6;
constexpr std::size_t kChannelsPerChip = 8;
constexpr std::size_t kHistoryLimit = 240;
constexpr Poco::UInt64 kFallbackReferenceClockHz = 1000000ULL;
constexpr Poco::UInt64 kMaxSafeAcquisitionClockHz = 50000ULL;
constexpr int kAcquisitionDdsDomain = 37;
constexpr int kAcquisitionCommandTimeoutMs = 5000;
constexpr Poco::Int64 kPreviewLeaseUs = 2 * 1000 * 1000;
constexpr Poco::Int64 kPreviewPublishIntervalUs = 1 * 1000 * 1000;

#if defined(__linux__)
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

struct AcquisitionActionResult
{
    bool ok = true;
    std::string message;
};

Poco::UInt64 configuredReferenceClockHz();
void setConfiguredReferenceClockHz(Poco::UInt64 referenceClockHz);
Poco::OSP::BundleContext::Ptr& runtimeBundleContextStorage();

Poco::Logger& logger();
std::string isoTimestamp(const Poco::Timestamp& timestamp = Poco::Timestamp());
Poco::OSP::Web::WebSession::Ptr findSession(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request);
bool isAuthenticated(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request);
void sendJSON(Poco::Net::HTTPServerResponse& response, Object::Ptr payload, Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_OK);
Object::Ptr createUnauthorizedPayload(const std::string& message);
Object::Ptr createErrorPayload(const std::string& message);
MyIoT::Services::JNDM123AcquisitionAgent::JNDM123AcquisitionService::Ptr acquisitionServiceOrThrow();
void stripWaveformSamples(Object::Ptr payload);

bool parseIntStrict(const std::string& text, int& value);
bool parseUInt64Strict(const std::string& text, Poco::UInt64& value);
bool parseBoolValue(const std::string& text, bool defaultValue = false);
int dividerLimitForOutput(int outputIndex);
std::vector<int> normalizeOutputIndicesOrThrow(const std::vector<int>& outputIndices);
std::vector<int> parseOutputIndexListOrThrow(const std::string& text);
void validateDividerOrThrow(int outputIndex, int divider);
void validateReferenceClockHzOrThrow(Poco::UInt64 referenceClockHz);
std::string describeOutputs(const std::vector<int>& outputIndices);
double maximumStartupOutputFrequencyHz(const DividerSnapshot& snapshot);
std::string acquisitionClockSafetyMessage(const DividerSnapshot& snapshot);
SavedDividerConfiguration loadSavedDividerConfiguration();
std::string resolvePreferredDevicePath(const std::string& requestedDevicePath);
Poco::UInt64 resolvePreferredReferenceClockHz(const std::string& requestedReferenceClockHz);
void saveDividerConfiguration(const DividerSnapshot& snapshot, const std::string& devicePath);
AcquisitionActionResult acquisitionResultFromPayload(Object::Ptr payload, const std::string& defaultMessage);
Object::Ptr cloneFromText(const std::string& text);
Object::Ptr createWaitingAcquisitionPayload(const std::string& message);
Object::Ptr dividerSnapshotToJson(const DividerSnapshot& snapshot);
DividerSnapshot dividerSnapshotFromPayload(Object::Ptr payload);

} } } // namespace MyIoT::WebUI::JNDM123

#endif // MyIoT_WebUI_JNDM123_JNDM123Support_INCLUDED
