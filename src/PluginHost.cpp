// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/PluginHost.h"
#include "nativedaw/PluginIPC.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <chrono>
#include <set>
#include <cmath>
#include <fstream>
#include <map>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ndaw {
static std::unique_ptr<juce::AudioPluginFormat> formatFor(const std::string& name) {
    if(name=="VST3")return std::make_unique<juce::VST3PluginFormat>();
#if JUCE_MAC
    if(name=="AudioUnit")return std::make_unique<juce::AudioUnitPluginFormat>();
#endif
    throw Error("plugin_format","Requested actual plugin format is unavailable on this platform");
}
static std::unique_ptr<juce::AudioPluginInstance> instantiate(juce::AudioPluginFormat& format,const juce::PluginDescription& d,double rate,int block) {
    std::unique_ptr<juce::AudioPluginInstance> instance;bool finished=false;juce::String error;
    format.createPluginInstanceAsync(d,rate,block,[&](std::unique_ptr<juce::AudioPluginInstance> p,const juce::String& e){instance=std::move(p);error=e;finished=true;});
    while(!finished)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    // The supervisor owns a finite process deadline even if a plugin or SDK
    // never returns/calls back; no such wait can enter the parent/audio thread.
    if(!instance)throw Error("plugin_instantiate","Actual instance creation failed: "+error.toStdString());return instance;
}
static bool stereoMainOnly(const juce::AudioPluginInstance& p) {
    if(p.getTotalNumInputChannels()!=2 || p.getTotalNumOutputChannels()!=2 || p.getBusCount(true)<1 || p.getBusCount(false)<1)return false;
    for(bool input:{true,false})for(int i=0;i<p.getBusCount(input);++i){const auto* bus=p.getBus(input,i);if(i==0){if(bus->getCurrentLayout()!=juce::AudioChannelSet::stereo())return false;}else if(bus->isEnabled())return false;}return true;
}
static Json describe(const juce::PluginDescription& d) {
    const auto xml=d.createXml()->toString().toStdString();
    return {{"id",digest(d.pluginFormatName.toStdString()+":"+d.createIdentifierString().toStdString())},
        {"name",d.name.toStdString()},{"descriptive_name",d.descriptiveName.toStdString()},{"format",d.pluginFormatName.toStdString()},
        {"manufacturer",d.manufacturerName.toStdString()},{"version",d.version.toStdString()},{"category",d.category.toStdString()},
        {"file_or_identifier",d.fileOrIdentifier.toStdString()},{"sdk_identifier",d.createIdentifierString().toStdString()},
        {"sdk_unique_id",d.uniqueId},{"description_xml",xml},{"instrument",d.isInstrument},{"ara_advertised",d.hasARAExtension},
        {"ara_integrated",false},{"private_internal_state_accessible",false}};
}
static Json buses(juce::AudioPluginInstance& p) {
    Json result=Json::array();for(bool input:{true,false})for(int b=0;b<p.getBusCount(input);++b){auto* bus=p.getBus(input,b);
        result.push_back({{"direction",input?"input":"output"},{"index",b},{"name",bus->getName().toStdString()},
            {"enabled",bus->isEnabled()},{"channels",bus->getNumberOfChannels()},{"layout",bus->getCurrentLayout().getDescription().toStdString()}});}
    return result;
}
static Json parameters(juce::AudioPluginInstance& p,const PluginLimits& limits) {
    if(static_cast<std::size_t>(p.getParameters().size())>limits.parameterCount)throw Error("plugin_resources","Actual parameter metadata exceeds declared budget");
    Json result=Json::array();std::set<std::string> ids;
    for(int index=0;index<p.getParameters().size();++index) {
        auto* param=p.getParameters()[index];auto* hosted=p.getHostedParameter(index);const auto id=hosted?hosted->getParameterID().toStdString():"";
        if(!id.empty() && !ids.insert(id).second)throw Error("plugin_parameter_id","Actual SDK parameter IDs are duplicated");
        const auto value=param->getValue(),def=param->getDefaultValue();
        if(!std::isfinite(value) || value<0 || value>1 || !std::isfinite(def) || def<0 || def>1)throw Error("plugin_parameter_value","Plugin reported an invalid normalized value");
        const int steps=param->getNumSteps();Json enumeration=Json::array();
        if(param->isDiscrete() && steps>=2 && steps<=256)for(int i=0;i<steps;++i){const auto v=static_cast<float>(i)/static_cast<float>(steps-1);enumeration.push_back({{"normalized",v},{"display",param->getText(v,128).toStdString()}});}
        result.push_back({{"id",id.empty()?Json(nullptr):Json(id)},{"index",index},{"name",param->getName(128).toStdString()},
            {"unit_label",param->getLabel().toStdString()},{"normalized_range",{{"min",0.0},{"max",1.0}}},{"value",value},{"default",def},
            {"display",param->getText(value,128).toStdString()},{"display_at_zero",param->getText(0,128).toStdString()},
            {"display_at_one",param->getText(1,128).toStdString()},{"discrete",param->isDiscrete()},{"boolean",param->isBoolean()},
            {"steps",steps},{"automatable",param->isAutomatable()},{"model_editable",!id.empty()},{"enum_values",enumeration},
            {"plain_unit_mapping","not inferred from text; host SDK normalized values only"},
            {"enum_complete",param->isDiscrete() && steps>=2 && steps<=256}});
    }return result;
}
static Json snapshot(juce::AudioPluginInstance& p,const PluginLimits& limits,const fs::path& job,int index) {
    juce::MemoryBlock state;p.getStateInformation(state);
    if(state.getSize()>limits.stateBytes)throw Error("plugin_state_resources","Actual opaque state exceeds declared budget");
    const auto file=job/("state-"+std::to_string(index)+".bin");
    atomicWrite(file,state.getSize()?std::string(static_cast<const char*>(state.getData()),state.getSize()):std::string{},false);
    return {{"path",file.string()},{"sha256",sha256(file)},{"bytes",state.getSize()},
        {"scope","opaque SDK state capture; private local data, not a state-restoration or preset-compatibility pass"}};
}
static Json scan(juce::AudioPluginFormat& format,const std::string& candidate,const PluginLimits& limits,const fs::path& job) {
    juce::OwnedArray<juce::PluginDescription> descriptions;
    std::unique_ptr<juce::AudioPluginInstance> initialInstance;
    if(format.getName()=="AudioUnit") {juce::PluginDescription initial;initial.pluginFormatName=format.getName();initial.fileOrIdentifier=candidate;
        initialInstance=instantiate(format,initial,48000,256);descriptions.add(new juce::PluginDescription(initialInstance->getPluginDescription()));}
    else format.findAllTypesForFile(descriptions,candidate);
    if(descriptions.isEmpty())throw Error("plugin_scan","SDK found no actual types for candidate");
    Json result=Json::array();std::set<std::string> ids;
    for(int i=0;i<descriptions.size();++i){auto p=i==0 && initialInstance?std::move(initialInstance):instantiate(format,*descriptions[i],48000,256);
        p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
        auto d=describe(p->getPluginDescription());if(!ids.insert(d.at("id")).second)throw Error("plugin_identity","Ambiguous actual plugin identity; no overwritten inventory");
        d["parameters"]=parameters(*p,limits);d["buses"]=buses(*p);d["reported_latency_frames"]=p->getLatencySamples();
        d["tail_seconds"]=p->getTailLengthSeconds();d["supports_double_precision"]=p->supportsDoublePrecisionProcessing();
        d["has_editor"]=p->hasEditor();d["program_count"]=p->getNumPrograms();d["opaque_state"]=snapshot(*p,limits,job,i);
        d["prepared_sample_rate"]=48000;d["prepared_max_block"]=256;d["scan_verified"]=true;d["audio_processing_verified"]=false;
        d["automation_precision_verified"]=false;p->releaseResources();p.reset();result.push_back(std::move(d));}
    return result;
}
class OfflinePlayhead final:public juce::AudioPlayHead {
public:Frame position=0;double rate=48000;
    juce::Optional<PositionInfo> getPosition() const override {PositionInfo p;p.setTimeInSamples(position);p.setTimeInSeconds(position/rate);p.setBpm(120);p.setTimeSignature(TimeSignature{4,4});p.setIsPlaying(true);return p;}
};
static Json measureDecodedFile(juce::AudioFormatManager& files,const fs::path& path) {
    auto r=std::unique_ptr<juce::AudioFormatReader>(files.createReaderFor(juce::File(juce::String(path.string()))));
    if(!r || r->numChannels!=2 || !r->usesFloatingPointData || r->bitsPerSample!=32 || r->lengthInSamples<=0)
        throw Error("plugin_export","Actual output decoder rejected float32 stereo PCM");
    juce::AudioBuffer<float> samples(2,4096);double peak=0;std::array<double,2> square{};
    for(Frame at=0;at<r->lengthInSamples;at+=4096){const auto n=static_cast<int>(std::min<Frame>(4096,r->lengthInSamples-at));
        if(!r->read(&samples,0,n,at,true,true))throw Error("plugin_export","Cannot read back staged PCM");
        for(int ch=0;ch<2;++ch)for(int i=0;i<n;++i){const double v=samples.getSample(ch,i);
            if(!std::isfinite(v))throw Error("plugin_export","Decoded staged PCM is non-finite");peak=std::max(peak,std::abs(v));square[ch]+=v*v;}}
    return {{"sample_peak",peak},{"rms",Json::array({std::sqrt(square[0]/r->lengthInSamples),std::sqrt(square[1]/r->lengthInSamples)})},
        {"finite",true},{"frames_per_channel",r->lengthInSamples},{"scope","actual new file decoded in full; sample peak/RMS, no true peak or LUFS claim"}};
}
static Json processFile(const Json& request,const PluginLimits& limits,const fs::path& job) {
    const auto& desc=request.at("plugin");auto format=formatFor(desc.at("format"));
    auto xml=juce::parseXML(juce::String(desc.at("description_xml").get<std::string>()));juce::PluginDescription d;
    if(!xml || !d.loadFromXml(*xml) || describe(d).at("id")!=desc.at("id") || d.fileOrIdentifier.toStdString()!=desc.at("file_or_identifier").get<std::string>() || d.version.toStdString()!=desc.at("version").get<std::string>())throw Error("plugin_description","Actual stored SDK description failed validation");
    const fs::path input=request.at("input").get<std::string>(),output=request.at("output").get<std::string>();
    if(fs::exists(output))throw Error("export_exists","Original output cannot be overwritten");
    if(sha256(input)!=request.at("input_sha256").get<std::string>())throw Error("plugin_source","Input file identity changed before processing");
    juce::AudioFormatManager files;files.registerBasicFormats();auto reader=std::unique_ptr<juce::AudioFormatReader>(files.createReaderFor(juce::File(juce::String(input.string()))));
    if(!reader || reader->numChannels<1 || reader->numChannels>2 || reader->lengthInSamples<=0)throw Error("plugin_source","Expected an actual mono/stereo PCM file");
    auto p=instantiate(*format,d,reader->sampleRate,256);
    const auto current=p->getPluginDescription();
    if(current.version!=d.version || current.manufacturerName!=d.manufacturerName || current.createIdentifierString()!=d.createIdentifierString())
        throw Error("plugin_stale","Actual SDK plugin identity/version differs from the captured description; rescan required");
    auto layout=p->getBusesLayout();for(int i=0;i<layout.inputBuses.size();++i)layout.inputBuses.set(i,i==0?juce::AudioChannelSet::stereo():juce::AudioChannelSet::disabled());
    for(int i=0;i<layout.outputBuses.size();++i)layout.outputBuses.set(i,i==0?juce::AudioChannelSet::stereo():juce::AudioChannelSet::disabled());
    if(layout.outputBuses.isEmpty() || !p->setBusesLayout(layout) || p->getTotalNumOutputChannels()!=2 || p->getTotalNumInputChannels()>2)
        throw Error("plugin_layout","Actual plugin rejected this stereo file-processing layout; no silent downmix");
    if(p->getTotalNumInputChannels()==0)throw Error("plugin_instrument","File processing requires an audio-effect input; instrument MIDI rendering is a separate unfinished path");
    const auto stateFile=fs::path(desc.at("opaque_state").at("path").get<std::string>());
    if(!fs::is_regular_file(stateFile) || sha256(stateFile)!=desc.at("opaque_state").at("sha256").get<std::string>())throw Error("plugin_state","Actual captured state is missing/changed");
    if(fs::file_size(stateFile)>limits.stateBytes)throw Error("plugin_state_resources","Opaque state exceeds budget");
    juce::MemoryBlock original;if(!juce::File(juce::String(stateFile.string())).loadFileAsData(original))throw Error("plugin_state","Cannot read retained state");
    if(original.getSize()!=desc.at("opaque_state").at("bytes").get<std::size_t>() || juce::SHA256(original.getData(),original.getSize()).toHexString().toStdString()!=desc.at("opaque_state").at("sha256").get<std::string>())throw Error("plugin_state","Opaque state bytes changed during read");
    p->setStateInformation(original.getData(),static_cast<int>(original.getSize()));
    if(!stereoMainOnly(*p))
        throw Error("plugin_layout_changed","State restoration changed the admitted stereo layout");
    // Query again after state restoration; indices/parameters may have changed.
    auto actual=parameters(*p,limits);std::map<std::string,juce::HostedAudioProcessorParameter*> handles;
    for(int i=0;i<p->getParameters().size();++i)if(auto* h=p->getHostedParameter(i)){const auto id=h->getParameterID().toStdString();if(!id.empty())handles.emplace(id,h);}
    std::set<std::string> changed;for(const auto& change:request.at("parameters")) {
        if(!change.is_object() || change.size()!=2 || !change.contains("id") || !change.contains("normalized") || !change.at("normalized").is_number())throw Error("plugin_parameter_schema","Use actual parameter id and normalized value only");
        const auto id=change.at("id").get<std::string>();const auto value=change.at("normalized").get<double>();
        if(!handles.contains(id) || !changed.insert(id).second || !std::isfinite(value) || value<0 || value>1)throw Error("plugin_parameter","Nonexistent/duplicate parameter or invalid SDK-normalized value");
        handles.at(id)->setValueNotifyingHost(static_cast<float>(value));
    }
    p->setNonRealtime(true);p->setRateAndBufferSizeDetails(reader->sampleRate,256);p->prepareToPlay(reader->sampleRate,256);
    const auto latency=p->getLatencySamples();if(latency<0 || latency>1000000)throw Error("plugin_latency","Actual reported latency exceeds offline admission budget");
    OfflinePlayhead playhead;playhead.rate=reader->sampleRate;p->setPlayHead(&playhead);
    struct PlayheadScope {std::unique_ptr<juce::AudioPluginInstance>& instance;~PlayheadScope(){if(instance)instance->setPlayHead(nullptr);}} playheadScope{p};
    fs::create_directories(output.parent_path());
    const auto stageDirectory=output.parent_path()/(".ndaw-plugin-"+job.filename().string());
    if(!fs::create_directory(stageDirectory))throw Error("plugin_write","Private staging directory conflict; existing files preserved");
    fs::permissions(stageDirectory,fs::perms::owner_all,fs::perm_options::replace);
    const auto stage=stageDirectory/"audio.partial.wav";
    std::unique_ptr<juce::OutputStream> stream=std::make_unique<juce::FileOutputStream>(juce::File(juce::String(stage.string())));
    if(!static_cast<juce::FileOutputStream*>(stream.get())->openedOk())throw Error("plugin_write","Cannot create new staged output");
    juce::WavAudioFormat wav;const auto options=juce::AudioFormatWriterOptions{}.withSampleRate(reader->sampleRate).withNumChannels(2).withBitsPerSample(32)
        .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto writer=wav.createWriterFor(stream,options);if(!writer)throw Error("plugin_write","Cannot create actual float32 WAV writer");
    juce::AudioBuffer<float> buffer(2,256);juce::MidiBuffer midi;std::vector<double> times;Frame written=0;const Frame total=reader->lengthInSamples+latency;double maximum=0;
    for(Frame at=0;at<total;at+=256){const int n=static_cast<int>(std::min<Frame>(256,total-at));buffer.clear();
        const int available=static_cast<int>(std::clamp<Frame>(reader->lengthInSamples-at,0,n));
        if(available>0 && !reader->read(&buffer,0,available,at,true,true))throw Error("plugin_read","Actual input read failed");
        if(reader->numChannels==1)buffer.copyFrom(1,0,buffer,0,0,n);
        for(int ch=0;ch<2;++ch)for(int i=0;i<n;++i)if(!std::isfinite(buffer.getSample(ch,i)))
            throw Error("plugin_source_nonfinite","Input contains non-finite PCM; retained without modification");
        buffer.setSize(2,n,true,false,true);midi.clear();playhead.position=at;
        const auto began=std::chrono::steady_clock::now();p->processBlock(buffer,midi);
        times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-began).count());
        if(p->getLatencySamples()!=latency)throw Error("plugin_latency_changed","Plugin latency changed during render; this output cannot be accepted");
        for(int ch=0;ch<2;++ch)for(int i=0;i<n;++i){const double v=buffer.getSample(ch,i);if(!std::isfinite(v))throw Error("plugin_nonfinite","Plugin produced non-finite PCM");maximum=std::max(maximum,std::abs(v));}
        const int skip=static_cast<int>(std::clamp<Frame>(latency-at,0,n));const int count=static_cast<int>(std::min<Frame>(n-skip,reader->lengthInSamples-written));
        if(count>0){if(!writer->writeFromAudioSampleBuffer(buffer,skip,count))throw Error("plugin_write","Actual processed WAV write failed");written+=count;}
        buffer.setSize(2,256,true,false,true);
    }
    p->setPlayHead(nullptr);const auto resultingParameters=parameters(*p,limits);const auto finalState=snapshot(*p,limits,job,999);
    p->releaseResources();p.reset();writer.reset();syncFile(stage);
    if(written!=reader->lengthInSamples)throw Error("plugin_frames","Output frame count differs from source duration");
    auto media=inspectMedia(stage);if(media.at("frames")!=written || media.at("channels")!=2 || media.at("sample_rate")!=reader->sampleRate || !media.at("floating_pcm").get<bool>())throw Error("plugin_export","Staged output format failed actual decode validation");
    const auto decodedMeasurement=measureDecodedFile(files,stage);
    // New external side effect, no replacement and no claim of Undo deleting it.
    fs::create_directories(output.parent_path());
#ifdef _WIN32
    if(!MoveFileExW(stage.c_str(),output.c_str(),MOVEFILE_WRITE_THROUGH))throw Error("plugin_publish","No-replace output publication failed; staged file retained");
#else
    if(::link(stage.c_str(),output.c_str())!=0)throw Error("plugin_publish","No-replace output publication failed; staged file retained");
    fs::remove(stage);
#endif
    fs::remove(stageDirectory);
    syncFile(output);
#ifndef _WIN32
    const int dirFd=::open(output.parent_path().c_str(),O_RDONLY|O_DIRECTORY);
    if(dirFd<0)throw Error("plugin_publish","Cannot open output directory for durability check; new output retained");
    const int syncResult=::fsync(dirFd);::close(dirFd);
    if(syncResult!=0)throw Error("plugin_publish","Output directory durability check failed; new output retained");
#endif
    media=inspectMedia(output);std::sort(times.begin(),times.end());
    return {{"plugin_id",desc.at("id")},{"parameters_after_restore",actual},{"parameters_after_processing",resultingParameters},{"final_state",finalState},
        {"export",{{"status","exported_and_verified"},{"path",output.string()},{"format",media},{"frames",written},{"measurement",decodedMeasurement},{"maximum_sample_peak",decodedMeasurement.at("sample_peak")},{"internal_block_sample_peak",maximum},
            {"algorithmic_latency_removed_frames",latency},{"processing_precision","plugin float32; plugin double support is only reported, not qualified"},
            {"tail_policy","fixed source duration; extended reverb/instrument tails not rendered"},{"side_effect","new external file; no implicit Undo deletion"}}},
        {"playhead_context",{{"bpm",120},{"meter",Json::array({4,4})},{"scope","explicit standalone file defaults; not session tempo or meter"}}},
        {"processing_blocks",times.size()},{"process_p99_us",times[static_cast<std::size_t>((times.size()-1)*.99)]},{"process_max_us",times.back()},
        {"verification_scope","actual isolated offline SDK plugin processing and decoded new file; no realtime, editor, MIDI, automation or listening acceptance"}};
}
#include "PluginEditorHost.h"
static Json stream(const Json& request,const PluginLimits& limits,const fs::path& job) {
    const auto ipc=fs::path(request.at("ipc_path").get<std::string>());if(ipc!=job/"ipc.bin")throw Error("plugin_ipc","IPC must be owned by this job");
    PluginMapping mapping(ipc,false);auto& shared=mapping.data();
    struct FaultGuard {PluginShared& shared;bool complete=false;~FaultGuard(){if(!complete){std::uint32_t none=0;shared.fault.compare_exchange_strong(none,static_cast<std::uint32_t>(PluginFault::Child));}}} guard{shared};
    const auto& desc=request.at("plugin");auto format=formatFor(desc.at("format"));
    auto xml=juce::parseXML(juce::String(desc.at("description_xml").get<std::string>()));juce::PluginDescription d;
    if(!xml || !d.loadFromXml(*xml) || describe(d).at("id")!=desc.at("id") || d.fileOrIdentifier.toStdString()!=desc.at("file_or_identifier").get<std::string>() || d.version.toStdString()!=desc.at("version").get<std::string>())throw Error("plugin_description","Invalid saved actual SDK description");
    const int rate=request.at("sample_rate");if(!std::set<int>{44100,48000,88200,96000,176400,192000}.contains(rate))throw Error("plugin_rate","Invalid project sample rate");
    auto p=instantiate(*format,d,rate,pluginQuantum);const auto current=p->getPluginDescription();
    if(current.version!=d.version || current.manufacturerName!=d.manufacturerName || current.createIdentifierString()!=d.createIdentifierString())throw Error("plugin_stale","Actual SDK identity/version differs; saved reference retained");
    auto layout=p->getBusesLayout();if(layout.inputBuses.isEmpty() || layout.outputBuses.isEmpty() || d.isInstrument)throw Error("plugin_layout","Actual stereo effect required");
    for(int i=0;i<layout.inputBuses.size();++i)layout.inputBuses.set(i,i==0?juce::AudioChannelSet::stereo():juce::AudioChannelSet::disabled());
    for(int i=0;i<layout.outputBuses.size();++i)layout.outputBuses.set(i,i==0?juce::AudioChannelSet::stereo():juce::AudioChannelSet::disabled());
    if(!p->setBusesLayout(layout) || p->getTotalNumInputChannels()!=2 || p->getTotalNumOutputChannels()!=2)throw Error("plugin_layout","Actual plugin rejected the stereo main-only layout");
    const auto state=fs::path(desc.at("opaque_state").at("path").get<std::string>());
    if(!fs::is_regular_file(state) || fs::file_size(state)>limits.stateBytes || fs::file_size(state)!=desc.at("opaque_state").at("bytes").get<std::size_t>() || sha256(state)!=desc.at("opaque_state").at("sha256").get<std::string>())throw Error("plugin_state","Saved opaque state is missing/changed");
    juce::MemoryBlock data;if(!juce::File(juce::String(state.string())).loadFileAsData(data))throw Error("plugin_state","Cannot read saved opaque state");
    if(data.getSize()!=desc.at("opaque_state").at("bytes").get<std::size_t>() || juce::SHA256(data.getData(),data.getSize()).toHexString().toStdString()!=desc.at("opaque_state").at("sha256").get<std::string>())throw Error("plugin_state","Opaque state bytes changed during read");p->setStateInformation(data.getData(),static_cast<int>(data.getSize()));
    if(!stereoMainOnly(*p))throw Error("plugin_layout_changed","Opaque state changed the admitted layout");
    const auto restored=parameters(*p,limits);std::map<std::string,juce::HostedAudioProcessorParameter*> handles;
    for(int i=0;i<p->getParameters().size();++i)if(auto* h=p->getHostedParameter(i)){const auto id=h->getParameterID().toStdString();if(!id.empty())handles.emplace(id,h);}
    std::set<std::string> changed;for(const auto& change:request.at("parameters")) {
        if(!change.is_object() || change.size()!=2 || !change.contains("id") || !change.contains("normalized") || !change.at("normalized").is_number())throw Error("plugin_parameter_schema","Use actual parameter ID/normalized value");
        const auto id=change.at("id").get<std::string>();const auto v=change.at("normalized").get<double>();
        if(!handles.contains(id) || !changed.insert(id).second || !std::isfinite(v) || v<0 || v>1)throw Error("plugin_parameter","Actual parameter ID/value rejected");handles.at(id)->setValueNotifyingHost(static_cast<float>(v));
    }
    const auto mode=request.at("mode").get<std::string>();if(mode!="offline" && mode!="realtime")throw Error("plugin_mode","Unknown actual processing mode");
    p->setNonRealtime(mode=="offline");p->setRateAndBufferSizeDetails(rate,pluginQuantum);p->prepareToPlay(rate,pluginQuantum);
    const auto latency=p->getLatencySamples();if(latency!=request.at("expected_latency_frames").get<int>())throw Error("plugin_latency_changed","Actual latency differs from saved PDC; reject and rescan/replan");
    class Playhead final:public juce::AudioPlayHead {public:Frame position{};int rate{};bool playing{},recording{};
        juce::Optional<PositionInfo> getPosition() const override {PositionInfo info;info.setTimeInSamples(position);info.setTimeInSeconds(static_cast<double>(position)/rate);info.setBpm(120);info.setTimeSignature(TimeSignature{4,4});info.setIsPlaying(playing);info.setIsRecording(recording);return info;}} playhead;
    playhead.rate=rate;p->setPlayHead(&playhead);struct ClearPlayhead{std::unique_ptr<juce::AudioPluginInstance>& p;~ClearPlayhead(){if(p)p->setPlayHead(nullptr);}} clear{p};
    const auto preparedParameters=parameters(*p,limits);std::vector<juce::HostedAudioProcessorParameter*> indexed(p->getParameters().size());
    const auto initialToken=request.at("control_token").get<PluginControlToken>();PluginControlToken appliedToken=initialToken;
    for(const auto& parameter:preparedParameters)if(!parameter.at("id").is_null()){
        const auto index=parameter.at("index").get<std::size_t>();if(index>=indexed.size() || index>=pluginParameterLimit)throw Error("plugin_parameter","Prepared SDK index exceeds admitted storage");
        auto* h=p->getHostedParameter(static_cast<int>(index));if(!h || h->getParameterID().toStdString()!=parameter.at("id").get<std::string>())throw Error("plugin_parameter","Prepared SDK ID/index changed");indexed[index]=h;
        const auto desired=std::find_if(request.at("parameters").begin(),request.at("parameters").end(),[&](const Json& x){return x.at("id")==parameter.at("id");});
        const float expected=desired==request.at("parameters").end()?h->getValue():static_cast<float>(desired->at("normalized").get<double>());
        if(!std::isfinite(h->getValue()) || std::abs(h->getValue()-expected)>1e-6f)throw Error("plugin_parameter_rejected","SDK changed a requested parameter during prepare");
        shared.requestedValues[index].store(expected);shared.actualValues[index].store(h->getValue());
    }
    Json ready{{"protocol",pluginProtocol},{"job_id",request.at("job_id")},{"status","prepared"},{"plugin_id",desc.at("id")},
        {"worker_pid",pluginOwnerPid()},
        {"sample_rate",rate},{"max_block",pluginQuantum},{"pipeline_latency_frames",pluginPipeline},{"reported_latency_frames",latency},
        {"parameters_after_state_restore",restored},{"parameters",preparedParameters},{"buses",buses(*p)},
        {"control_stream_protocol",pluginStreamProtocol},{"initial_control_token",pluginControlText(initialToken)},
        {"scope","actual isolated stereo SDK effect prepared; not a completed audio/latency/editor receipt"}};
    atomicWrite(job/"ready.json",ready.dump(),false);shared.status.store(1,std::memory_order_release);
    juce::AudioBuffer<float> buffer(2,pluginQuantum);juce::MidiBuffer midi;std::uint64_t epoch=UINT64_MAX;auto lastDispatch=std::chrono::steady_clock::now();
    SDKEditorPreview editor(*p,limits,shared,job,desc.at("id"),latency);
    while(!shared.stop.load(std::memory_order_acquire) && !shared.fault.load(std::memory_order_acquire)) {
        shared.heartbeat.fetch_add(1,std::memory_order_release);PluginSlot* next=nullptr;
        for(auto& slot:shared.slots)if(slot.state.load(std::memory_order_acquire)==1 && (!next || slot.sequence<next->sequence))next=&slot;
        if(!next || editor.active())editor.serve();
        if(editor.active())next=nullptr; // only the explicit stopped engine lease admits an editor
        if(next){std::uint32_t expected=1;if(!next->state.compare_exchange_strong(expected,2,std::memory_order_acq_rel))continue;
            if(next->epoch!=shared.epoch.load(std::memory_order_acquire)){next->state.store(3,std::memory_order_release);continue;}
            if(epoch!=next->epoch){p->reset();epoch=next->epoch;}
            const auto began=std::chrono::steady_clock::now(); // includes actual SDK parameter work
            auto rejectControl=[&]{std::uint32_t none=0;shared.fault.compare_exchange_strong(none,static_cast<std::uint32_t>(PluginFault::Parameter));};
            bool controlValid=next->controlCount<=pluginParameterLimit;std::array<bool,pluginParameterLimit> seen{};
            for(std::uint32_t i=0;controlValid && i<next->controlCount;++i){const auto& v=next->controls[i];
                if(v.index>=indexed.size() || !indexed[v.index] || seen[v.index] || !std::isfinite(v.normalized) || v.normalized<0 || v.normalized>1){controlValid=false;break;}
                seen[v.index]=true;}
            if(!controlValid || (!next->controlCount && next->requestedControl!=appliedToken)){rejectControl();break;}
            for(std::uint32_t i=0;i<next->controlCount;++i){const auto& v=next->controls[i];indexed[v.index]->setValueNotifyingHost(v.normalized);}
            playhead.position=next->timeline;playhead.playing=next->playing;playhead.recording=next->recording;
            for(int ch=0;ch<2;++ch)std::copy_n(next->input[ch],pluginQuantum,buffer.getWritePointer(ch));midi.clear();
            {juce::ScopedNoDenormals denormals;p->processBlock(buffer,midi);}
            if(p->getLatencySamples()!=latency){std::uint32_t none=0;shared.fault.compare_exchange_strong(none,static_cast<std::uint32_t>(PluginFault::Latency));break;}
            bool finite=true;for(int ch=0;ch<2;++ch)for(int i=0;i<pluginQuantum;++i){const auto v=buffer.getSample(ch,i);if(!std::isfinite(v))finite=false;next->output[ch][i]=v;}
            if(!finite){std::uint32_t none=0;shared.fault.compare_exchange_strong(none,static_cast<std::uint32_t>(PluginFault::NonFinite));break;}
            std::array<float,pluginParameterLimit> readbacks;
            for(std::uint32_t i=0;controlValid && i<next->controlCount;++i){const auto& v=next->controls[i];readbacks[i]=indexed[v.index]->getValue();if(!std::isfinite(readbacks[i]) || std::abs(readbacks[i]-v.normalized)>1e-6f)controlValid=false;}
            if(!controlValid){rejectControl();break;}
            if(next->controlCount || shared.controlStamp.load()==0){
                shared.controlStamp.fetch_add(1,std::memory_order_acq_rel);
                for(std::uint32_t i=0;i<next->controlCount;++i){const auto& v=next->controls[i];shared.requestedValues[v.index].store(v.normalized,std::memory_order_relaxed);shared.actualValues[v.index].store(readbacks[i],std::memory_order_relaxed);}
                appliedToken=next->requestedControl;for(std::size_t i=0;i<appliedToken.size();++i)shared.acknowledgedControl[i].store(appliedToken[i],std::memory_order_relaxed);
                shared.controlAt.store(next->timeline,std::memory_order_relaxed);shared.controlAppliedNs.store(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(),std::memory_order_relaxed);
                shared.controlUpdates.fetch_add(1);shared.controlStamp.fetch_add(1,std::memory_order_release);
            }
            next->appliedControl=appliedToken; // visible only with the owned finite output slot
            const auto ns=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count());
            shared.maxProcessNs.store(std::max(ns,shared.maxProcessNs.load()));std::size_t bin=0;std::uint64_t bound=1000;while(ns>bound && bin+1<shared.processHistogram.size()){bound*=2;++bin;}shared.processHistogram[bin].fetch_add(1);
            std::uint64_t nonzero=0;for(int i=0;i<pluginQuantum;++i)if(std::abs(next->output[0][i])>1e-12f || std::abs(next->output[1][i])>1e-12f)++nonzero;
            if(next->playing)shared.playingFrames.fetch_add(pluginQuantum);shared.nonzeroOutputFrames.fetch_add(nonzero);
            shared.processed.fetch_add(1);next->state.store(3,std::memory_order_release);
        }else std::this_thread::sleep_for(std::chrono::microseconds(50));
        if(std::chrono::steady_clock::now()-lastDispatch>std::chrono::milliseconds(editor.active()?5:100)){juce::MessageManager::getInstance()->runDispatchLoopUntil(1);lastDispatch=std::chrono::steady_clock::now();}
    }
    if(shared.fault.load())throw Error("plugin_stream_failed","Shared stream fault "+std::to_string(shared.fault.load()));
    editor.shutdown();const auto finalState=snapshot(*p,limits,job,0);const auto finalParameters=parameters(*p,limits);p->releaseResources();p->setPlayHead(nullptr);p.reset();guard.complete=true;shared.status.store(2,std::memory_order_release);
    return {{"plugin_id",desc.at("id")},{"processed_quanta",shared.processed.load()},{"final_state",finalState},{"parameters",finalParameters},
        {"scope","actual SDK stream released/destroyed; parent must also validate normal process exit"}};
}
Json runPluginJob(const Json& request,const fs::path& job) {
    PluginLimits limits;const auto& b=request.at("limits");limits.timeoutMs=b.at("timeout_ms");limits.exitGraceMs=b.at("exit_grace_ms");limits.pollMs=b.at("poll_ms");
    limits.responseBytes=b.at("response_bytes");limits.stateBytes=b.at("state_bytes");limits.parameterCount=b.at("parameter_count");limits.rssBytes=b.at("rss_bytes");limits.validate();
    const auto action=request.at("action").get<std::string>();Json out{{"protocol",pluginProtocol},{"job_id",request.at("job_id")},{"action",action},{"status","succeeded"}};
    if(action=="discover") {
        Json candidates=Json::array();std::vector<std::string> names{"VST3"};
#if JUCE_MAC
        names.push_back("AudioUnit");
#endif
        for(const auto& name:names){auto f=formatFor(name);for(const auto& id:f->searchPathsForPlugins(f->getDefaultLocationsToSearch(),true,true))
            candidates.push_back({{"format",name},{"candidate",id.toStdString()},{"scan_verified",false}});}
        out["candidates"]=candidates;out["scope"]="actual SDK candidates, not instantiated/scanned plugins";
    } else if(action=="scan") {auto f=formatFor(request.at("format"));out["plugins"]=scan(*f,request.at("candidate"),limits,job);}
    else if(action=="process_file")out.update(processFile(request,limits,job));
    else if(action=="stream")out["stream"]=stream(request,limits,job);
    else throw Error("plugin_action","Unsupported worker action; no arbitrary code/tool execution");
    return out;
}
}
