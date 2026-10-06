// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/PluginScanning.h>
#include <set>
#include <cmath>
namespace ndaw::v2 {
static std::unique_ptr<juce::AudioPluginFormat> formatFor(const std::string& name) {
    if(name=="VST3")return std::make_unique<juce::VST3PluginFormat>();
#if JUCE_MAC
    if(name=="AudioUnit")return std::make_unique<juce::AudioUnitPluginFormat>();
#endif
    throw ScanError("plugin_format","Requested actual plugin format is unavailable on this platform");
}
static std::unique_ptr<juce::AudioPluginInstance> instantiate(juce::AudioPluginFormat& format,const juce::PluginDescription& d,double rate,int block) {
    std::unique_ptr<juce::AudioPluginInstance> instance;bool finished=false;juce::String error;
    format.createPluginInstanceAsync(d,rate,block,[&](std::unique_ptr<juce::AudioPluginInstance> p,const juce::String& e){instance=std::move(p);error=e;finished=true;});
    while(!finished)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    // The supervisor owns a finite process deadline even if a plugin or SDK
    // never returns/calls back; no such wait can enter the parent/audio thread.
    if(!instance)throw ScanError("plugin_instantiate","Actual instance creation failed: "+error.toStdString());return instance;
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
    if(static_cast<std::size_t>(p.getParameters().size())>limits.parameterCount)throw ScanError("plugin_resources","Actual parameter metadata exceeds declared budget");
    Json result=Json::array();std::set<std::string> ids;
    for(int index=0;index<p.getParameters().size();++index) {
        auto* param=p.getParameters()[index];auto* hosted=p.getHostedParameter(index);const auto id=hosted?hosted->getParameterID().toStdString():"";
        if(!id.empty() && !ids.insert(id).second)throw ScanError("plugin_parameter_id","Actual SDK parameter IDs are duplicated");
        const auto value=param->getValue(),def=param->getDefaultValue();
        if(!std::isfinite(value) || value<0 || value>1 || !std::isfinite(def) || def<0 || def>1)throw ScanError("plugin_parameter_value","Plugin reported an invalid normalized value");
        const int steps=param->getNumSteps();Json enumeration=Json::array();
        if(param->isDiscrete() && steps>=2 && steps<=256)for(int i=0;i<steps;++i){const auto v=static_cast<float>(i)/static_cast<float>(steps-1);enumeration.push_back({{"normalized",v},{"display",param->getText(v,128).toStdString()}});}
        result.push_back({{"id",id.empty()?Json(nullptr):Json(id)},{"index",index},{"name",param->getName(128).toStdString()},
            {"unit_label",param->getLabel().toStdString()},{"normalized_range",{{"min",0.0},{"max",1.0}}},{"value",value},{"default",def},
            {"display",param->getText(value,128).toStdString()},{"display_at_zero",param->getText(0,128).toStdString()},
            {"display_at_one",param->getText(1,128).toStdString()},{"discrete",param->isDiscrete()},{"boolean",param->isBoolean()},
            {"steps",steps},{"automatable",param->isAutomatable()},{"model_editable",param->isAutomatable()&&!id.empty()},{"enum_values",enumeration},
            {"plain_unit_mapping","not inferred from text; host SDK normalized values only"},
            {"enum_complete",param->isDiscrete() && steps>=2 && steps<=256}});
    }return result;
}
static Json snapshot(juce::AudioPluginInstance& p,const PluginLimits& limits,const fs::path& job,int index) {
    juce::MemoryBlock state;p.getStateInformation(state);
    if(state.getSize()>limits.stateBytes)throw ScanError("plugin_state_resources","Actual opaque state exceeds declared budget");
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
    if(descriptions.size()>64)throw ScanError("plugin_types","Candidate exposes more than 64 types");
    if(descriptions.isEmpty())throw ScanError("plugin_scan","SDK found no actual types for candidate");
    Json result=Json::array();std::set<std::string> ids;
    for(int i=0;i<descriptions.size();++i){auto p=i==0 && initialInstance?std::move(initialInstance):instantiate(format,*descriptions[i],48000,256);
        p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
        auto d=describe(p->getPluginDescription());if(!ids.insert(d.at("id")).second)throw ScanError("plugin_identity","Ambiguous actual plugin identity; no overwritten inventory");
        d["parameters"]=parameters(*p,limits);d["buses"]=buses(*p);d["reported_latency_frames"]=p->getLatencySamples();
        d["tail_seconds"]=p->getTailLengthSeconds();d["supports_double_precision"]=p->supportsDoublePrecisionProcessing();
        d["has_editor"]=p->hasEditor();d["program_count"]=p->getNumPrograms();d["opaque_state"]=snapshot(*p,limits,job,i);
        d["prepared_sample_rate"]=48000;d["prepared_max_block"]=256;d["scan_verified"]=true;d["audio_processing_verified"]=false;
        d["automation_precision_verified"]=false;p->releaseResources();p.reset();result.push_back(std::move(d));}
    return result;
}

Json runPluginScan(const Json& request,const fs::path& job){
 PluginLimits limits;const auto& l=request.at("limits");limits.timeoutMs=l.at("timeout_ms");limits.responseBytes=l.at("response_bytes");limits.stateBytes=l.at("state_bytes");limits.parameterCount=l.at("parameter_count");limits.validate();
 Json out={{"protocol",pluginProtocol},{"job_id",request.at("job_id")},{"action",request.at("action")},{"status","succeeded"}};
 const std::string action=request.at("action");
 if(action=="discover"){Json candidates=Json::array();for(const auto& name:{"VST3",
#if JUCE_MAC
 "AudioUnit",
#endif
 }){auto f=formatFor(name);for(const auto& id:f->searchPathsForPlugins(f->getDefaultLocationsToSearch(),true,true)){if(candidates.size()>=2048)throw ScanError("plugin_inventory","Candidate inventory exceeds budget");candidates.push_back({{"format",name},{"candidate",id.toStdString()},{"scan_verified",false}});}}
 out["candidates"]=candidates;out["scope"]="SDK candidate inventory only; no instantiation";
 }else if(action=="scan")out["plugins"]=scan(*formatFor(request.at("format")),request.at("candidate"),limits,job);
 else throw ScanError("plugin_action","Scanner only supports discover and scan");
 return out;
}
}
