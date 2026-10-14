#include <nativedaw/v2/EngineCommands.h>
#include "NativeAudioFades.h"
#include "WarpState.h"
#include "ElasticState.h"
#include "ClipFxWindows.h"
#include "ProcessedClipTrim.h"
#include "AudioClipboardState.h"
#include "ClipReferenceState.h"
#include "PlaylistLinks.h"
namespace ndaw::v2
{
namespace
{
void require(bool b, const char* s)
{
    if (!b)
        throw std::runtime_error(s);
}
constexpr double rate = 48000;
// Exact sample grid, independent of the source file's PCM sample rate.
int64_t sample(double t)
{
    return std::llround(t * rate);
}
auto pos(int64_t n)
{
    return tracktion::TimePosition::fromSeconds(n / rate);
}
auto dur(int64_t n)
{
    return tracktion::TimeDuration::fromSeconds(n / rate);
}
std::string curveName(te::AudioFadeCurve::Type t)
{
    switch (t)
    {
    case te::AudioFadeCurve::convex:
        return "convex";
    case te::AudioFadeCurve::concave:
        return "concave";
    case te::AudioFadeCurve::sCurve:
        return "s_curve";
    default:
        return "linear";
    }
}
te::AudioFadeCurve::Type curveType(const std::string& s)
{
    if (s == "convex")
        return te::AudioFadeCurve::convex;
    if (s == "concave")
        return te::AudioFadeCurve::concave;
    if (s == "s_curve")
        return te::AudioFadeCurve::sCurve;
    require(s == "linear", "unsupported fade curve");
    return te::AudioFadeCurve::linear;
}
void fades(te::WaveAudioClip& c, int64_t in, int64_t out, const std::string& ci, const std::string& co)
{
    c.setAutoCrossfade(false);
    c.setFadeInBehaviour(te::AudioClipBase::gainFade);
    c.setFadeOutBehaviour(te::AudioClipBase::gainFade);
    c.setFadeIn(dur(0));
    c.setFadeOut(dur(0));
    c.setFadeIn(dur(in));
    c.setFadeOut(dur(out));
    c.setFadeInType(curveType(ci));
    c.setFadeOutType(curveType(co));
}
struct Model
{
    int64_t start = 0, length = 0, offset = 0, source = 0, fadeIn = 0, fadeOut = 0;
    double gain = 0;
    int64_t trimBefore = 0, trimAfter = 0;
    bool trimmable = true, splittable = true, deletable = true, copyable = true, processedTrim = false,
         fxWindow = false;
    bool locked = false, moveLocked = false, editable = true, movable = true, envelopeEditable = false,
         inheritedFades = false;
    std::string track, path, hash, referenceHash, inCurve = "linear", outCurve = "linear";
    bool sourcePresent = true;
    Json fxTypes = Json::array();
    std::vector<std::string> fxIDs;
    Json facts() const
    {
        return {{"track", track},
                {"start_samples", start},
                {"length_samples", length},
                {"source_offset_samples", offset},
                {"gain_db", gain},
                {"inherited_fades", inheritedFades},
                {"fade_in_samples", fadeIn},
                {"fade_out_samples", fadeOut},
                {"fade_in_curve", inCurve},
                {"fade_out_curve", outCurve},
                {"locked", locked},
                {"fx_types", fxTypes}};
    }
    void fitFades()
    {
        if (fadeIn + fadeOut > length)
        {
            const auto total = fadeIn + fadeOut;
            fadeIn = std::llround(double(fadeIn) * length / total);
            fadeOut = length - fadeIn;
        }
    }
};
} // namespace
void Commands::preservePasteBoundaryFades(const Json& plan)
{
    if (!plan.contains("clipboard_paste"))
        return;
    auto& um = edit->getUndoManager();
    std::set<std::string> done;
    for (const auto& op : plan["operations"])
        if (op["command"] == "clip.trim" || op["command"] == "clip.split")
        {
            const std::string id = op["args"]["clip"];
            if (done.insert(id).second)
                // Processed splitting already freezes each full-input output
                // window. Never attach output-space fades to a schema-1 Warp
                // source before its native window has been prepared.
                if (auto* c = audioClip(id); c && !processed_trim::preservesInput(c->state))
                {
                    const auto p = c->getPosition();
                    const auto frozen =
                        native_fades::fragment(c->state, p.getStart().inSeconds(), p.getEnd().inSeconds(), true);
                    for (int i = c->state.getNumChildren(); --i >= 0;)
                        if (c->state.getChild(i).hasType("NDAW_FADE_ORIGIN"))
                            c->state.removeChild(i, &um);
                    for (auto origin : frozen)
                        if (origin.hasType("NDAW_FADE_ORIGIN"))
                            c->state.addChild(origin.createCopy(), -1, &um);
                    c->setFadeIn(tracktion::TimeDuration::fromSeconds(0));
                    c->setFadeOut(tracktion::TimeDuration::fromSeconds(0));
                }
        }
}
te::WaveAudioClip* Commands::audioClip(const std::string& id) const
{
    for (auto* t : te::getAudioTracks(*edit))
        for (auto* c : t->getClips())
            if (c->itemID.toString().toStdString() == id)
                return dynamic_cast<te::WaveAudioClip*>(c);
    return nullptr;
}
Json Commands::audioClipQuery(te::WaveAudioClip& c) const
{
    auto f = c.getOriginalFile();
    te::AudioFile a(edit->engine, f);
    auto info = a.getInfo();
    auto p = c.getPosition();
    const auto warp = warpFacts(c);
    const bool editable = !c.isLooping() && !c.isGrouped() && !c.getAutoTempo() && !c.getAutoPitch() &&
                          !c.getWarpTime() && !clip_fx_window::exists(c.state) && !c.getIsReversed() &&
                          c.getPitchChange() == 0 && std::abs(c.getSpeedRatio() - 1) < 1e-9;
    const bool processed = (c.getWarpTime() && warp.value("available", false)) ||
                           elastic_state::descriptor(c.state).isValid() || clip_fx_window::exists(c.state);
    const bool movable =
        editable || (processed && !c.isLooping() && !c.isGrouped() && !c.getAutoTempo() && !c.getAutoPitch() &&
                     !c.getIsReversed() && c.getSyncType() == te::Clip::syncAbsolute && !c.effectsEnabled() &&
                     c.getFadeInBehaviour() == te::AudioClipBase::gainFade &&
                     c.getFadeOutBehaviour() == te::AudioClipBase::gainFade);
    const bool preserveInput = processed_trim::preservesInput(c.state);
    bool trimmable = editable || movable;
    if (preserveInput)
    {
        trimmable = trimmable && !c.isLooping() && !c.isGrouped() && !c.getAutoTempo() && !c.getAutoPitch() &&
                    !c.getIsReversed() && c.getSyncType() == te::Clip::syncAbsolute && !c.effectsEnabled() &&
                    c.getFadeInBehaviour() == te::AudioClipBase::gainFade &&
                    c.getFadeOutBehaviour() == te::AudioClipBase::gainFade &&
                    (!c.getWarpTime() || warp_state::outputEnvelopeOrigins(c.state));
        try
        {
            clip_fx_window::qualify(c.state);
        }
        catch (const std::exception&)
        {
            trimmable = false;
        }
    }
    const auto handles =
        preserveInput && trimmable
            ? processed_trim::handles(c.state)
            : std::pair<int64_t, int64_t>{
                  std::max(int64_t(0), int64_t(std::floor(p.getOffset().inSeconds() * rate + 1e-7))),
                  std::max(int64_t(0), int64_t(std::floor((c.getSourceLength().inSeconds() - p.getOffset().inSeconds() -
                                                           p.getLength().inSeconds()) *
                                                              rate +
                                                          1e-7)))};
    return {
        {"path", f.getFullPathName().toStdString()},
        {"source_reference", c.state[te::IDs::source].toString().toStdString()},
        {"source_present", f.existsAsFile()},
        {"warp", warp},
        {"elastic", elastic_state::facts(c.state)},
        {"source_sample_rate", info.sampleRate},
        {"source_frames", int64_t(info.lengthInSamples)},
        {"source_offset_samples", sample(p.getOffset().inSeconds())},
        {"source_offset_seconds", c.getWarpTime() && warp.value("available", false)
                                      ? warp["source_start_seconds"].get<double>()
                                      : p.getOffset().inSeconds() * c.getSpeedRatio()},
        {"playback_offset_seconds",
         elastic_state::isWindow(c.state)
             ? elastic_state::integer(elastic_state::descriptor(c.state), "window_start_samples") / 48000.
             : p.getOffset().inSeconds()},
        {"source_offset_samples_space", c.getWarpTime() ? "prepared-warp-output" : "linear-source-playback"},
        {"speed_ratio", c.getSpeedRatio()},
        {"pitch_semitones", c.getPitchChange()},
        {"stretch_algorithm", te::TimeStretcher::getNameOfMode(c.getTimeStretchMode()).toStdString()},
        {"source_mapping_available", !c.isLooping() && !c.getAutoTempo() && !c.getWarpTime() && !c.getIsReversed() &&
                                         std::isfinite(c.getSpeedRatio()) && c.getSpeedRatio() > 0},
        {"gain_db", c.getGainDB()},
        {"inherited_fades", c.state.getChildWithName("NDAW_FADE_ORIGIN").isValid()},
        {"fade_in_samples", sample(c.getFadeIn().inSeconds())},
        {"fade_out_samples", sample(c.getFadeOut().inSeconds())},
        {"fade_in_curve", curveName(c.getFadeInType())},
        {"fade_out_curve", curveName(c.getFadeOutType())},
        {"locked", bool(c.state.getProperty("ndaw_locked", false)) ||
                       (dynamic_cast<te::AudioTrack*>(c.getTrack()) &&
                        activePlaylistLocked(*static_cast<te::AudioTrack*>(c.getTrack())))},
        {"linked_comp",
         dynamic_cast<te::AudioTrack*>(c.getTrack()) &&
             !playlist_links::value(playlist_links::active(*static_cast<te::AudioTrack*>(c.getTrack())), "linked_set")
                  .empty()},
        {"move_locked", bool(c.state.getProperty("ndaw_locked", false)) ||
                            (dynamic_cast<te::AudioTrack*>(c.getTrack()) &&
                             activePlaylistLocked(*static_cast<te::AudioTrack*>(c.getTrack()), true))},
        {"plugins", processorQuery(*c.getPluginList())},
        {"clip_fx_count", c.getPluginList()->size()},
        {"offline_clip_effects", c.effectsEnabled()},
        {"legacy_media_available", bool(c.state.getProperty("ndaw_legacy_media_available", true))},
        {"legacy_id", c.state.getProperty("ndaw_legacy_id").toString().toStdString()},
        {"legacy_source_id", c.state.getProperty("ndaw_legacy_source_id").toString().toStdString()},
        {"editable_audio", editable},
        {"movable_audio", movable},
        {"deletable_audio", !c.isGrouped()},
        {"copyable_audio", trimmable && c.getSyncType() == te::Clip::syncAbsolute},
        {"trimmable_audio", trimmable},
        {"splittable_audio", trimmable && c.getFadeInBehaviour() == te::AudioClipBase::gainFade &&
                                 c.getFadeOutBehaviour() == te::AudioClipBase::gainFade},
        {"trim_preserves_processing", preserveInput},
        {"trim_handles_before_samples", handles.first},
        {"trim_handles_after_samples", handles.second},
        {"clip_fx", clip_fx_window::facts(c.state)},
        {"editable_envelope",
         editable || clip_fx_window::exists(c.state) ||
             (elastic_state::descriptor(c.state).isValid() && !c.isLooping() && !c.isGrouped() && !c.getAutoTempo() &&
              !c.getAutoPitch() && !c.getIsReversed() && !c.effectsEnabled() &&
              c.getFadeInBehaviour() == te::AudioClipBase::gainFade &&
              c.getFadeOutBehaviour() == te::AudioClipBase::gainFade) ||
             (c.getWarpTime() && warp.value("available", false) && !c.isLooping() && !c.isGrouped() &&
              !c.getAutoTempo() && !c.getAutoPitch() && !c.getIsReversed() && c.getSpeedRatio() == 1. &&
              c.getPitchChange() == 0 && c.getSyncType() == te::Clip::syncAbsolute &&
              warp_state::outputEnvelopeOrigins(c.state) && !c.effectsEnabled() &&
              c.getFadeInBehaviour() == te::AudioClipBase::gainFade &&
              c.getFadeOutBehaviour() == te::AudioClipBase::gainFade)},
        {"parent_clip", c.state.getProperty("ndaw_parent_clip").toString().toStdString()},
        {"origin", c.state.getProperty("ndaw_origin", "import").toString().toStdString()}};
}
void Commands::normaliseClipDeletion(Json& args) const
{
    checkThread();
    auto* clip = audioClip(args.at("clip"));
    require(clip != nullptr, "delete target disappeared");
    const auto source = clip->getOriginalFile();
    const bool present = source.existsAsFile();
    const auto hash = clip_reference::fingerprint(*clip);
    require(!args.contains("source_present") || args.at("source_present") == present,
            "source file presence changed since deletion preview");
    require(!args.contains("source_reference_hash") || args.at("source_reference_hash") == hash,
            "source reference changed since deletion preview");
    if (cachedFactsEpoch == factsEpoch && factCache.contains("tracks"))
        for (const auto& owner : factCache["tracks"])
            for (const auto& cached : owner["clips"])
                if (cached["id"] == args.at("clip") && cached.value("source_present", present) != present)
                    const_cast<Commands*>(this)->invalidateFacts(); // External presence is not an Edit transaction.
    args["source_present"] = present;
    args["source_reference_hash"] = hash;
    if (present)
    {
        const auto actual = mediaHash(source);
        require(!args.contains("media_hash") || args.at("media_hash") == actual, "supplied clip media hash is stale");
        args["media_hash"] = actual;
    }
    else
        require(!args.contains("media_hash"), "missing media cannot claim a content hash");
}
void Commands::registerClipCommands(Json& r)
{
    const Json id = {{"type", "string"}}, integer = {{"type", "integer"}, {"minimum", 0}}, num = {{"type", "number"}};
    auto add = [&](const char* name, Json props, Json required)
    {
        props["clip"] = id;
        props["media_hash"] = id;
        required.push_back("clip");
        required.push_back("media_hash");
        r.push_back(
            {{"id", name},
             {"schema",
              {{"type", "object"}, {"required", required}, {"properties", props}, {"additionalProperties", false}}},
             {"risk", "low"},
             {"permission", "edit"},
             {"reversible", true},
             {"units",
              {{"positions", "session samples at 48000 Hz"},
               {"source_offset", "source seconds represented at the session sample rate; not file PCM frame numbers"},
               {"db", "dB"}}},
             {"test", "M1-CLIP-01"}});
    };
    add("clip.move", {{"position_samples", integer}}, Json::array({"position_samples"}));
    add("clip.trim", {{"start_samples", integer}, {"end_samples", integer}},
        Json::array({"start_samples", "end_samples"}));
    add("clip.split", {{"position_samples", integer}, {"ref", id}}, Json::array({"position_samples", "ref"}));
    add("clip.copy", {{"position_samples", integer}, {"track", id}, {"ref", id}},
        Json::array({"position_samples", "track", "ref"}));
    add("clip.delete", {{"source_reference_hash", id}, {"source_present", {{"type", "boolean"}}}}, Json::array());
    // Existing targets bind their project reference and live presence. Local
    // aliases use the source hash already sealed by an earlier Plan operation.
    r.back()["schema"]["required"] = Json::array({"clip"});
    r.back()["test"] = "P3-MISSING-MEDIA-DELETE-01";
    r.back()["units"]["source_reference_hash"] = "SHA256 of native object/owner/source/extent; not media bytes";
    r.back()["units"]["source_present"] = "observed original regular-file presence; rechecked before commit";
    add("clip.gain", {{"db", num}}, Json::array({"db"}));
    add("clip.fade", {{"in_samples", integer}, {"out_samples", integer}, {"in_curve", id}, {"out_curve", id}},
        Json::array({"in_samples", "out_samples", "in_curve", "out_curve"}));
    add("clip.lock", {{"locked", {{"type", "boolean"}}}}, Json::array({"locked"}));
    add("clip.elastic", {{"length_samples", integer}, {"pitch_semitones", num}},
        Json::array({"length_samples", "pitch_semitones"}));
    r.back()["tool_visibility"] = "local_gui";
    r.back()["test"] = "P3-ELASTIC-01";
    r.back()["units"]["length_samples"] = "output duration at session 48000 Hz, source span preserved";
    r.back()["units"]["pitch_semitones"] = "semitones, -12..12; independent of duration";
    add("clip.warp",
        {{"enabled", {{"type", "boolean"}}},
         {"anchors",
          {{"type", "array"},
           {"maxItems", 128},
           {"items",
            {{"type", "object"},
             {"required", {"id", "source_time_seconds", "clip_time_samples"}},
             {"properties", {{"id", id}, {"source_time_seconds", num}, {"clip_time_samples", integer}}},
             {"additionalProperties", false}}}}}},
        Json::array({"enabled"}));
    r.back()["tool_visibility"] = "local_gui";
    r.back()["test"] = "P3-WARP-01";
    r.back()["units"]["anchors"] =
        "stable clip-owned ID, absolute original-media seconds, clip-relative 48000 Hz samples";
    Json types = Json::array();
    for (const auto& p : processorCatalog())
        if (p["type"] != te::FourOscPlugin::xmlTypeName)
            types.push_back(p["type"]);
    add("clip.fx.insert", {{"type", {{"type", "string"}, {"enum", types}}}, {"wet_only", {{"type", "boolean"}}}},
        Json::array({"type"}));
    r.back()["test"] = "M3-CLIPFX-01";
    r.back()["units"]["wet_only"] =
        "Reverb only: dry=0, wet=1/3; native clip gain/pan before plugins and fades after plugins";
}
Json Commands::makeAudioImportPlan(const juce::Array<juce::File>& files, const std::string& target,
                                   int64_t positionSamples) const
{
    checkThread();
    require(!files.isEmpty() && files.size() <= (target.empty() ? 32 : 64),
            "一次导入最多新建32轨，或在现有轨连续导入64个文件");
    constexpr int64_t limit = int64_t(1) << 53;
    require(positionSamples >= 0 && positionSamples < limit, "导入位置超出工程采样范围");
    if (!target.empty())
    {
        auto* t = track(target);
        require(t && (trackType(*t) == "audio" || trackType(*t) == "instrument"), "导入目标必须为实际音频轨或乐器轨");
    }
    Json operations = Json::array();
    int64_t next = positionSamples;
    for (int i = 0; i < files.size(); ++i)
    {
        const auto& source = files[i];
        std::string destination = target;
        if (target.empty())
        {
            destination = "$import" + std::to_string(i);
            operations.push_back(
                {{"command", "track.create"},
                 {"args", {{"name", source.getFileNameWithoutExtension().toStdString()}, {"ref", destination}}}});
        }
        operations.push_back(
            {{"command", "clip.import"},
             {"args",
              {{"track", destination}, {"path", source.getFullPathName().toStdString()}, {"position_samples", next}}}});
        if (!target.empty())
        {
            // Bind the metadata used for consecutive placement to the same media bytes.
            // makePlan and commit recheck this supplied hash before any Edit mutation.
            operations.back()["args"]["media_hash"] = mediaHash(source);
            te::AudioFile audio(edit->engine, source);
            const double frames = audio.getLength() * rate;
            require(audio.isValid() && std::isfinite(frames) && frames >= .5 && frames < double(limit),
                    "音频文件无效或时长超出导入范围");
            const auto length = std::llround(frames);
            require(length < limit - next, "批量音频终点超出工程采样范围");
            next += length;
        }
    }
    return makePlan("human", std::move(operations));
}

Json Commands::validateClipPlan(const Json& ops) const
{
    std::map<std::string, Model> clips;
    std::map<std::string, std::string> tracks;
    std::set<std::string> refs, removedPlugins;
    std::map<std::string, std::string> hashes;
    std::map<std::string, std::pair<std::string, std::string>> clipPlugins;
    Json diff = Json::array();
    for (auto* t : te::getAllTracks(*edit))
        tracks[t->itemID.toString().toStdString()] =
            dynamic_cast<te::AudioTrack*>(t) ? trackType(*static_cast<te::AudioTrack*>(t)) : "folder";
    for (auto* t : te::getAudioTracks(*edit))
        for (auto* clip : t->getClips())
            if (auto* c = dynamic_cast<te::WaveAudioClip*>(clip))
            {
                auto q = audioClipQuery(*c);
                Model m;
                m.track = t->itemID.toString().toStdString();
                m.start = sample(c->getPosition().getStart().inSeconds());
                m.length = sample(c->getPosition().getLength().inSeconds());
                m.offset = q["source_offset_samples"];
                m.source = sample(c->getSourceLength().inSeconds());
                m.path = q["path"];
                m.gain = q["gain_db"];
                m.inheritedFades = q["inherited_fades"];
                m.fadeIn = q["fade_in_samples"];
                m.fadeOut = q["fade_out_samples"];
                m.locked = q["locked"];
                m.moveLocked = q["move_locked"];
                m.editable = q["editable_audio"];
                m.movable = q["movable_audio"];
                m.trimmable = q["trimmable_audio"];
                m.splittable = q["splittable_audio"];
                m.deletable = q["deletable_audio"];
                m.referenceHash = clip_reference::fingerprint(*c);
                m.sourcePresent = c->getOriginalFile().existsAsFile();
                m.copyable = q["copyable_audio"];
                m.processedTrim = q["trim_preserves_processing"];
                m.fxWindow = q["clip_fx"].value("full_input_window", false);
                m.trimBefore = q["trim_handles_before_samples"];
                m.trimAfter = q["trim_handles_after_samples"];
                m.envelopeEditable = q["editable_envelope"];
                m.inCurve = q["fade_in_curve"];
                m.outCurve = q["fade_out_curve"];
                const auto id = c->itemID.toString().toStdString();
                for (auto* p : *c->getPluginList())
                {
                    const auto type = p->getPluginType().toStdString();
                    m.fxTypes.push_back(type);
                    m.fxIDs.push_back(p->itemID.toString().toStdString());
                    clipPlugins[p->itemID.toString().toStdString()] = {id, type};
                }
                clips[id] = m;
            }
    auto newRef = [&](const Json& a)
    {
        auto ref = a.at("ref").get<std::string>();
        require(ref.size() > 1 && ref.starts_with("$") && refs.insert(ref).second,
                "invalid or duplicate local object reference");
        return ref;
    };
    auto bounds = [&](const Model& m)
    {
        require(m.start >= 0 && m.length > 0 && m.start <= sample(te::Edit::maximumLength) - m.length,
                "clip outside supported timeline");
        require(m.offset >= 0 && m.length <= m.source - m.offset, "trim exceeds original source media");
    };
    for (const auto& op : ops)
    {
        std::string cmd = op["command"];
        const auto& a = op["args"];
        if (cmd == "clip.warp")
        {
            require(ops.size() == 1, "Warp is a standalone stopped edit");
            diff.push_back(warpChange(a));
            continue;
        }
        if (cmd == "clip.elastic")
        {
            require(ops.size() == 1, "Elastic audio is a standalone stopped edit");
            diff.push_back(elasticChange(a));
            continue;
        }
        if (cmd.starts_with("clip."))
            for (const char* key : {"position_samples", "start_samples", "end_samples"})
                if (a.contains(key))
                {
                    auto n = a.at(key).get<int64_t>();
                    require(n >= 0 && n <= sample(te::Edit::maximumLength),
                            "clip position outside native 48 hour timeline");
                }
        if (cmd == "track.create")
        {
            auto ref = newRef(a);
            tracks[ref] = a.value("type", std::string("audio"));
            continue;
        }
        if (cmd == "midi.clip.create" || cmd == "midi.note.add")
        {
            if (a.contains("ref"))
                newRef(a);
            continue;
        }
        if (cmd.starts_with("plugin.") && a.contains("plugin") && clipPlugins.contains(a["plugin"]))
        {
            require(!removedPlugins.contains(a["plugin"]), "clip processor removed earlier in Plan");
            const auto& id = clipPlugins.at(a["plugin"]).first;
            require(clips.contains(id), "clip processor target deleted earlier in Plan");
            auto& m = clips.at(id);
            require(!m.locked, "clip is locked");
            if (cmd == "plugin.remove")
            {
                auto i = std::find(m.fxIDs.begin(), m.fxIDs.end(), a["plugin"].get<std::string>());
                require(i != m.fxIDs.end(), "clip processor removed earlier in Plan");
                m.fxTypes.erase(m.fxTypes.begin() + std::distance(m.fxIDs.begin(), i));
                m.fxIDs.erase(i);
                removedPlugins.insert(a["plugin"]);
            }
            continue;
        }
        if (cmd == "clip.import")
        {
            std::string track = a["track"], path = a["path"];
            require(tracks.contains(track) && (tracks[track] == "audio" || tracks[track] == "instrument"),
                    "audio/instrument track required for audio clip");
            juce::File f(juce::String{path});
            require(mediaHash(f) == a.at("media_hash").get<std::string>(), "media changed since planning");
            te::AudioFile audio(edit->engine, f);
            require(audio.isValid() && audio.getLength() > 0, "invalid audio media");
            Model m;
            m.start = a["position_samples"];
            m.length = m.source = sample(audio.getLength());
            m.path = path;
            m.hash = a["media_hash"];
            m.track = track;
            bounds(m);
            auto ref = a.contains("ref") ? newRef(a) : "#import-" + std::to_string(diff.size());
            clips[ref] = m;
            diff.push_back({{"command", cmd}, {"clip", ref}, {"before", nullptr}, {"after", m.facts()}});
            continue;
        }
        if (!cmd.starts_with("clip."))
            continue;
        std::string id = a["clip"];
        if (cmd == "clip.copy")
            if (const auto* entry = clipboardEntry(id))
            {
                const auto& q = entry->facts;
                Model frozen;
                frozen.track = q["track"];
                frozen.start = q["start_samples"];
                frozen.length = q["length_samples"];
                frozen.offset = q["source_offset_samples"];
                frozen.source = q["source_length_samples"];
                frozen.path = q["path"];
                if (!hashes.contains(frozen.path))
                    hashes[frozen.path] = mediaHash(juce::File(juce::String(frozen.path)));
                require(hashes.at(frozen.path) == q["media_hash"].get<std::string>(),
                        "clipboard source media changed after planning");
                frozen.hash = q["media_hash"];
                frozen.gain = q["gain_db"];
                frozen.inheritedFades = q.value("inherited_fades", false);
                frozen.fadeIn = q["fade_in_samples"];
                frozen.fadeOut = q["fade_out_samples"];
                frozen.inCurve = q["fade_in_curve"];
                frozen.outCurve = q["fade_out_curve"];
                frozen.moveLocked = frozen.locked = q["locked"];
                frozen.fxTypes = q["fx_types"];
                frozen.copyable = q["copyable_audio"];
                frozen.editable = q["editable_audio"];
                frozen.processedTrim = processed_trim::preservesInput(entry->state);
                clips[id] = std::move(frozen);
            }
        require(clips.contains(id), "audio clip not found or deleted earlier in Plan");
        auto& m = clips.at(id);
        const bool envelopeOnly =
            cmd == "clip.fade" || cmd == "clip.gain" || cmd == "clip.lock" || cmd == "clip.fx.insert";
        require((cmd == "clip.trim"     ? m.trimmable
                 : cmd == "clip.split"  ? m.splittable
                 : cmd == "clip.delete" ? m.deletable
                 : cmd == "clip.copy"   ? m.copyable
                                        : m.editable) ||
                    (cmd == "clip.move" && m.movable) || (envelopeOnly && m.envelopeEditable),
                "此片段仅支持已资格的增益包络编辑；源映射或结构编辑尚未资格");
        const bool existingDelete = cmd == "clip.delete" && audioClip(id) != nullptr;
        if (existingDelete)
        {
            require(a.contains("source_reference_hash") && a.at("source_reference_hash") == m.referenceHash &&
                        a.contains("source_present") && a.at("source_present") == m.sourcePresent,
                    "clip source reference or file presence changed; preview deletion again");
            require(m.sourcePresent || !a.contains("media_hash"), "missing media cannot claim a content hash");
        }
        if (!existingDelete || m.sourcePresent)
        {
            if (m.hash.empty())
            {
                if (!hashes.contains(m.path))
                    hashes[m.path] = mediaHash(juce::File(juce::String{m.path}));
                m.hash = hashes.at(m.path);
            }
            require(m.hash == a.at("media_hash").get<std::string>(), "clip source changed since planning");
        }
        if (cmd == "clip.move" ||
            ((cmd == "clip.trim" || cmd == "clip.split" || cmd == "clip.copy") && m.processedTrim))
            if (auto* source = audioClip(id))
            {
                if (cmd == "clip.move")
                    require(a["position_samples"].get<int64_t>() / 48000. +
                                    source->getPosition().getLength().inSeconds() <=
                                te::Edit::maximumLength,
                            "move exceeds actual native duration boundary");
                if (!m.editable || m.processedTrim)
                {
                    warp_state::validate(source->state);
                    elastic_state::validate(source->state);
                    clip_fx_window::validate(source->state);
                    for (const char* node : {"NDAW_WARP", "NDAW_ELASTIC_PROXY", "NDAW_CLIPFX_WINDOW"})
                        if (auto provenance = source->state.getChildWithName(node); provenance.isValid())
                            require(provenance["media_hash"].toString().toStdString() == m.hash,
                                    "processed clip source media changed");
                }
            }
        require(!(cmd == "clip.move" ? m.moveLocked : m.locked) || cmd == "clip.lock" || cmd == "clip.copy",
                "clip is locked");
        auto before = m.facts();
        std::string created;
        if (cmd == "clip.move")
            m.start = a["position_samples"];
        else if (cmd == "clip.trim")
        {
            int64_t start = a["start_samples"], end = a["end_samples"];
            require(end > start, "empty or inverted clip trim");
            const auto left = start - m.start, right = end - m.start - m.length;
            if (m.processedTrim)
            {
                require(left >= -m.trimBefore && right <= m.trimAfter, "trim exceeds complete processed input handles");
                m.trimBefore += left;
                m.trimAfter -= right;
                m.inheritedFades =
                    m.inheritedFades || (m.fadeIn > 0 && start < m.start + m.fadeIn && end > m.start) ||
                    (m.fadeOut > 0 && end > m.start + m.length - m.fadeOut && start < m.start + m.length);
                m.fadeIn = m.fadeOut = 0;
                m.editable = false;
            }
            m.offset += left;
            m.start = start;
            m.length = end - start;
            if (!m.processedTrim)
                m.fitFades();
        }
        else if (cmd == "clip.gain")
        {
            double db = a["db"];
            require(std::isfinite(db) && db >= -100 && db <= 24, "clip gain outside -100..24 dB");
            m.gain = db;
        }
        else if (cmd == "clip.fade")
        {
            int64_t in = a["in_samples"], out = a["out_samples"];
            require(in >= 0 && out >= 0 && in <= m.length && out <= m.length - in,
                    "fade lengths exceed clip; explicit lengths required");
            curveType(a["in_curve"]);
            curveType(a["out_curve"]);
            m.inheritedFades = false;
            m.fadeIn = in;
            m.fadeOut = out;
            m.inCurve = a["in_curve"];
            m.outCurve = a["out_curve"];
        }
        else if (cmd == "clip.lock")
            m.moveLocked = m.locked = a["locked"];
        else if (cmd == "clip.fx.insert")
        {
            require(m.fxTypes.size() < size_t(engine.getEngineBehaviour().getEditLimits().maxPluginsOnClip),
                    "clip processor resource limit reached");
            require(!a.value("wet_only", false) || a["type"] == te::ReverbPlugin::xmlTypeName,
                    "wet_only requires Reverb");
            if (!m.fxWindow && m.fxTypes.empty())
                m.trimBefore = m.trimAfter = 0;
            m.processedTrim = true;
            m.fxTypes.push_back(a["type"]);
            m.fxIDs.push_back("#planned-clip-fx-" + std::to_string(diff.size()));
        }
        else if (cmd == "clip.split")
        {
            int64_t point = a["position_samples"];
            require(point > m.start && point < m.start + m.length, "split must be strictly inside clip");
            auto right = m;
            right.start = point;
            right.offset += point - m.start;
            right.length = m.start + m.length - point;
            if (m.processedTrim)
            {
                right.trimBefore += point - m.start;
                m.trimAfter += right.length;
                right.inheritedFades =
                    m.inheritedFades || m.fadeOut > 0 || (m.fadeIn > 0 && point < m.start + m.fadeIn);
                m.inheritedFades =
                    m.inheritedFades || m.fadeIn > 0 || (m.fadeOut > 0 && point > m.start + m.length - m.fadeOut);
                right.fadeIn = right.fadeOut = m.fadeIn = m.fadeOut = 0;
                right.editable = m.editable = false;
            }
            else
            {
                const bool cutIn = m.fadeIn > 0 && point < m.start + m.fadeIn,
                           cutOut = m.fadeOut > 0 && point > m.start + m.length - m.fadeOut;
                right.inheritedFades = m.inheritedFades || cutIn || cutOut;
                m.inheritedFades = m.inheritedFades || cutIn || cutOut;
                right.fadeIn = 0;
                right.fadeOut = cutOut ? 0 : right.fadeOut;
                m.fadeOut = 0;
                m.fadeIn = cutIn ? 0 : m.fadeIn;
            }
            m.length = point - m.start;
            created = newRef(a);
            clips[created] = right;
        }
        else if (cmd == "clip.copy")
        {
            auto copy = m;
            std::string target = a["track"];
            require(tracks.contains(target) && (tracks[target] == "audio" || tracks[target] == "instrument"),
                    "audio/instrument copy target required");
            copy.start = a["position_samples"];
            copy.track = target;
            if (!copy.processedTrim)
                bounds(copy);
            else
                require(copy.start <= sample(te::Edit::maximumLength) - copy.length, "copy exceeds timeline");
            created = newRef(a);
            clips[created] = copy;
        }
        else if (cmd != "clip.delete")
            throw std::runtime_error("unknown clip command");
        if (cmd != "clip.delete" && m.editable && !m.processedTrim)
            bounds(m);
        else
            require(m.start >= 0 && m.length > 0 && m.start <= sample(te::Edit::maximumLength) - m.length,
                    "clip outside supported timeline");
        diff.push_back({{"command", cmd},
                        {"clip", id},
                        {"before", before},
                        {"after", cmd == "clip.delete" ? Json(nullptr) : m.facts()}});
        if (!created.empty())
            diff.back()["created"] = {{"clip", created}, {"after", clips.at(created).facts()}};
        if ((cmd == "clip.split" || cmd == "clip.copy") && m.processedTrim)
            diff.back()["processing"] = {
                {"full_input_preserved", true},
                {"identity_policy", cmd == "clip.split" ? "left clip/plugins retained; right clip/plugins new IDs"
                                                        : "source retained; copy clip/plugins new IDs"},
                {"source_envelope", "original phase retained on each output window"}};
        if (cmd == "clip.delete")
        {
            diff.back()["media"] = {{"source_present", m.sourcePresent},
                                    {"source_path", m.path},
                                    {"policy", "remove project object only; retain media and native state in Undo"}};
            clips.erase(id);
        }
    }
    return diff;
}
void Commands::executeClipOperation(const std::string& cmd, const Json& a, Json& objects,
                                    std::map<std::string, std::string>& aliases)
{
    if (cmd == "clip.warp")
    {
        executeWarp(a);
        return;
    }
    if (cmd == "clip.elastic")
    {
        executeElastic(a);
        return;
    }
    auto& um = edit->getUndoManager();
    if (cmd == "clip.import")
    {
        auto* t = track(a["track"]);
        require(t != nullptr, "audio import target disappeared");
        juce::File f(juce::String{a.at("path").get<std::string>()});
        te::AudioFile audio(engine, f);
        auto start = pos(a["position_samples"]);
        auto c = t->insertWaveClip(f.getFileNameWithoutExtension(), f,
                                   {{start, start + dur(sample(audio.getLength()))}, {}}, false);
        require(c != nullptr, "audio import failed");
        c->setSyncType(te::Clip::syncAbsolute);
        c->setAutoCrossfade(false);
        c->state.setProperty("ndaw_origin", "import", &um);
        objects.push_back({{"id", c->itemID.toString().toStdString()}, {"kind", "clip"}});
        if (a.contains("ref"))
            aliases[a["ref"]] = c->itemID.toString().toStdString();
        return;
    }
    const std::string id = a["clip"];
    if (cmd == "clip.copy")
        if (const auto* entry = clipboardEntry(id))
        {
            auto* target = track(a["track"]);
            require(target != nullptr, "clipboard destination disappeared");
            const auto copy = audio_clipboard::clone(*edit, entry->state, pos(a["position_samples"]).inSeconds(),
                                                     entry->facts["clip"]);
            auto* c = dynamic_cast<te::WaveAudioClip*>(
                te::insertClipCopy(*target, te::ClipCopy::fromClipboardState(copy, false)));
            require(c != nullptr, "native clipboard insertion failed");
            const auto actual = c->itemID.toString().toStdString();
            aliases[a["ref"]] = actual;
            objects.push_back({{"id", actual}, {"kind", "clip"}, {"clipboard_token", id}});
            return;
        }
    auto* c = audioClip(aliases.contains(id) ? aliases.at(id) : id);
    require(c != nullptr, "audio clip disappeared");
    auto p = c->getPosition();
    auto copy = [&](te::AudioTrack& target, const char* origin)
    {
        auto* inserted = te::insertClipCopy(target, te::ClipCopy::fromClip(*c).withNewItemID(*edit));
        auto* n = dynamic_cast<te::WaveAudioClip*>(inserted);
        require(n != nullptr, "clip copy failed");
        n->state.setProperty("ndaw_parent_clip", c->itemID.toString(), &um);
        n->state.setProperty("ndaw_origin", origin, &um);
        auto actual = n->itemID.toString().toStdString();
        aliases[a["ref"]] = actual;
        objects.push_back({{"id", actual}, {"kind", "clip"}});
        return n;
    };
    if (cmd == "clip.move")
    {
        // Clip::clipStart is a native CachedValue bound to this property. Move
        // only that field; setPosition would recompute/round the stored duration.
        c->state.setProperty(te::IDs::start, pos(a["position_samples"]).inSeconds(), &um);
    }
    else if (cmd == "clip.trim" && processed_trim::preservesInput(c->state))
        processed_trim::apply(*c, processed_trim::prepare(*edit, c->state, a["start_samples"], a["end_samples"]));
    else if (cmd == "clip.trim")
    {
        int64_t start = a["start_samples"], end = a["end_samples"], in = sample(c->getFadeIn().inSeconds()),
                out = sample(c->getFadeOut().inSeconds()), length = end - start;
        if (in + out > length)
        {
            const auto total = in + out;
            in = std::llround(double(in) * length / total);
            out = length - in;
        }
        auto offset = p.getOffset() + (pos(start) - p.getStart());
        auto ci = curveName(c->getFadeInType()), co = curveName(c->getFadeOutType());
        c->setPosition({{pos(start), pos(end)}, offset});
        fades(*c, in, out, ci, co);
    }
    else if (cmd == "clip.gain")
        c->setGainDB(a["db"]);
    else if (cmd == "clip.fx.insert")
    {
        auto p = edit->getPluginCache().createNewPlugin(juce::String(a.at("type").get<std::string>()), {});
        require(p != nullptr, "clip processor creation failed");
        const auto error = c->canAddClipPlugin(p);
        require(error.isEmpty(), error.toRawUTF8());
        c->getPluginList()->insertPlugin(p, -1, nullptr);
        require(c->getPluginList()->contains(p.get()), "clip processor insertion failed");
        if (a.value("wet_only", false))
        {
            auto& reverb = dynamic_cast<te::ReverbPlugin&>(*p);
            setParameterValue(*p, *reverb.dryParam, 0);
            setParameterValue(*p, *reverb.wetParam, 1.f / 3);
        }
        objects.push_back({{"id", p->itemID.toString().toStdString()},
                           {"kind", "plugin"},
                           {"owner_clip", c->itemID.toString().toStdString()}});
    }
    else if (cmd == "clip.fade")
    {
        // Explicit fade editing replaces the inherited envelope. Structural
        // trim/split keeps it anchored to the original source samples.
        for (int i = c->state.getNumChildren(); --i >= 0;)
            if (c->state.getChild(i).hasType("NDAW_FADE_ORIGIN"))
                c->state.removeChild(i, &um);
        fades(*c, a["in_samples"], a["out_samples"], a["in_curve"], a["out_curve"]);
    }
    else if (cmd == "clip.lock")
        c->state.setProperty("ndaw_locked", a.at("locked").get<bool>(), &um);
    else if (cmd == "clip.delete")
    {
        require(!a.contains("source_present") || a.at("source_present") == c->getOriginalFile().existsAsFile(),
                "source file presence changed before project deletion");
        c->removeFromParent();
    }
    else if (cmd == "clip.copy")
    {
        auto* t = track(a["track"]);
        require(t != nullptr, "copy target disappeared");
        const auto snapshot = audio_clipboard::fragment(*edit, c->state, sample(p.getStart().inSeconds()),
                                                        sample(p.getEnd().inSeconds()));
        const auto parent = c->itemID.toString().toStdString();
        const auto state =
            audio_clipboard::clone(*edit, snapshot, pos(a["position_samples"]).inSeconds(), parent, "copy");
        auto* n = te::insertClipCopy(*t, te::ClipCopy::fromClipboardState(state, false));
        require(n != nullptr, "native processed copy insertion failed");
        const auto actual = n->itemID.toString().toStdString();
        aliases[a["ref"]] = actual;
        objects.push_back({{"id", actual}, {"kind", "clip"}, {"parent_clip", parent}});
    }
    else if (cmd == "clip.split" && processed_trim::preservesInput(c->state))
    {
        auto* t = dynamic_cast<te::AudioTrack*>(c->getTrack());
        require(t != nullptr, "split source track disappeared");
        const auto oldID = c->itemID.toString().toStdString();
        const int64_t point = a["position_samples"], first = sample(p.getStart().inSeconds()),
                      last = sample(p.getEnd().inSeconds());
        const auto left = processed_trim::prepare(*edit, c->state, first, point);
        auto right = processed_trim::prepare(*edit, c->state, point, last);
        right = te::ClipCopy::fromClipboardState(right, false).withNewItemID(*edit).getState().createCopy();
        warp_state::remapIDs(right, oldID);
        elastic_state::remap(right, oldID);
        right.setProperty("ndaw_parent_clip", juce::String(oldID), nullptr);
        right.setProperty("ndaw_origin", "split", nullptr);
        auto* n =
            dynamic_cast<te::WaveAudioClip*>(te::insertClipCopy(*t, te::ClipCopy::fromClipboardState(right, false)));
        require(n != nullptr, "native processed split insertion failed");
        processed_trim::apply(*c, left);
        const auto actual = n->itemID.toString().toStdString();
        aliases[a["ref"]] = actual;
        objects.push_back({{"id", actual}, {"kind", "clip"}, {"parent_clip", oldID}, {"full_input_preserved", true}});
    }
    else if (cmd == "clip.split")
    {
        auto* t = dynamic_cast<te::AudioTrack*>(c->getTrack());
        require(t != nullptr, "clip track disappeared");
        const auto point = pos(a["position_samples"]);
        auto left = native_fades::fragment(c->state, p.getStart().inSeconds(), point.inSeconds());
        auto right = native_fades::fragment(c->state, point.inSeconds(), p.getEnd().inSeconds());
        left.setProperty(te::IDs::length, (point - p.getStart()).inSeconds(), nullptr);
        right.setProperty(te::IDs::start, point.inSeconds(), nullptr);
        right.setProperty(te::IDs::length, (p.getEnd() - point).inSeconds(), nullptr);
        right.setProperty(te::IDs::offset, (p.getOffset() + (point - p.getStart())).inSeconds(), nullptr);
        auto* n = copy(*t, "split");
        // Keep the original ramp phase even when Cmd E cuts inside a fade.
        // The editor SDK helper's 1ms exclusion is not used for sample-grid edits.
        processed_trim::apply(*c, left);
        processed_trim::apply(*n, right);
    }
    else
        throw std::runtime_error("unhandled clip command");
}
} // namespace ndaw::v2
