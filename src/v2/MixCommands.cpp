#include <nativedaw/v2/EngineCommands.h>
#include "NativePluginStates.h"

namespace ndaw::v2 {
namespace {
void require(bool condition,const char* reason) {if(!condition)throw std::runtime_error(reason);}
bool supported(te::Plugin& plugin) {
    return dynamic_cast<te::EqualiserPlugin*>(&plugin) || dynamic_cast<te::CompressorPlugin*>(&plugin)
        || dynamic_cast<te::ExternalPlugin*>(&plugin) || dynamic_cast<te::ReverbPlugin*>(&plugin) || dynamic_cast<te::DelayPlugin*>(&plugin) || dynamic_cast<te::FourOscPlugin*>(&plugin);
}
Json property(const char* type) {return {{"type",type}};}
struct ExternalParameterAction final:juce::UndoableAction{
    ExternalParameterAction(te::Plugin& p,te::AutomatableParameter& a,float value):edit(p.edit),id(p.itemID),parameter(a.paramID),before(a.getCurrentExplicitValue()),after(value){}
    bool perform()override{return set(after);}bool undo()override{return set(before);}int getSizeInUnits()override{return 1;}
    bool set(float value){if(auto p=edit.getPluginCache().getPluginFor(id))if(auto a=p->getAutomatableParameterByID(parameter)){a->setParameter(value,juce::dontSendNotification);
        // ValueTree Undo can restore the wrapper's cached base before this
        // action runs. Force the real processor as well, even if equal to cache.
        if(auto* ext=dynamic_cast<te::ExternalPlugin*>(p.get()))if(auto* instance=ext->getAudioPluginInstance()){
          const auto& params=instance->getParameters();for(int i=0;i<params.size();++i){auto* withID=dynamic_cast<juce::AudioProcessorParameterWithID*>(params[i]);if((withID?withID->paramID:juce::String(i))==parameter){params[i]->setValue(value);break;}}
        }
        return true;}return false;}
    te::Edit& edit;te::EditItemID id;juce::String parameter;float before,after;
};
struct ParameterBaseAction final : juce::UndoableAction {
    ParameterBaseAction(te::Plugin& p,juce::String parameter,bool after)
        :edit(p.edit),id(p.itemID),parameter(std::move(parameter)),after(after){}
    bool perform() override {return !after || sync();}
    bool undo() override {return after || sync();}
    int getSizeInUnits() override {return 1;}
    bool sync() {
        if(auto p=edit.getPluginCache().getPluginFor(id))
            if(auto a=p->getAutomatableParameterByID(parameter)){a->updateFromAttachedValue();return true;}
        return false;
    }
    te::Edit& edit;te::EditItemID id;juce::String parameter;bool after;
};
}
Json Commands::processorCatalog() {
    return Json::array({{{"type",te::EqualiserPlugin::xmlTypeName},{"name","Equaliser"}},
        {{"type",te::CompressorPlugin::xmlTypeName},{"name","Compressor"}},
        {{"type",te::ReverbPlugin::xmlTypeName},{"name","Reverb"}},
        {{"type",te::DelayPlugin::xmlTypeName},{"name","Delay"}},
        {{"type",te::FourOscPlugin::xmlTypeName},{"name","FourOsc"}}});
}
void Commands::registerProcessorCommands(Json& registry) {
    auto add=[&](const char* id,Json properties) {
        Json required=Json::array();for(auto it=properties.begin();it!=properties.end();++it)required.push_back(it.key());
        registry.push_back({{"id",id},{"schema",{{"type","object"},{"properties",properties},{"required",required},{"additionalProperties",false}}},
            {"permission","edit"},{"risk","low"},{"reversible",true},{"live",false},{"test","M1-FX-01"}});
    };
    add("plugin.insert",{{"track",property("string")},{"type",property("string")}});
    registry.back()["schema"]["properties"]["wet_only"]=property("boolean");
    registry.back()["units"]={{"wet_only","Reverb only: actual dry level=0, wet level=1/3 (SDK wet gain 0 dB)"}};
    add("plugin.external.insert",{{"track",property("string")},{"descriptor",property("string")},{"module_hash",property("string")}});registry.back()["test"]="M1-EXT-01";registry.back()["risk"]="medium";
    add("plugin.parameter",{{"plugin",property("string")},{"parameter",property("string")},{"value",property("number")}});
    add("plugin.bypass",{{"plugin",property("string")},{"bypassed",property("boolean")}});
    add("plugin.remove",{{"plugin",property("string")}});
    add("plugin.delay_time",{{"plugin",property("string")},{"ms",{{"type","integer"},{"minimum",1},{"maximum",2000}}}});
    registry.back()["units"]={{"ms","milliseconds; stopped transport only; SDK property, not an automatable parameter"}};
    registry[registry.size()-4]["units"]={{"value","native SDK parameter range; resolve from queried plugin instance"}};
    add("plugin.program",{{"plugin",property("string")},{"index",{{"type","integer"},{"minimum",0}}}});registry.back()["test"]="M1-STATE-01";registry.back()["risk"]="medium";
    registry.back()["units"]={{"index","zero-based native SDK program index; does not enumerate a proprietary preset browser"}};
}
te::Plugin* Commands::processor(const std::string& id) const {
    for(auto* t:te::getAudioTracks(*edit))
        for(auto* p:t->pluginList)
            if(p->itemID.toString().toStdString()==id && supported(*p))return p;
    return nullptr;
}
Json Commands::processorQuery(te::AudioTrack& track) const {
    Json plugins=Json::array();
    for(auto* plugin:track.pluginList) {
        if(!supported(*plugin))continue;
        Json parameters=Json::array();
        for(auto* p:plugin->getAutomatableParameters())
            parameters.push_back({{"id",p->paramID.toStdString()},{"name",p->getParameterName().toStdString()},
                {"value",p->getCurrentExplicitValue()},{"current_value",p->getCurrentValue()},{"minimum",p->valueRange.start},{"maximum",p->valueRange.end},
                {"interval",p->valueRange.interval},{"skew",p->valueRange.skew},{"unit",p->getLabel().toStdString()},
                {"display",p->getCurrentValueAsStringWithLabel().toStdString()},{"automatable",true}});
        Json result{{"id",plugin->itemID.toString().toStdString()},{"type",plugin->getPluginType().toStdString()},
            {"name",plugin->getName().toStdString()},{"bypassed",!plugin->isEnabled()},{"parameters",parameters}};
        if(auto* ext=dynamic_cast<te::ExternalPlugin*>(plugin)){
            const bool loaded=ext->getAudioPluginInstance()!=nullptr,allowed=mayLoadExternal(*ext);
            result["external"]={{"descriptor",ext->state.getProperty("ndaw_external_descriptor").toString().toStdString()},{"format",ext->desc.pluginFormatName.toStdString()},{"version",ext->desc.version.toStdString()},{"manufacturer",ext->desc.manufacturerName.toStdString()},{"loaded",loaded},{"allowed",allowed},{"initialising",ext->isInitialisingAsync()},{"load_error",ext->getLoadError().toStdString()},{"status",!allowed?"blocked":loaded?"loaded":"missing"},{"latency_seconds",ext->getLatencySeconds()},{"host_mode","in_process"},{"fixed_ipc_latency_frames",0},{"has_editor",loaded&&ext->getAudioPluginInstance()->hasEditor()},{"private_state_interpreted",false},{"sdk_parameter_count",loaded?ext->getAudioPluginInstance()->getParameters().size():0},{"command_parameter_count",parameters.size()}};
            if(loaded){auto& native=*ext->getAudioPluginInstance();int index=native.getCurrentProgram();result["external"]["program_count"]=native.getNumPrograms();result["external"]["program_index"]=index;result["external"]["program_name"]=index>=0&&index<native.getNumPrograms()?native.getProgramName(index).toStdString():std::string{};}
            juce::MemoryBlock blob;ext->getPluginStateFromTree(blob);result["external"]["saved_state_bytes"]=blob.getSize();result["external"]["saved_state_hash"]=juce::SHA256(blob.getData(),blob.getSize()).toHexString().toStdString();
            for(auto& a:result["parameters"]){a["unit_mapping"]="native Tracktion normalized range; formatted text is not a plain-unit conversion";a["identity_scope"]="Tracktion persisted parameter ID, format adapter index for AU/VST3";}
        }
        if(auto* delay=dynamic_cast<te::DelayPlugin*>(plugin))result["delay_time_ms"]=delay->lengthMs.get();
        plugins.push_back(result);
    }
    return plugins;
}
void Commands::validateProcessorOperation(const std::string& cmd,const Json& a) const {
    if(cmd=="plugin.external.insert"){auto p=externalDescriptor(a.at("descriptor"));require(p["module_hash"]==a["module_hash"],"external module hash changed since planning");if(auto* t=track(a.at("track")))require(t->pluginList.canInsertPlugin(),"processor resource limit reached");return;}
    if(cmd=="plugin.insert") {
        const auto catalog=processorCatalog();
        require(std::any_of(catalog.begin(),catalog.end(),[&](const auto& p){return p["type"]==a["type"];}),"unsupported processor type");
        require(!a.value("wet_only",false) || a["type"]==te::ReverbPlugin::xmlTypeName,"wet_only requires Reverb");
        if(auto* t=track(a.at("track")))require(t->pluginList.canInsertPlugin(),"processor resource limit reached");
        return;
    }
    auto* p=processor(a.at("plugin"));require(p!=nullptr,"processor instance not found");
    if(cmd=="plugin.parameter") {
        if(auto* ext=dynamic_cast<te::ExternalPlugin*>(p))require(ext->getAudioPluginInstance()!=nullptr&&mayLoadExternal(*ext),"missing or changed external plugin cannot accept parameter writes");
        auto parameter=p->getAutomatableParameterByID(juce::String(a.at("parameter").get<std::string>()));
        require(parameter!=nullptr,"parameter not enumerated by this instance");
        auto value=a.at("value").get<double>();
        require(std::isfinite(value) && value>=parameter->valueRange.start && value<=parameter->valueRange.end,"value outside actual parameter range");
    } else if(cmd=="plugin.program") {
        auto* ext=dynamic_cast<te::ExternalPlugin*>(p);require(ext&&ext->getAudioPluginInstance()&&mayLoadExternal(*ext),"program requires a loaded allowed external instance");
        auto index=a.at("index").get<int64_t>();require(index>=0&&index<ext->getAudioPluginInstance()->getNumPrograms(),"program index outside actual native range");
    } else if(cmd=="plugin.delay_time") {
        require(dynamic_cast<te::DelayPlugin*>(p)!=nullptr,"time property requires Delay instance");
        auto ms=a.at("ms").get<int64_t>();require(ms>=1 && ms<=2000,"Delay time outside 1..2000 ms");
    }
}
void Commands::executeProcessorOperation(const std::string& cmd,const Json& a,Json& objects) {
    if(cmd=="plugin.insert"||cmd=="plugin.external.insert") {
        auto* t=track(a.at("track"));require(t!=nullptr,"processor target track disappeared");
        te::Plugin::Ptr p;
        if(cmd=="plugin.external.insert"){
            const auto descriptor=externalDescriptor(a.at("descriptor"));juce::PluginDescription desc;auto xml=juce::parseXML(juce::String(descriptor["description_xml"].get<std::string>()));
            require(xml&&desc.loadFromXml(*xml),"scanned external description invalid");
            auto state=te::ExternalPlugin::create(engine,desc);state.setProperty("ndaw_external_descriptor",juce::String(descriptor["id"].get<std::string>()),nullptr);state.setProperty("ndaw_external_module_hash",juce::String(descriptor["module_hash"].get<std::string>()),nullptr);state.setProperty("ndaw_external_description_xml",juce::String(descriptor["description_xml"].get<std::string>()),nullptr);
            p=edit->getPluginCache().createNewPlugin(state);auto* ext=dynamic_cast<te::ExternalPlugin*>(p.get());
            require(ext&&ext->getAudioPluginInstance()!=nullptr&&!ext->isInitialisingAsync(),"external plugin creation failed or requires asynchronous loading; no insertion committed");
            require(ext->getAudioPluginInstance()->getParameters().size()<=8192,"external instance parameter budget exceeded");
            ext->setDeletesPluginInstanceSynchronously(true);externalPreparedRates[ext->itemID.toString().toStdString()]=ext->getAudioPluginInstance()->getSampleRate();externalLayoutChanged(*ext);
        }else p=edit->getPluginCache().createNewPlugin(juce::String(a.at("type").get<std::string>()),{});
        require(p!=nullptr && supported(*p),"processor creation failed");
        auto index=t->pluginList.indexOf(t->getVolumePlugin());
        // Keep Pre sends after the inserted effects and before the fader.
        for(auto* existing:t->pluginList)if(dynamic_cast<te::AuxSendPlugin*>(existing))index=std::min(index,t->pluginList.indexOf(existing));
        t->pluginList.insertPlugin(p,index,nullptr);
        require(t->pluginList.contains(p.get()),"processor insertion failed");
        if(a.value("wet_only",false)) {
            auto& reverb=dynamic_cast<te::ReverbPlugin&>(*p);
            setParameterValue(*p,*reverb.dryParam,0);setParameterValue(*p,*reverb.wetParam,1.0f/3);
        }
        objects.push_back({{"id",p->itemID.toString().toStdString()},{"kind","plugin"}});return;
    }
    auto* p=processor(a.at("plugin"));require(p!=nullptr,"processor disappeared");
    if(cmd=="plugin.parameter") {
        // The SDK records CachedValue actions but Undo deliberately leaves the
        // explicit DSP base unchanged. Bracket that action with ordered sync actions
        // so both values restore inside Undo/Redo, without writing after its boundary.
        auto parameter=p->getAutomatableParameterByID(juce::String(a.at("parameter").get<std::string>()));
        require(parameter!=nullptr,"parameter disappeared");
        setParameterValue(*p,*parameter,a.at("value").get<float>());
    } else if(cmd=="plugin.program") {require(nativeStates!=nullptr,"native state service unavailable");nativeStates->setProgram(a.at("plugin"),a.at("index").get<int>());}
    else if(cmd=="plugin.bypass") p->setEnabled(!a.at("bypassed").get<bool>());
    else if(cmd=="plugin.remove") p->deleteFromParent();
    else if(cmd=="plugin.delay_time") dynamic_cast<te::DelayPlugin&>(*p).lengthMs=a.at("ms").get<int>();
}
void Commands::setParameterValue(te::Plugin& p,te::AutomatableParameter& parameter,float value,juce::NotificationType nt) {
    ParameterWriteGuard guard(*this);
    auto& um=edit->getUndoManager();
    if(dynamic_cast<te::ExternalPlugin*>(&p)&&parameter.paramID!="dry level"&&parameter.paramID!="wet level"){require(um.perform(new ExternalParameterAction(p,parameter,value)),"external parameter Undo setup failed");if(nativeStates)nativeStates->noteParameter(p,parameter,value);return;}
    require(um.perform(new ParameterBaseAction(p,parameter.paramID,false)),"parameter history setup failed");
    parameter.setParameter(value,nt);
    require(um.perform(new ParameterBaseAction(p,parameter.paramID,true)),"parameter history sync failed");
}

}
