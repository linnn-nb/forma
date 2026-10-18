// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#if JUCE_MAC
#include <AudioUnit/AudioUnit.h>
#endif
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
} // namespace
Json Commands::refreshPluginInventory(const juce::File& directory)
{
    checkThread();
    auto root = directory == juce::File{} ? defaultPluginCatalogDirectory()
                                          : fs::path(directory.getFullPathName().toStdString());
    auto inventory = readPluginCatalog(root);
    require(inventory["entries"].size() <= 2048, "plugin catalog entry budget exceeded");
    auto& list = engine.getPluginManager().knownPluginList;
    list.clear();
    Json entries = Json::array();
    for (const auto& [_, e] : inventory["entries"].items())
    {
        if (e["status"] != "verified" || e.value("blacklisted", false))
            continue;
        require(e.contains("plugins") && e["plugins"].size() <= 64, "invalid plugin descriptor catalog");
        for (const auto& p : e["plugins"])
        {
            juce::PluginDescription desc;
            auto xml = juce::parseXML(juce::String(p["description_xml"].get<std::string>()));
            require(xml && desc.loadFromXml(*xml), "invalid scanned SDK descriptor");
            require((desc.pluginFormatName == "VST3" || desc.pluginFormatName == "AudioUnit") &&
                        digest(desc.pluginFormatName.toStdString() + ":" +
                               desc.createIdentifierString().toStdString()) == p["id"].get<std::string>(),
                    "scanned descriptor identity mismatch");
            list.addType(desc);
            auto item = p;
            try
            {
                item["module_available"] = pluginModuleFingerprint(e["fingerprint"]["format"],
                                                                   e["fingerprint"]["candidate"]) == e["fingerprint"];
            }
            catch (const std::exception&)
            {
                item["module_available"] = false;
            }
            item["module_fingerprint"] = e["fingerprint"];
            item["module_hash"] = e["fingerprint"]["digest"];
            entries.push_back(item);
        }
    }
    externalInventory = {{"directory", root.string()}, {"revision", inventory["revision"]}, {"plugins", entries}};
    processorCache.clear();
    invalidateFacts();
    return externalInventory;
}
Json Commands::pluginInventory() const
{
    checkThread();
    return externalInventory;
}
Json Commands::externalDescriptor(const std::string& id) const
{
    const auto entries = externalInventory.find("plugins");
    if (entries != externalInventory.end())
        for (const auto& p : *entries)
            if (p["id"] == id)
            {
                const auto& f = p["module_fingerprint"];
                require(pluginModuleFingerprint(f["format"], f["candidate"]) == f,
                        "plugin module changed or missing; rescan before loading");
                return p;
            }
    throw std::runtime_error("scanned plugin descriptor unavailable; scan or rescan first");
}
bool Commands::mayLoadExternal(te::ExternalPlugin& plugin) const
{
    // This gate runs on the message thread before the SDK creates a native instance.
    // Missing/blacklisted/stale plugins retain their original ValueTree and blob.
    try
    {
        auto id = plugin.state.getProperty("ndaw_external_descriptor").toString().toStdString();
        if (id.empty())
            return false;
        const auto entries = externalInventory.find("plugins");
        if (entries == externalInventory.end())
            return false;
        const Json* p = nullptr;
        // The catalog contains full scan results. Borrow it while checking an
        // instance instead of copying every descriptor on every GUI refresh.
        for (const auto& item : *entries)
            if (item["id"] == id)
                p = &item;
        if (p == nullptr || !p->value("module_available", false))
            return false;
        return (*p)["module_hash"] == plugin.state.getProperty("ndaw_external_module_hash").toString().toStdString() &&
               (*p)["file_or_identifier"] == plugin.desc.fileOrIdentifier.toStdString();
    }
    catch (const std::exception&)
    {
        return false;
    }
}
void Commands::synchroniseExternalParameters(te::ExternalPlugin* only)
{
    checkThread();
    ParameterWriteGuard guard(*this);
    bool changed = false;
    // Native preparation can change normalized values (e.g. AU frequency ranges
    // follow Nyquist). Reconcile owned lifecycle changes before queued SDK echoes;
    // never treat them as user gestures or reset an existing automation base.
    for (auto* p : te::getAllPlugins(*edit, true))
        if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p); ext && (!only || ext == only))
            if (auto* instance = ext->getAudioPluginInstance())
            {
                const auto key = ext->itemID.toString().toStdString();
                const auto previous = externalPreparedRates.find(key);
                const auto rate = instance->getSampleRate();
                const bool mappingChanged = externalLayoutChanged(*ext);
                changed |= previous == externalPreparedRates.end() || previous->second != rate || mappingChanged;
                externalPreparedRates[key] = rate;
                for (auto* parameter : ext->getAutomatableParameters())
                {
                    auto id = parameter->paramID;
                    int index = id.getIntValue();
                    if (id != juce::String(index) || index < 0 || index >= instance->getParameters().size() ||
                        parameter->getCurve().getNumPoints() != 0)
                        continue;
                    const auto value = instance->getParameters()[index]->getValue();
                    changed |= parameter->getCurrentExplicitValue() != value;
                    parameter->setParameter(value, juce::dontSendNotification);
                }
            }
    // A quiet VST3 can change rate without emitting a parameter notification.
    // Publish the owned preparation now; never discard its first later human
    // change as a delayed rate echo or leave query facts on the old AU mapping.
    if (changed)
    {
        if (audioConfigurationPending())
            invalidateFacts();
        else
            bumpRevision();
    }
}
bool Commands::externalLayoutChanged(te::ExternalPlugin& ext)
{
    checkThread();
    auto* instance = ext.getAudioPluginInstance();
    if (!instance || ext.desc.pluginFormatName != "AudioUnit")
        return false;
    auto id = ext.itemID.toString().toStdString();
    std::vector<juce::AudioProcessorParameter*> identities;
    for (auto* p : instance->getParameters())
        identities.push_back(p);
    auto previous = externalParameterLayouts.find(id);
    if (previous != externalParameterLayouts.end() && previous->second.identities == identities)
        return false;
    // AU publishes new parameter objects when the native metadata changes. Read
    // actual endpoint/unit descriptions once per replacement, not on each gesture.
    // A rate-dependent mapping change is host preparation, not a human knob edit.
    Json descriptions = Json::array();
    for (auto* p : identities)
    {
        auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*>(p);
        descriptions.push_back(
            {hosted ? hosted->getParameterID().toStdString() : std::to_string(p->getParameterIndex()),
             p->getLabel().toStdString(), p->getNumSteps(), p->getText(0, 256).toStdString(),
             p->getText(1, 256).toStdString()});
    }
    const auto signature = descriptions.dump();
    const bool changed = previous != externalParameterLayouts.end() && previous->second.signature != signature;
    externalParameterLayouts[id] = {std::move(identities), signature};
    return changed;
}
bool Commands::reconcileExternalPreparation(te::AutomatableParameter& parameter)
{
    auto* ext = dynamic_cast<te::ExternalPlugin*>(parameter.getPlugin());
    if (!ext || !ext->getAudioPluginInstance())
        return false;
    auto id = ext->itemID.toString().toStdString();
    auto rate = ext->getAudioPluginInstance()->getSampleRate();
    auto previous = externalPreparedRates.find(id);
    const bool mappingChanged = externalLayoutChanged(*ext);
    if (previous != externalPreparedRates.end() && previous->second == rate && !mappingChanged)
        return false;
    externalPreparedRates[id] = rate;
    const auto before = revision;
    synchroniseExternalParameters(ext);
    if (revision == before && !audioConfigurationPending())
        bumpRevision();
    return true;
}
void Commands::validateExternalRuntime() const
{
    checkThread();
    for (auto* p : te::getAllPlugins(*edit, true))
        if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p);
            ext && ext->isEnabled() && !ext->isFrozen() && !bool(ext->state.getProperty("ndaw_print_baked", false)))
        {
            externalDescriptor(ext->state.getProperty("ndaw_external_descriptor").toString().toStdString());
            require(mayLoadExternal(*ext),
                    "external plugin unavailable or changed; bypass explicitly or restore/rescan its original module");
            require(!ext->isInitialisingAsync() && ext->getAudioPluginInstance() != nullptr,
                    "external plugin has not loaded; no audio success claim");
#if JUCE_MAC
            // AU property events arrive later on the message loop. An immediate
            // render after a native non-automatable edit must not construct PDC
            // from JUCE's old cached latency while the AU already delays by more.
            // Refresh only processor metadata from the actual AU, outside RT;
            // ExternalPlugin::initialise reads it when preparing the next graph.
            auto& native = *ext->getAudioPluginInstance();
            if (auto* client = native.getAudioUnitClient())
            {
                Float64 seconds = 0;
                UInt32 size = sizeof(seconds);
                const auto status = AudioUnitGetProperty(client->getAudioUnitHandle(), kAudioUnitProperty_Latency,
                                                         kAudioUnitScope_Global, 0, &seconds, &size);
                const auto rate = native.getSampleRate();
                require(status == noErr && size == sizeof(seconds) && std::isfinite(seconds) && seconds >= 0 &&
                            std::isfinite(rate) && rate > 0 && seconds * rate <= std::numeric_limits<int>::max(),
                        "AU latency query failed or invalid; audio graph not qualified for execution");
                native.setLatencySamples(juce::roundToInt(seconds * rate));
            }
#endif
        }
}
} // namespace ndaw::v2
