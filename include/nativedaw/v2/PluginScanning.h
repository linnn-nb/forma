// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <json.hpp>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_cryptography/juce_cryptography.h>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <mutex>
#include <functional>
namespace ndaw::v2 {
using Json=nlohmann::json;
namespace fs=std::filesystem;
struct ScanError:std::runtime_error{std::string code;ScanError(std::string c,std::string m):std::runtime_error(std::move(m)),code(std::move(c)){}};
inline constexpr int pluginProtocol=2;
struct PluginLimits {
 int timeoutMs=10000,exitGraceMs=500,pollMs=20;
 size_t responseBytes=16*1024*1024,stateBytes=16*1024*1024,rssBytes=2ull*1024*1024*1024,parameterCount=8192;
 void validate()const;Json facts()const;
};
struct PluginProcessResult{std::string status;int exitCode=-1,signal=0,pid=0;bool reaped=false;double wallMs=0;size_t peakRss=0;Json facts()const;};
PluginProcessResult supervisePluginWorker(const fs::path&,const fs::path&,const fs::path&,const PluginLimits&,std::atomic<bool>* cancel=nullptr,const std::function<bool()>& yield={},const std::function<bool()>& resident={},std::atomic<int>* ownedPid=nullptr);
std::string digest(const std::string&);std::string sha256(const fs::path&);std::string uuid();
Json readJson(const fs::path&);void atomicWrite(const fs::path&,const std::string&,bool replace);
fs::path defaultPluginCatalogDirectory();fs::path pluginWorkerExecutable();int pluginOwnerPid();
Json readPluginCatalog(const fs::path&);Json pluginModuleFingerprint(const std::string& format,const std::string& candidate);
// Inventory and process supervision only. No legacy session or realtime engine.
class PluginCatalog {
public:
 explicit PluginCatalog(fs::path directory=defaultPluginCatalogDirectory(),fs::path worker=pluginWorkerExecutable(),PluginLimits={});
 ~PluginCatalog();PluginCatalog(const PluginCatalog&)=delete;
 Json query()const;Json discover(std::atomic<bool>* cancel=nullptr,const std::function<bool()>& yield={});
 Json scan(std::string format,std::string candidate,bool force=false,std::atomic<bool>* cancel=nullptr,const std::function<bool()>& yield={});
private:
 Json invoke(Json,std::atomic<bool>*,const std::function<bool()>&);void save();
 fs::path directory_,worker_;PluginLimits limits_;Json state_,committed_;mutable std::mutex mutex_;intptr_t lock_=-1;
};
Json runPluginScan(const Json&,const fs::path&);
}
