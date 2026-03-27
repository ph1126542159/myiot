#include "SystemMetricsRequestHandler.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/String.h"
#include "Poco/Timestamp.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <comdef.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <pdh.h>
#include <tlhelp32.h>
#include <wbemidl.h>
#include <windows.h>
#pragma comment(lib, "wbemuuid.lib")
#endif

namespace {

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

void sendJSON(Poco::Net::HTTPServerResponse& response, Poco::JSON::Object::Ptr payload, Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_OK)
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
        const std::string message = exc.displayText();
        if (message.find("broken pipe") == std::string::npos &&
            message.find("Broken pipe") == std::string::npos)
        {
            throw;
        }
    }
}

Poco::JSON::Object::Ptr createUnauthorizedPayload()
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", false);
    payload->set("message", "未登录，无法读取系统监控数据。");
    payload->set("metrics", Poco::JSON::Object::Ptr(new Poco::JSON::Object));
    return payload;
}

#if defined(_WIN32)

struct DiskMetric
{
    std::string name;
    std::uint64_t totalBytes = 0;
    std::uint64_t usedBytes = 0;
    std::uint64_t freeBytes = 0;
    double usagePercent = 0.0;
};

struct MetricsSample
{
    std::uint64_t sampleIntervalMs = 0;
    double cpuUsagePercent = 0.0;
    double cpuTemperatureCelsius = 0.0;
    bool cpuTemperatureAvailable = false;
    std::uint64_t memoryTotalBytes = 0;
    std::uint64_t memoryUsedBytes = 0;
    std::uint64_t memoryFreeBytes = 0;
    double memoryUsagePercent = 0.0;
    std::uint32_t processCount = 0;
    std::uint32_t threadCount = 0;
    double diskReadBytesPerSec = 0.0;
    double diskWriteBytesPerSec = 0.0;
    double networkReceiveBytesPerSec = 0.0;
    double networkSendBytesPerSec = 0.0;
    std::vector<DiskMetric> disks;
};

std::string utf8FromWide(const std::wstring& value)
{
    if (value.empty()) return std::string();

    const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &result[0], utf8Length, nullptr, nullptr);
    return result;
}

std::uint64_t fileTimeToUInt64(const FILETIME& fileTime)
{
    ULARGE_INTEGER value;
    value.LowPart = fileTime.dwLowDateTime;
    value.HighPart = fileTime.dwHighDateTime;
    return value.QuadPart;
}

std::uint32_t countProcesses()
{
    std::uint32_t total = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return total;

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(entry);
    if (Process32First(snapshot, &entry))
    {
        do
        {
            ++total;
        }
        while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return total;
}

std::uint32_t countThreads()
{
    std::uint32_t total = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return total;

    THREADENTRY32 entry;
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry))
    {
        do
        {
            ++total;
        }
        while (Thread32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return total;
}

std::vector<DiskMetric> collectDiskMetrics()
{
    std::vector<DiskMetric> disks;
    std::vector<wchar_t> driveBuffer(512, L'\0');
    DWORD written = GetLogicalDriveStringsW(static_cast<DWORD>(driveBuffer.size()), driveBuffer.data());
    if (written == 0 || written >= driveBuffer.size()) return disks;

    const wchar_t* current = driveBuffer.data();
    while (*current != L'\0')
    {
        const UINT driveType = GetDriveTypeW(current);
        if (driveType == DRIVE_FIXED || driveType == DRIVE_RAMDISK || driveType == DRIVE_REMOVABLE)
        {
            ULARGE_INTEGER freeBytesAvailable;
            ULARGE_INTEGER totalBytes;
            ULARGE_INTEGER totalFreeBytes;

            if (GetDiskFreeSpaceExW(current, &freeBytesAvailable, &totalBytes, &totalFreeBytes) && totalBytes.QuadPart > 0)
            {
                DiskMetric metric;
                metric.name = utf8FromWide(current);
                metric.totalBytes = totalBytes.QuadPart;
                metric.freeBytes = totalFreeBytes.QuadPart;
                metric.usedBytes = metric.totalBytes - metric.freeBytes;
                metric.usagePercent = metric.totalBytes == 0
                    ? 0.0
                    : (static_cast<double>(metric.usedBytes) * 100.0 / static_cast<double>(metric.totalBytes));
                disks.push_back(metric);
            }
        }

        current += wcslen(current) + 1;
    }

    std::sort(disks.begin(), disks.end(), [](const DiskMetric& left, const DiskMetric& right) {
        return Poco::icompare(left.name, right.name) < 0;
    });
    return disks;
}

bool tryCollectCpuTemperature(double& temperatureCelsius)
{
    HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = SUCCEEDED(initializeResult);
    if (FAILED(initializeResult) && initializeResult != RPC_E_CHANGED_MODE)
    {
        return false;
    }

    HRESULT securityResult = CoInitializeSecurity(
        nullptr,
        -1,
        nullptr,
        nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE,
        nullptr);
    if (FAILED(securityResult) && securityResult != RPC_E_TOO_LATE)
    {
        if (shouldUninitialize) CoUninitialize();
        return false;
    }

    IWbemLocator* pLocator = nullptr;
    IWbemServices* pServices = nullptr;
    IEnumWbemClassObject* pEnumerator = nullptr;
    bool hasTemperature = false;
    double bestTemperature = std::numeric_limits<double>::lowest();

    HRESULT result = CoCreateInstance(
        CLSID_WbemLocator,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        reinterpret_cast<void**>(&pLocator));

    if (SUCCEEDED(result))
    {
        result = pLocator->ConnectServer(
            _bstr_t(L"ROOT\\WMI"),
            nullptr,
            nullptr,
            nullptr,
            0,
            nullptr,
            nullptr,
            &pServices);
    }

    if (SUCCEEDED(result))
    {
        result = CoSetProxyBlanket(
            pServices,
            RPC_C_AUTHN_WINNT,
            RPC_C_AUTHZ_NONE,
            nullptr,
            RPC_C_AUTHN_LEVEL_CALL,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            nullptr,
            EOAC_NONE);
    }

    if (SUCCEEDED(result))
    {
        result = pServices->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &pEnumerator);
    }

    if (SUCCEEDED(result) && pEnumerator)
    {
        while (true)
        {
            IWbemClassObject* pObject = nullptr;
            ULONG returned = 0;
            result = pEnumerator->Next(200, 1, &pObject, &returned);
            if (FAILED(result) || returned == 0) break;

            VARIANT value;
            VariantInit(&value);
            if (SUCCEEDED(pObject->Get(L"CurrentTemperature", 0, &value, nullptr, nullptr)))
            {
                double rawTemperature = 0.0;
                switch (value.vt)
                {
                case VT_I4:
                    rawTemperature = static_cast<double>(value.lVal);
                    break;
                case VT_UI4:
                    rawTemperature = static_cast<double>(value.ulVal);
                    break;
                case VT_I8:
                    rawTemperature = static_cast<double>(value.llVal);
                    break;
                case VT_UI8:
                    rawTemperature = static_cast<double>(value.ullVal);
                    break;
                case VT_R8:
                    rawTemperature = value.dblVal;
                    break;
                default:
                    break;
                }

                // WMI reports deci-Kelvin.
                const double celsius = rawTemperature > 0.0 ? (rawTemperature / 10.0) - 273.15 : 0.0;
                if (celsius >= -20.0 && celsius <= 150.0)
                {
                    bestTemperature = std::max(bestTemperature, celsius);
                    hasTemperature = true;
                }
            }

            VariantClear(&value);
            pObject->Release();
        }
    }

    if (pEnumerator) pEnumerator->Release();
    if (pServices) pServices->Release();
    if (pLocator) pLocator->Release();
    if (shouldUninitialize) CoUninitialize();

    if (hasTemperature)
    {
        temperatureCelsius = bestTemperature;
    }
    return hasTemperature;
}

class MetricsSampler
{
public:
    MetricsSampler():
        _lastTickMs(GetTickCount64()),
        _query(nullptr),
        _diskReadCounter(nullptr),
        _diskWriteCounter(nullptr),
        _diskCountersReady(false),
        _hasCpuBaseline(false),
        _hasNetworkBaseline(false)
    {
        FILETIME idleTime;
        FILETIME kernelTime;
        FILETIME userTime;
        if (GetSystemTimes(&idleTime, &kernelTime, &userTime))
        {
            _lastIdleTime = fileTimeToUInt64(idleTime);
            _lastKernelTime = fileTimeToUInt64(kernelTime);
            _lastUserTime = fileTimeToUInt64(userTime);
            _hasCpuBaseline = true;
        }

        initializeDiskCounters();
    }

    ~MetricsSampler()
    {
        if (_query)
        {
            PdhCloseQuery(_query);
            _query = nullptr;
        }
    }

    MetricsSample sample()
    {
        std::lock_guard<std::mutex> lock(_mutex);

        MetricsSample sample;
        const ULONGLONG nowTickMs = GetTickCount64();
        sample.sampleIntervalMs = nowTickMs > _lastTickMs ? static_cast<std::uint64_t>(nowTickMs - _lastTickMs) : 1000;
        _lastTickMs = nowTickMs;

        FILETIME idleTime;
        FILETIME kernelTime;
        FILETIME userTime;
        if (GetSystemTimes(&idleTime, &kernelTime, &userTime))
        {
            const std::uint64_t idle = fileTimeToUInt64(idleTime);
            const std::uint64_t kernel = fileTimeToUInt64(kernelTime);
            const std::uint64_t user = fileTimeToUInt64(userTime);

            if (_hasCpuBaseline)
            {
                const std::uint64_t idleDelta = idle - _lastIdleTime;
                const std::uint64_t kernelDelta = kernel - _lastKernelTime;
                const std::uint64_t userDelta = user - _lastUserTime;
                const std::uint64_t totalDelta = kernelDelta + userDelta;

                if (totalDelta > 0)
                {
                    sample.cpuUsagePercent = std::clamp(100.0 - (static_cast<double>(idleDelta) * 100.0 / static_cast<double>(totalDelta)), 0.0, 100.0);
                }
            }

            _lastIdleTime = idle;
            _lastKernelTime = kernel;
            _lastUserTime = user;
            _hasCpuBaseline = true;
        }

        sample.cpuTemperatureAvailable = tryCollectCpuTemperature(sample.cpuTemperatureCelsius);

        MEMORYSTATUSEX memoryStatus;
        memoryStatus.dwLength = sizeof(memoryStatus);
        if (GlobalMemoryStatusEx(&memoryStatus))
        {
            sample.memoryTotalBytes = memoryStatus.ullTotalPhys;
            sample.memoryFreeBytes = memoryStatus.ullAvailPhys;
            sample.memoryUsedBytes = sample.memoryTotalBytes - sample.memoryFreeBytes;
            sample.memoryUsagePercent = memoryStatus.dwMemoryLoad;
        }

        sample.processCount = countProcesses();
        sample.threadCount = countThreads();
        sample.disks = collectDiskMetrics();

        refreshDiskIo(sample);
        refreshNetwork(sample);

        return sample;
    }

private:
    void initializeDiskCounters()
    {
        if (PdhOpenQueryW(nullptr, 0, &_query) != ERROR_SUCCESS) return;

        if (PdhAddEnglishCounterW(_query, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &_diskReadCounter) != ERROR_SUCCESS) return;
        if (PdhAddEnglishCounterW(_query, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &_diskWriteCounter) != ERROR_SUCCESS) return;
        if (PdhCollectQueryData(_query) != ERROR_SUCCESS) return;

        _diskCountersReady = true;
    }

    void refreshDiskIo(MetricsSample& sample)
    {
        if (!_diskCountersReady) return;
        if (PdhCollectQueryData(_query) != ERROR_SUCCESS) return;

        sample.diskReadBytesPerSec = readCounterValue(_diskReadCounter);
        sample.diskWriteBytesPerSec = readCounterValue(_diskWriteCounter);
    }

    double readCounterValue(PDH_HCOUNTER counter) const
    {
        PDH_FMT_COUNTERVALUE value;
        DWORD counterType = 0;
        if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, &counterType, &value) != ERROR_SUCCESS)
        {
            return 0.0;
        }

        if (value.CStatus != ERROR_SUCCESS) return 0.0;
        return std::max(0.0, value.doubleValue);
    }

    void refreshNetwork(MetricsSample& sample)
    {
        MIB_IF_TABLE2* table = nullptr;
        if (GetIfTable2(&table) != NO_ERROR || !table) return;

        std::uint64_t totalIn = 0;
        std::uint64_t totalOut = 0;

        for (ULONG i = 0; i < table->NumEntries; ++i)
        {
            const MIB_IF_ROW2& row = table->Table[i];
            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (row.OperStatus != IfOperStatusUp) continue;

            totalIn += row.InOctets;
            totalOut += row.OutOctets;
        }

        FreeMibTable(table);

        if (_hasNetworkBaseline && sample.sampleIntervalMs > 0)
        {
            const double seconds = static_cast<double>(sample.sampleIntervalMs) / 1000.0;
            sample.networkReceiveBytesPerSec = totalIn >= _lastNetworkIn
                ? static_cast<double>(totalIn - _lastNetworkIn) / seconds
                : 0.0;
            sample.networkSendBytesPerSec = totalOut >= _lastNetworkOut
                ? static_cast<double>(totalOut - _lastNetworkOut) / seconds
                : 0.0;
        }

        _lastNetworkIn = totalIn;
        _lastNetworkOut = totalOut;
        _hasNetworkBaseline = true;
    }

    std::mutex _mutex;
    std::uint64_t _lastIdleTime = 0;
    std::uint64_t _lastKernelTime = 0;
    std::uint64_t _lastUserTime = 0;
    std::uint64_t _lastNetworkIn = 0;
    std::uint64_t _lastNetworkOut = 0;
    ULONGLONG _lastTickMs;
    PDH_HQUERY _query;
    PDH_HCOUNTER _diskReadCounter;
    PDH_HCOUNTER _diskWriteCounter;
    bool _diskCountersReady;
    bool _hasCpuBaseline;
    bool _hasNetworkBaseline;
};

MetricsSampler& metricsSampler()
{
    static MetricsSampler sampler;
    return sampler;
}

Poco::JSON::Object::Ptr createSamplePayload(const MetricsSample& sample)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", true);
    payload->set("message", "系统监控指标同步正常。");
    payload->set("updatedAt", Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::ISO8601_FORMAT));
    payload->set("sampleIntervalMs", static_cast<Poco::UInt64>(sample.sampleIntervalMs));

    Poco::JSON::Object::Ptr cpu = new Poco::JSON::Object;
    cpu->set("usagePercent", sample.cpuUsagePercent);
    cpu->set("temperatureAvailable", sample.cpuTemperatureAvailable);
    if (sample.cpuTemperatureAvailable)
    {
        cpu->set("temperatureCelsius", sample.cpuTemperatureCelsius);
    }
    payload->set("cpu", cpu);

    Poco::JSON::Object::Ptr memory = new Poco::JSON::Object;
    memory->set("totalBytes", static_cast<Poco::UInt64>(sample.memoryTotalBytes));
    memory->set("usedBytes", static_cast<Poco::UInt64>(sample.memoryUsedBytes));
    memory->set("freeBytes", static_cast<Poco::UInt64>(sample.memoryFreeBytes));
    memory->set("usagePercent", sample.memoryUsagePercent);
    payload->set("memory", memory);

    Poco::JSON::Object::Ptr counts = new Poco::JSON::Object;
    counts->set("processes", sample.processCount);
    counts->set("threads", sample.threadCount);
    payload->set("counts", counts);

    Poco::JSON::Object::Ptr io = new Poco::JSON::Object;
    io->set("readBytesPerSec", sample.diskReadBytesPerSec);
    io->set("writeBytesPerSec", sample.diskWriteBytesPerSec);
    io->set("totalBytesPerSec", sample.diskReadBytesPerSec + sample.diskWriteBytesPerSec);
    payload->set("io", io);

    Poco::JSON::Object::Ptr network = new Poco::JSON::Object;
    network->set("receiveBytesPerSec", sample.networkReceiveBytesPerSec);
    network->set("sendBytesPerSec", sample.networkSendBytesPerSec);
    network->set("totalBytesPerSec", sample.networkReceiveBytesPerSec + sample.networkSendBytesPerSec);
    payload->set("network", network);

    Poco::JSON::Array::Ptr disks = new Poco::JSON::Array;
    for (const auto& disk: sample.disks)
    {
        Poco::JSON::Object::Ptr item = new Poco::JSON::Object;
        item->set("name", disk.name);
        item->set("totalBytes", static_cast<Poco::UInt64>(disk.totalBytes));
        item->set("usedBytes", static_cast<Poco::UInt64>(disk.usedBytes));
        item->set("freeBytes", static_cast<Poco::UInt64>(disk.freeBytes));
        item->set("usagePercent", disk.usagePercent);
        disks->add(item);
    }
    payload->set("disks", disks);

    return payload;
}

#else

Poco::JSON::Object::Ptr createUnsupportedPayload()
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", true);
    payload->set("message", "当前平台暂未实现系统监控数据采样。");
    payload->set("updatedAt", Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::ISO8601_FORMAT));
    payload->set("sampleIntervalMs", 0);
    Poco::JSON::Object::Ptr cpu = new Poco::JSON::Object;
    cpu->set("temperatureAvailable", false);
    payload->set("cpu", cpu);
    payload->set("memory", Poco::JSON::Object::Ptr(new Poco::JSON::Object));
    payload->set("counts", Poco::JSON::Object::Ptr(new Poco::JSON::Object));
    payload->set("io", Poco::JSON::Object::Ptr(new Poco::JSON::Object));
    payload->set("network", Poco::JSON::Object::Ptr(new Poco::JSON::Object));
    payload->set("disks", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    return payload;
}

#endif

} // namespace

namespace MyIoT {
namespace WebUI {
namespace SystemMonitor {

SystemMetricsRequestHandler::SystemMetricsRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
    _pContext(pContext)
{
}

void SystemMetricsRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    if (!isAuthenticated(_pContext, request))
    {
        sendJSON(response, createUnauthorizedPayload(), Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        return;
    }

#if defined(_WIN32)
    sendJSON(response, createSamplePayload(metricsSampler().sample()));
#else
    sendJSON(response, createUnsupportedPayload());
#endif
}

Poco::Net::HTTPRequestHandler* SystemMetricsRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new SystemMetricsRequestHandler(context());
}

} } } // namespace MyIoT::WebUI::SystemMonitor
