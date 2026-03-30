#include "BundleCatalogRequestHandler.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Exception.h"
#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/Preferences.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/String.h"
#include "Poco/Timestamp.h"
#include "Poco/URI.h"
#include "Poco/Util/AbstractConfiguration.h"
#include <algorithm>
#include <map>
#include <set>
#include <vector>

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

bool isProtectedBundleSymbolicName(const std::string& symbolicName)
{
    static const std::set<std::string> protectedBundles =
    {
        "com.appinf.osp.simple-auth",
        "io.myiot.webui.launcher",
        "io.myiot.webui.packages",
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

std::string protectedReasonFor(const std::string& symbolicName)
{
    if (symbolicName == "io.myiot.webui.launcher") return "登录入口包为访问链路核心组件，已设为只读。";
    if (symbolicName == "io.myiot.webui.packages") return "当前包列表页本身承担管理职责，不能在这里停用自己。";
    return "系统核心包不在控制范围内，仅展示状态和配置。";
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

Poco::JSON::Array::Ptr collectPreferences(const Poco::OSP::Preferences::Ptr& pPreferences)
{
    Poco::JSON::Array::Ptr result = new Poco::JSON::Array;
    if (!pPreferences) return result;

    std::vector<std::string> keys;
    collectKeys(*pPreferences, keys);
    std::sort(keys.begin(), keys.end());

    for (const auto& key: keys)
    {
        Poco::JSON::Object::Ptr entry = new Poco::JSON::Object;
        entry->set("key", key);
        entry->set("value", pPreferences->getRawString(key, std::string()));
        result->add(entry);
    }
    return result;
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

Poco::JSON::Object::Ptr createUnauthorizedPayload()
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("authenticated", false);
    payload->set("message", "未登录，无法读取系统包列表。");
    payload->set("bundles", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    return payload;
}

Poco::JSON::Object::Ptr createBundlePayload(const Poco::OSP::Bundle::Ptr& pBundle, Poco::OSP::BundleContext::Ptr pContext, Poco::OSP::PreferencesService::Ptr pPreferencesService)
{
    const bool manageable = isManageable(pBundle, pContext);
    const auto dependents = collectActiveDependents(pContext, pBundle->symbolicName());
    Poco::OSP::Preferences::Ptr pPreferences;
    if (pPreferencesService)
    {
        try
        {
            pPreferences = pPreferencesService->preferences(pBundle->symbolicName());
        }
        catch (const Poco::Exception& exc)
        {
            pContext->logger().warning(
                "Unable to load preferences for bundle " + pBundle->symbolicName() + ": " + exc.displayText());
        }
        catch (const std::exception& exc)
        {
            pContext->logger().warning(
                "Unable to load preferences for bundle " + pBundle->symbolicName() + ": " + std::string(exc.what()));
        }
    }

    Poco::JSON::Object::Ptr bundle = new Poco::JSON::Object;
    bundle->set("id", pBundle->id());
    bundle->set("name", pBundle->name());
    bundle->set("symbolicName", pBundle->symbolicName());
    bundle->set("version", pBundle->version().toString());
    bundle->set("vendor", pBundle->vendor());
    bundle->set("state", pBundle->stateString());
    bundle->set("runLevel", pBundle->runLevel());
    bundle->set("path", pBundle->path());
    bundle->set("extensionBundle", pBundle->isExtensionBundle());
    bundle->set("manageable", manageable);
    bundle->set("configurable", manageable);
    bundle->set("manageReason", manageable ? std::string() : protectedReasonFor(pBundle->symbolicName()));
    bundle->set("canResolve", manageable && pBundle->state() == Poco::OSP::Bundle::BUNDLE_INSTALLED);
    bundle->set("canStart", manageable && pBundle->state() == Poco::OSP::Bundle::BUNDLE_RESOLVED);
    bundle->set("canStop", manageable && pBundle->state() == Poco::OSP::Bundle::BUNDLE_ACTIVE && dependents.empty());
    bundle->set("preferences", collectPreferences(pPreferences));

    Poco::JSON::Array::Ptr dependentArray = new Poco::JSON::Array;
    for (const auto& dependent: dependents)
    {
        dependentArray->add(dependent);
    }
    bundle->set("activeDependents", dependentArray);
    return bundle;
}

Poco::JSON::Object::Ptr createMessagePayload(bool ok, const std::string& message, Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_OK)
{
    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("ok", ok);
    payload->set("message", message);
    payload->set("updatedAt", Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::ISO8601_FORMAT));
    payload->set("authenticated", true);
    payload->set("bundles", Poco::JSON::Array::Ptr(new Poco::JSON::Array));
    payload->set("status", static_cast<int>(status));
    return payload;
}

Poco::JSON::Array::Ptr parsePreferenceArray(const std::string& json)
{
    if (json.empty()) return new Poco::JSON::Array;

    Poco::JSON::Parser parser;
    Poco::Dynamic::Var parsed = parser.parse(json);
    return parsed.extract<Poco::JSON::Array::Ptr>();
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

    if (!target.empty())
    {
        message += " target=" + target;
    }
    if (!detail.empty())
    {
        message += " detail=" + detail;
    }

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

} // namespace

namespace MyIoT {
namespace WebUI {
namespace BundleList {

BundleCatalogRequestHandler::BundleCatalogRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
    _pContext(pContext)
{
}

void BundleCatalogRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    try
    {
        if (!isAuthenticated(_pContext, request))
        {
            logAudit(_pContext, request, "bundle_catalog", "unauthorized", "");
            sendJSON(response, createUnauthorizedPayload(), Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
            return;
        }

        const std::string username = currentUsername(_pContext, request);

        if (Poco::icompare(request.getMethod(), std::string("POST")) == 0)
        {
            Poco::Net::HTMLForm form(request, request.stream());
            const std::string symbolicName = form.get("symbolicName", "");
            const std::string action = form.get("action", "");
            Poco::OSP::Bundle::Ptr pBundle = _pContext->findBundle(symbolicName).cast<Poco::OSP::Bundle>();

            if (!pBundle)
            {
                logAudit(_pContext, request, action.empty() ? "bundle_manage" : action, "target_not_found", username, symbolicName);
                sendJSON(response, createMessagePayload(false, "目标包不存在。", Poco::Net::HTTPResponse::HTTP_NOT_FOUND), Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
                return;
            }

            if (!isManageable(pBundle, _pContext))
            {
                logAudit(_pContext, request, action.empty() ? "bundle_manage" : action, "forbidden", username, symbolicName, "protected_bundle");
                sendJSON(response, createMessagePayload(false, protectedReasonFor(symbolicName), Poco::Net::HTTPResponse::HTTP_FORBIDDEN), Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
                return;
            }

            try
            {
                if (action == "resolve")
                {
                    pBundle->resolve();
                    logAudit(_pContext, request, "resolve", "success", username, symbolicName);
                    sendJSON(response, createMessagePayload(true, "包已解析，可以继续启动。"));
                    return;
                }

                if (action == "start")
                {
                    pBundle->start();
                    logAudit(_pContext, request, "start", "success", username, symbolicName);
                    sendJSON(response, createMessagePayload(true, "包已启动。"));
                    return;
                }

                if (action == "stop")
                {
                    const auto dependents = collectActiveDependents(_pContext, symbolicName);
                    if (!dependents.empty())
                    {
                        std::string message = "仍有依赖包处于运行状态，不能停用：";
                        message += Poco::cat(std::string(", "), dependents.begin(), dependents.end());
                        logAudit(_pContext, request, "stop", "dependency_blocked", username, symbolicName, Poco::cat(std::string(","), dependents.begin(), dependents.end()));
                        sendJSON(response, createMessagePayload(false, message, Poco::Net::HTTPResponse::HTTP_CONFLICT), Poco::Net::HTTPResponse::HTTP_CONFLICT);
                        return;
                    }

                    pBundle->stop();
                    logAudit(_pContext, request, "stop", "success", username, symbolicName);
                    sendJSON(response, createMessagePayload(true, "包已停用。"));
                    return;
                }

                if (action == "savePreferences")
                {
                    Poco::OSP::PreferencesService::Ptr pPreferencesService = findPreferencesService(_pContext);
                    if (!pPreferencesService)
                    {
                        sendJSON(response, createMessagePayload(false, "当前系统未提供 PreferencesService。", Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE), Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
                        return;
                    }

                    Poco::OSP::Preferences::Ptr pPreferences = pPreferencesService->preferences(symbolicName);
                    Poco::JSON::Array::Ptr incoming = parsePreferenceArray(form.get("preferences", "[]"));

                    std::set<std::string> newKeys;
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
                    logAudit(_pContext, request, "savePreferences", "success", username, symbolicName, "keys=" + std::to_string(newKeys.size()));
                    sendJSON(response, createMessagePayload(true, "配置参数已保存。"));
                    return;
                }

                logAudit(_pContext, request, "bundle_manage", "unsupported_action", username, symbolicName, action);
                sendJSON(response, createMessagePayload(false, "不支持的管理动作。", Poco::Net::HTTPResponse::HTTP_BAD_REQUEST), Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                return;
            }
            catch (const Poco::Exception& exc)
            {
                logAudit(_pContext, request, action.empty() ? "bundle_manage" : action, "backend_error", username, symbolicName, exc.displayText(), true);
                sendJSON(response, createMessagePayload(false, exc.displayText(), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR), Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
        }

        std::vector<Poco::OSP::Bundle::Ptr> bundleVector;
        _pContext->listBundles(bundleVector);
        std::sort(bundleVector.begin(), bundleVector.end(), [](const Poco::OSP::Bundle::Ptr& left, const Poco::OSP::Bundle::Ptr& right) {
            if (left->stateString() != right->stateString()) return left->stateString() < right->stateString();
            return Poco::icompare(left->symbolicName(), right->symbolicName()) < 0;
        });

        Poco::JSON::Array::Ptr bundles = new Poco::JSON::Array;
        std::map<std::string, int> stateTotals;
        Poco::OSP::PreferencesService::Ptr pPreferencesService = findPreferencesService(_pContext);

        for (const auto& pBundle: bundleVector)
        {
            try
            {
                bundles->add(createBundlePayload(pBundle, _pContext, pPreferencesService));
            }
            catch (const Poco::Exception& exc)
            {
                _pContext->logger().error(
                    "Unable to build package payload for bundle " + pBundle->symbolicName() + ": " + exc.displayText());
            }
            catch (const std::exception& exc)
            {
                _pContext->logger().error(
                    "Unable to build package payload for bundle " + pBundle->symbolicName() + ": " + std::string(exc.what()));
            }

            stateTotals[pBundle->stateString()] += 1;
        }

        Poco::JSON::Object::Ptr stateSummary = new Poco::JSON::Object;
        for (const auto& entry: stateTotals)
        {
            stateSummary->set(entry.first, entry.second);
        }

        Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
        payload->set("authenticated", true);
        payload->set("message", bundleVector.empty() ? "当前没有发现已加载的系统包。" : "系统包列表同步完成。");
        payload->set("updatedAt", Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::ISO8601_FORMAT));
        payload->set("bundleCount", static_cast<int>(bundleVector.size()));
        payload->set("stateSummary", stateSummary);
        payload->set("bundles", bundles);
        logAudit(_pContext, request, "bundle_catalog", "success", username, "", "bundleCount=" + std::to_string(bundleVector.size()));
        sendJSON(response, payload);
    }
    catch (const Poco::Exception& exc)
    {
        _pContext->logger().error("Bundle list request failed: " + exc.displayText());
        sendJSON(response,
            createMessagePayload(false, "系统包列表读取失败。", Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR),
            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (const std::exception& exc)
    {
        _pContext->logger().error("Bundle list request failed: " + std::string(exc.what()));
        sendJSON(response,
            createMessagePayload(false, "系统包列表读取失败。", Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR),
            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

Poco::Net::HTTPRequestHandler* BundleCatalogRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest&)
{
    return new BundleCatalogRequestHandler(context());
}

} } } // namespace MyIoT::WebUI::BundleList
