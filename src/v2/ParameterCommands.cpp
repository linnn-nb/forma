#include <nativedaw/v2/EngineCommands.h>
#include "NativePluginStates.h"

namespace ndaw::v2 {
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::string key(te::AutomatableParameter& p){return p.getOwnerID().toString().toStdString()+"::"+p.paramID.toStdString();}
double automationValue(te::AutomatableParameter& p,float v){
    return (dynamic_cast<te::VolumeAndPanPlugin*>(p.getPlugin())&&p.paramID.contains("volume"))||dynamic_cast<te::VCAPlugin*>(p.getPlugin())?te::volumeFaderPositionToDB(v):v;
}
}
void Commands::registerParameterCommands(Json& registry){
    for(const char* id:{"plugin.editor.open","plugin.editor.close"})registry.push_back({{"id",id},{"schema",{{"type","object"},{"properties",{{"plugin",{{"type","string"}}}}},{"required",{"plugin"}},{"additionalProperties",false}}},{"execution","control"},{"permission","local_gui"},{"actor","human"},{"risk","low"},{"reversible",false},{"test","M1-EDITOR-01"}});

    for(const char* id:{"plugin.state.retry","plugin.state.restore_checkpoint"})registry.push_back({{"id",id},{"schema",{{"type","object"},{"properties",{{"plugin",{{"type","string"}}}}},{"required",{"plugin"}},{"additionalProperties",false}}},{"execution","control"},{"permission","local_gui"},{"actor","human"},{"risk","medium"},{"reversible",false},{"test","M1-STATE-01"}});
    for(const char* action:{"begin","value","end"}){
        Json props={{"plugin",{{"type","string"}}},{"parameter",{{"type","string"}}}};
        Json required={"plugin","parameter"};
        if(std::string(action)=="value"){props["value"]={{"type","number"}};required.push_back("value");}
        registry.push_back({{"id",std::string("parameter.gesture.")+action},{"schema",{{"type","object"},{"properties",props},{"required",required},{"additionalProperties",false}}},
            {"execution","control"},{"permission","edit"},{"actor","human"},{"risk","low"},{"reversible",true},{"live",true},{"test","M1-HUMAN-01"},
            {"units",{{"value","actual enumerated native parameter range"}}}});
    }
}
void Commands::beginParameterCapture(){
    if(!parameterCapture.is_null())return;
    require(recordingCapture.is_null(),"stop recording before native parameter edits");
    require(midiConfiguration.is_null()||midiConfiguration.value("state",std::string{})!="requested","wait for MIDI configuration");
    parameterCapture={{"plan_id",juce::Uuid().toString().toStdString()},{"actor","human"},{"source",parameterSource},{"state","editing"},{"changes",Json::array()},{"base_revision",revision},{"audio_verified",false}};
    parameterTransactionStarted=false;parameterFailure=nullptr;bumpRevision();
}
void Commands::finishParameterCapture(bool interrupted){
    if(parameterCapture.is_null())return;
    auto result=parameterCapture;auto id=result["plan_id"].get<std::string>();
    result["interrupted"]=interrupted;result["revision"]=revision+1;result["state"]=parameterTransactionStarted?"committed":"no_changes";
    if(nativeStates)nativeStates->finishParameters(parameterTransactionStarted);
    if(parameterTransactionStarted){
        juce::ValueTree tx("TRANSACTION");tx.setProperty("plan_id",juce::String(id),nullptr);tx.setProperty("actor","human",nullptr);
        tx.setProperty("source",juce::String(result["source"].get<std::string>()),nullptr);tx.setProperty("capture",juce::String(result.dump()),nullptr);
        metadata.addChild(tx,-1,&edit->getUndoManager());history.resize(historyCursor);history.push_back(id);++historyCursor;
    }
    parameterCapture=nullptr;parameterTransactionStarted=false;bumpRevision();result["revision"]=revision;lastParameterCapture=result;
}
void Commands::endParameterGestures(){
    if(parameterCapture.is_null())return;
    ParameterWriteGuard guard(*this);
    for(auto& [_,p]:parameterGestures)p->parameterChangeGestureEnd();
    parameterGestures.clear();finishParameterCapture(true);
}
bool Commands::requestParameterGesture(te::AutomatableParameter& p,bool beginning){
    if(ownedParameterWrites>0)return true;
    juce::ScopedValueSetter<std::string> source(parameterSource,parameterSource=="sdk-parameter"&&nativePluginEditorOpen(p.getOwnerID().toString().toStdString())?"plugin_ui":parameterSource);
    try {
        checkThread();if(nativeStates&&nativeStates->beforeParameter(p))return false;require(p.getPlugin()!=nullptr,"parameter has no supported plugin owner");if(auto* clip=p.getPlugin()->getOwnerClip())require(!bool(clip->state.getProperty("ndaw_locked",false)),"clip is locked");
        const auto id=key(p);
        reconcileExternalPreparation(p);
        if(!capture.is_null()){
            require(p.getTrack()!=nullptr,"automation gesture needs a track");
            automationControl(beginning?"automation.gesture.begin":"automation.gesture.end",{{"track",p.getTrack()->itemID.toString().toStdString()},{"parameter",id}});
        }else {
            if(beginning){
                require(!parameterGestures.contains(id),"duplicate native parameter gesture");
                // Read playback curves retain ownership of the DSP value.
                require(!edit->getTransport().isPlaying()||p.getCurve().getNumPoints()==0,"Read curve owns the playing parameter");
                if(nativeStates)nativeStates->beginParameters(p);beginParameterCapture();parameterGestures[id]=&p;
                ParameterWriteGuard guard(*this);p.parameterChangeGestureBegin();
            }else {
                require(parameterGestures.contains(id),"no matching native parameter gesture");
                {ParameterWriteGuard guard(*this);p.parameterChangeGestureEnd();}
                parameterGestures.erase(id);if(parameterGestures.empty())finishParameterCapture();
            }
        }
    }catch(const std::exception& e){parameterFailure={{"state","failed"},{"source",parameterSource},{"parameter",key(p)},{"error",e.what()},{"revision",revision},{"audio_verified",false}};}
    // Re-entered through L1 with the ownership guard, or rejected before writing.
    return false;
}
bool Commands::requestParameterChange(te::AutomatableParameter& p,float value,juce::NotificationType nt){
    if(ownedParameterWrites>0)return true;
    juce::ScopedValueSetter<std::string> source(parameterSource,parameterSource=="sdk-parameter"&&nativePluginEditorOpen(p.getOwnerID().toString().toStdString())?"plugin_ui":parameterSource);
    try {
        checkThread();if(nativeStates&&nativeStates->beforeParameter(p))return false;require(p.getPlugin()!=nullptr,"parameter has no supported plugin owner");if(auto* clip=p.getPlugin()->getOwnerClip())require(!bool(clip->state.getProperty("ndaw_locked",false)),"clip is locked");
        require(std::isfinite(value)&&value>=p.valueRange.start&&value<=p.valueRange.end,"native parameter value outside actual range");
        require(recordingCapture.is_null(),"stop recording before native parameter edits");
        const auto id=key(p);
        if(reconcileExternalPreparation(p))return false;
        if(!capture.is_null()){
            require(p.getTrack()!=nullptr,"automation parameter needs a track");
            bool single=!gestures.contains(id);const auto track=p.getTrack()->itemID.toString().toStdString();
            if(single)automationControl("automation.gesture.begin",{{"track",track},{"parameter",id}});
            automationControl("automation.gesture.value",{{"track",track},{"parameter",id},{"value",automationValue(p,value)}});
            if(single)automationControl("automation.gesture.end",{{"track",track},{"parameter",id}});
            return false;
        }
        require(!edit->getTransport().isPlaying()||p.getCurve().getNumPoints()==0,"Read curve owns the playing parameter");
        float before=p.getCurrentExplicitValue();if(before==value)return false;
        if(nativeStates)nativeStates->beginParameters(p);beginParameterCapture();
        if(!parameterTransactionStarted){edit->getUndoManager().beginNewTransaction("human:"+juce::String(parameterCapture["plan_id"].get<std::string>()));parameterTransactionStarted=true;}
        auto& changes=parameterCapture["changes"];
        auto change=std::find_if(changes.begin(),changes.end(),[&](const auto& c){return c["target"]==id;});
        if(change==changes.end()){
            changes.push_back({{"target",id},{"plugin",p.getOwnerID().toString().toStdString()},{"parameter",p.paramID.toStdString()},
                {"name",p.getFullName().toStdString()},{"before",before},{"value",before},{"minimum",p.valueRange.start},{"maximum",p.valueRange.end},{"unit",p.getLabel().toStdString()}});change=std::prev(changes.end());
        }
        setParameterValue(*p.getPlugin(),p,value,nt);(*change)["value"]=p.getCurrentExplicitValue();bumpRevision();
        if(parameterGestures.empty())finishParameterCapture();
    }catch(const std::exception& e){parameterFailure={{"state","failed"},{"source",parameterSource},{"parameter",key(p)},{"error",e.what()},{"revision",revision},{"audio_verified",false}};}
    return false;
}
Json Commands::parameterControl(const std::string& cmd,const Json& args){
    checkThread();require(!audioConfigurationPending(),"wait for audio device preparation");require(cmd=="parameter.gesture.begin"||cmd=="parameter.gesture.value"||cmd=="parameter.gesture.end","unknown parameter control");
    bool value=cmd=="parameter.gesture.value";
    require(args.is_object()&&args.size()==(value?3:2)&&args.at("plugin").is_string()&&args.at("parameter").is_string(),"invalid parameter control arguments");
    auto* plugin=processor(args.at("plugin"));require(plugin!=nullptr,"processor instance not found");
    auto p=plugin->getAutomatableParameterByID(juce::String(args.at("parameter").get<std::string>()));require(p!=nullptr,"parameter not enumerated by instance");
    juce::ScopedValueSetter<std::string> source(parameterSource,"gui-parameter");parameterFailure=nullptr;auto beforeRevision=revision;
    if(value){require(args.at("value").is_number(),"parameter value must be numeric");double v=args.at("value");require(std::isfinite(v)&&v>=p->valueRange.start&&v<=p->valueRange.end,"parameter value outside actual range");p->setParameter(float(v),juce::sendNotification);}
    else if(cmd=="parameter.gesture.begin")p->parameterChangeGestureBegin();else p->parameterChangeGestureEnd();
    if(!parameterFailure.is_null())throw std::runtime_error(parameterFailure["error"].get<std::string>());
    if(value&&beforeRevision==revision&&parameterCapture.is_null()&&capture.is_null())return {{"state","no_changes"},{"revision",revision},{"audio_verified",false}};
    return !parameterCapture.is_null()?parameterCapture:!capture.is_null()?capture:lastParameterCapture;
}
}
