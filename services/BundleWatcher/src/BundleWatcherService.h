#pragma once

#include "Poco/AutoPtr.h"
#include "Poco/DirectoryWatcher.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleInstallerService.h"
#include "Poco/OSP/Service.h"
#include "Poco/Timestamp.h"
#include "Poco/Mutex.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace MyIoT {
namespace Services {
namespace BundleWatcher {

class BundleWatcherService: public Poco::OSP::Service
{
public:
    using Ptr = Poco::AutoPtr<BundleWatcherService>;

    static const std::string SERVICE_NAME;

    explicit BundleWatcherService(Poco::OSP::BundleContext::Ptr pContext);

    void startWatching();
    void stopWatching();

    const std::type_info& type() const override;
    bool isA(const std::type_info& otherType) const override;

protected:
    ~BundleWatcherService() override;

private:
    using DirectoryEvent = Poco::DirectoryWatcher::DirectoryEvent;

    struct WatchOptions
    {
        int scanInterval = 1;
        int settleDelayMs = 300;
        int retryCount = 20;
    };

    std::string normalizePath(const std::string& path) const;
    std::vector<std::string> resolveWatchDirectories() const;
    WatchOptions loadOptions() const;
    bool isBundleFile(const std::string& path) const;
    std::string symbolicNameFromPath(const std::string& path) const;
    bool shouldIgnorePath(const std::string& path);
    void ignorePath(const std::string& path);
    bool tryReadBundlePayload(const std::string& path, std::string& payload) const;
    bool tryRemoveIncomingBundleFile(const std::string& path) const;
    bool tryForceUnload(const Poco::OSP::Bundle::Ptr& pBundle) const;
    Poco::OSP::Bundle::Ptr findBundleByPath(const std::string& path) const;
    Poco::OSP::Bundle::Ptr findBundleBySymbolicName(const std::string& symbolicName) const;
    Poco::OSP::BundleInstallerService::Ptr bundleInstaller() const;
    void handleBundleAvailable(const std::string& path);
    void handleBundleRemoved(const std::string& path);
    void onItemAdded(const void* pSender, const DirectoryEvent& event);
    void onItemRemoved(const void* pSender, const DirectoryEvent& event);
    void onItemModified(const void* pSender, const DirectoryEvent& event);
    void onItemMovedTo(const void* pSender, const DirectoryEvent& event);

    Poco::OSP::BundleContext::Ptr _pContext;
    std::vector<std::unique_ptr<Poco::DirectoryWatcher>> _watchers;
    std::map<std::string, Poco::Timestamp> _ignoredPaths;
    WatchOptions _options;
    std::string _selfSymbolicName;
    mutable Poco::FastMutex _watcherMutex;
    mutable Poco::FastMutex _ignoredPathsMutex;
    mutable Poco::FastMutex _operationMutex;
};

BundleWatcherService::Ptr createBundleWatcherService(Poco::OSP::BundleContext::Ptr pContext);

} } } // namespace MyIoT::Services::BundleWatcher
