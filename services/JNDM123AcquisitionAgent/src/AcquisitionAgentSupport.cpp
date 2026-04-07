#include "AcquisitionAgentSupport.h"

#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Environment.h"
#include "Poco/NumberParser.h"

#include <string>

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace MyIoT {
namespace Services {
namespace JNDM123AcquisitionAgent {

Poco::Logger& logger()
{
    static Poco::Logger& instance = Poco::Logger::get("MyIoT.JNDM123.AcquisitionAgent");
    return instance;
}

std::string isoTimestamp(const Poco::Timestamp& timestamp)
{
    return Poco::DateTimeFormatter::format(timestamp, Poco::DateTimeFormat::ISO8601_FORMAT);
}

int parseIntEnv(const char* name, int fallback)
{
    try
    {
        if (!Poco::Environment::has(name)) return fallback;
        return Poco::NumberParser::parse(Poco::Environment::get(name));
    }
    catch (...)
    {
        return fallback;
    }
}

#if defined(__linux__)
void writeSysfsValue(const std::string& path, const std::string& value)
{
    const int fd = ::open(path.c_str(), O_WRONLY);
    if (fd < 0) return;
    ::write(fd, value.c_str(), value.size());
    ::close(fd);
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
#endif

} } } // namespace MyIoT::Services::JNDM123AcquisitionAgent
