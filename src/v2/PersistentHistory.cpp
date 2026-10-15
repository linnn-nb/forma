// SPDX-License-Identifier: AGPL-3.0-only
#include "PersistentHistory.h"
#include "NativeEditXml.h"
#include "NativePluginStates.h"
#include <juce_cryptography/juce_cryptography.h>

namespace ndaw::v2
{
namespace
{
constexpr size_t maximumSteps = 200, maximumBytes = 32 * 1024 * 1024;
constexpr size_t legacySteps = 2048, legacyBytes = 256 * 1024 * 1024;
void require(bool value, const char* reason)
{
    if (!value)
        throw std::runtime_error(reason);
}
te::Plugin* findPlugin(te::Edit& edit, const std::string& id)
{
    // History also owns native fader/VCA plugins hidden from the insert API.
    for (auto* plugin : te::getAllPlugins(edit, true))
        if (plugin->itemID.toString().toStdString() == id)
            return plugin;
    return nullptr;
}
std::string identity(const juce::ValueTree& n)
{
    auto key = history_delta::identity(n);
    if (n.hasType(te::IDs::AUTOMATIONCURVE))
        for (auto parent = n.getParent(); parent.isValid(); parent = parent.getParent())
            if (parent.hasProperty(te::IDs::id))
                return "CURVE:" + parent[te::IDs::id].toString().toStdString() + ":" +
                       (n.hasProperty(te::IDs::paramID) ? n[te::IDs::paramID] : n[te::IDs::name])
                           .toString()
                           .toStdString();
    return key;
}
juce::ValueTree withoutHistory(juce::ValueTree state)
{
    state = state.createCopy();
    auto meta = state.getChildWithName("NATIVEDAW");
    meta.removeChild(meta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
    meta.removeChild(meta.getChildWithName("UI"), nullptr);
    for (int i = meta.getNumChildren(); --i >= 0;)
        if (meta.getChild(i).hasType("REQUEST_AUDIT"))
            meta.removeChild(i, nullptr);
    meta.removeProperty("revision", nullptr);
    return state;
}
juce::String serialise(const juce::ValueTree& state, bool compact = false)
{
    auto xml = preciseEditXml(state);
    require(bool(xml), "history XML could not be serialized");
    return compact ? xml->toString(juce::XmlElement::TextFormat{}.singleLine()) : xml->toString();
}
std::string digest(const juce::String& data)
{
    return juce::SHA256(data.toRawUTF8(), size_t(data.getNumBytesAsUTF8())).toHexString().toStdString();
}
void highWater(const juce::ValueTree& node, uint64_t& highest)
{
    highest = std::max(highest, te::EditItemID::fromProperty(node, te::IDs::id).getRawID());
    for (auto child : node)
        highWater(child, highest);
}
void resolveSources(juce::ValueTree node, const juce::File& file)
{
    if (node.hasType(te::IDs::AUDIOCLIP))
    {
        auto source = node[te::IDs::source].toString();
        if (source.isNotEmpty() && !juce::File::isAbsolutePath(source))
            node.setProperty(te::IDs::source, file.getParentDirectory().getChildFile(source).getFullPathName(),
                             nullptr);
    }
    for (auto child : node)
        resolveSources(child, file);
}
juce::ValueTree transaction(const juce::ValueTree& node, const std::string& id)
{
    if (node.hasType("TRANSACTION") && node["plan_id"].toString().toStdString() == id)
        return node;
    for (auto child : node)
        if (auto found = transaction(child, id); found.isValid())
            return found;
    return {};
}
bool excluded(juce::ValueTree node)
{
    for (; node.isValid(); node = node.getParent())
        if (node.hasType("UI") || node.hasType("REQUEST_AUDIT") || node.hasType("PERSISTENT_HISTORY"))
            return true;
    return false;
}
std::string mismatch(const juce::ValueTree& a, const juce::ValueTree& b, std::string path = {})
{
    path += "/" + a.getType().toString().toStdString() + ":" + a["id"].toString().toStdString();
    if (!a.hasType(b.getType()))
        return path + " type";
    std::set<juce::Identifier> names;
    for (const auto tree : {a, b})
        for (int i = 0; i < tree.getNumProperties(); ++i)
            names.insert(tree.getPropertyName(i));
    for (const auto& name : names)
        if (a.hasProperty(name) != b.hasProperty(name) || a[name] != b[name])
            return path + " property " + name.toString().toStdString();
    if (a.getNumChildren() != b.getNumChildren())
        return path + " child count " + std::to_string(a.getNumChildren()) +
               " != " + std::to_string(b.getNumChildren());
    for (int i = 0; i < a.getNumChildren(); ++i)
        if (auto why = mismatch(a.getChild(i), b.getChild(i), path); !why.empty())
            return why;
    return {};
}
} // namespace
struct PersistentHistory::Action final : juce::UndoableAction
{
    Action(PersistentHistory& service, juce::ValueTree delta) : service(service), delta(std::move(delta)) {}
    bool perform() override
    {
        if (active)
            service.restore(delta, true);
        return true;
    }
    bool undo() override
    {
        if (active)
            service.restore(delta, false);
        return true;
    }
    int getSizeInUnits() override
    {
        return 1;
    }
    PersistentHistory& service;
    juce::ValueTree delta;
    bool active = false;
};
PersistentHistory::PersistentHistory(Commands& c) : owner(c), observed(c.edit->state)
{
    mirror = capture();
    bases = mirror.getChildWithName("NATIVEDAW").getChildWithName("PARAMETER_BASES").createCopy();
    for (auto base : bases)
        baseNodes[identity(base)] = base;
    auto liveBases = owner.metadata.getChildWithName("PARAMETER_BASES");
    owner.metadata.removeChild(liveBases, nullptr);
    owner.metadata.addChild(bases.createCopy(), -1, nullptr);
    index(observed);
    observed.addListener(this);
}
PersistentHistory::~PersistentHistory()
{
    observed.removeListener(this);
}
juce::ValueTree PersistentHistory::capture()
{
    Commands::ParameterWriteGuard guard(owner);
    auto copy = withoutHistory(owner.edit->state);
    juce::ValueTree values("PARAMETER_BASES");
    // The pinned SDK enumerates ClipTrack plugins, then audio clip plugins
    // again. Only repeated references to the same native instance are aliases.
    std::set<te::Plugin*> captured;
    for (auto* plugin : te::getAllPlugins(*owner.edit, true))
    {
        if (!captured.insert(plugin).second)
            continue;
        for (auto* parameter : plugin->getAutomatableParameters())
        {
            juce::ValueTree value("BASE");
            value.setProperty("plugin", plugin->itemID.toString(), nullptr);
            value.setProperty("parameter", parameter->paramID, nullptr);
            value.setProperty("value", double(parameter->getCurrentExplicitValue()), nullptr);
            values.addChild(value, -1, nullptr);
        }
    }
    auto info = copy.getChildWithName("NATIVEDAW");
    info.removeChild(info.getChildWithName("PARAMETER_BASES"), nullptr);
    info.addChild(values, -1, nullptr);
    if (owner.nativeStates)
        owner.nativeStates->copyCheckpoints(copy);
    std::map<juce::String, juce::String> sources;
    for (auto* track : te::getAudioTracks(*owner.edit))
        for (auto* clip : track->getClips())
            if (auto* wave = dynamic_cast<te::WaveAudioClip*>(clip))
                if (const auto file = wave->getOriginalFile(); file != juce::File{})
                    sources[wave->itemID.toString()] = file.getFullPathName();
    auto visit = [&](auto&& self, juce::ValueTree n) -> void
    {
        if (auto it = sources.find(n[te::IDs::id].toString()); it != sources.end() && n.hasProperty(te::IDs::source))
            n.setProperty(te::IDs::source, it->second, nullptr);
        for (auto child : n)
            self(self, child);
    };
    visit(visit, copy);
    return copy;
}
juce::ValueTree PersistentHistory::normalise(juce::ValueTree n)
{
    if (n.hasType("NATIVEDAW"))
    {
        n = n.createCopy();
        n.removeProperty("revision", nullptr);
        for (int i = n.getNumChildren(); --i >= 0;)
            if (auto child = n.getChild(i); child.hasType("UI") || child.hasType("REQUEST_AUDIT") ||
                                            child.hasType("PERSISTENT_HISTORY") || child.hasType("PARAMETER_BASES"))
                n.removeChild(i, nullptr);
        n.addChild(bases.createCopy(), -1, nullptr);
    }
    else if (n.hasType(te::IDs::PLUGIN) && owner.nativeStates && !saving)
    {
        n = n.createCopy();
        owner.nativeStates->copyCheckpoints(n);
    }
    else if (n.hasType(te::IDs::AUDIOCLIP))
        if (auto* clip = owner.timelineClip(n[te::IDs::id].toString().toStdString()))
            if (auto* wave = dynamic_cast<te::WaveAudioClip*>(clip))
                if (auto source = wave->getOriginalFile(); source != juce::File{})
                {
                    n = n.createCopy();
                    n.setProperty(te::IDs::source, source.getFullPathName(), nullptr);
                }
    return n;
}
void PersistentHistory::mark(juce::ValueTree node, bool recursive)
{
    if (replaying || excluded(node) || !node.isValid())
        return;
    history_delta::Path path;
    for (;;)
    {
        try
        {
            path = history_delta::address(node);
            break;
        }
        catch (const history_delta::DuplicateIdentity&)
        {
            // Native insertion (notably TimeSig) clones the previous ID before
            // L1 assigns the new ID. Track its stable ancestor during that
            // synchronous notification; final snapshots still reject duplicates.
            node = node.getParent();
            recursive = true;
            if (!node.isValid())
                throw;
        }
    }
    for (auto it = dirty.begin(); it != dirty.end();)
    {
        const auto& [old, deep] = it->second;
        if (deep && history_delta::prefix(old, path))
            return;
        if (recursive && history_delta::prefix(path, old))
            it = dirty.erase(it);
        else
            ++it;
    }
    auto& item = dirty[path.dump()];
    item.first = std::move(path);
    item.second |= recursive;
}
void PersistentHistory::notePlugins(juce::ValueTree n)
{
    if (n.hasType(te::IDs::PLUGIN))
        dirtyPlugins.insert(n[te::IDs::id].toString().toStdString());
    for (auto child : n)
        notePlugins(child);
}
void PersistentHistory::parameterTouched(te::AutomatableParameter& p)
{
    if (!replaying)
        dirtyPlugins.insert(p.getOwnerID().toString().toStdString());
}
void PersistentHistory::valueTreePropertyChanged(juce::ValueTree& n, const juce::Identifier& property)
{
    if (excluded(n) || (n.hasType("NATIVEDAW") && property == juce::Identifier("revision")))
        return;
    mark((property == te::IDs::id || property == juce::Identifier("ndaw_id") || property == juce::Identifier("plan_id"))
             ? n.getParent()
             : n,
         property == te::IDs::id || property == juce::Identifier("ndaw_id"));
    for (auto p = n; p.isValid(); p = p.getParent())
        if (p.hasType(te::IDs::PLUGIN))
        {
            dirtyPlugins.insert(p[te::IDs::id].toString().toStdString());
            break;
        }
    if (n.hasType("BASE"))
        dirtyPlugins.insert(n["plugin"].toString().toStdString());
}
void PersistentHistory::valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child)
{
    mark(parent, true);
    if (!replaying && !excluded(child))
    {
        notePlugins(child);
        index(child);
    }
}
void PersistentHistory::valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int)
{
    mark(parent, true);
    if (!replaying && !excluded(parent))
        notePlugins(child);
}
void PersistentHistory::valueTreeChildOrderChanged(juce::ValueTree& parent, int, int)
{
    mark(parent, true);
}
void PersistentHistory::refreshBases()
{
    if (dirtyPlugins.empty())
        return;
    const auto metaPath = history_delta::address(owner.metadata);
    auto basesPath = metaPath;
    basesPath.push_back({{"type", "PARAMETER_BASES"}, {"key", ""}, {"ordinal", 0}});
    auto addDirty = [&](const history_delta::Path& path, bool deep)
    {
        for (const auto& [_, item] : dirty)
            if (item.second && history_delta::prefix(item.first, path))
                return;
        auto& entry = dirty[path.dump()];
        entry.first = path;
        entry.second |= deep;
    };
    for (const auto& id : dirtyPlugins)
    {
        auto* plugin = findPlugin(*owner.edit, id);
        std::set<std::string> valid;
        if (plugin)
        {
            mark(plugin->state, false);
            for (auto* parameter : plugin->getAutomatableParameters())
            {
                const auto key = "BASE:" + id + ":" + parameter->paramID.toStdString();
                valid.insert(key);
                auto it = baseNodes.find(key);
                if (it == baseNodes.end())
                {
                    juce::ValueTree value("BASE");
                    value.setProperty("plugin", juce::String(id), nullptr);
                    value.setProperty("parameter", parameter->paramID, nullptr);
                    value.setProperty("value", double(parameter->getCurrentExplicitValue()), nullptr);
                    bases.addChild(value, -1, nullptr);
                    baseNodes[key] = value;
                    addDirty(basesPath, true);
                }
                else if (double(it->second["value"]) != double(parameter->getCurrentExplicitValue()))
                {
                    auto path = basesPath;
                    path.push_back({{"type", "BASE"}, {"key", key}, {"ordinal", 0}});
                    addDirty(path, false);
                    it->second.setProperty("value", double(parameter->getCurrentExplicitValue()), nullptr);
                }
            }
        }
        for (auto it = baseNodes.begin(); it != baseNodes.end();)
            if (it->second["plugin"].toString().toStdString() == id && !valid.contains(it->first))
            {
                bases.removeChild(it->second, nullptr);
                it = baseNodes.erase(it);
                addDirty(basesPath, true);
            }
            else
                ++it;
    }
    dirtyPlugins.clear();
}
juce::ValueTree PersistentHistory::collect(bool publish)
{
    refreshBases();
    juce::ValueTree result("DELTA");
    // Some SDK readers materialise state and emit new notifications. Never hold
    // references into a map that a re-entrant listener can erase.
    auto pending = std::move(dirty);
    dirty.clear();
    for (const auto& [_, item] : pending)
    {
        const auto& [path, recursive] = item;
        bool covered = false;
        for (const auto& [key, parent] : pending)
            if (key != path.dump() && parent.second && history_delta::prefix(parent.first, path))
            {
                covered = true;
                break;
            }
        if (covered)
            continue;
        auto before = history_delta::resolve(mirror, path);
        juce::ValueTree after;
        if (path.size() >= 2 && path[0]["type"] == "NATIVEDAW" && path[1]["type"] == "PARAMETER_BASES")
        {
            after = bases;
            auto relative = path;
            relative.erase(relative.begin(), relative.begin() + 2);
            after = history_delta::resolve(after, relative);
        }
        else
            after = history_delta::resolve(observed, path);
        if (!before.isValid() || !after.isValid())
            continue; // Parent structural event owns removed/new nodes.
        auto delta = history_delta::difference(before, after, path, recursive,
                                               [this](juce::ValueTree n) { return normalise(n); });
        for (auto change : delta)
            result.addChild(change.createCopy(), -1, nullptr);
    }
    if (publish && result.getNumChildren())
        history_delta::apply(mirror, result, true);
    return result;
}
void PersistentHistory::checkpoint()
{
    auto pending = collect();
    if (pending.getNumChildren() == 0)
        return;
    // SDK asynchronous changes belong to the open transaction boundary. Updating
    // one boundary affects its predecessor's after and successor's before, never
    // requires full snapshots. Keep the DELTA handle held by restored actions.
    auto replace = [&](size_t at, juce::ValueTree composed)
    {
        auto root = steps.at(at).delta;
        root.copyPropertiesAndChildrenFrom(composed, nullptr);
        steps.at(at) = encode(root);
    };
    if (owner.historyCursor > 0 && owner.historyCursor <= steps.size())
    {
        auto composed = steps[owner.historyCursor - 1].delta.createCopy();
        for (auto change : pending)
            composed.addChild(change.createCopy(), -1, nullptr);
        replace(owner.historyCursor - 1, composed);
    }
    if (owner.historyCursor < steps.size())
    {
        auto composed = history_delta::inverse(pending);
        for (auto change : steps[owner.historyCursor].delta)
            composed.addChild(change.createCopy(), -1, nullptr);
        replace(owner.historyCursor, composed);
    }
    pruneBlocks();
}
void PersistentHistory::moved(bool forward)
{
    // Live native actions update the Edit but not the detached journal. Restored
    // actions already move both. Never mistake the Undo itself for a late echo.
    if (!journalApplied)
    {
        const auto at = forward ? owner.historyCursor - 1 : owner.historyCursor;
        history_delta::apply(mirror, steps.at(at).delta, forward);
    }
    journalApplied = false;
    checkpoint();
}
PersistentHistory::Step PersistentHistory::encode(juce::ValueTree delta)
{
    auto packed = delta.createCopy();
    std::set<std::string> references;
    auto pack = [&](auto&& self, juce::ValueTree n) -> void
    {
        for (int i = 0; i < n.getNumProperties(); ++i)
        {
            const auto name = n.getPropertyName(i);
            const auto value = n[name];
            if ((!value.isString() || value.toString().getNumBytesAsUTF8() < 1024) && !value.isBinaryData())
                continue;
            juce::ValueTree block("BLOCK");
            block.setProperty("value", value, nullptr);
            const auto data = serialise(block, true);
            const auto key = digest(data);
            references.insert(key);
            blocks.try_emplace(key, Block{value, data});
            juce::ValueTree ref("PACKED_BLOCK");
            ref.setProperty("property", name.toString(), nullptr);
            ref.setProperty("hash", juce::String(key), nullptr);
            n.setProperty(name, "", nullptr);
            n.addChild(ref, -1, nullptr);
        }
        for (auto child : n)
            if (!child.hasType("PACKED_BLOCK"))
                self(self, child);
    };
    pack(pack, packed);
    auto data = serialise(packed, true);
    return {std::move(delta), data, digest(data), std::move(references)};
}
void PersistentHistory::appended()
{
    require(owner.historyCursor > 0 && owner.historyCursor <= owner.history.size(),
            "persistent history cursor missing");
    // Prepare and encode before publishing the journal or discarding Redo.
    // Failure must leave the prior mirror and step handles available for rollback.
    steps.reserve(owner.historyCursor);
    auto delta = collect(false);
    auto step = encode(delta);
    history_delta::apply(mirror, delta, true);
    steps.resize(owner.historyCursor - 1);
    steps.push_back(std::move(step));
    trim();
}
void PersistentHistory::aborted()
{
    juce::ScopedValueSetter<bool> guard(replaying, true);
    bases = mirror.getChildWithName("NATIVEDAW").getChildWithName("PARAMETER_BASES").createCopy();
    baseNodes.clear();
    for (auto base : bases)
        baseNodes[identity(base)] = base;
    auto live = owner.metadata.getChildWithName("PARAMETER_BASES");
    owner.metadata.removeChild(live, nullptr);
    owner.metadata.addChild(bases.createCopy(), -1, nullptr);
    dirty.clear();
    dirtyPlugins.clear();
    nodes.clear();
    index(observed);
    applyBases(mirror);
    pruneBlocks();
    rebuildUndo();
    journalApplied = false;
}
void PersistentHistory::pruneBlocks()
{
    std::set<std::string> used;
    for (const auto& step : steps)
        used.insert(step.references.begin(), step.references.end());
    std::erase_if(blocks, [&](const auto& pair) { return !used.contains(pair.first); });
}
void PersistentHistory::trim()
{
    pruneBlocks();
    auto bytes = [&] { return status()["encoded_bytes"].get<size_t>(); };
    bool changed = false;
    while (steps.size() > maximumSteps || (bytes() > maximumBytes && steps.size() > 1))
    {
        if (owner.historyCursor > 0)
        {
            steps.erase(steps.begin());
            owner.historyRecords.erase(owner.history.front());
            owner.history.erase(owner.history.begin());
            --owner.historyCursor;
        }
        else
        {
            // A deep-Undo legacy file must keep its current state. With no past
            // left, truncate the distant future rather than cross the cursor.
            steps.pop_back();
            owner.historyRecords.erase(owner.history.back());
            owner.history.pop_back();
        }
        ++discarded;
        changed = true;
        pruneBlocks();
    }
    if (changed)
    {
        nodes.clear();
        index(observed);
        rebuildUndo();
    }
}
Json PersistentHistory::status() const
{
    size_t bytes = 0;
    for (const auto& step : steps)
        bytes += size_t(step.data.getNumBytesAsUTF8());
    for (const auto& [_, block] : blocks)
        bytes += size_t(block.data.getNumBytesAsUTF8());
    return {{"schema", 2},
            {"retained_steps", steps.size()},
            {"maximum_steps", maximumSteps},
            {"maximum_bytes", maximumBytes},
            {"encoded_bytes", bytes},
            {"discarded_steps", discarded},
            {"notice", discarded ? "历史上限200步或32MiB；历史窗口已截断，当前工程与媒体保留" : ""}};
}
project_document::Requirements PersistentHistory::requirements() const
{
    project_document::Requirements result;
    project_document::collect(mirror, result);
    for (const auto& step : steps)
        project_document::collect(step.delta, result);
    project_document::validate(result);
    return result;
}
juce::ValueTree PersistentHistory::save()
{
    juce::ScopedValueSetter<bool> flushedState(saving, true);
    checkpoint();
    require(steps.size() == owner.history.size(), "persistent history boundary is missing");
    require(status()["encoded_bytes"].get<size_t>() <= maximumBytes, "one transaction exceeds 32 MiB history budget");
    auto saved = mirror.createCopy();
    auto currentData = serialise(saved);
    juce::ValueTree archive("PERSISTENT_HISTORY");
    archive.setProperty("schema", 2, nullptr);
    archive.setProperty("cursor", int(owner.historyCursor), nullptr);
    archive.setProperty("discarded", juce::int64(discarded), nullptr);
    archive.setProperty("current_hash", juce::String(digest(currentData)), nullptr);
    for (size_t i = 0; i < steps.size(); ++i)
    {
        juce::ValueTree step("STEP");
        step.setProperty("plan_id", juce::String(owner.history[i]), nullptr);
        step.setProperty("xml", steps[i].data, nullptr);
        step.setProperty("sha256", juce::String(steps[i].hash), nullptr);
        archive.addChild(step, -1, nullptr);
    }
    juce::ValueTree pool("BLOCKS");
    for (const auto& [key, value] : blocks)
    {
        juce::ValueTree block("ENTRY");
        block.setProperty("sha256", juce::String(key), nullptr);
        block.setProperty("xml", value.data, nullptr);
        pool.addChild(block, -1, nullptr);
    }
    archive.addChild(pool, -1, nullptr);
    auto info = saved.getChildWithName("NATIVEDAW");
    info.setProperty("revision", juce::int64(owner.revision), nullptr);
    for (auto current : owner.metadata)
        if (current.hasType("UI") || current.hasType("REQUEST_AUDIT"))
            info.addChild(current.createCopy(), -1, nullptr);
    info.addChild(archive, -1, nullptr);
    return saved;
}
PersistentHistory::Archive PersistentHistory::read(juce::ValueTree current, const juce::File& file)
{
    Archive result;
    project_document::collect(current, result.requirements);
    project_document::validate(result.requirements);
    const auto archive = current.getChildWithName("NATIVEDAW").getChildWithName("PERSISTENT_HISTORY");
    if (!archive.isValid())
        return result;
    result.current = withoutHistory(current);
    resolveSources(result.current, file);
    const int schema = int(archive["schema"]), cursor = int(archive["cursor"]);
    require(schema == 1 || schema == 2, "invalid persistent history schema");
    size_t total = 0;
    std::set<std::string> ids;
    auto validateID = [&](const juce::ValueTree& step)
    {
        auto id = step["plan_id"].toString().toStdString();
        require(!id.empty() && id.size() <= 128 && ids.insert(id).second, "invalid persistent transaction ID");
        result.ids.push_back(id);
        return id;
    };
    auto dataOf = [&](const juce::ValueTree& entry, size_t limit)
    {
        auto data = entry["xml"].toString();
        total += size_t(data.getNumBytesAsUTF8());
        require(total <= limit, "persistent history exceeds byte budget");
        require(digest(data) == entry["sha256"].toString().toStdString(), "persistent history checksum mismatch");
        return data;
    };
    if (schema == 1)
    {
        require(archive.getNumChildren() >= 1 && size_t(archive.getNumChildren()) <= legacySteps + 1,
                "invalid legacy history capacity");
        require(cursor >= 0 && cursor < archive.getNumChildren(), "invalid persistent history cursor");
        juce::ValueTree previous;
        for (int i = 0; i < archive.getNumChildren(); ++i)
        {
            auto step = archive.getChild(i);
            require(step.hasType("STATE"), "invalid legacy history entry");
            auto data = dataOf(step, legacyBytes);
            auto xml = juce::XmlDocument::parse(data);
            require(xml && xml->hasTagName("EDIT"), "invalid legacy history state");
            auto state = juce::ValueTree::fromXml(*xml);
            project_document::collect(state, result.requirements);
            require(!state.getChildWithName("NATIVEDAW").getChildWithName("PERSISTENT_HISTORY").isValid(),
                    "recursive persistent history is not allowed");
            highWater(state, result.highestID);
            if (i > 0)
            {
                const auto id = validateID(step);
                require(transaction(state, id).isValid(), "persistent transaction description missing");
                result.deltas.push_back(history_delta::difference(previous, state));
            }
            if (i == cursor)
            {
                require(digest(data) == archive["current_hash"].toString().toStdString(),
                        "persistent history current-state checksum mismatch");
                require(digest(serialise(result.current)) == digest(data), "saved Edit and history disagree");
            }
            previous = std::move(state);
        }
    }
    else
    {
        std::map<std::string, juce::var> values;
        const auto pool = archive.getChildWithName("BLOCKS");
        require(pool.isValid(), "persistent block pool missing");
        for (auto entry : pool)
        {
            require(entry.hasType("ENTRY"), "invalid persistent block entry");
            auto data = dataOf(entry, maximumBytes);
            auto key = entry["sha256"].toString().toStdString();
            auto xml = juce::XmlDocument::parse(data);
            require(xml && xml->hasTagName("BLOCK"), "invalid persistent block");
            auto block = juce::ValueTree::fromXml(*xml);
            require(values.emplace(key, block["value"]).second, "duplicate persistent block");
        }
        for (auto step : archive)
        {
            if (step == pool)
                continue;
            require(step.hasType("STEP"), "invalid persistent history entry");
            auto id = validateID(step);
            auto data = dataOf(step, maximumBytes);
            auto xml = juce::XmlDocument::parse(data);
            require(xml && xml->hasTagName("DELTA"), "invalid persistent delta");
            auto delta = juce::ValueTree::fromXml(*xml);
            auto unpack = [&](auto&& self, juce::ValueTree n) -> void
            {
                for (int i = n.getNumChildren(); --i >= 0;)
                {
                    auto child = n.getChild(i);
                    if (child.hasType("PACKED_BLOCK"))
                    {
                        auto key = child["hash"].toString().toStdString();
                        require(values.contains(key), "missing persistent block");
                        auto name = child["property"].toString();
                        require(name.isNotEmpty() && n.hasProperty(name), "invalid block property");
                        n.setProperty(name, values.at(key), nullptr);
                        n.removeChild(i, nullptr);
                    }
                    else
                        self(self, child);
                }
            };
            unpack(unpack, delta);
            project_document::collect(delta, result.requirements);
            require(transaction(delta, id).isValid(), "persistent transaction description missing");
            highWater(delta, result.highestID);
            result.deltas.push_back(std::move(delta));
        }
        require(result.deltas.size() <= maximumSteps, "invalid persistent history capacity");
        require(digest(serialise(result.current)) == archive["current_hash"].toString().toStdString(),
                "saved Edit and history disagree");
        result.discarded = size_t(std::max<juce::int64>(0, juce::int64(archive["discarded"])));
    }
    project_document::validate(result.requirements);
    require(cursor >= 0 && size_t(cursor) <= result.deltas.size(), "invalid persistent history cursor");
    result.cursor = size_t(cursor);
    // Validate every stored path/structural operation on one detached Edit before adoption.
    auto validation = result.current.createCopy();
    for (size_t i = result.cursor; i > 0; --i)
        history_delta::apply(validation, result.deltas[i - 1], false);
    for (const auto& delta : result.deltas)
        history_delta::apply(validation, delta, true);
    for (size_t i = result.deltas.size(); i > result.cursor; --i)
        history_delta::apply(validation, result.deltas[i - 1], false);
    const auto a = juce::ValueTree::fromXml(*preciseEditXml(validation));
    const auto b = juce::ValueTree::fromXml(*preciseEditXml(result.current));
    if (!a.isEquivalentTo(b))
        throw std::runtime_error("persistent delta chain does not restore current Edit: " + mismatch(a, b));
    highWater(result.current, result.highestID);
    return result;
}
void PersistentHistory::index(juce::ValueTree node)
{
    if (auto key = identity(node); !key.empty())
        nodes[key] = node;
    else if (node.getParent() == observed)
        nodes["ROOT:" + node.getType().toString().toStdString()] = node;
    for (auto child : node)
        index(child);
}
void PersistentHistory::patch(juce::ValueTree target, const juce::ValueTree& desired)
{
    target.copyPropertiesFrom(desired, nullptr);
    std::vector<juce::ValueTree> children;
    std::map<juce::String, int> ordinals;
    for (auto child : desired)
    {
        juce::ValueTree next;
        const auto key = identity(child);
        if (!key.empty())
        {
            if (auto it = nodes.find(key); it != nodes.end())
                next = it->second;
        }
        else
        {
            int ordinal = ordinals[child.getType().toString()]++;
            for (auto existing : target)
                if (existing.hasType(child.getType()) && identity(existing).empty() && ordinal-- == 0)
                {
                    next = existing;
                    break;
                }
        }
        if (!next.isValid())
        {
            next = juce::ValueTree(child.getType());
            if (!key.empty())
                nodes[key] = next;
        }
        patch(next, child);
        children.push_back(next);
    }
    for (int i = target.getNumChildren(); --i >= 0;)
        if (std::find(children.begin(), children.end(), target.getChild(i)) == children.end())
            target.removeChild(i, nullptr);
    for (int i = 0; i < int(children.size()); ++i)
    {
        auto next = children[size_t(i)];
        if (next.getParent() != target)
        {
            if (auto parent = next.getParent(); parent.isValid())
                parent.removeChild(next, nullptr);
            target.addChild(next, i, nullptr);
        }
        else
            target.moveChild(target.indexOf(next), i, nullptr);
    }
}

void PersistentHistory::restore(const juce::ValueTree& delta, bool forward)
{
    owner.checkThread();
    Commands::ParameterWriteGuard guard(owner);
    juce::ScopedValueSetter<bool> restoring(replaying, true);
    std::set<std::string> affected, opaque, baseKeys;
    std::map<std::string, std::set<juce::String>> curves;
    auto note = [&](auto&& self, const juce::ValueTree& n) -> void
    {
        if (n.hasType("BASE"))
            baseKeys.insert(identity(n));
        if (n.hasType(te::IDs::PLUGIN))
        {
            auto id = n[te::IDs::id].toString().toStdString();
            affected.insert(id);
            opaque.insert(id);
        }
        for (auto child : n)
            self(self, child);
    };
    for (auto change : delta)
    {
        const auto path = Json::parse(change["path"].toString().toStdString());
        auto target = history_delta::resolve(observed, path);
        std::string plugin;
        for (auto p = target; p.isValid(); p = p.getParent())
            if (p.hasType(te::IDs::PLUGIN))
            {
                plugin = p[te::IDs::id].toString().toStdString();
                affected.insert(plugin);
                break;
            }
        for (const auto& part : path)
            if (part["type"] == "BASE")
                baseKeys.insert(part["key"].get<std::string>());
        if (!plugin.empty())
        {
            for (const auto& part : path)
                if (part["type"] == "AUTOMATIONCURVE")
                    curves[plugin].insert(juce::String(part["key"].get<std::string>().substr(6)));
            for (auto child : change)
                if (child.hasType("ADD") || child.hasType("REMOVE"))
                    if (auto curve = child.getChild(0); curve.hasType(te::IDs::AUTOMATIONCURVE))
                        curves[plugin].insert(
                            (curve.hasProperty(te::IDs::paramID) ? curve[te::IDs::paramID] : curve[te::IDs::name])
                                .toString());
            if (target.hasType(te::IDs::PLUGIN) && change.hasProperty("properties"))
                for (const auto& name : Json::parse(change["properties"].toString().toStdString()))
                    if (name == "state" || name == "programNum" ||
                        (name == "parameters" && dynamic_cast<te::ExternalPlugin*>(findPlugin(*owner.edit, plugin))))
                        opaque.insert(plugin);
        }
        for (auto child : change)
            if (child.hasType("ADD") || child.hasType("REMOVE"))
                note(note, child);
    }
    auto reconcile = [&](juce::ValueTree parent, const juce::ValueTree& desired)
    {
        auto key = identity(desired);
        if (key.empty() && parent == observed)
            key = "ROOT:" + desired.getType().toString().toStdString();
        if (desired.hasType(te::IDs::AUTOMATIONCURVE))
            for (auto p = parent; p.isValid(); p = p.getParent())
                if (p.hasType(te::IDs::PLUGIN))
                {
                    key = "CURVE:" + p[te::IDs::id].toString().toStdString() + ":" +
                          (desired.hasProperty(te::IDs::paramID) ? desired[te::IDs::paramID] : desired[te::IDs::name])
                              .toString()
                              .toStdString();
                    break;
                }
        auto it = nodes.find(key);
        auto target = it != nodes.end() ? it->second : juce::ValueTree(desired.getType());
        patch(target, desired);
        index(target);
        return target;
    };
    for (const auto& [id, parameters] : curves)
        if (auto* plugin = findPlugin(*owner.edit, id))
            for (const auto& name : parameters)
                if (auto parameter = plugin->getAutomatableParameterByID(name))
                {
                    auto& curve = parameter->getCurve();
                    nodes["CURVE:" + id + ":" + name.toStdString()] = curve.state;
                    // Point deltas require existing points; only removing the whole curve clears the retained SDK node.
                    for (auto change : delta)
                        for (auto removal : change)
                            if (removal.hasType(forward ? "REMOVE" : "ADD"))
                                if (auto removed = removal.getChild(0);
                                    removed.hasType(te::IDs::AUTOMATIONCURVE) &&
                                    (removed.hasProperty(te::IDs::paramID) ? removed[te::IDs::paramID]
                                                                           : removed[te::IDs::name])
                                            .toString() == name &&
                                    history_delta::resolve(observed,
                                                           Json::parse(change["path"].toString().toStdString())) ==
                                        plugin->state)
                                    curve.clear(nullptr);
                }
    history_delta::apply(observed, delta, forward, reconcile);
    for (const auto& id : opaque)
        if (auto* plugin = findPlugin(*owner.edit, id))
        {
            plugin->restorePluginStateFromValueTree(plugin->state.createCopy());
            if (auto* external = dynamic_cast<te::ExternalPlugin*>(plugin))
                owner.synchroniseExternalParameters(external);
        }
    juce::ValueTree values("EDIT"), info("NATIVEDAW"), selected("PARAMETER_BASES");
    for (auto base : owner.metadata.getChildWithName("PARAMETER_BASES"))
        if (baseKeys.contains(identity(base)))
            selected.addChild(base.createCopy(), -1, nullptr);
    info.addChild(selected, -1, nullptr);
    values.addChild(info, -1, nullptr);
    applyBases(values, true);
    for (const auto& [id, parameters] : curves)
        if (auto* plugin = findPlugin(*owner.edit, id))
            for (const auto& name : parameters)
                if (auto parameter = plugin->getAutomatableParameterByID(name))
                    parameter->updateStream();
    if (owner.nativeStates)
        owner.nativeStates->sync(true, opaque);
    // These are the only native instances whose bases can have changed.
    dirtyPlugins.insert(affected.begin(), affected.end());
    for (const auto& key : baseKeys)
    {
        const auto end = key.find(':', 5);
        if (end != std::string::npos)
            dirtyPlugins.insert(key.substr(5, end - 5));
    }
    // The mirror already has the current boundary; move only this delta in it.
    history_delta::apply(mirror, delta, forward);
    journalApplied = true;
    dirty.clear();
}
void PersistentHistory::applyBases(const juce::ValueTree& image, bool changedExternal)
{
    Commands::ParameterWriteGuard guard(owner);
    std::map<juce::String, te::Plugin*> plugins;
    for (auto* p : te::getAllPlugins(*owner.edit, true))
        plugins[p->itemID.toString()] = p;
    for (auto base : image.getChildWithName("NATIVEDAW").getChildWithName("PARAMETER_BASES"))
    {
        auto found = plugins.find(base["plugin"].toString());
        if (found == plugins.end())
            continue;
        auto* plugin = found->second;
        if (auto* ext = dynamic_cast<te::ExternalPlugin*>(plugin); ext && !ext->getAudioPluginInstance())
            continue;
        auto parameter = plugin->getAutomatableParameterByID(base["parameter"].toString());
        require(parameter != nullptr, "saved history parameter is unavailable in this plugin version");
        if (!changedExternal && dynamic_cast<te::ExternalPlugin*>(plugin) && parameter->getCurve().getNumPoints() == 0)
            continue;
        const double value = double(base["value"]);
        require(std::isfinite(value) && value >= parameter->valueRange.start && value <= parameter->valueRange.end,
                "saved history base parameter is outside the native range");
        parameter->updateFromAttachedValue();
        parameter->updateStream();
        if (parameter->getCurrentExplicitValue() != float(value))
            parameter->setParameter(float(value), juce::dontSendNotification);
        if (auto* ext = dynamic_cast<te::ExternalPlugin*>(plugin))
            if (auto* instance = ext->getAudioPluginInstance())
                for (auto* actual : instance->getParameters())
                {
                    auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(actual);
                    if ((identified ? identified->paramID : juce::String(actual->getParameterIndex())) ==
                        parameter->paramID)
                    {
                        // The explicit base and currently rendered value are
                        // distinct when automation is active. Restoring only
                        // the base into DSP leaves its first curve plateau at
                        // odds with the engine cache, which suppresses an equal
                        // first automation value. Keep the native endpoint in
                        // agreement with the current effective readout instead.
                        const auto applied = parameter->getCurrentValue();
                        if (actual->getValue() != applied)
                            actual->setValue(applied);
                        break;
                    }
                }
    }
}
void PersistentHistory::rebuildUndo()
{
    auto& manager = owner.edit->getUndoManager();
    manager.clearUndoHistory();
    manager.setMaxNumberOfStoredUnits(100000000, int(owner.history.size()) + 100);
    std::vector<Action*> actions;
    for (size_t i = 0; i < owner.history.size(); ++i)
    {
        const auto& id = owner.history[i];
        auto record = transaction(steps[i].delta, id);
        require(record.isValid(), "persistent transaction description missing");
        owner.historyRecords[id] = record;
        manager.beginNewTransaction(record["actor"].toString() + ":" + juce::String(id));
        auto* action = new Action(*this, steps[i].delta);
        actions.push_back(action);
        require(manager.perform(action), "persistent Undo setup failed");
    }
    for (size_t i = owner.history.size(); i > owner.historyCursor; --i)
        require(manager.undo(), "persistent Redo setup failed");
    for (auto* action : actions)
        action->active = true;
}
void PersistentHistory::install(Archive archive)
{
    if (!archive.current.isValid())
        return;
    mirror = std::move(archive.current);
    bases = mirror.getChildWithName("NATIVEDAW").getChildWithName("PARAMETER_BASES").createCopy();
    baseNodes.clear();
    for (auto base : bases)
        baseNodes[identity(base)] = base;
    owner.history = std::move(archive.ids);
    owner.historyCursor = archive.cursor;
    discarded = archive.discarded;
    owner.historyRecords.clear();
    steps.clear();
    blocks.clear();
    for (auto delta : archive.deltas)
        steps.push_back(encode(std::move(delta)));
    dirty.clear();
    dirtyPlugins.clear();
    nodes.clear();
    index(observed);
    applyBases(mirror);
    owner.synchroniseExternalParameters();
    if (owner.nativeStates)
        owner.nativeStates->sync(true);
    trim();
    dirty.clear();
    dirtyPlugins.clear();
    // Restoring native parameter bases may use the SDK UndoManager. Install
    // the saved stack last so those owned initialization writes cannot append
    // an unnamed transaction or discard a saved Redo branch.
    rebuildUndo();
}
void Commands::checkpointHistory()
{
    if (persistentHistory)
        persistentHistory->checkpoint();
}
void Commands::recordHistory(const std::string& id)
{
    history.resize(historyCursor);
    history.push_back(id);
    ++historyCursor;
    if (persistentHistory)
        persistentHistory->appended();
}
} // namespace ndaw::v2
