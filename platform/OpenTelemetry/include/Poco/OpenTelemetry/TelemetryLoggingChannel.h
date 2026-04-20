#ifndef OpenTelemetry_TelemetryLoggingChannel_INCLUDED
#define OpenTelemetry_TelemetryLoggingChannel_INCLUDED

#include "Poco/Channel.h"
#include "Poco/OpenTelemetry/OpenTelemetry.h"
#include "Poco/OpenTelemetry/TelemetryService.h"

namespace Poco {
namespace OpenTelemetry {

class OpenTelemetry_API TelemetryLoggingChannel: public Poco::Channel
{
public:
    using Ptr = Poco::AutoPtr<TelemetryLoggingChannel>;

    explicit TelemetryLoggingChannel(TelemetryService::Ptr pService);
    void log(const Poco::Message& message) override;

private:
    TelemetryService::Ptr _pService;
};

} } // namespace Poco::OpenTelemetry

#endif // OpenTelemetry_TelemetryLoggingChannel_INCLUDED
