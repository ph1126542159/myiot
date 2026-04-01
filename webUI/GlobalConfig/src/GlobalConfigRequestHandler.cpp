#include "GlobalConfigRequestHandler.h"
#include "Poco/AutoPtr.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Dynamic/Var.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/NetException.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/Preferences.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/Path.h"
#include "Poco/String.h"
#include "Poco/Timestamp.h"
#include "Poco/URI.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include <algorithm>
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

std::string currentUsername(Poco::OSP::BundleContext::Ptr pContext, Poco::Net::HTTPServerRequest& request)
{
    try
    {
        Poco::OSP::Web::WebSession::Ptr pSession = findSession(pContext, request);
        return pSession ? pSession->getValue<std::string>("username", "") : "";
    }
    catch (...)
    {
        return "";
    }
}

std::string requestPath(Poco::Net::HTTPServerRequest& request)
{
    try
    {
        return Poco::URI(request.getURI()).getPathEtc();
    }
    catch (...)
    {
        return request.getURI();
    }
}

std::string requestRoute(Poco::Net::HTTPServerRequest& request)
{
    try
    {
        return Poco::URI(request.getURI()).getPath();
    }
    catch (...)
    {
        return request.getURI();
    }
}

std::string normalizeValue(const std::string& value)
{
    return value.empty() ? "-" : value;
}

void logAudit(Poco::OSP::BundleContext::Ptr pContext,
    Poco::Net::HTTPServerRequest& request,
    const std::string& action,
    const std::string& result,
    const std::string& username,
    const std::string& target = std::string(),
    const std::string& detail = std::string(),
    bool isError = false)
{
    std::string message =
        "WEB-AUDIT action=" + action +
        " result=" + result +
        " user=" + normalizeValue(username) +
        " client=" + request.clientAddress().host().toString() +
        " endpoint=" + requestPath(request);

    if (!target.empty()) message += " target=" + target;
    if (!detail.empty()) message += " detail=" + detail;

    if (isError)
    {
        pContext->logger().error(message);
    }
    else if (result == "success")
    {
        pContext->logger().information(message);
    }
    else
    {
        pContext->logger().warning(message);
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

Poco::JSON::Array::Ptr collectEntries(const Poco::Util::AbstractConfiguration& configuration)
{
    Poco::JSON::Array::Ptr result = new Poco::JSON::Array;
    std::vector<std::string> keys;
    collectKeys(configuration, keys);
    std::sort(keys.begin(), keys.end());

    for (const auto& key: keys)
    {
        Poco::JSON::Object::Ptr entry = new Poco::JSON::Object;
        entry->set("key", key);
        entry->set("value", configuration.getRawString(key, std::string()));
        result->add(entry);
    }
    return result;
}

Poco::OSP::PreferencesService::Ptr findPreferencesService(Poco::OSP::BundleContext::Ptr pContext)
{
    try
    {
        return Poco::OSP::ServiceFinder::find<Poco::OSP::PreferencesService>(pContext);
    }
    catch (...)
    {
        return Poco::OSP::PreferencesService::Ptr();
    }
}

bool isProtectedBundleSymbolicName(const std::string& symbolicName)
{
    static const std::set<std::string> protectedBundles =
    {
        "com.appinf.osp.simple-auth",
        "io.myiot.webui.launcher",
        "io.myiot.webui.config",
        "osp.core",
        "osp.web",
        "osp.web.server",
        "osp.web.server.secure"
    };

    if (protectedBundles.find(symbolicName) != protectedBundles.end()) return true;
    if (Poco::startsWith(symbolicName, std::string("poco."))) return true;
    if (Poco::startsWith(symbolicName, std::string("com.appinf.osp."))) return true;
    return false;
}

std::string protectedReasonFor(Poco::Net::HTTPServerRequest& request, const std::string& symbolicName)
{
    if (symbolicName == "io.myiot.webui.launcher")
    {
        return localized(request, "登录入口是系统访问链路的核心组件，这里只允许查看。", "The login entry bundle is part of the core access path and remains view-only here.");
    }
    if (symbolicName == "io.myiot.webui.config")
    {
        return localized(request, "当前配置中心负责系统管理，不允许在这里停用自己。", "The configuration center is managing the system and cannot disable itself here.");
    }
    return localized(request, "这是系统核心 Bundle，允许查看配置，但不在这里执行启停。", "This is a core system bundle. Its configuration stays visible, but runtime actions are blocked here.");
}

std::vector<std::string> collectActiveDependents(Poco::OSP::BundleContext::Ptr pContext, const std::string& symbolicName)
{
    std::vector<Poco::OSP::Bundle::Ptr> bundles;
    pContext->listBundles(bundles);

    std::vector<std::string> dependents;
    for (const auto& pBundle: bundles)
    {
        if (pBundle->symbolicName() == symbolicName || !pBundle->isStarted()) continue;

        const auto& requiredBundles = pBundle->requiredBundles();
        for (const auto& dependency: requiredBundles)
        {
            if (dependency.symbolicName == symbolicName)
            {
                dependents.push_back(pBundle->symbolicName());
                break;
            }
        }
    }

    std::sort(dependents.begin(), dependents.end());
    return dependents;
}

bool isManageable(const Poco::OSP::Bundle::Ptr& pBundle, Poco::OSP::BundleContext::Ptr pContext)
{
    return pBundle->symbolicName() != pContext->thisBundle()->symbolicName() && !isProtectedBundleSymbolicName(pBundle->symbolicName());
}

std::string joinStrings(const std::vector<std::string>& values, const std::string& delimiter)
{
    std::string result;
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0) result += delimiter;
        result += values[i];
    }
    return result;
}

Poco::JSON::Array::Ptr parseEntryArray(const std::string& json)
{
    if (json.empty()) return new Poco::JSON::Array;

    Poco::JSON::Parser parser;
    Poco::Dynamic::Var parsed = parser.parse(json);
    return parsed.extract<Poco::JSON::Array::Ptr>();
}

std::string applicationConfigPath()
{
    std::vector<std::string> candidates;

    const auto appendCandidate = [&candidates](const std::string& directory) {
        if (directory.empty()) return;
        Poco::Path path(directory);
        path.setFileName("macchina.properties");
        const std::string candidate = path.toString();
        if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end())
        {
            candidates.push_back(candidate);
        }
    };

    Poco::Path currentDir = Poco::Path::current();
    appendCandidate(currentDir.toString());

    Poco::Path parentDir(currentDir);
    parentDir.makeParent();
    appendCandidate(parentDir.toString());

    for (const auto& candidate: candidates)
    {
        if (Poco::File(candidate).exists()) return candidate;
    }

    if (!candidates.empty()) return candidates.front();
    return "macchina.properties";
}

Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> loadPropertyConfiguration(const std::string& path)
{
    if (Poco::File(path).exists())
    {
        return new Poco::Util::PropertyFileConfiguration(path);
    }
    return new Poco::Util::PropertyFileConfiguration;
}

Poco::JSON::Object::Ptr createSection(const std::string& id,
    const std::string& title,
    const std::string& scope,
    const std::string& icon,
    bool editable,
    const std::string& target,
    const std::string& sourcePath,
    const std::string& helper,
    Poco::JSON::Array::Ptr entries)
{
    Poco::JSON::Object::Ptr section = new Poco::JSON::Object;
    section->set("id", id);
    section->set("title", title);
    section->set("scope", scope);
    section->set("icon", icon);
    section->set("editable", editable);
    section->set("target", target);
    section->set("sourcePath", sourcePath);
    section->set("helper", helper);
    section->set("entryCount", static_cast<int>(entries ? entries->size() : 0));
    section->set("entries", entries ? entries : Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    return section;
}

void appendBundleRuntimeMetadata(Poco::JSON::Object::Ptr section,
    Poco::Net::HTTPServerRequest& request,
    const Poco::OSP::Bundle::Ptr& pBundle,
    Poco::OSP::BundleContext::Ptr pContext)
{
    const bool manageable = isManageable(pBundle, pContext);
    const auto dependents = collectActiveDependents(pContext, pBundle->symbolicName());

    Poco::JSON::Array::Ptr dependentArray = new Poco::JSON::Array;
    for (const auto& dependent: dependents)
    {
        dependentArray->add(dependent);
    }

    section->set("symbolicName", pBundle->symbolicName());
    section->set("state", pBundle->stateString());
    section->set("version", pBundle->version().toString());
    section->set("vendor", pBundle->vendor());
    section->set("path", pBundle->path());
    section->set("runLevel", pBundle->runLevel());
    section->set("extensionBundle", pBundle->isExtensionBundle());
    section->set("manageable", manageable);
    section->set("manageReason", manageable ? std::string() : protectedReasonFor(request, pBundle->symbolicName()));
    section->set("canResolve", manageable && pBundle->state() == Poco::OSP::Bundle::BUNDLE_INSTALLED);
    section->set("canStart", manageable && pBundle->state() == Poco::OSP::Bundle::BUNDLE_RESOLVED);
    section->set("canStop", manageable && pBundle->state() == Poco::OSP::Bundle::BUNDLE_ACTIVE && dependents.empty());
    section->set("activeDependents", dependentArray);
}

Poco::JSON::Object::Ptr createMessagePayload(bool ok,
    const std::string& message,
    const std::string& sectionId,
    Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_OK)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("ok", ok);
    payload->set("message", message);
    payload->set("sectionId", sectionId);
    payload->set("updatedAt", Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::ISO8601_FORMAT));
    payload->set("authenticated", true);
    payload->set("status", static_cast<int>(status));
    return payload;
}

} // namespace

namespace MyIoT {
namespace WebUI {
namespace GlobalConfig {

GlobalConfigRequestHandler::GlobalConfigRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
    _pContext(pContext)
{
}

void GlobalConfigRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    try
    {
        if (!isAuthenticated(_pContext, request))
        {
            logAudit(_pContext, request, "global_config", "unauthorized", "");
            Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
            payload->set("authenticated", false);
            payload->set("message", localized(request, "未登录，无法读取全局配置。", "You are not signed in, so the global configuration cannot be read."));
            payload->set("sections", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
            sendJSON(response, payload, Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
            return;
        }

        const std::string username = currentUsername(_pContext, request);

        if (Poco::icompare(request.getMethod(), std::string("POST")) == 0)
        {
            const std::string route = requestRoute(request);
            Poco::Net::HTMLForm form(request, request.stream());
            const std::string scope = form.get("scope", "");
            const std::string target = form.get("target", "");
            const std::string sectionId = form.get("sectionId", "");
            Poco::JSON::Array::Ptr incoming = parseEntryArray(form.get("entries", "[]"));
            std::set<std::string> newKeys;

            try
            {
                if (Poco::endsWith(route, std::string("/manage.json")))
                {
                    const std::string action = form.get("action", "");
                    Poco::OSP::Bundle::Ptr pBundle = _pContext->findBundle(target).cast<Poco::OSP::Bundle>();

                    if (!pBundle)
                    {
                        logAudit(_pContext, request, action.empty() ? "bundle_manage" : action, "target_not_found", username, target);
                        sendJSON(response,
                            createMessagePayload(false,
                                localized(request, "目标 Bundle 不存在。", "The target bundle does not exist."),
                                sectionId,
                                Poco::Net::HTTPResponse::HTTP_NOT_FOUND),
                            Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
                        return;
                    }

                    if (!isManageable(pBundle, _pContext))
                    {
                        logAudit(_pContext, request, action.empty() ? "bundle_manage" : action, "forbidden", username, target, "protected_bundle");
                        sendJSON(response,
                            createMessagePayload(false,
                                protectedReasonFor(request, target),
                                sectionId,
                                Poco::Net::HTTPResponse::HTTP_FORBIDDEN),
                            Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
                        return;
                    }

                    if (action == "resolve")
                    {
                        pBundle->resolve();
                        logAudit(_pContext, request, "resolve", "success", username, target);
                        sendJSON(response, createMessagePayload(true, localized(request, "Bundle 已解析，可以继续启动。", "The bundle has been resolved and can now be started."), sectionId));
                        return;
                    }

                    if (action == "start")
                    {
                        pBundle->start();
                        logAudit(_pContext, request, "start", "success", username, target);
                        sendJSON(response, createMessagePayload(true, localized(request, "Bundle 已启动。", "The bundle has been started."), sectionId));
                        return;
                    }

                    if (action == "stop")
                    {
                        const auto dependents = collectActiveDependents(_pContext, target);
                        if (!dependents.empty())
                        {
                            const std::string detail = joinStrings(dependents, ",");
                            std::string message = localized(request, "以下依赖 Bundle 仍在运行，当前无法停止：", "The bundle cannot be stopped because dependent bundles are still running: ");
                            message += joinStrings(dependents, ", ");
                            logAudit(_pContext, request, "stop", "dependency_blocked", username, target, detail);
                            sendJSON(response,
                                createMessagePayload(false, message, sectionId, Poco::Net::HTTPResponse::HTTP_CONFLICT),
                                Poco::Net::HTTPResponse::HTTP_CONFLICT);
                            return;
                        }

                        pBundle->stop();
                        logAudit(_pContext, request, "stop", "success", username, target);
                        sendJSON(response, createMessagePayload(true, localized(request, "Bundle 已停止。", "The bundle has been stopped."), sectionId));
                        return;
                    }

                    logAudit(_pContext, request, "bundle_manage", "unsupported_action", username, target, action);
                    sendJSON(response,
                        createMessagePayload(false,
                            localized(request, "不支持的运行管理动作。", "Unsupported runtime management action."),
                            sectionId,
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST),
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                    return;
                }

                if (scope == "application")
                {
                    const std::string configPath = applicationConfigPath();
                    Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pConfiguration = loadPropertyConfiguration(configPath);

                    for (std::size_t i = 0; i < incoming->size(); ++i)
                    {
                        Poco::JSON::Object::Ptr entry = incoming->getObject(i);
                        if (!entry) continue;

                        const std::string key = Poco::trim(entry->getValue<std::string>("key"));
                        if (key.empty()) continue;

                        newKeys.insert(key);
                        pConfiguration->setString(key, entry->getValue<std::string>("value"));
                    }

                    std::vector<std::string> existingKeys;
                    collectKeys(*pConfiguration, existingKeys);
                    for (const auto& existingKey: existingKeys)
                    {
                        if (newKeys.find(existingKey) == newKeys.end())
                        {
                            pConfiguration->remove(existingKey);
                        }
                    }

                    Poco::Path path(configPath);
                    Poco::File(path.parent()).createDirectories();
                    pConfiguration->save(configPath);

                    logAudit(_pContext, request, "save_application_config", "success", username, configPath, "keys=" + std::to_string(newKeys.size()));
                    sendJSON(response,
                        createMessagePayload(true,
                            localized(request, "全局配置已保存。部分设置需要重启服务后生效。", "The application configuration has been saved. Some settings may require a service restart."),
                            sectionId.empty() ? "application" : sectionId));
                    return;
                }

                if (scope == "bundle")
                {
                    Poco::OSP::PreferencesService::Ptr pPreferencesService = findPreferencesService(_pContext);
                    if (!pPreferencesService)
                    {
                        sendJSON(response,
                            createMessagePayload(false,
                                localized(request, "当前系统未提供 PreferencesService。", "PreferencesService is not available in the current system."),
                                sectionId,
                                Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE),
                            Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
                        return;
                    }

                    Poco::OSP::Bundle::Ptr pBundle = _pContext->findBundle(target).cast<Poco::OSP::Bundle>();
                    if (!pBundle)
                    {
                        sendJSON(response,
                            createMessagePayload(false,
                                localized(request, "目标功能包不存在。", "The target bundle does not exist."),
                                sectionId,
                                Poco::Net::HTTPResponse::HTTP_NOT_FOUND),
                            Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
                        return;
                    }

                    Poco::OSP::Preferences::Ptr pPreferences = pPreferencesService->preferences(target);
                    for (std::size_t i = 0; i < incoming->size(); ++i)
                    {
                        Poco::JSON::Object::Ptr entry = incoming->getObject(i);
                        if (!entry) continue;

                        const std::string key = Poco::trim(entry->getValue<std::string>("key"));
                        if (key.empty()) continue;

                        newKeys.insert(key);
                        pPreferences->setString(key, entry->getValue<std::string>("value"));
                    }

                    std::vector<std::string> existingKeys;
                    collectKeys(*pPreferences, existingKeys);
                    for (const auto& existingKey: existingKeys)
                    {
                        if (newKeys.find(existingKey) == newKeys.end())
                        {
                            pPreferences->remove(existingKey);
                        }
                    }

                    pPreferences->save();
                    logAudit(_pContext, request, "save_bundle_preferences", "success", username, target, "keys=" + std::to_string(newKeys.size()));
                    sendJSON(response,
                        createMessagePayload(true,
                            localized(request, "功能包配置已保存。", "The bundle preferences have been saved."),
                            sectionId));
                    return;
                }

                logAudit(_pContext, request, "config_request", "unsupported_scope", username, target, scope);
                sendJSON(response,
                    createMessagePayload(false,
                        localized(request, "不支持的配置范围。", "Unsupported configuration scope."),
                        sectionId,
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST),
                    Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                return;
            }
            catch (const Poco::Exception& exc)
            {
                logAudit(_pContext, request, Poco::endsWith(route, std::string("/manage.json")) ? "bundle_manage" : "save_global_config", "backend_error", username, target, exc.displayText(), true);
                sendJSON(response,
                    createMessagePayload(false, exc.displayText(), sectionId, Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR),
                    Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
        }

        Poco::JSON::Array::Ptr sections = new Poco::JSON::Array;
        std::size_t totalKeys = 0;

        const std::string configPath = applicationConfigPath();
        Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pApplicationConfiguration = loadPropertyConfiguration(configPath);
        Poco::JSON::Object::Ptr applicationSection = createSection(
            "application",
            localized(request, "应用全局配置", "Application Configuration"),
            "application",
            "mdi-application-cog-outline",
            true,
            "application",
            configPath,
            localized(request, "保存到 macchina.properties。部分配置修改后需要重启服务。", "Saved to macchina.properties. Some changes may require restarting the service."),
            collectEntries(*pApplicationConfiguration));
        totalKeys += static_cast<std::size_t>(applicationSection->getValue<int>("entryCount"));
        sections->add(applicationSection);

        std::vector<Poco::OSP::Bundle::Ptr> bundleVector;
        _pContext->listBundles(bundleVector);
        std::sort(bundleVector.begin(), bundleVector.end(), [](const Poco::OSP::Bundle::Ptr& left, const Poco::OSP::Bundle::Ptr& right) {
            return Poco::icompare(left->symbolicName(), right->symbolicName()) < 0;
        });

        Poco::OSP::PreferencesService::Ptr pPreferencesService = findPreferencesService(_pContext);
        std::size_t bundleSectionCount = 0;

        for (const auto& pBundle: bundleVector)
        {
            Poco::JSON::Array::Ptr entries = new Poco::JSON::Array;
            if (pPreferencesService)
            {
                try
                {
                    Poco::OSP::Preferences::Ptr pPreferences = pPreferencesService->preferences(pBundle->symbolicName());
                    entries = collectEntries(*pPreferences);
                }
                catch (const Poco::Exception& exc)
                {
                    _pContext->logger().warning("Unable to load preferences for bundle " + pBundle->symbolicName() + ": " + exc.displayText());
                }
            }

            Poco::JSON::Object::Ptr section = createSection(
                "bundle:" + pBundle->symbolicName(),
                pBundle->name().empty() ? pBundle->symbolicName() : pBundle->name(),
                "bundle",
                "mdi-package-variant-closed",
                static_cast<bool>(pPreferencesService),
                pBundle->symbolicName(),
                pBundle->symbolicName(),
                localized(request, "写入当前功能包的 PreferencesService 配置域。", "Writes to the current bundle PreferencesService namespace."),
                entries);
            appendBundleRuntimeMetadata(section, request, pBundle, _pContext);
            totalKeys += static_cast<std::size_t>(section->getValue<int>("entryCount"));
            ++bundleSectionCount;
            sections->add(section);
        }

        Poco::JSON::Object::Ptr summary = new Poco::JSON::Object;
        summary->set("sectionCount", static_cast<int>(sections->size()));
        summary->set("bundleSectionCount", static_cast<int>(bundleSectionCount));
        summary->set("totalKeys", static_cast<int>(totalKeys));
        summary->set("applicationConfigPath", configPath);

        Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
        payload->set("authenticated", true);
        payload->set("message", localized(request, "全局配置中心同步完成。", "The global configuration center has been synchronized."));
        payload->set("updatedAt", Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::ISO8601_FORMAT));
        payload->set("summary", summary);
        payload->set("sections", sections);

        logAudit(_pContext, request, "global_config", "success", username, "", "sectionCount=" + std::to_string(sections->size()));
        sendJSON(response, payload);
    }
    catch (const Poco::Exception& exc)
    {
        _pContext->logger().error("Global config request failed: " + exc.displayText());
        sendJSON(response,
            createMessagePayload(false,
                localized(request, "全局配置中心读取失败。", "Failed to read the global configuration center."),
                std::string(),
                Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR),
            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (const std::exception& exc)
    {
        _pContext->logger().error("Global config request failed: " + std::string(exc.what()));
        sendJSON(response,
            createMessagePayload(false,
                localized(request, "全局配置中心读取失败。", "Failed to read the global configuration center."),
                std::string(),
                Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR),
            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

Poco::Net::HTTPRequestHandler* GlobalConfigRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new GlobalConfigRequestHandler(context());
}

} } } // namespace MyIoT::WebUI::GlobalConfig
