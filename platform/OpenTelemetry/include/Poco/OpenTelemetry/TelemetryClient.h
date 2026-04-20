#ifndef OpenTelemetry_TelemetryClient_INCLUDED
#define OpenTelemetry_TelemetryClient_INCLUDED

#include "Poco/OSP/BundleContext.h"
#include "Poco/OpenTelemetry/OpenTelemetry.h"
#include "Poco/OpenTelemetry/TelemetryModel.h"
#include "Poco/OpenTelemetry/TelemetryService.h"
#include <string>

namespace Poco {
namespace OpenTelemetry {

class OpenTelemetry_API TelemetryActivity
{
public:
    TelemetryActivity();
    TelemetryActivity(TelemetryService::Ptr pService, std::string activityId);
    TelemetryActivity(TelemetryActivity&& other) noexcept;
    TelemetryActivity& operator = (TelemetryActivity&& other) noexcept;
    ~TelemetryActivity();

    TelemetryActivity(const TelemetryActivity&) = delete;
    TelemetryActivity& operator = (const TelemetryActivity&) = delete;

    bool valid() const;
    const std::string& activityId() const;

    void input(const std::string& payload);
    void output(const std::string& payload);
    void tag(const std::string& key, const std::string& value);
    void step(
        const std::string& name,
        const std::string& detail = std::string(),
        const std::string& status = "ok",
        const TelemetryAttributes& attributes = TelemetryAttributes());

    void success(const std::string& output = std::string());
    void fail(const std::string& error, const std::string& output = std::string());

private:
    void finish(const std::string& status, const std::string& output, const std::string& error);

    TelemetryService::Ptr _pService;
    std::string _activityId;
    std::string _pendingOutput;
    bool _finished = false;
};

class OpenTelemetry_API TelemetryClient
{
public:
    TelemetryClient();
    explicit TelemetryClient(TelemetryService::Ptr pService);
    explicit TelemetryClient(Poco::OSP::BundleContext::Ptr pContext);

    bool available() const;

    TelemetryActivity beginActivity(
        const std::string& name,
        const std::string& category = std::string(),
        const std::string& input = std::string(),
        const TelemetryAttributes& attributes = TelemetryAttributes());

    void log(
        const std::string& level,
        const std::string& source,
        const std::string& message,
        const TelemetryAttributes& attributes = TelemetryAttributes());

    void metric(
        const std::string& name,
        double value,
        const std::string& unit = std::string(),
        const std::string& description = std::string(),
        const TelemetryAttributes& attributes = TelemetryAttributes());

    std::string currentActivityId() const;
    TelemetrySnapshot snapshot(const TelemetrySnapshotOptions& options = TelemetrySnapshotOptions()) const;

    static TelemetryService::Ptr find(Poco::OSP::BundleContext::Ptr pContext);

private:
    TelemetryService::Ptr _pService;
};

} } // namespace Poco::OpenTelemetry

#endif // OpenTelemetry_TelemetryClient_INCLUDED
