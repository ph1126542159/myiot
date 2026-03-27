#include "BundleWatcherService.h"

#include "Poco/Delegate.h"
#include "Poco/DOM/DOMParser.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleInstallerService.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/Path.h"
#include "Poco/SAX/InputSource.h"
#include "Poco/StreamCopier.h"
#include "Poco/String.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Thread.h"
#include "Poco/Zip/ZipArchive.h"
#include "Poco/Zip/ZipStream.h"
#include <sstream>
#include <set>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace MyIoT {
namespace Services {
namespace BundleWatcher {

using Poco::DirectoryWatcher;
using Poco::FastMutex;
using Poco::File;
using Poco::FileInputStream;
using Poco::FileOutputStream;
using Poco::OSP::Bundle;
using Poco::OSP::BundleContext;
using Poco::OSP::BundleInstallerService;
using Poco::OSP::PreferencesService;
using Poco::OSP::ServiceRef;
using Poco::Path;
using Poco::StringTokenizer;
using Poco::XML::DOMParser;
using Poco::Zip::ZipArchive;
using Poco::Zip::ZipIOS;
using Poco::Zip::ZipLocalFileHeader;

namespace
{
const int IGNORE_WINDOW_SECONDS = 8;

class PayloadZipEntryInputStream: public ZipIOS, public std::istream
{
public:
    PayloadZipEntryInputStream(std::istream& input, const ZipLocalFileHeader& entry):
        ZipIOS(input, entry, true),
        std::istream(&_buf)
    {
    }
};

std::string trimRepositoryToken(const std::string& token)
{
    Path path(token);
    path.makeAbsolute();
    return path.toString();
}

#if defined(_WIN32)
std::wstring widePathFromUtf8(const std::string& path)
{
    if (path.empty()) return std::wstring();

    const int length = MultiByteToWideChar(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), result.data(), length);
    return result;
}

bool canOpenBundleExclusively(const std::string& path)
{
    const std::wstring widePath = widePathFromUtf8(path);
    HANDLE handle = CreateFileW(
        widePath.c_str(),
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE) return false;
    CloseHandle(handle);
    return true;
}
#endif

bool endsWithExtensionsXml(const std::string& name)
{
    if (name.size() < 14) return false;
    return Poco::icompare(name.substr(name.size() - 14), "extensions.xml") == 0;
}

bool hasValidExtensionsXml(const std::string& payload)
{
    try
    {
        std::istringstream archiveStream(payload, std::ios::binary);
        ZipArchive archive(archiveStream);

        for (auto it = archive.headerBegin(); it != archive.headerEnd(); ++it)
        {
            if (!it->second.isFile()) continue;
            if (!endsWithExtensionsXml(it->first)) continue;

            PayloadZipEntryInputStream entryStream(archiveStream, it->second);
            std::ostringstream xmlBuffer;
            Poco::StreamCopier::copyStream(entryStream, xmlBuffer);
            const std::string xmlText = xmlBuffer.str();
            if (xmlText.empty()) return false;

            DOMParser parser;
            std::istringstream xmlInput(xmlText);
            Poco::XML::InputSource source(xmlInput);
            parser.parse(&source);
            return true;
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}
}

const std::string BundleWatcherService::SERVICE_NAME("io.myiot.services.bundlewatcher");

BundleWatcherService::BundleWatcherService(BundleContext::Ptr pContext):
    _pContext(pContext),
    _selfSymbolicName(pContext->thisBundle()->symbolicName())
{
}

BundleWatcherService::~BundleWatcherService()
{
    stopWatching();
}

BundleWatcherService::Ptr createBundleWatcherService(BundleContext::Ptr pContext)
{
    return new BundleWatcherService(pContext);
}

void BundleWatcherService::startWatching()
{
    FastMutex::ScopedLock lock(_watcherMutex);
    if (!_watchers.empty()) return;

    _options = loadOptions();
    const auto directories = resolveWatchDirectories();
    for (const auto& directory: directories)
    {
        auto watcher = std::make_unique<DirectoryWatcher>(
            directory,
            DirectoryWatcher::DW_ITEM_ADDED |
                DirectoryWatcher::DW_ITEM_REMOVED |
                DirectoryWatcher::DW_ITEM_MODIFIED |
                DirectoryWatcher::DW_ITEM_MOVED_TO,
            _options.scanInterval);
        watcher->itemAdded += Poco::delegate(this, &BundleWatcherService::onItemAdded);
        watcher->itemRemoved += Poco::delegate(this, &BundleWatcherService::onItemRemoved);
        watcher->itemModified += Poco::delegate(this, &BundleWatcherService::onItemModified);
        watcher->itemMovedTo += Poco::delegate(this, &BundleWatcherService::onItemMovedTo);
        _pContext->logger().information("Watching bundle repository directory: " + directory);
        _watchers.push_back(std::move(watcher));
    }
}

void BundleWatcherService::stopWatching()
{
    FastMutex::ScopedLock lock(_watcherMutex);
    for (auto& watcher: _watchers)
    {
        watcher->itemAdded -= Poco::delegate(this, &BundleWatcherService::onItemAdded);
        watcher->itemRemoved -= Poco::delegate(this, &BundleWatcherService::onItemRemoved);
        watcher->itemModified -= Poco::delegate(this, &BundleWatcherService::onItemModified);
        watcher->itemMovedTo -= Poco::delegate(this, &BundleWatcherService::onItemMovedTo);
    }
    _watchers.clear();
}

const std::type_info& BundleWatcherService::type() const
{
    return typeid(BundleWatcherService);
}

bool BundleWatcherService::isA(const std::type_info& otherType) const
{
    return typeid(BundleWatcherService) == otherType || Poco::OSP::Service::isA(otherType);
}

std::string BundleWatcherService::normalizePath(const std::string& path) const
{
    Path normalized(path);
    normalized.makeAbsolute();
    return normalized.toString();
}

std::vector<std::string> BundleWatcherService::resolveWatchDirectories() const
{
    std::vector<std::string> directories;
    std::set<std::string> uniqueDirectories;

    ServiceRef::Ptr pPrefsRef = _pContext->registry().findByName(PreferencesService::SERVICE_NAME);
    if (!pPrefsRef)
    {
        _pContext->logger().warning("PreferencesService is unavailable. Bundle watcher has no repository paths.");
        return directories;
    }

    auto pPrefs = pPrefsRef->castedInstance<PreferencesService>();
    const std::string bundleRepository = pPrefs->configuration()->getString("osp.bundleRepository", "");
    StringTokenizer tokenizer(bundleRepository, ";", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

    for (const auto& token: tokenizer)
    {
        const std::string directory = trimRepositoryToken(token);
        File repo(directory);
        if (repo.exists() && repo.isDirectory() && uniqueDirectories.insert(directory).second)
        {
            directories.push_back(directory);
        }
    }

    return directories;
}

BundleWatcherService::WatchOptions BundleWatcherService::loadOptions() const
{
    WatchOptions options;

    ServiceRef::Ptr pPrefsRef = _pContext->registry().findByName(PreferencesService::SERVICE_NAME);
    if (pPrefsRef)
    {
        auto pPrefs = pPrefsRef->castedInstance<PreferencesService>();
        options.scanInterval = pPrefs->configuration()->getInt(
            "myiot.bundleWatcher.scanInterval",
            _pContext->thisBundle()->properties().getInt("bundleWatcher.scanInterval", options.scanInterval));
        options.settleDelayMs = pPrefs->configuration()->getInt(
            "myiot.bundleWatcher.settleDelayMs",
            _pContext->thisBundle()->properties().getInt("bundleWatcher.settleDelayMs", options.settleDelayMs));
        options.retryCount = pPrefs->configuration()->getInt(
            "myiot.bundleWatcher.retryCount",
            _pContext->thisBundle()->properties().getInt("bundleWatcher.retryCount", options.retryCount));
    }

    if (options.retryCount < 1) options.retryCount = 1;
    if (options.scanInterval < 1) options.scanInterval = 1;
    if (options.settleDelayMs < 50) options.settleDelayMs = 50;
    return options;
}

bool BundleWatcherService::isBundleFile(const std::string& path) const
{
    Path bundlePath(path);
    return Poco::icompare(bundlePath.getExtension(), "bndl") == 0;
}

std::string BundleWatcherService::symbolicNameFromPath(const std::string& path) const
{
    Path bundlePath(path);
    std::string baseName = bundlePath.getBaseName();
    const auto separator = baseName.rfind('_');
    if (separator == std::string::npos) return baseName;
    if (separator + 1 >= baseName.size()) return baseName;
    const char versionHead = baseName[separator + 1];
    if (versionHead < '0' || versionHead > '9') return baseName;
    return baseName.substr(0, separator);
}

bool BundleWatcherService::shouldIgnorePath(const std::string& path)
{
    const std::string normalizedPath = normalizePath(path);
    FastMutex::ScopedLock lock(_ignoredPathsMutex);
    Poco::Timestamp now;
    for (auto it = _ignoredPaths.begin(); it != _ignoredPaths.end();)
    {
        if (it->second < now) it = _ignoredPaths.erase(it);
        else ++it;
    }

    auto it = _ignoredPaths.find(normalizedPath);
    if (it == _ignoredPaths.end()) return false;
    if (it->second < now)
    {
        _ignoredPaths.erase(it);
        return false;
    }
    return true;
}

void BundleWatcherService::ignorePath(const std::string& path)
{
    const std::string normalizedPath = normalizePath(path);
    FastMutex::ScopedLock lock(_ignoredPathsMutex);
    Poco::Timestamp expiry;
    expiry += IGNORE_WINDOW_SECONDS * Poco::Timestamp::resolution();
    _ignoredPaths[normalizedPath] = expiry;
}

bool BundleWatcherService::tryReadBundlePayload(const std::string& path, std::string& payload) const
{
    Poco::Timestamp lastModified;
    bool hasBaseline = false;

    for (int attempt = 0; attempt < _options.retryCount; ++attempt)
    {
        try
        {
            File bundleFile(path);
            if (!bundleFile.exists() || !bundleFile.isFile())
            {
                Poco::Thread::sleep(_options.settleDelayMs);
                continue;
            }

            const auto fileSize = bundleFile.getSize();
            if (fileSize == 0)
            {
                Poco::Thread::sleep(_options.settleDelayMs);
                continue;
            }

            const Poco::Timestamp currentModified = bundleFile.getLastModified();
            if (!hasBaseline || currentModified != lastModified)
            {
                lastModified = currentModified;
                hasBaseline = true;
                Poco::Thread::sleep(_options.settleDelayMs);
                continue;
            }

#if defined(_WIN32)
            if (!canOpenBundleExclusively(path))
            {
                Poco::Thread::sleep(_options.settleDelayMs);
                continue;
            }
#endif

            FileInputStream input(path, std::ios::binary);
            if (!input.good())
            {
                Poco::Thread::sleep(_options.settleDelayMs);
                continue;
            }

            std::ostringstream output;
            Poco::StreamCopier::copyStream(input, output);
            payload = output.str();

            Poco::Thread::sleep(_options.settleDelayMs);
            File verified(path);
            if (verified.exists() &&
                verified.isFile() &&
                verified.getSize() == fileSize &&
                verified.getLastModified() == currentModified &&
                !payload.empty() &&
                hasValidExtensionsXml(payload))
            {
                return true;
            }
        }
        catch (...)
        {
        }

        Poco::Thread::sleep(_options.settleDelayMs);
    }

    return false;
}

bool BundleWatcherService::tryRemoveIncomingBundleFile(const std::string& path) const
{
    for (int attempt = 0; attempt < _options.retryCount; ++attempt)
    {
        try
        {
            File bundleFile(path);
            if (!bundleFile.exists()) return true;
            bundleFile.remove();
            return true;
        }
        catch (...)
        {
            Poco::Thread::sleep(_options.settleDelayMs);
        }
    }

    return false;
}

Bundle::Ptr BundleWatcherService::findBundleByPath(const std::string& path) const
{
    const std::string normalizedPath = normalizePath(path);
    std::vector<Bundle::Ptr> bundles;
    _pContext->listBundles(bundles);
    for (const auto& bundle: bundles)
    {
        if (normalizePath(bundle->path()) == normalizedPath)
        {
            return bundle;
        }
    }
    return nullptr;
}

Bundle::Ptr BundleWatcherService::findBundleBySymbolicName(const std::string& symbolicName) const
{
    auto bundle = _pContext->findBundle(symbolicName);
    if (!bundle) return nullptr;

    Bundle::Ptr result(const_cast<Bundle*>(bundle.get()), true);
    return result;
}

BundleInstallerService::Ptr BundleWatcherService::bundleInstaller() const
{
    ServiceRef::Ptr installerRef = _pContext->registry().findByName(BundleInstallerService::SERVICE_NAME);
    if (!installerRef) return nullptr;
    return installerRef->castedInstance<BundleInstallerService>();
}

bool BundleWatcherService::tryForceUnload(const Bundle::Ptr& pBundle) const
{
    try
    {
        _pContext->unloadBundle(pBundle);
        return true;
    }
    catch (Poco::Exception& exc)
    {
        _pContext->logger().warning(
            "Bundle watcher force unload failed for '" + pBundle->symbolicName() + "': " + exc.displayText());
        return false;
    }
}

void BundleWatcherService::handleBundleAvailable(const std::string& path)
{
    if (!isBundleFile(path) || shouldIgnorePath(path)) return;

    FastMutex::ScopedLock operationLock(_operationMutex);
    if (shouldIgnorePath(path)) return;

    const std::string symbolicName = symbolicNameFromPath(path);
    if (symbolicName.empty() || symbolicName == _selfSymbolicName) return;

    std::string payload;
    if (!tryReadBundlePayload(path, payload))
    {
        _pContext->logger().warning("Bundle watcher skipped unstable bundle file: " + path);
        return;
    }

    auto installer = bundleInstaller();
    if (!installer)
    {
        _pContext->logger().error("Bundle watcher cannot find osp.core.installer.");
        return;
    }

    try
    {
        ignorePath(path);

        Bundle::Ptr pInstalledBundle;
        Bundle::Ptr pExistingBundle = findBundleBySymbolicName(symbolicName);
        std::istringstream input(payload);
        if (pExistingBundle)
        {
            pInstalledBundle = installer->replaceBundle(symbolicName, input);
            _pContext->logger().information("Bundle watcher replaced bundle: " + symbolicName);
        }
        else
        {
            if (!tryRemoveIncomingBundleFile(path))
            {
                _pContext->logger().warning("Bundle watcher could not normalize incoming bundle file before install: " + path);
                return;
            }

            pInstalledBundle = installer->installBundle(input);
            _pContext->logger().information("Bundle watcher installed bundle: " + pInstalledBundle->symbolicName());
        }

        if (pInstalledBundle && pInstalledBundle->state() == Bundle::BUNDLE_INSTALLED)
        {
            pInstalledBundle->resolve();
        }
        if (pInstalledBundle && pInstalledBundle->state() == Bundle::BUNDLE_RESOLVED && !pInstalledBundle->lazyStart())
        {
            pInstalledBundle->start();
        }
    }
    catch (Poco::Exception& exc)
    {
        _pContext->logger().error("Bundle watcher failed to load '" + path + "': " + exc.displayText());
    }
}

void BundleWatcherService::handleBundleRemoved(const std::string& path)
{
    if (!isBundleFile(path) || shouldIgnorePath(path)) return;

    FastMutex::ScopedLock operationLock(_operationMutex);
    if (shouldIgnorePath(path)) return;

    Bundle::Ptr pBundle = findBundleByPath(path);
    if (!pBundle)
    {
        const std::string symbolicName = symbolicNameFromPath(path);
        if (!symbolicName.empty()) pBundle = findBundleBySymbolicName(symbolicName);
    }

    if (!pBundle || pBundle->symbolicName() == _selfSymbolicName) return;
    if (pBundle->preventUninstall())
    {
        _pContext->logger().warning("Bundle watcher ignored removal of protected bundle: " + pBundle->symbolicName());
        return;
    }

    std::string phase("before removal");
    try
    {
        phase = "ignoring path";
        ignorePath(pBundle->path());
        phase = "stopping bundle";
        if (pBundle->state() == Bundle::BUNDLE_ACTIVE)
        {
            pBundle->stop();
        }
        phase = "checking bundle state";
        if (pBundle->state() == Bundle::BUNDLE_RESOLVED || pBundle->state() == Bundle::BUNDLE_INSTALLED)
        {
            phase = "unloading bundle";
            if (tryForceUnload(pBundle))
            {
                _pContext->logger().information("Bundle watcher unloaded bundle: " + pBundle->symbolicName());
            }
            else
            {
                _pContext->logger().error(
                    "Bundle watcher could not unload bundle after repository removal: " + pBundle->symbolicName());
            }
        }
    }
    catch (Poco::Exception& exc)
    {
        File bundleFile(pBundle->path());
        if (!bundleFile.exists() && !pBundle->isStarted() && tryForceUnload(pBundle))
        {
            _pContext->logger().information(
                "Bundle watcher unloaded bundle after stop exception: " + pBundle->symbolicName());
        }
        else
        {
            _pContext->logger().error(
                "Bundle watcher failed to unload '" + pBundle->symbolicName() + "' during " + phase + ": " + exc.displayText());
        }
    }
}

void BundleWatcherService::onItemAdded(const void*, const DirectoryEvent& event)
{
    handleBundleAvailable(event.item.path());
}

void BundleWatcherService::onItemRemoved(const void*, const DirectoryEvent& event)
{
    handleBundleRemoved(event.item.path());
}

void BundleWatcherService::onItemModified(const void*, const DirectoryEvent& event)
{
    handleBundleAvailable(event.item.path());
}

void BundleWatcherService::onItemMovedTo(const void*, const DirectoryEvent& event)
{
    handleBundleAvailable(event.item.path());
}

} } } // namespace MyIoT::Services::BundleWatcher
