#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
struct Law
{
    const char* id;
    const char* label;
    te::PanLaw native;
};
constexpr Law laws[] = {{"default", "工程默认", te::PanLawDefault},
                        {"linear", "Linear · 线性", te::PanLawLinear},
                        {"center_2.5db", "中心 −2.5 dB", te::PanLaw2point5dBCenter},
                        {"center_3db", "中心 −3 dB", te::PanLaw3dBCenter},
                        {"center_4.5db", "中心 −4.5 dB", te::PanLaw4point5dBCenter},
                        {"center_6db", "中心 −6 dB", te::PanLaw6dBCenter}};
std::string name(te::PanLaw value)
{
    for (const auto& law : laws)
        if (law.native == value)
            return law.id;
    return "unknown";
}
te::PanLaw native(const std::string& value)
{
    for (const auto& law : laws)
        if (law.id == value)
            return law.native;
    throw std::runtime_error("unsupported Pan Law");
}
float effectivePan(double value)
{
    require(std::isfinite(value) && value >= -1 && value <= 1, "pan outside -1..1");
    const auto p = float(value);
    return p >= -.005f && p <= .005f ? 0.f : p;
}
struct PanAction final : juce::UndoableAction
{
    PanAction(te::AudioTrack& t, float value)
        : edit(t.edit), id(t.itemID), before(t.getVolumePlugin()->panParam->getCurrentExplicitValue()), after(value)
    {
    }
    bool perform() override
    {
        return set(after);
    }
    bool undo() override
    {
        return set(before);
    }
    int getSizeInUnits() override
    {
        return 1;
    }
    bool set(float value)
    {
        auto* t = dynamic_cast<te::AudioTrack*>(te::findTrackForID(edit, id));
        if (!t)
            return false;
        t->getVolumePlugin()->setPan(value);
        return true;
    }
    te::Edit& edit;
    te::EditItemID id;
    float before, after;
};
} // namespace
Json Commands::panLawCatalog()
{
    auto result = Json::array();
    for (const auto& law : laws)
        result.push_back({{"id", law.id}, {"label", law.label}});
    return result;
}
void Commands::registerPanCommands(Json& registry)
{
    registry.push_back(
        {{"id", "track.pan"},
         {"schema",
          {{"type", "object"},
           {"properties",
            {{"track", {{"type", "string"}}}, {"value", {{"type", "number"}, {"minimum", -1}, {"maximum", 1}}}}},
           {"required", {"track", "value"}},
           {"additionalProperties", false}}},
         {"units",
          {{"value",
            "native audio Pan/Balance: -1 left, 0 centre, +1 right; [-0.005,0.005] snaps to zero; not MIDI CC10"}}},
         {"permission", "edit"},
         {"risk", "low"},
         {"reversible", true},
         {"live", true},
         {"test", "M1-PAN-01"}});
    auto choices = Json::array();
    for (const auto& law : laws)
        choices.push_back(law.id);
    registry.push_back(
        {{"id", "track.pan_law"},
         {"schema",
          {{"type", "object"},
           {"properties", {{"track", {{"type", "string"}}}, {"law", {{"type", "string"}, {"enum", choices}}}}},
           {"required", {"track", "law"}},
           {"additionalProperties", false}}},
         {"units",
          {{"law", "SDK channel gain law; linear endpoints are +6.02 dB relative to centre, non-linear endpoints "
                   "unity; stopped transport only"}}},
         {"permission", "edit"},
         {"risk", "medium"},
         {"reversible", true},
         {"live", false},
         {"test", "M1-PAN-01"}});
}
Json Commands::panQuery(te::AudioTrack& t) const
{
    const auto* p = t.getVolumePlugin();
    return {{"pan", p->getPan()},
            {"base_pan", p->panParam->getCurrentExplicitValue()},
            {"pan_law", int(p->getPanLaw())},
            {"pan_law_setting", name(te::PanLaw(p->panLaw.get()))},
            {"pan_law_effective", name(p->getPanLaw())},
            {"automation_pan_points", p->panParam->getCurve().getNumPoints()},
            {"pan_semantics", "audio_channel_gains; no stereo channel exchange or MIDI CC10"}};
}
Json Commands::validatePanPlan(const Json& ops) const
{
    bool relevant = false;
    for (const auto& op : ops)
        relevant |= op.at("command") == "track.pan" || op.at("command") == "track.pan_law";
    if (!relevant)
        return Json::array();
    struct State
    {
        std::string type;
        float value;
        std::string law;
    };
    std::map<std::string, State> states;
    for (auto* t : te::getAllTracks(*edit))
        if (auto* audio = dynamic_cast<te::AudioTrack*>(t))
        {
            auto q = panQuery(*audio);
            states[t->itemID.toString().toStdString()] = {trackType(*audio), q["base_pan"], q["pan_law_setting"]};
        }
        else if (dynamic_cast<te::FolderTrack*>(t))
            states[t->itemID.toString().toStdString()] = {"folder", 0, "default"};
    Json changes = Json::array();
    size_t index = 0;
    auto facts = [](const State& state)
    {
        const auto value = native(state.law);
        return Json{{"value", state.value},
                    {"law", state.law},
                    {"effective_law", name(value == te::PanLawDefault ? te::getDefaultPanLaw() : value)}};
    };
    for (const auto& op : ops)
    {
        const std::string cmd = op.at("command");
        const auto& a = op.at("args");
        if (cmd == "track.create")
            states[a.at("ref")] = {a.value("type", std::string("audio")), 0, "linear"};
        if (cmd != "track.pan" && cmd != "track.pan_law")
        {
            ++index;
            continue;
        }
        const std::string target = a.at("track");
        require(states.contains(target) && states[target].type != "folder" && states[target].type != "vca",
                "track has no audio Pan/Balance control");
        auto& state = states.at(target);
        const auto before = facts(state);
        if (cmd == "track.pan")
        {
            if (auto* t = track(target);
                t && edit->getTransport().isPlaying() && t->automationMode == te::AutomationMode::read)
                require(t->getVolumePlugin()->panParam->getCurve().getNumPoints() == 0,
                        "Read curve owns the playing panner; stop or use Touch/Latch/Write");
            state.value = effectivePan(a.at("value"));
        }
        else
        {
            require(!edit->getTransport().isPlaying(), "stop playback before changing Pan Law");
            state.law = a.at("law");
            native(state.law);
        }
        Json change = {{"command", cmd},
                       {"operation_index", index},
                       {"track", target},
                       {"before", before},
                       {"after", facts(state)}};
        if (cmd == "track.pan")
            change["requested_value"] = a.at("value");
        changes.push_back(std::move(change));
        ++index;
    }
    return changes;
}
void Commands::executePanOperation(const std::string& cmd, const Json& args)
{
    auto* t = track(args.at("track"));
    require(t != nullptr, "audio panner target disappeared");
    if (cmd == "track.pan")
        require(edit->getUndoManager().perform(new PanAction(*t, effectivePan(args.at("value")))),
                "native pan operation failed");
    else
        t->getVolumePlugin()->setPanLaw(native(args.at("law")));
}
} // namespace ndaw::v2
