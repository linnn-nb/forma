// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
#include "Plugins.h"
#include <mutex>
#include <map>
#include <set>

namespace ndaw {
enum class Actor { Gui, Cli, AI };
enum class Permission { ReadOnly, Preview, ScopedLowRisk };
struct Scope { Permission mode = Permission::Preview; std::set<std::string> targets; Frame begin = 0, end = INT64_MAX; };
struct MediaCopy { fs::path from, to; std::string hash; };
struct Plan {
    std::string id, key, fingerprint;
    std::uint64_t base{};
    Actor actor{};
    Json operations, before, after, diff;
    std::vector<MediaCopy> media;
    Json preview() const;
};
struct Transaction { std::string id; Actor actor; Json before, after, diff; };
class SessionLease;
class Commands {
public:
    Commands(Json session, fs::path root,fs::path pluginCatalog=defaultPluginCatalogDirectory());
    ~Commands();
    Json query() const;
    Json pluginInventory() const; // configured local catalog; no model-supplied path, opaque bytes or SDK execution
    Json retainPluginState(const PluginStateCapture&); // host-only receipt retention; no project edit
    Json pluginStateCaptures() const; // IDs/facts only; no private paths or bytes
    Plan dryRun(const Json& operations, std::uint64_t revision, Actor, std::string key);
    void approve(const Plan&); // UI/CLI approval entry, never exposed as a model tool
    Json commit(const Plan&, const Scope& = {});
    Json undo();
    Json redo();
    Json undoTransaction(const std::string&);
    void save(const fs::path&);
    const fs::path& root() const { return root_; }
    Json history() const;
    Json retryReceipt(const Json& canonicalOperations,const std::string& key) const;
    Json beginCapture();
    void endCapture(const std::string& leaseId);
    Json captureLease() const;
    static Json registry();
private:
    Plan planLocked(const Json&, std::uint64_t, Actor, const std::string&);
    Json edit(Json&, Json& operation, std::vector<MediaCopy>&);
    void persistJournal(const Json& next, const std::string& id, Actor, std::string action = "commit", std::string target = "");
    void checkCaptureState(const Json&) const;
    fs::path root_,pluginCatalog_;
    std::unique_ptr<SessionLease> lease_;
    mutable std::mutex mutex_;
    Json state_;
    Json capture_;
    std::vector<Transaction> history_, redo_;
    std::map<std::string, std::pair<std::string, Json>> receipts_;
    std::map<std::string, std::string> approvals_;
};
}
