// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
#include <atomic>
#include <functional>
#include <mutex>

namespace ndaw {
inline constexpr int pluginProtocol=1;
struct PluginLimits {
    int timeoutMs=10000,exitGraceMs=500,pollMs=20;
    std::size_t responseBytes=16*1024*1024,stateBytes=16*1024*1024;
    std::size_t rssBytes=2ull*1024*1024*1024,parameterCount=8192;
    void validate() const;
    Json facts() const;
};
// Native supervisor; never called from audio. Signal exits are distinct from
// exit 0 (JUCE's pinned POSIX ChildProcess loses this distinction).
struct PluginProcessResult {
    std::string status;int exitCode=-1,signal=0,pid=0;
    bool reaped=false;double wallMs=0;std::size_t peakRss=0;
    Json facts() const;
};
PluginProcessResult supervisePluginWorker(const fs::path& executable,const fs::path& request,
    const fs::path& response,const PluginLimits&,std::atomic<bool>* cancelled=nullptr,
    const std::function<bool()>& yield={},const std::function<bool()>& admittedResident={},std::atomic<int>* ownedPid=nullptr);
fs::path pluginWorkerExecutable();
fs::path defaultPluginCatalogDirectory();
int pluginOwnerPid(); // NRT process identity, never from a callback
Json readPluginCatalog(const fs::path& directory); // immutable query; no plugin code
Json pluginModuleFingerprint(const std::string& format,const std::string& candidate);
struct SessionPluginAsset {Json instance;fs::path stateFile;};
class AudioEngine;
class Commands;
// Created only by the host from the actual quiesced resident SDK process.
// It is not a model tool argument or a caller-supplied state-file path.
class PluginStateCapture {
public:
    const Json& facts() const {return receipt_;}
private:
    friend class AudioEngine;friend class Commands;
    PluginStateCapture(Json session,Json instance,fs::path root,Json receipt)
        :session_(std::move(session)),instance_(std::move(instance)),root_(std::move(root)),receipt_(std::move(receipt)){}
    Json session_,instance_;fs::path root_;Json receipt_;
};
SessionPluginAsset sessionPluginFromCatalog(const fs::path& catalog,const std::string& pluginId,
    const std::string& instanceId,const std::string& retainedStateId="");
void validateSessionPlugin(const Json&); // saved references survive missing modules/media
std::string sessionPluginAudioToken(const Json&); // excludes UI locks/labels, includes actual audio state
std::string sessionPluginRuntimeToken(const Json&); // excludes desired values; module/state/layout/latency identity
Json readPluginStateCapture(const fs::path& root,const std::string& captureId); // checksum/bytes validation; ID only

// One production inventory/command path for desktop and CLI. A scan receipt is
// not a project insert, live audio or editor acceptance receipt.
class PluginCatalog {
public:
    explicit PluginCatalog(fs::path directory,fs::path worker=pluginWorkerExecutable(),PluginLimits={});
    ~PluginCatalog();
    PluginCatalog(const PluginCatalog&)=delete;
    Json query() const;
    Json discover(std::atomic<bool>* cancelled=nullptr,const std::function<bool()>& yield={});
    Json scan(std::string format,std::string candidate,bool force=false,
        std::atomic<bool>* cancelled=nullptr,const std::function<bool()>& yield={});
    Json processFile(const std::string& pluginId,const fs::path& input,const fs::path& newOutput,
        const Json& parameters=Json::array(),std::atomic<bool>* cancelled=nullptr,const std::string& retainedStateId="");
    const fs::path& directory() const{return directory_;}
private:
    Json invoke(Json,std::atomic<bool>*,const std::function<bool()>&);
    void save();
    fs::path directory_,worker_;PluginLimits limits_;Json state_,committed_;
    mutable std::mutex mutex_;
    std::intptr_t lock_=-1;
};
}
