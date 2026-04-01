#include "LogStreamRequestHandler.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DirectoryIterator.h"
#include "Poco/File.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/NumberParser.h"
#include "Poco/Path.h"
#include "Poco/String.h"
#include "Poco/Timestamp.h"
#include "Poco/URI.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Util/Application.h"
#include <algorithm>
#include <deque>
#include <fstream>
#include <map>
#include <set>
#include <vector>

namespace {

std::string normalizeLocale(std::string value)
{
    Poco::toLowerInPlace(value);
    return Poco::startsWith(value, std::string("en")) ? "en" : "zh";
}

std::string localized(Poco::Net::HTTPServerRequest& request, const std::string& zh, const std::string& en)
{
    const std::string explicitLocale = request.get("X-MyIoT-Locale", "");
    if (!explicitLocale.empty())
    {
        return normalizeLocale(explicitLocale) == "en" ? en : zh;
    }

    const std::string acceptLanguage = request.get("Accept-Language", "");
    if (!acceptLanguage.empty())
    {
        return normalizeLocale(acceptLanguage) == "en" ? en : zh;
    }

    return zh;
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

int parseLineLimit(Poco::Net::HTTPServerRequest& request)
{
    try
    {
        Poco::URI uri(request.getURI());
        int lines = 60;
        for (const auto& parameter: uri.getQueryParameters())
        {
            if (parameter.first == "lines")
            {
                lines = Poco::NumberParser::parse(parameter.second);
                break;
            }
        }
        if (lines < 10) lines = 10;
        if (lines > 200) lines = 200;
        return lines;
    }
    catch (...)
    {
        return 60;
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

void collectKeys(const Poco::Util::AbstractConfiguration& configuration, std::vector<std::string>& keys, const std::string& root = std::string())
{
    Poco::Util::AbstractConfiguration::Keys childKeys;
    configuration.keys(root, childKeys);

    if (childKeys.empty() && !root.empty() && configuration.hasProperty(root))
    {
        keys.push_back(root);
        return;
    }

    for (const auto& key: childKeys)
    {
        std::string fullKey = root;
        if (!fullKey.empty()) fullKey += '.';
        fullKey += key;
        collectKeys(configuration, keys, fullKey);
    }
}

void appendCandidateDirectory(std::vector<Poco::Path>& directories, std::set<std::string>& seen, const std::string& candidate)
{
    if (candidate.empty()) return;

    try
    {
        Poco::Path path(candidate);
        Poco::File file(path);

        if (file.exists() && file.isFile())
        {
            path.makeParent();
        }
        else if (!path.getExtension().empty())
        {
            path.makeParent();
        }

        const std::string normalized = path.toString();
        if (!normalized.empty() && seen.insert(normalized).second)
        {
            directories.push_back(path);
        }
    }
    catch (...)
    {
    }
}

std::vector<Poco::Path> candidateLogDirectories()
{
    std::vector<Poco::Path> directories;
    std::set<std::string> seen;
    auto& configuration = Poco::Util::Application::instance().config();

    try
    {
        appendCandidateDirectory(directories, seen, configuration.getString("application.dir", ""));
        appendCandidateDirectory(directories, seen, configuration.getString("application.configDir", ""));

        std::vector<std::string> configKeys;
        collectKeys(configuration, configKeys, "logging");
        for (const auto& key: configKeys)
        {
            if (Poco::endsWith(key, std::string(".path")))
            {
                appendCandidateDirectory(directories, seen, configuration.getString(key, ""));
            }
        }
    }
    catch (...)
    {
    }

    appendCandidateDirectory(directories, seen, "/var/log/myiot");
    appendCandidateDirectory(directories, seen, "/opt/myiot/log");
    appendCandidateDirectory(directories, seen, "/tmp");

    Poco::Path currentPath = Poco::Path::current();
    const std::string currentNormalized = currentPath.toString();
    if (seen.insert(currentNormalized).second) directories.push_back(currentPath);

    return directories;
}

std::map<std::string, std::vector<Poco::Path>> collectProcessLogs()
{
    std::map<std::string, std::vector<Poco::Path>> processLogs;
    std::set<std::string> seenFiles;

    for (const auto& basePath: candidateLogDirectories())
    {
        Poco::File baseDirectory(basePath);
        if (!baseDirectory.exists() || !baseDirectory.isDirectory())
        {
            continue;
        }

        for (Poco::DirectoryIterator it(basePath); it != Poco::DirectoryIterator(); ++it)
        {
            if (!it->isFile())
            {
                continue;
            }

            const std::string fileName = it.name();
            if (fileName.size() < 4 || !hasLogSuffix(fileName))
            {
                continue;
            }

            const std::string fullPath = it.path().toString();
            if (!seenFiles.insert(fullPath).second)
            {
                continue;
            }

            processLogs[detectProcessId(fileName)].push_back(it.path());
        }
    }

    for (auto& entry: processLogs)
    {
        std::sort(entry.second.begin(), entry.second.end(), [](const Poco::Path& left, const Poco::Path& right) {
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
    payload->set("message", localized(request, "未登录，无法读取后端日志。", "You are not signed in, so backend logs cannot be read."));
    payload->set("processes", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    return payload;
}

} // namespace

namespace MyIoT {
namespace WebUI {
namespace Home {

LogStreamRequestHandler::LogStreamRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
    _pContext(pContext)
{
}

void LogStreamRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    if (!isAuthenticated(_pContext, request))
    {
        sendJSON(response, createUnauthorizedPayload(request), Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        return;
    }

    const int lineLimit = parseLineLimit(request);
    Poco::JSON::Array::Ptr processes = new Poco::JSON::Array;
    const auto processLogs = collectProcessLogs();

    for (const auto& processEntry: processLogs)
    {
        Poco::JSON::Array::Ptr files = new Poco::JSON::Array;
        std::size_t totalLines = 0;

        for (const auto& path: processEntry.second)
        {
            Poco::File file(path);
            if (!file.exists() || !file.isFile())
            {
                continue;
            }

            Poco::JSON::Array::Ptr lines = readTailLines(path, lineLimit);
            totalLines += lines->size();

            Poco::JSON::Object::Ptr filePayload = new Poco::JSON::Object;
            filePayload->set("name", path.getFileName());
            filePayload->set("path", path.toString());
            filePayload->set("stream", detectStreamName(path.getFileName()));
            filePayload->set("size", static_cast<Poco::UInt64>(file.getSize()));
            filePayload->set("modifiedAt", Poco::DateTimeFormatter::format(file.getLastModified(), Poco::DateTimeFormat::ISO8601_FORMAT));
            filePayload->set("lines", lines);
            files->add(filePayload);
        }

        Poco::JSON::Object::Ptr processPayload = new Poco::JSON::Object;
        processPayload->set("id", processEntry.first);
        processPayload->set("name", processEntry.first);
        processPayload->set("fileCount", files->size());
        processPayload->set("lineCount", totalLines);
        processPayload->set("files", files);
        processes->add(processPayload);
    }

    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", true);
    payload->set("message", processes->size() > 0
        ? localized(request, "后端日志同步正常。", "Backend logs are synchronized.")
        : localized(request, "当前未发现可读取的后端日志文件。", "No readable backend log files are available right now."));
    payload->set("updatedAt", Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::ISO8601_FORMAT));
    payload->set("processes", processes);
    sendJSON(response, payload);
}

Poco::Net::HTTPRequestHandler* LogStreamRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new LogStreamRequestHandler(context());
}

} } } // namespace MyIoT::WebUI::Home
