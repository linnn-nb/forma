// SPDX-License-Identifier: AGPL-3.0-only
#include "PersistentHistory.h"
#include "NativeEditXml.h"
#include "NativePluginStates.h"
#include <juce_cryptography/juce_cryptography.h>

namespace ndaw::v2
{
namespace
{
constexpr int maximumSteps = 2048;
constexpr size_t maximumBytes = 256 * 1024 * 1024;
void require(bool value, const char* reason)
{
    if (!value)
        throw std::runtime_error(reason);
}
std::string identity(const juce::ValueTree& n)
{
    for (const char* property : {"id", "ndaw_id"})
        if (n.hasProperty(property))
            return n.getType().toString().toStdString() + ":" + property + ":" + n[property].toString().toStdString();
    if (n.hasType(te::IDs::AUTOMATIONCURVE))
    {
        const auto parameter = n.hasProperty(te::IDs::paramID) ? n[te::IDs::paramID] : n[te::IDs::name];
        if (parameter.toString().isNotEmpty())
            for (auto parent = n.getParent(); parent.isValid(); parent = parent.getParent())
                if (parent.hasProperty(te::IDs::id))
                    return "CURVE:" + parent[te::IDs::id].toString().toStdString() + ":" +
                           parameter.toString().toStdString();
    }
    return {};
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
std::string digest(const juce::ValueTree& state)
{
    auto xml = preciseEditXml(state);
    require(bool(xml), "history XML could not be serialized");
    const auto data = xml->toString().toStdString();
    return juce::SHA256(data.data(), data.size()).toHexString().toStdString();
}
} // namespace
struct PersistentHistory::Action final : juce::UndoableAction
{
    Action(PersistentHistory& service, juce::ValueTree before, juce::ValueTree after)
        : service(service), before(std::move(before)), after(std::move(after))
    {
    }
    bool perform() override
    {
        if (active)
            service.restore(after);
        return true;
    }
    bool undo() override
    {
        if (active)
            service.restore(before);
        return true;
    }
    int getSizeInUnits() override
    {
        return 1;
    }
    PersistentHistory& service;
    juce::ValueTree before, after;
    bool active = false;
};
PersistentHistory::PersistentHistory(Commands& c) : owner(c)
{
    states.push_back(capture());
}
juce::ValueTree PersistentHistory::capture(bool saved)
{
    Commands::ParameterWriteGuard guard(owner);
    juce::ValueTree bases("PARAMETER_BASES");
    std::map<juce::String, juce::String> sources;
    for (auto* plugin : te::getAllPlugins(*owner.edit, true))
        for (auto* parameter : plugin->getAutomatableParameters())
        {
            juce::ValueTree value("BASE");
            value.setProperty("plugin", plugin->itemID.toString(), nullptr);
            value.setProperty("parameter", parameter->paramID, nullptr);
            value.setProperty("value", double(parameter->getCurrentExplicitValue()), nullptr);
            bases.addChild(value, -1, nullptr);
        }
    auto copy = withoutHistory(owner.edit->state);
    if (!saved && owner.nativeStates)
        owner.nativeStates->copyCheckpoints(copy);
    auto info = copy.getChildWithName("NATIVEDAW");
    info.removeChild(info.getChildWithName("PARAMETER_BASES"), nullptr);
    info.addChild(bases, -1, nullptr);
    // History is independent of the directory in which a Save Copy is reopened.
    for (auto* track : te::getAudioTracks(*owner.edit))
        for (auto* clip : track->getClips())
            if (auto* wave = dynamic_cast<te::WaveAudioClip*>(clip))
                if (const auto original = wave->getOriginalFile(); original != juce::File{})
                    sources[wave->itemID.toString()] = original.getFullPathName();
    auto map = [&](auto&& self, juce::ValueTree node) -> void
    {
        if (auto source = sources.find(node[te::IDs::id].toString());
            source != sources.end() && node.hasProperty(te::IDs::source))
            node.setProperty(te::IDs::source, source->second, nullptr);
        for (auto child : node)
            self(self, child);
    };
    map(map, copy);
    return copy;
}
void PersistentHistory::checkpoint()
{
    require(states.size() == owner.history.size() + 1, "persistent history boundary is missing");
    states.at(owner.historyCursor) = capture();
}
void PersistentHistory::appended()
{
    states.resize(owner.historyCursor);
    states.push_back(capture());
}
juce::ValueTree PersistentHistory::save()
{
    require(states.size() == owner.history.size() + 1, "persistent history boundary is missing");
    states.at(owner.historyCursor) = capture(true);
    require(owner.history.size() <= maximumSteps, "history exceeds 2048 transactions; save a recovery snapshot first");
    juce::ValueTree archive("PERSISTENT_HISTORY");
    archive.setProperty("schema", 1, nullptr);
    archive.setProperty("cursor", int(owner.historyCursor), nullptr);
    archive.setProperty("current_hash", juce::String(digest(states.at(owner.historyCursor))), nullptr);
    size_t total = 0;
    for (size_t i = 0; i < states.size(); ++i)
    {
        juce::ValueTree step("STATE");
        if (i > 0)
            step.setProperty("plan_id", juce::String(owner.history.at(i - 1)), nullptr);
        auto document = preciseEditXml(states[i]);
        const auto data = document->toString();
        total += size_t(data.getNumBytesAsUTF8());
        require(total <= maximumBytes, "persistent history exceeds 256 MiB; save a recovery snapshot first");
        step.setProperty("xml", data, nullptr);
        step.setProperty("sha256", juce::String(digest(states[i])), nullptr);
        archive.addChild(step, -1, nullptr);
    }
    auto saved = states.at(owner.historyCursor).createCopy();
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
    auto meta = current.getChildWithName("NATIVEDAW");
    auto archive = meta.getChildWithName("PERSISTENT_HISTORY");
    if (!archive.isValid())
        return result; // Legacy documents retain their facts, with no invented history.
    require(int(archive["schema"]) == 1 && archive.getNumChildren() >= 1 &&
                archive.getNumChildren() <= maximumSteps + 1,
            "invalid persistent history schema or capacity");
    const int cursor = int(archive["cursor"]);
    require(cursor >= 0 && cursor < archive.getNumChildren(), "invalid persistent history cursor");
    result.cursor = size_t(cursor);
    size_t total = 0;
    std::set<std::string> ids;
    for (int i = 0; i < archive.getNumChildren(); ++i)
    {
        const auto step = archive.getChild(i);
        require(step.hasType("STATE"), "invalid persistent history entry");
        auto data = step["xml"].toString();
        total += size_t(data.getNumBytesAsUTF8());
        require(total <= maximumBytes, "persistent history exceeds 256 MiB");
        auto xml = juce::XmlDocument::parse(data);
        require(xml && xml->hasTagName("EDIT"), "invalid persistent history state");
        auto state = juce::ValueTree::fromXml(*xml);
        require(!state.getChildWithName("NATIVEDAW").getChildWithName("PERSISTENT_HISTORY").isValid(),
                "recursive persistent history is not allowed");
        require(digest(state) == step["sha256"].toString().toStdString(), "persistent history checksum mismatch");
        if (i > 0)
        {
            const auto id = step["plan_id"].toString().toStdString();
            require(!id.empty() && id.size() <= 128 && ids.insert(id).second, "invalid persistent transaction ID");
            auto record = state.getChildWithName("NATIVEDAW").getChildWithProperty("plan_id", juce::String(id));
            require(record.hasType("TRANSACTION"), "persistent transaction description missing");
            result.ids.push_back(id);
        }
        auto highWater = [&](auto&& self, const juce::ValueTree& node) -> void
        {
            result.highestID = std::max(result.highestID, te::EditItemID::fromProperty(node, te::IDs::id).getRawID());
            for (auto child : node)
                self(self, child);
        };
        highWater(highWater, state);
        result.states.push_back(std::move(state));
    }
    require(digest(result.states.at(result.cursor)) == archive["current_hash"].toString().toStdString(),
            "persistent history current-state checksum mismatch");
    // The actual saved Edit is authoritative; an unrelated journal cannot be applied.
    auto facts = withoutHistory(current);
    auto resolveSources = [&](auto&& self, juce::ValueTree node) -> void
    {
        if (node.hasType(te::IDs::AUDIOCLIP))
        {
            const auto source = node[te::IDs::source].toString();
            if (source.isNotEmpty() && !juce::File::isAbsolutePath(source))
                node.setProperty(te::IDs::source, file.getParentDirectory().getChildFile(source).getFullPathName(),
                                 nullptr);
        }
        for (auto child : node)
            self(self, child);
    };
    resolveSources(resolveSources, facts);
    // Save snapshots use resolved absolute sources. The saved Edit must use the same
    // source representation, installed by save() before serializing this archive.
    require(digest(facts) == digest(result.states.at(result.cursor)), "saved Edit and history disagree");
    return result;
}
void PersistentHistory::index(juce::ValueTree node)
{
    if (auto key = identity(node); !key.empty())
        nodes[key] = node;
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
void PersistentHistory::restore(const juce::ValueTree& image)
{
    owner.checkThread();
    Commands::ParameterWriteGuard guard(owner);
    const auto previous = capture();
    auto apply = [&](const juce::ValueTree& snapshot)
    {
        auto desired = snapshot.createCopy();
        auto info = desired.getChildWithName("NATIVEDAW");
        info.setProperty("revision", juce::int64(owner.revision), nullptr);
        // UI state and request audit are current non-edit state. History never grants
        // credentials, current-run receipts or permission to replay an Agent request.
        for (const auto type : {juce::Identifier("UI"), juce::Identifier("REQUEST_AUDIT")})
        {
            for (int i = info.getNumChildren(); --i >= 0;)
                if (info.getChild(i).hasType(type))
                    info.removeChild(i, nullptr);
            for (auto current : owner.metadata)
                if (current.hasType(type))
                    info.addChild(current.createCopy(), -1, nullptr);
        }
        owner.closePluginEditors(true);
        if (owner.nativeStates)
            owner.nativeStates->reset();
        index(owner.edit->state);
        // A native AutomationCurve retains its detached node when the parent removes
        // the final curve. Empty its points before reconciliation so Undo cannot leave
        // an invisible, still-running curve in the SDK parameter object.
        for (auto* plugin : te::getAllPlugins(*owner.edit, true))
            for (auto* parameter : plugin->getAutomatableParameters())
            {
                auto& curve = parameter->getCurve();
                curve.clear(nullptr);
                // PluginCache may have retired the old instance while the user
                // paused in history. A new parameter then owns a different empty,
                // detached curve. Reconcile into that live node, not the archive's
                // old node: the SDK curve source and its listeners retain it.
                nodes["CURVE:" + plugin->itemID.toString().toStdString() + ":" + parameter->paramID.toStdString()] =
                    curve.state;
            }
        patch(owner.edit->state, desired);
        for (auto* plugin : te::getAllPlugins(*owner.edit, true))
        {
            plugin->restorePluginStateFromValueTree(plugin->state.createCopy());
            for (auto* parameter : plugin->getAutomatableParameters())
                parameter->updateFromAttachedValue();
        }
        applyBases(snapshot);
        // Adopt actual native values while this lifecycle change is owned. Queued
        // SDK echoes then compare equal instead of becoming a phantom human edit.
        owner.synchroniseExternalParameters();
    };
    try
    {
        apply(image);
    }
    catch (...)
    {
        // Throwing keeps the JUCE cursor unchanged. Restore the previous complete
        // state before reporting failure, instead of leaving a partial history move.
        const auto failure = std::current_exception();
        apply(previous);
        if (owner.nativeStates)
            owner.nativeStates->sync(true);
        std::rethrow_exception(failure);
    }
}
void PersistentHistory::applyBases(const juce::ValueTree& image)
{
    Commands::ParameterWriteGuard guard(owner);
    for (auto base : image.getChildWithName("NATIVEDAW").getChildWithName("PARAMETER_BASES"))
    {
        te::Plugin* plugin = nullptr;
        for (auto* candidate : te::getAllPlugins(*owner.edit, true))
            if (candidate->itemID.toString() == base["plugin"].toString())
            {
                plugin = candidate;
                break;
            }
        if (!plugin)
            continue; // Missing plugin state remains in its original native placeholder.
        if (auto* external = dynamic_cast<te::ExternalPlugin*>(plugin); external && !external->getAudioPluginInstance())
            continue;
        auto parameter = plugin->getAutomatableParameterByID(base["parameter"].toString());
        require(parameter != nullptr, "saved history parameter is unavailable in this plugin version");
        // The opaque native state owns unautomated external parameters. Rewriting
        // every equal VST3 parameter can mark its preset dirty or alter private state.
        if (dynamic_cast<te::ExternalPlugin*>(plugin) && parameter->getCurve().getNumPoints() == 0)
            continue;
        const double value = double(base["value"]);
        require(std::isfinite(value) && value >= parameter->valueRange.start && value <= parameter->valueRange.end,
                "saved history base parameter is outside the native range");
        parameter->updateStream();
        if (parameter->getCurrentExplicitValue() != float(value))
            parameter->setParameter(float(value), juce::dontSendNotification);
        if (auto* ext = dynamic_cast<te::ExternalPlugin*>(plugin))
            if (auto* instance = ext->getAudioPluginInstance())
                for (int i = 0; i < instance->getParameters().size(); ++i)
                {
                    auto* actual = instance->getParameters()[i];
                    auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(actual);
                    if ((identified ? identified->paramID : juce::String(i)) == parameter->paramID)
                    {
                        if (actual->getValue() != float(value))
                            actual->setValue(float(value));
                        break;
                    }
                }
    }
}
void PersistentHistory::install(Archive archive)
{
    if (archive.states.empty())
        return;
    states = std::move(archive.states);
    owner.history = std::move(archive.ids);
    owner.historyCursor = archive.cursor;
    owner.historyRecords.clear();
    index(owner.edit->state);
    auto& manager = owner.edit->getUndoManager();
    manager.setMaxNumberOfStoredUnits(100000000, int(owner.history.size()) + 100);
    std::vector<Action*> actions;
    for (size_t i = 0; i < owner.history.size(); ++i)
    {
        const auto id = owner.history[i];
        auto record = states[i + 1].getChildWithName("NATIVEDAW").getChildWithProperty("plan_id", juce::String(id));
        owner.historyRecords[id] = record;
        manager.beginNewTransaction(record["actor"].toString() + ":" + juce::String(id));
        auto* action = new Action(*this, states[i], states[i + 1]);
        actions.push_back(action);
        require(manager.perform(action), "persistent Undo setup failed");
    }
    for (size_t i = owner.history.size(); i > owner.historyCursor; --i)
        require(manager.undo(), "persistent Redo setup failed");
    for (auto* action : actions)
        action->active = true;
    applyBases(states.at(owner.historyCursor));
    owner.synchroniseExternalParameters();
    if (owner.nativeStates)
        owner.nativeStates->sync(true);
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
