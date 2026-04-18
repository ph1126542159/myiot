#include "LogStreamRequestHandler.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/File.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/NumberParser.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/Path.h"
#include "Poco/String.h"
#include "Poco/Thread.h"
#include "Poco/Timestamp.h"
#include "Poco/URI.h"
#include "Poco/Util/Application.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include <algorithm>
#include <atomic>
#include <deque>
#include <exception>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

std::atomic<bool> gLogStreamShuttingDown(false);
std::atomic<int> gActiveLogRequests(0);

struct LogSnapshot
{
    Poco::JSON::Array::Ptr processes = new Poco::JSON::Array;
    std::string message;
    std::string updatedAt;
    std::string signature;
};

struct StreamAuditInfo
{
    std::string username = "-";
    std::string client = "-";
    std::string endpoint;
    std::string locale = "zh";
    int lineLimit = 0;
};

class ScopedLogRequestActivity
{
public:
    ScopedLogRequestActivity():
        _active(MyIoT::WebUI::Home::LogStreamRequestLifecycle::tryAcquire())
    {
    }

    ~ScopedLogRequestActivity()
    {
        if (_active)
        {
            MyIoT::WebUI::Home::LogStreamRequestLifecycle::release();
        }
    }

    bool active() const
    {
        return _active;
    }

private:
    bool _active;
};

std::string normalizeLocale(std::string value)
{
    Poco::toLowerInPlace(value);
    return Poco::startsWith(value, std::string("en")) ? "en" : "zh";
}

std::string queryParameter(Poco::Net::HTTPServerRequest& request, const std::string& name)
{
    try
    {
        Poco::URI uri(request.getURI());
        for (const auto& parameter: uri.getQueryParameters())
        {
            if (parameter.first == name) return parameter.second;
        }
    }
    catch (...)
    {
    }

    return "";
}

std::string requestLocale(Poco::Net::HTTPServerRequest& request)
{
    const std::string queryLocale = queryParameter(request, "locale");
    if (!queryLocale.empty()) return normalizeLocale(queryLocale);

    const std::string explicitLocale = request.get("X-MyIoT-Locale", "");
    if (!explicitLocale.empty()) return normalizeLocale(explicitLocale);

    const std::string acceptLanguage = request.get("Accept-Language", "");
    if (!acceptLanguage.empty()) return normalizeLocale(acceptLanguage);

    return "zh";
}

std::string detectExecutablePath()
{
#if defined(_WIN32)
    char buffer[4096] = {0};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length > 0 && length < sizeof(buffer))
    {
        return std::string(buffer, buffer + length);
    }
#endif

    return "";
}

std::string detectApplicationDir()
{
    const std::string applicationPathValue = detectExecutablePath();
    if (!applicationPathValue.empty())
    {
        Poco::Path applicationPath(applicationPathValue);
        applicationPath.makeParent();
        return applicationPath.toString();
    }

    return Poco::Path::current();
}

std::string detectApplicationConfigDir()
{
    const std::string applicationDir = detectApplicationDir();
    Poco::Path settingsPath(applicationDir);
    settingsPath.setFileName("settings.properties");
    if (Poco::File(settingsPath).exists())
    {
        settingsPath.makeParent();
        return settingsPath.toString();
    }

    return applicationDir;
}

std::string detectApplicationBaseName()
{
    std::string baseName;
    const std::string applicationPathValue = detectExecutablePath();
    if (!applicationPathValue.empty())
    {
        Poco::Path applicationPath(applicationPathValue);
        baseName = applicationPath.getBaseName();
        if (!baseName.empty()) return baseName;
    }

    return "macchina";
}

std::string localized(Poco::Net::HTTPServerRequest& request, const std::string& zh, const std::string& en)
{
    return requestLocale(request) == "en" ? en : zh;
}

Poco::OSP::Web::WebSession::Ptr findSession(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request);

std::string currentUsername(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request)
{
    try
    {
        Poco::OSP::Web::WebSession::Ptr pSession = findSession(pContext, request);
        if (!pSession) return "-";

        const std::string username = pSession->getValue<std::string>("username", "");
        return username.empty() ? "-" : username;
    }
    catch (...)
    {
        return "-";
    }
}

std::string remoteAddress(Poco::Net::HTTPServerRequest& request)
{
    try
    {
        return request.clientAddress().toString();
    }
    catch (...)
    {
        return "-";
    }
}

void logStreamAudit(
    Poco::OSP::BundleContext::Ptr pContext,
    const std::string& action,
    const StreamAuditInfo& info,
    const std::string& detail)
{
    std::ostringstream message;
    message
        << "LOG-STREAM action=" << action
        << " user=" << info.username
        << " client=" << info.client
        << " endpoint=" << info.endpoint
        << " lines=" << info.lineLimit
        << " locale=" << info.locale;

    if (!detail.empty())
    {
        message << " detail=" << detail;
    }

    pContext->logger().information(message.str());
}

void logSnapshotWarning(
    Poco::OSP::BundleContext::Ptr pContext,
    const Poco::Path& path,
    const std::string& detail)
{
    pContext->logger().warning(
        "LOG-SNAPSHOT action=skip path=" + path.toString() + " detail=" + detail);
}

StreamAuditInfo buildStreamAuditInfo(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request, int lineLimit)
{
    StreamAuditInfo info;
    info.username = currentUsername(pContext, request);
    info.client = remoteAddress(request);
    info.endpoint = request.getURI();
    info.locale = requestLocale(request);
    info.lineLimit = lineLimit;
    return info;
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

std::string stringifyJSON(Poco::JSON::Object::Ptr payload)
{
    std::ostringstream buffer;
    Poco::JSON::Stringifier::stringify(payload, buffer);
    return buffer.str();
}

void sendJSON(
    Poco::Net::HTTPServerResponse& response,
    Poco::JSON::Object::Ptr payload,
    Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_OK)
{
    response.setStatus(status);
    response.setChunkedTransferEncoding(true);
    response.setContentType("application/json");
    response.set("Cache-Control", "no-cache, no-store, must-revalidate");
    response.set("Pragma", "no-cache");
    response.set("Expires", "0");

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

int parseLineLimit(Poco::Net::HTTPServerRequest& request)
{
    try
    {
        int lines = 400;
        const std::string requestedLines = queryParameter(request, "lines");
        if (!requestedLines.empty())
        {
            lines = Poco::NumberParser::parse(requestedLines);
        }

        if (lines < 10) lines = 10;
        if (lines > 1000) lines = 1000;
        return lines;
    }
    catch (...)
    {
        return 400;
    }
}

bool hasLogSuffix(const std::string& fileName)
{
    return Poco::icompare(fileName.substr(fileName.size() >= 4 ? fileName.size() - 4 : 0), std::string(".log")) == 0;
}

std::string detectStreamName(const std::string& fileName)
{
    if (Poco::endsWith(fileName, std::string(".stdout.log"))) return "stdout";
    if (Poco::endsWith(fileName, std::string(".stderr.log"))) return "stderr";
    return "main";
}

std::string detectProcessId(const std::string& fileName)
{
    if (Poco::endsWith(fileName, std::string(".stdout.log"))) return fileName.substr(0, fileName.size() - std::string(".stdout.log").size());
    if (Poco::endsWith(fileName, std::string(".stderr.log"))) return fileName.substr(0, fileName.size() - std::string(".stderr.log").size());
    if (Poco::endsWith(fileName, std::string(".log"))) return fileName.substr(0, fileName.size() - std::string(".log").size());
    return fileName;
}

int streamOrder(const std::string& streamName)
{
    if (streamName == "main") return 0;
    if (streamName == "stdout") return 1;
    if (streamName == "stderr") return 2;
    return 3;
}

void appendCandidateLogFile(std::vector<Poco::Path>& files, std::set<std::string>& seen, const std::string& candidate)
{
    if (candidate.empty()) return;

    try
    {
        Poco::Path path(candidate);
        const std::string normalized = path.toString();
        if (!normalized.empty() && seen.insert(normalized).second)
        {
            files.push_back(path);
        }
    }
    catch (...)
    {
    }
}

std::vector<std::string> candidateConfigurationFiles()
{
    std::vector<std::string> files;
    std::set<std::string> seen;

    const std::string applicationDir = detectApplicationDir();
    if (!applicationDir.empty())
    {
        Poco::Path applicationPath(applicationDir);
        const std::string settingsConfig = applicationPath.append("settings.properties").toString();
        if (seen.insert(settingsConfig).second)
        {
            files.push_back(settingsConfig);
        }

        applicationPath = Poco::Path(applicationDir);
        const std::string macchinaConfig = applicationPath.append("macchina.properties").toString();
        if (seen.insert(macchinaConfig).second)
        {
            files.push_back(macchinaConfig);
        }
    }

    const auto appendFile = [&files, &seen](const Poco::Path& baseDir, const std::string& fileName)
    {
        Poco::Path filePath(baseDir);
        filePath.setFileName(fileName);
        const std::string candidate = filePath.toString();
        if (seen.insert(candidate).second)
        {
            files.push_back(candidate);
        }
    };

    Poco::Path currentDir = Poco::Path::current();
    appendFile(currentDir, "macchina.properties");
    appendFile(currentDir, "settings.properties");

    Poco::Path parentDir(currentDir);
    parentDir.makeParent();
    appendFile(parentDir, "macchina.properties");
    appendFile(parentDir, "settings.properties");

    return files;
}

std::string expandKnownPlaceholders(std::string value)
{
    const std::string applicationDir = detectApplicationDir();
    const std::string applicationConfigDir = detectApplicationConfigDir();
    const std::string applicationBaseName = detectApplicationBaseName();

    Poco::replaceInPlace(value, std::string("${application.dir}"), applicationDir);
    Poco::replaceInPlace(value, std::string("${application.configDir}"), applicationConfigDir);
    Poco::replaceInPlace(value, std::string("${application.baseName}"), applicationBaseName);
    return value;
}

std::vector<Poco::Path> candidateLogFiles()
{
    std::vector<Poco::Path> files;
    std::set<std::string> seen;

    for (const auto& configFile: candidateConfigurationFiles())
    {
        try
        {
            Poco::File file(configFile);
            if (!file.exists() || !file.isFile()) continue;

            Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pConfiguration = new Poco::Util::PropertyFileConfiguration(configFile);
            const std::string configuredLog = expandKnownPlaceholders(
                pConfiguration->getString("logging.channels.file.path", ""));
            if (!configuredLog.empty())
            {
                appendCandidateLogFile(files, seen, configuredLog);
            }
        }
        catch (...)
        {
        }
    }

    Poco::Path currentLog = Poco::Path::current();
    currentLog.setFileName("macchina.log");
    appendCandidateLogFile(files, seen, currentLog.toString());

    Poco::Path applicationLog(detectApplicationDir());
    applicationLog.setFileName(detectApplicationBaseName() + ".log");
    appendCandidateLogFile(files, seen, applicationLog.toString());

    Poco::Path parentLog = Poco::Path::current();
    parentLog.makeParent();
    parentLog.setFileName("macchina.log");
    appendCandidateLogFile(files, seen, parentLog.toString());

    return files;
}

std::map<std::string, std::vector<Poco::Path>> collectProcessLogs(Poco::OSP::BundleContext::Ptr pContext)
{
    std::map<std::string, std::vector<Poco::Path>> processLogs;

    for (const auto& logPath: candidateLogFiles())
    {
        try
        {
            Poco::File logFile(logPath);
            if (!logFile.exists() || !logFile.isFile()) continue;

            const std::string fileName = logPath.getFileName();
            if (fileName.size() < 4 || !hasLogSuffix(fileName)) continue;
            processLogs[detectProcessId(fileName)].push_back(logPath);
        }
        catch (const Poco::Exception& exc)
        {
            logSnapshotWarning(pContext, logPath, exc.displayText());
        }
        catch (const std::exception& exc)
        {
            logSnapshotWarning(pContext, logPath, exc.what());
        }
        catch (...)
        {
            logSnapshotWarning(pContext, logPath, "unknown_exception");
        }
    }

    for (auto& entry: processLogs)
    {
        std::sort(entry.second.begin(), entry.second.end(), [](const Poco::Path& left, const Poco::Path& right)
        {
            const int leftOrder = streamOrder(detectStreamName(left.getFileName()));
            const int rightOrder = streamOrder(detectStreamName(right.getFileName()));
            if (leftOrder != rightOrder) return leftOrder < rightOrder;
            return Poco::icompare(left.getFileName(), right.getFileName()) < 0;
        });
    }

    return processLogs;
}

Poco::JSON::Array::Ptr readTailLines(const Poco::Path& path, int lineLimit)
{
    std::ifstream input(path.toString(), std::ios::in);
    std::deque<std::string> lineBuffer;
    std::string line;

    while (std::getline(input, line))
    {
        lineBuffer.push_back(line);
        if (static_cast<int>(lineBuffer.size()) > lineLimit)
        {
            lineBuffer.pop_front();
        }
    }

    Poco::JSON::Array::Ptr lines = new Poco::JSON::Array;
    for (const auto& item: lineBuffer)
    {
        lines->add(item);
    }
    return lines;
}

Poco::JSON::Object::Ptr createUnauthorizedPayload(Poco::Net::HTTPServerRequest& request)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", false);
    payload->set("message", "auth_required");
    payload->set("processes", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    payload->set("updatedAt", "");
    return payload;
}

Poco::JSON::Object::Ptr createServiceUnavailablePayload(Poco::Net::HTTPServerRequest& request)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", true);
    payload->set("message", localized(request, "日志服务正在重启，请稍后重试。", "The log service is restarting. Please retry shortly."));
    payload->set("processes", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    payload->set("updatedAt", "");
    return payload;
}

LogSnapshot collectLogSnapshot(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request, int lineLimit)
{
    LogSnapshot snapshot;
    const auto processLogs = collectProcessLogs(pContext);
    Poco::Timestamp latestModified;
    bool hasLatestModified = false;

    for (const auto& processEntry: processLogs)
    {
        Poco::JSON::Array::Ptr files = new Poco::JSON::Array;
        std::size_t totalLines = 0;

        for (const auto& path: processEntry.second)
        {
            try
            {
                Poco::File file(path);
                if (!file.exists() || !file.isFile()) continue;

                Poco::JSON::Array::Ptr lines = readTailLines(path, lineLimit);
                totalLines += lines->size();

                const Poco::Timestamp modifiedAt = file.getLastModified();
                if (!hasLatestModified || modifiedAt > latestModified)
                {
                    latestModified = modifiedAt;
                    hasLatestModified = true;
                }

                Poco::JSON::Object::Ptr filePayload = new Poco::JSON::Object;
                filePayload->set("name", path.getFileName());
                filePayload->set("path", path.toString());
                filePayload->set("stream", detectStreamName(path.getFileName()));
                filePayload->set("size", static_cast<Poco::UInt64>(file.getSize()));
                filePayload->set("modifiedAt", Poco::DateTimeFormatter::format(modifiedAt, Poco::DateTimeFormat::ISO8601_FORMAT));
                filePayload->set("lines", lines);
                files->add(filePayload);
            }
            catch (const Poco::Exception& exc)
            {
                logSnapshotWarning(pContext, path, exc.displayText());
            }
            catch (const std::exception& exc)
            {
                logSnapshotWarning(pContext, path, exc.what());
            }
            catch (...)
            {
                logSnapshotWarning(pContext, path, "unknown_exception");
            }
        }

        Poco::JSON::Object::Ptr processPayload = new Poco::JSON::Object;
        processPayload->set("id", processEntry.first);
        processPayload->set("name", processEntry.first);
        processPayload->set("fileCount", files->size());
        processPayload->set("lineCount", totalLines);
        processPayload->set("files", files);
        snapshot.processes->add(processPayload);
    }

    snapshot.message = snapshot.processes->size() > 0 ? "synchronized" : "empty";
    snapshot.updatedAt = hasLatestModified
        ? Poco::DateTimeFormatter::format(latestModified, Poco::DateTimeFormat::ISO8601_FORMAT)
        : "";

    Poco::JSON::Object::Ptr signaturePayload = new Poco::JSON::Object;
    signaturePayload->set("message", snapshot.message);
    signaturePayload->set("processes", snapshot.processes);
    snapshot.signature = stringifyJSON(signaturePayload);
    return snapshot;
}

Poco::JSON::Object::Ptr createAuthorizedPayload(const LogSnapshot& snapshot)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", true);
    payload->set("message", snapshot.message);
    payload->set("updatedAt", snapshot.updatedAt);
    payload->set("processes", snapshot.processes);
    return payload;
}

Poco::JSON::Object::Ptr createSnapshotFailurePayload(Poco::Net::HTTPServerRequest& request)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", true);
    payload->set("message", localized(request, "日志快照读取失败，请稍后重试。", "Failed to read the log snapshot. Please retry shortly."));
    payload->set("updatedAt", "");
    payload->set("processes", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    return payload;
}

bool isDisconnectedMessage(const std::string& message)
{
    return message.find("broken pipe") != std::string::npos ||
        message.find("Broken pipe") != std::string::npos ||
        message.find("connection reset") != std::string::npos ||
        message.find("Connection reset") != std::string::npos ||
        message.find("connection aborted") != std::string::npos ||
        message.find("Connection aborted") != std::string::npos;
}

void sendSSEHeaders(Poco::Net::HTTPServerResponse& response)
{
    response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
    response.setChunkedTransferEncoding(true);
    response.setContentType("text/event-stream");
    response.set("Cache-Control", "no-cache");
    response.set("Connection", "keep-alive");
    response.set("X-Accel-Buffering", "no");
}

void writeSSEFrame(std::ostream& out, const std::string& eventName, const std::string& payload)
{
    out << "event: " << eventName << "\n";

    std::istringstream payloadStream(payload);
    std::string line;
    while (std::getline(payloadStream, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out << "data: " << line << "\n";
    }

    out << "\n";
    out.flush();
}

void writeSSEHeartbeat(std::ostream& out)
{
    out << ": keep-alive\n\n";
    out.flush();
}

void sendServiceUnavailableResponse(
    Poco::OSP::BundleContext::Ptr pContext,
    Poco::Net::HTTPServerRequest& request,
    Poco::Net::HTTPServerResponse& response,
    bool streamMode)
{
    const StreamAuditInfo auditInfo = buildStreamAuditInfo(pContext, request, parseLineLimit(request));
    logStreamAudit(pContext, "shutdown_reject", auditInfo, streamMode ? "stream" : "snapshot");

    if (streamMode)
    {
        response.setStatus(Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
        response.setContentType("text/plain");
        response.set("Retry-After", "1");
        response.setContentLength(0);
        try
        {
            response.send();
        }
        catch (const Poco::Net::ConnectionResetException&)
        {
        }
        catch (const Poco::Net::ConnectionAbortedException&)
        {
        }
    }
    else
    {
        sendJSON(
            response,
            createServiceUnavailablePayload(request),
            Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
    }
}

void sendUnauthorizedStream(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    logStreamAudit(pContext, "unauthorized", buildStreamAuditInfo(pContext, request, parseLineLimit(request)), "stream");
    response.setStatus(Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
    response.setContentType("text/plain");
    response.setContentLength(0);
    response.send();
}

void sendSnapshotResponse(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    const int lineLimit = parseLineLimit(request);
    sendJSON(response, createAuthorizedPayload(collectLogSnapshot(pContext, request, lineLimit)));
}

void sendStreamResponse(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    const int lineLimit = parseLineLimit(request);
    const StreamAuditInfo auditInfo = buildStreamAuditInfo(pContext, request, lineLimit);
    int pushCount = 0;
    int heartbeatCount = 0;
    bool shutdownRequested = false;
    logStreamAudit(pContext, "connect", auditInfo, "");
    sendSSEHeaders(response);

    try
    {
        std::ostream& out = response.send();
        out << "retry: 1000\n\n";
        out.flush();

        std::string lastSignature;
        int idleCycles = 0;

        for (;;)
        {
            if (MyIoT::WebUI::Home::LogStreamRequestLifecycle::isShuttingDown())
            {
                shutdownRequested = true;
                break;
            }

            const LogSnapshot snapshot = collectLogSnapshot(pContext, request, lineLimit);
            if (snapshot.signature != lastSignature)
            {
                writeSSEFrame(out, "snapshot", stringifyJSON(createAuthorizedPayload(snapshot)));
                lastSignature = snapshot.signature;
                idleCycles = 0;
                ++pushCount;
            }
            else if (++idleCycles >= 15)
            {
                writeSSEHeartbeat(out);
                idleCycles = 0;
                ++heartbeatCount;
            }

            Poco::Thread::sleep(750);
        }

        if (shutdownRequested)
        {
            logStreamAudit(
                pContext,
                "disconnect",
                auditInfo,
                "reason=shutdown pushes=" + std::to_string(pushCount) + " heartbeats=" + std::to_string(heartbeatCount));
        }
    }
    catch (const Poco::Net::ConnectionResetException&)
    {
        logStreamAudit(
            pContext,
            "disconnect",
            auditInfo,
            "reason=connection_reset pushes=" + std::to_string(pushCount) + " heartbeats=" + std::to_string(heartbeatCount));
    }
    catch (const Poco::Net::ConnectionAbortedException&)
    {
        logStreamAudit(
            pContext,
            "disconnect",
            auditInfo,
            "reason=connection_aborted pushes=" + std::to_string(pushCount) + " heartbeats=" + std::to_string(heartbeatCount));
    }
    catch (const Poco::IOException& exc)
    {
        if (!isDisconnectedMessage(exc.displayText()))
        {
            throw;
        }

        logStreamAudit(
            pContext,
            "disconnect",
            auditInfo,
            std::string("reason=io_error:") + exc.displayText() +
                " pushes=" + std::to_string(pushCount) +
                " heartbeats=" + std::to_string(heartbeatCount));
    }
    catch (const Poco::Exception& exc)
    {
        logStreamAudit(
            pContext,
            "error",
            auditInfo,
            std::string("reason=poco_exception:") + exc.displayText() +
                " pushes=" + std::to_string(pushCount) +
                " heartbeats=" + std::to_string(heartbeatCount));
    }
    catch (const std::exception& exc)
    {
        logStreamAudit(
            pContext,
            "error",
            auditInfo,
            std::string("reason=std_exception:") + exc.what() +
                " pushes=" + std::to_string(pushCount) +
                " heartbeats=" + std::to_string(heartbeatCount));
    }
    catch (...)
    {
        logStreamAudit(
            pContext,
            "error",
            auditInfo,
            "reason=unknown_exception pushes=" + std::to_string(pushCount) +
                " heartbeats=" + std::to_string(heartbeatCount));
    }
}

} // namespace

namespace MyIoT {
namespace WebUI {
namespace Home {

void LogStreamRequestLifecycle::beginStartup()
{
    gLogStreamShuttingDown.store(false);
}

void LogStreamRequestLifecycle::beginShutdown(Poco::OSP::BundleContext::Ptr pContext)
{
    gLogStreamShuttingDown.store(true);

    const int waitSliceMs = 100;
    const int waitTimeoutMs = 5000;
    int waitedMs = 0;
    int activeRequests = gActiveLogRequests.load();

    if (activeRequests > 0)
    {
        pContext->logger().information(
            "Log stream lifecycle is waiting for " + std::to_string(activeRequests) + " active request(s) before bundle stop.");
    }

    while ((activeRequests = gActiveLogRequests.load()) > 0 && waitedMs < waitTimeoutMs)
    {
        Poco::Thread::sleep(waitSliceMs);
        waitedMs += waitSliceMs;
    }

    activeRequests = gActiveLogRequests.load();
    if (activeRequests > 0)
    {
        pContext->logger().warning(
            "Log stream lifecycle timed out while waiting for " + std::to_string(activeRequests) +
            " active request(s) during bundle stop.");
    }
}

bool LogStreamRequestLifecycle::isShuttingDown()
{
    return gLogStreamShuttingDown.load();
}

bool LogStreamRequestLifecycle::tryAcquire()
{
    if (gLogStreamShuttingDown.load()) return false;

    gActiveLogRequests.fetch_add(1);
    if (gLogStreamShuttingDown.load())
    {
        gActiveLogRequests.fetch_sub(1);
        return false;
    }

    return true;
}

void LogStreamRequestLifecycle::release()
{
    gActiveLogRequests.fetch_sub(1);
}

LogStreamRequestHandler::LogStreamRequestHandler(Poco::OSP::BundleContext::Ptr pContext, Mode mode):
    _pContext(pContext),
    _mode(mode)
{
}

void LogStreamRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    ScopedLogRequestActivity activity;
    if (!activity.active())
    {
        sendServiceUnavailableResponse(_pContext, request, response, _mode == Mode::stream);
        return;
    }

    if (!isAuthenticated(_pContext, request))
    {
        if (_mode == Mode::stream)
        {
            sendUnauthorizedStream(_pContext, request, response);
        }
        else
        {
            sendJSON(response, createUnauthorizedPayload(request), Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        }
        return;
    }

    try
    {
        if (_mode == Mode::stream)
        {
            sendStreamResponse(_pContext, request, response);
        }
        else
        {
            sendSnapshotResponse(_pContext, request, response);
        }
    }
    catch (const Poco::Net::ConnectionResetException&)
    {
    }
    catch (const Poco::Net::ConnectionAbortedException&)
    {
    }
    catch (const Poco::IOException& exc)
    {
        if (!isDisconnectedMessage(exc.displayText()))
        {
            _pContext->logger().error(
                "LOG-SNAPSHOT action=error endpoint=" + request.getURI() + " detail=" + exc.displayText());
        }
    }
    catch (const Poco::Exception& exc)
    {
        _pContext->logger().error(
            "LOG-SNAPSHOT action=error endpoint=" + request.getURI() + " detail=" + exc.displayText());
        sendJSON(response, createSnapshotFailurePayload(request), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (const std::exception& exc)
    {
        _pContext->logger().error(
            "LOG-SNAPSHOT action=error endpoint=" + request.getURI() + " detail=" + exc.what());
        sendJSON(response, createSnapshotFailurePayload(request), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (...)
    {
        _pContext->logger().error(
            "LOG-SNAPSHOT action=error endpoint=" + request.getURI() + " detail=unknown_exception");
        sendJSON(response, createSnapshotFailurePayload(request), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

Poco::Net::HTTPRequestHandler* LogStreamRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new LogStreamRequestHandler(context(), LogStreamRequestHandler::Mode::snapshot);
}

Poco::Net::HTTPRequestHandler* LogStreamEventsRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new LogStreamRequestHandler(context(), LogStreamRequestHandler::Mode::stream);
}

} } } // namespace MyIoT::WebUI::Home
