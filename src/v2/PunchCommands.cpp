// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "PunchSignalGate.h"
#include "TimelineState.h"
#include "MidiPunchState.h"
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
int64_t samples(double seconds)
{
    return std::llround(seconds * 48000.);
}
} // namespace
Json Commands::punchRange() const
{
    const auto range = timelineRange();
    if (range.is_null())
        return nullptr;
    const auto roll = readRollState(metadata);
    const int64_t begin = range["start_samples"], end = range["end_samples"];
    const auto first =
        std::max(int64_t(0), begin - (roll["pre_enabled"].get<bool>() ? roll["pre_samples"].get<int64_t>() : 0));
    const auto last = end + (roll["post_enabled"].get<bool>() ? roll["post_samples"].get<int64_t>() : 0);
    return {{"start_samples", begin},      {"end_samples", end},   {"capture_start_samples", first},
            {"capture_end_samples", last}, {"sample_rate", 48000}, {"timebase", "session_samples"}};
}
void Commands::validatePunchCapture() const
{
    auto range = punchRange();
    require(!range.is_null(), "Punch requires a timeline selection");
    require(range["capture_end_samples"].get<int64_t>() <= samples(te::Edit::maximumLength),
            "Punch post-roll exceeds Edit maximum length");
    require(!edit->getTransport().looping, "Punch and Loop recording are mutually exclusive");
    auto begin = range["start_samples"].get<int64_t>(), end = range["end_samples"].get<int64_t>();
    for (auto* t : te::getAudioTracks(*edit))
        if (bool(t->state.getProperty("ndaw_armed", false)))
        {
            const bool midi = trackType(*t) == "midi" || trackType(*t) == "instrument";
            require(trackType(*t) == "audio" || midi, "Punch requires audio, MIDI or instrument tracks");
            for (auto* clip : t->getClips())
                if (clip->getPosition().getStart().inSeconds() < end / 48000. &&
                    clip->getPosition().getEnd().inSeconds() > begin / 48000.)
                {
                    if (midi)
                    {
                        auto* performance = dynamic_cast<te::MidiClip*>(clip);
                        require(performance != nullptr, "MIDI Punch target contains non-MIDI material");
                        const auto facts = midiQuery(*performance);
                        require(!facts["locked"].get<bool>(), "Punch intersects a locked MIDI clip");
                        require(facts["sample_mapping_available"].get<bool>(),
                                "Punch through looped/quantised/Groove/expressive MIDI is not qualified");
                        continue;
                    }
                    auto* wave = dynamic_cast<te::WaveAudioClip*>(clip);
                    require(wave != nullptr, "Punch target contains a non-audio clip");
                    const auto facts = audioClipQuery(*wave);
                    require(!facts["locked"].get<bool>(), "Punch intersects a locked clip");
                    require(facts["editable_audio"].get<bool>() && wave->getSyncType() == te::Clip::syncAbsolute,
                            "Punch requires time-based unstretched audio");
                    require(wave->getPluginList()->size() == 0 && !wave->effectsEnabled(),
                            "Punch through Clip FX requires separate qualification");
                    require(wave->getFadeInBehaviour() == te::AudioClipBase::gainFade &&
                                wave->getFadeOutBehaviour() == te::AudioClipBase::gainFade,
                            "Punch through speed fades is not yet qualified");
                }
        }
}
void Commands::preparePunchGraph(const Json& range)
{
    const double begin = range["start_samples"].get<double>() / 48000.,
                 end = range["end_samples"].get<double>() / 48000.;
    const double first = range["capture_start_samples"].get<double>() / 48000.,
                 last = range["capture_end_samples"].get<double>() / 48000.;
    std::map<std::string, std::string> modes;
    for (auto* t : te::getAudioTracks(*edit))
        if (bool(t->state.getProperty("ndaw_armed", false)))
            modes[t->itemID.toString().toStdString()] = recordingQuery(*t)["monitor"];
    // Original MIDI windows use the native MIDI nodes, including boundary
    // NoteOff/chase. No new MIDI filtering or message copying in the callback.
    struct MidiSources
    {
        std::unique_ptr<te::Edit> edit;
        std::map<std::string, juce::Array<te::Clip*>> clips;
    };
    auto midi = std::make_shared<MidiSources>();
    auto snapshot = te::createEmptyEdit(engine);
    snapshot.removeChild(snapshot.getChildWithName(te::IDs::TEMPOSEQUENCE), nullptr);
    snapshot.addChild(edit->tempoSequence.getState().createCopy(), -1, nullptr);
    std::set<std::string> midiTracks;
    for (auto* t : te::getAudioTracks(*edit))
        if (const auto id = t->itemID.toString().toStdString(); modes.contains(id) && trackType(*t) != "audio")
        {
            midiTracks.insert(id);
            juce::ValueTree sourceTrack(te::IDs::TRACK);
            sourceTrack.copyPropertiesFrom(t->state, nullptr);
            if (modes.at(id) != "on")
                for (auto* c : t->getClips())
                {
                    auto* notes = dynamic_cast<te::MidiClip*>(c);
                    require(notes != nullptr, "MIDI Punch playback source must be MIDI");
                    const auto a = samples(c->getPosition().getStart().inSeconds());
                    const auto b = samples(c->getPosition().getEnd().inSeconds());
                    auto append = [&](int64_t low, int64_t high)
                    {
                        if (high > low)
                            sourceTrack.addChild(midi_punch::fragment(*edit, *notes, low, high), -1, nullptr);
                    };
                    if (b <= samples(begin) || a >= samples(end))
                        append(a, b);
                    else
                    {
                        append(a, std::min(b, samples(begin)));
                        append(std::max(a, samples(end)), b);
                    }
                }
            snapshot.addChild(sourceTrack, -1, nullptr);
        }
    if (!midiTracks.empty())
    {
        te::Edit::Options options{engine, snapshot, edit->getProjectItemRef()};
        options.role = te::Edit::forRendering;
        options.numAudioTracks = 0;
        midi->edit = te::Edit::createEdit(std::move(options));
        require(midi->edit != nullptr, "MIDI Punch source preparation failed");
        for (auto* t : te::getAudioTracks(*midi->edit))
            midi->clips[t->itemID.toString().toStdString()] = t->getClips();
    }
    rollGate = std::make_shared<SelectionOutputGateState>();
    rollGate->startSeconds = first;
    rollGate->endSeconds = last;
    const auto gate = rollGate;
    edit->getTransport().prepareAuditionPlayback(
        tracktion::TimePosition::fromSeconds(first),
        [modes, begin, end, first, last, gate, midi, midiTracks](te::EditPlaybackContext& context)
        {
            context.clearNodes();
            context.setAuditionGraphCallback(
                [modes, begin, end, first, last, midi, midiTracks](te::CreateNodeParams& params)
                {
                    params.transformTrackSignal =
                        [modes, begin, end, first, last, midi, midiTracks](
                            te::AudioTrack& t, bool live, const te::CreateNodeParams& p,
                            std::unique_ptr<tracktion::graph::Node> node) -> std::unique_ptr<tracktion::graph::Node>
                    {
                        const auto id = t.itemID.toString().toStdString();
                        auto found = modes.find(id);
                        if (midiTracks.contains(id))
                        {
                            // MIDI Auto preserves the native armed live path during
                            // pre/Punch/post; On is input-only. Off has no live node.
                            if (live)
                                return node;
                            if (found->second == "on")
                                return {};
                            return te::createNodeForMidiPunchClips(midi->clips.at(id), p);
                        }
                        if (found == modes.end() || (live && found->second == "on"))
                            return node;
                        const bool always = found->second == "on";
                        return std::make_unique<PunchSignalGate>(std::move(node), p.processState,
                                                                 always ? first : begin, always ? last : end, live);
                    };
                });
            context.setInsertOptionalLastStageNodeForDeviceCallback(
                [gate](te::OutputDevice& device, const te::CreateNodeParams& p,
                       std::unique_ptr<tracktion::graph::Node> node) -> std::unique_ptr<tracktion::graph::Node>
                {
                    if (dynamic_cast<te::WaveOutputDevice*>(&device))
                        return std::make_unique<SelectionOutputGate>(std::move(node), p.processState, gate);
                    return node;
                });
        });
}
void Commands::makePunchPlaylists(te::AudioTrack& track, te::WaveAudioClip& recording, Json& receipt)
{
    const auto range = receipt["punch_range"];
    const auto position = recording.getPosition();
    const int64_t first = samples(position.getStart().inSeconds()), last = samples(position.getEnd().inSeconds());
    const int64_t begin = std::max(first, range["start_samples"].get<int64_t>()),
                  end = std::min(last, range["end_samples"].get<int64_t>());
    auto& undo = edit->getUndoManager();
    auto lists = ensurePlaylists(track);
    int takes = 0;
    for (auto list : lists)
        if (list.getProperty("role") == juce::var("take"))
            ++takes;
    auto templateState = recording.state.createCopy();
    auto source = recording.getOriginalFile().getFullPathName().toStdString();
    std::string hash;
    for (const auto& f : receipt["files"])
        if (f["path"] == source)
            hash = f["media_hash"];
    require(!hash.empty(), "Punch source was not verified");
    track.state.removeChild(recording.state, &undo);
    auto makeList = [&](const char* role, const juce::String& name)
    {
        juce::ValueTree list("PLAYLIST");
        list.setProperty("id", juce::Uuid().toString(), nullptr);
        list.setProperty("name", name, nullptr);
        list.setProperty("role", role, nullptr);
        list.setProperty("capture_id", juce::String(receipt["plan_id"].get<std::string>()), nullptr);
        return list;
    };
    auto take = makeList("take", "Punch Take " + juce::String(takes + 1));
    const auto takeID = juce::Uuid().toString();
    take.setProperty("take_id", takeID, nullptr);
    take.setProperty("media_hash", juce::String(hash), nullptr);
    take.setProperty("complete", end >= range["end_samples"].get<int64_t>(), nullptr);
    take.setProperty("length_samples", juce::int64(last - first), nullptr);
    take.setProperty("source_offset_samples", juce::int64(0), nullptr);
    templateState.setProperty(te::IDs::id, edit->createNewItemID().toString(), nullptr);
    templateState.setProperty("ndaw_take_id", takeID, nullptr);
    take.addChild(templateState, -1, nullptr);
    lists.addChild(take, -1, &undo);
    const auto activeBefore = lists.getProperty("active");
    if (end > begin)
    {
        auto comp = makeList("punch", "Punch " + juce::String(takes + 1));
        for (auto* original : track.getClips())
        {
            auto p = original->getPosition();
            const auto a = samples(p.getStart().inSeconds()), b = samples(p.getEnd().inSeconds());
            auto append = [&](int64_t from, int64_t to)
            {
                if (to <= from)
                    return;
                auto clip = te::ClipCopy::fromClip(*original).withNewItemID(*edit).getState().createCopy();
                if (from != a || to != b)
                {
                    auto* wave = dynamic_cast<te::WaveAudioClip*>(original);
                    require(wave, "Punch slice must be audio");
                    if (!clip.getChildWithName("NDAW_FADE_ORIGIN").isValid() &&
                        (wave->getFadeIn().inSeconds() > 0 || wave->getFadeOut().inSeconds() > 0))
                    {
                        juce::ValueTree envelope("NDAW_FADE_ORIGIN");
                        envelope.setProperty("in_origin", p.getOffset().inSeconds(), nullptr);
                        envelope.setProperty("out_origin", p.getOffset().inSeconds() + p.getLength().inSeconds(),
                                             nullptr);
                        envelope.setProperty("in_length", wave->getFadeIn().inSeconds(), nullptr);
                        envelope.setProperty("out_length", wave->getFadeOut().inSeconds(), nullptr);
                        clip.addChild(envelope, -1, nullptr);
                    }
                    clip.setProperty(te::IDs::fadeIn, 0., nullptr);
                    clip.setProperty(te::IDs::fadeOut, 0., nullptr);
                    clip.setProperty(te::IDs::start, from / 48000., nullptr);
                    clip.setProperty(te::IDs::length, (to - from) / 48000., nullptr);
                    clip.setProperty(te::IDs::offset, p.getOffset().inSeconds() + (from - a) / 48000., nullptr);
                }
                comp.addChild(clip, -1, nullptr);
            };
            if (b <= begin || a >= end)
                append(a, b);
            else
            {
                append(a, std::min(b, begin));
                append(std::max(a, end), b);
            }
        }
        auto inserted = templateState.createCopy();
        inserted.setProperty(te::IDs::id, edit->createNewItemID().toString(), nullptr);
        inserted.setProperty(te::IDs::start, begin / 48000., nullptr);
        inserted.setProperty(te::IDs::length, (end - begin) / 48000., nullptr);
        inserted.setProperty(te::IDs::offset, position.getOffset().inSeconds() + (begin - first) / 48000., nullptr);
        comp.addChild(inserted, -1, nullptr);
        lists.addChild(comp, -1, &undo);
        selectPlaylist(track, comp.getProperty("id").toString().toStdString());
    }
    receipt["takes"].push_back({{"track", track.itemID.toString().toStdString()},
                                {"playlist", take.getProperty("id").toString().toStdString()},
                                {"take", takeID.toStdString()},
                                {"clip", templateState[te::IDs::id].toString().toStdString()},
                                {"path", source},
                                {"media_hash", hash},
                                {"capture_start_samples", first},
                                {"length_samples", last - first},
                                {"recorded_start_samples", begin},
                                {"recorded_end_samples", std::max(begin, end)},
                                {"source_offset_samples", std::max(int64_t(0), begin - first)},
                                {"previous_playlist", activeBefore.toString().toStdString()},
                                {"active_playlist", lists.getProperty("active").toString().toStdString()}});
    track.flushPendingClipOrder();
}
} // namespace ndaw::v2
