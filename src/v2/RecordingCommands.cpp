#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2 {
namespace {
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
std::string id(te::Track& t){return t.itemID.toString().toStdString();}
std::string stored(te::AudioTrack& t,const char* key,const char* fallback){return t.state.getProperty(key,fallback).toString().toStdString();}
te::InputDeviceInstance* instance(te::Edit& e,const std::string& device){for(auto* i:e.getAllInputDevices())if(i->owner.getDeviceID().toStdString()==device)return i;return nullptr;}
}
void Commands::registerRecordingCommands(Json& r){
    auto add=[&](const char* cmd,Json props){auto required=Json::array();for(auto it=props.begin();it!=props.end();++it)required.push_back(it.key());r.push_back({{"id",cmd},{"schema",{{"type","object"},{"properties",props},{"required",required},{"additionalProperties",false}}},{"risk","low"},{"permission","edit"},{"reversible",true},{"live",false},{"test","M1-REC-01"}});};
    Json string={{"type","string"}};add("track.input",{{"track",string},{"device",string}});add("track.arm",{{"track",string},{"enabled",{{"type","boolean"}}}});add("track.monitor",{{"track",string},{"mode",{{"type","string"},{"enum",{"off","auto","on"}}}}});
}
Json Commands::recordingQuery(te::AudioTrack& t) const {
    auto device=stored(t,"ndaw_input","none");auto* i=instance(*edit,device);auto* d=engine.getDeviceManager().findInputDeviceForID(juce::String(device));const bool available=d&&d->isEnabled();const bool keyboard=d&&d->isMidi()&&(d->getName()=="NativeDAW Keyboard"||(dynamic_cast<te::MidiInputDevice*>(d)&&te::HostedAudioDeviceInterface::isHostedMidiInputDevice(*static_cast<te::MidiInputDevice*>(d))));
    return {{"screen_keyboard",keyboard},{"kind",trackType(t)=="audio"?"audio":"midi"},{"device",device},{"name",available?d->getName().toStdString():device=="none"?"None":"Missing input: "+device},{"available",available},{"armed",bool(t.state.getProperty("ndaw_armed",false))},{"monitor",stored(t,"ndaw_monitor","off")},{"monitoring",available&&i&&i->isLivePlayEnabled(t)},{"recording",available&&i&&i->isRecording(t.itemID)},{"recording_file",available&&i?i->getRecordingFile(t.itemID).getFullPathName().toStdString():""}};
}
void Commands::validateRecordingPlan(const Json& ops) const {
    std::map<std::string,std::string> inputs,types;
    for(auto* t:te::getAudioTracks(*edit)){inputs[id(*t)]=stored(*t,"ndaw_input","none");types[id(*t)]=trackType(*t);}
    for(const auto& op:ops){std::string cmd=op.at("command");const auto& a=op.at("args");
        if(cmd=="track.create"){std::string key=a.at("ref");types[key]=a.value("type",std::string("audio"));inputs[key]="none";continue;}
        if(cmd!="track.input"&&cmd!="track.arm"&&cmd!="track.monitor")continue;
        std::string key=a.at("track");require(types.contains(key)&&(types[key]=="audio"||types[key]=="midi"||types[key]=="instrument"),"input requires an audio, MIDI or instrument track");
        if(cmd=="track.input"){std::string device=a.at("device");auto* d=engine.getDeviceManager().findInputDeviceForID(juce::String(device));require(device=="none"||(d&&(types[key]=="audio"?d->getDeviceType()==te::InputDevice::waveDevice:d->isMidi()&&!d->isTrackDevice())&&d->isEnabled()),"input unavailable or incompatible with track type");inputs[key]=device;}
        else if(cmd=="track.arm"){if(a.at("enabled").get<bool>())require(inputs[key]!="none"&&engine.getDeviceManager().findInputDeviceForID(juce::String(inputs[key]))!=nullptr&&engine.getDeviceManager().findInputDeviceForID(juce::String(inputs[key]))->isEnabled(),"choose an available input before arming");}
        else {std::string mode=a.at("mode");require(mode=="off"||mode=="auto"||mode=="on","unsupported monitor mode");require(mode=="off"||inputs[key]!="none","choose input before monitoring");}
    }
}
void Commands::executeRecordingOperation(const std::string& cmd,const Json& a){
    auto* t=track(a.at("track"));require(t!=nullptr,"recording track disappeared");auto& um=edit->getUndoManager();edit->getTransport().ensureContextAllocated();
    if(cmd=="track.input"){
        edit->getEditInputDevices().clearAllInputs(*t,&um);std::string device=a.at("device");t->state.setProperty("ndaw_input",juce::String(device),&um);
        if(device=="none"){t->state.setProperty("ndaw_armed",false,&um);t->state.setProperty("ndaw_monitor","off",&um);return;}
        auto* i=instance(*edit,device);require(i!=nullptr,"input instance unavailable");auto target=i->setTarget(t->itemID,false,&um,0);require(bool(target),"input assignment failed");
        target.value()->state.setProperty(te::IDs::armed,t->state.getProperty("ndaw_armed",false),&um);target.value()->state.setProperty("ndaw_monitor",t->state.getProperty("ndaw_monitor","off"),&um);
    }else{
        const char* key=cmd=="track.arm"?"ndaw_armed":"ndaw_monitor";juce::var value=cmd=="track.arm"?juce::var(a.at("enabled").get<bool>()):juce::var(juce::String(a.at("mode").get<std::string>()));t->state.setProperty(key,value,&um);
        if(auto* i=instance(*edit,stored(*t,"ndaw_input","none")))if(auto* d=te::getDestination(*i,*t,0))d->state.setProperty(cmd=="track.arm"?te::IDs::armed:juce::Identifier("ndaw_monitor"),value,&um);
    }
    edit->restartPlayback();
}
void Commands::restoreInputAssignments(){
    // Native input states survive playback-context destruction. Only reconstruct a
    // missing destination from the retained reference; unavailable hardware stays visible.
    bool assigned=false;for(auto* t:te::getAudioTracks(*edit))assigned|=stored(*t,"ndaw_input","none")!="none";if(!assigned)return;
    edit->getTransport().ensureContextAllocated();
    for(auto* t:te::getAudioTracks(*edit))if(auto* i=instance(*edit,stored(*t,"ndaw_input","none"))){auto* d=te::getDestination(*i,*t,0);if(!d){auto result=i->setTarget(t->itemID,false,nullptr,0);if(result)d=result.value();}if(d){d->state.setProperty(te::IDs::armed,t->state.getProperty("ndaw_armed",false),nullptr);d->state.setProperty("ndaw_monitor",t->state.getProperty("ndaw_monitor","off"),nullptr);}}
}
Json Commands::configureInput(const std::string& deviceName){
    // The recording inspector is an adapter to the same stopped, permission-
    // checked device control as Audio Settings. It cannot bypass readiness or
    // silently replace the user's sample rate/buffer with a JUCE fallback.
    checkThread();auto devices=audioDevices();auto args=devices["actual"];args.erase("open");args.erase("running");args["input"]=deviceName;
    auto capability=audioCapabilities({{"type",args["type"]},{"output",args["output"]},{"input",deviceName}});args["input_channels"]=Json::array();
    for(size_t n=0;n<capability["input_channel_names"].size();++n)args["input_channels"].push_back(n);
    args["base_setup_hash"]=devices["setup_hash"];return audioDeviceControl(args);
}
Json Commands::record(const juce::File& directory){
    checkThread();require(!audioConfigurationPending(),"wait for audio device preparation");require(parameterCapture.is_null(),"finish native parameter gesture first");ParameterWriteGuard parameterGuard(*this);require(!edit->getTransport().isPlaying()&&capture.is_null()&&recordingCapture.is_null(),"start recording from stopped transport");require(midiConfiguration.is_null()||midiConfiguration.value("state",std::string{})!="requested","wait for MIDI configuration");require(engine.getDeviceManager().deviceManager.getCurrentAudioDevice()!=nullptr,"audio device unavailable");
    require(directory.isDirectory()&&directory.hasWriteAccess(),"choose a writable recording directory");require(directory.getBytesFreeOnVolume()>64*1024*1024,"less than 64 MiB free for recording");
    for(auto* t:te::getAllTracks(*edit))require(t->automationMode==te::AutomationMode::read,"recording currently requires Read automation; simultaneous automation writing is pending");
    restoreInputAssignments();Json targets=Json::array(),existing=Json::array();
    for(auto* t:te::getAudioTracks(*edit)){for(auto* c:t->getClips())existing.push_back(c->itemID.toString().toStdString());auto q=recordingQuery(*t);if(q["armed"].get<bool>()){require(q["available"].get<bool>(),"armed input unavailable");if(q["kind"]=="midi"){auto d=engine.getDeviceManager().findMidiInputDeviceForID(juce::String(q["device"].get<std::string>()));require(d&&d->recordingEnabled,"MIDI recording disabled");}targets.push_back(id(*t));}}
    require(!targets.empty(),"no armed tracks");
    require(!edit->getTransport().looping&&!edit->recordingPunchInOut,"linear recording only; Punch/Loop qualification is M6");recordingDirectory=directory;recordingError.clear();auto plan=juce::Uuid().toString().toStdString();
    recordingCapture={{"plan_id",plan},{"actor","human"},{"state","starting"},{"targets",targets},{"existing_clips",existing},{"start_samples",std::llround(edit->getTransport().getPosition().inSeconds()*timelineRate)},{"directory",directory.getFullPathName().toStdString()}};
    auto& um=edit->getUndoManager();um.beginNewTransaction(juce::String("human:"+plan));
    // Fixed non-destructive capture policy. Device preferences are restored after
    // Stop; no native merge, replace, quantisation or expression conversion may
    // silently edit an existing MIDI clip.
    std::set<std::string> configured;
    for(const auto& target:targets){auto* t=track(target);auto q=recordingQuery(*t);std::string device=q["device"];if(q["kind"]=="midi"&&configured.insert(device).second){auto d=engine.getDeviceManager().findMidiInputDeviceForID(juce::String(device));require(d&&d->recordingEnabled,"MIDI recording disabled");midiRecordSettings.push_back({d,d->mergeRecordings,d->replaceExistingClips,d->recordToNoteAutomation,d->quantisation});d->mergeRecordings=false;d->replaceExistingClips=false;d->recordToNoteAutomation=false;d->quantisation=te::QuantisationType();}}
    try {edit->getTransport().record(false,false);}catch(const std::exception& e){recordingError=e.what();edit->getTransport().stop(false,false);finishRecordingCapture();throw;}
    bool success=edit->getTransport().isRecording();for(const auto& target:targets){auto* t=track(target);auto q=recordingQuery(*t);success&=q["recording"].get<bool>()&&(q["kind"]=="midi"||!q["recording_file"].get<std::string>().empty());}
    if(!success){edit->getTransport().stop(false,false);if(recordingError.empty())recordingError="Tracktion did not start all armed inputs";finishRecordingCapture();throw std::runtime_error(recordingError);}
    recordingCapture["state"]="recording";bumpRevision();startTimerHz(20);return recordingCapture;
}
void Commands::timerCallback(){finishAudioConfiguration();captureNativeStates();finishMidiConfiguration();if(!recordingCapture.is_null()){if(!edit->getTransport().isRecording())finishRecordingCapture(true);else for(const auto& target:recordingCapture["targets"]){auto* t=track(target);if(!t||!recordingQuery(*t)["available"].get<bool>()){recordingError="Armed input disappeared during recording";stop();break;}}}}
void Commands::finishRecordingCapture(bool unexpected){
    stopTimer();if(recordingCapture.is_null())return;checkThread();if(unexpected&&recordingError.empty())recordingError="Recording stopped unexpectedly; files may be partial";
    auto receipt=recordingCapture;receipt.erase("existing_clips");if(!receipt.contains("end_samples"))receipt["end_samples"]=std::llround(edit->getTransport().getPosition().inSeconds()*timelineRate);receipt["files"]=Json::array();receipt["clips"]=Json::array();
    auto& undo=edit->getUndoManager();initialiseMusicIDs(&undo);
    receipt["target_results"]=Json::array();
    for(const auto& target:recordingCapture["targets"]){auto* t=track(target);if(!t){recordingError="Recorded track disappeared";continue;}const bool midi=trackType(*t)!="audio";int count=0;
        for(auto* c:t->getClips())if(std::find(recordingCapture["existing_clips"].begin(),recordingCapture["existing_clips"].end(),Json(c->itemID.toString().toStdString()))==recordingCapture["existing_clips"].end()){
            if(auto* wave=dynamic_cast<te::WaveAudioClip*>(c)){auto f=wave->getOriginalFile();try {auto facts=analyse(f);require(facts["frames"].get<int64_t>()>0,"empty recorded file");receipt["files"].push_back(facts);receipt["clips"].push_back({{"id",c->itemID.toString().toStdString()},{"track",id(*t)},{"kind","audio"},{"path",f.getFullPathName().toStdString()}});++count;c->state.setProperty("ndaw_origin","recording",&undo);}catch(const std::exception& e){recordingError=e.what();}}
            else if(auto* m=dynamic_cast<te::MidiClip*>(c)){auto facts=midiQuery(*m);if(facts["notes"].empty()&&facts["controller_events"].empty()&&facts["sysex_count"]==0){recordingError="Native MIDI clip contains no events";continue;}receipt["clips"].push_back({{"id",c->itemID.toString().toStdString()},{"track",id(*t)},{"kind","midi"},{"events",facts}});++count;c->state.setProperty("ndaw_origin","recording",&undo);}
        }
        receipt["target_results"].push_back({{"track",id(*t)},{"kind",midi?"midi":"audio"},{"clips",count},{"state",count?"captured":midi?"no_events":"failed"}});
        if(!midi&&count!=1&&recordingError.empty())recordingError="Armed audio track did not produce one valid file";
    }
    for(auto& settings:midiRecordSettings){settings.device->mergeRecordings=settings.merge;settings.device->replaceExistingClips=settings.replace;settings.device->recordToNoteAutomation=settings.expression;settings.device->quantisation=settings.quantisation;}midiRecordSettings.clear();
    const bool empty=receipt["clips"].empty();
    receipt["outcome"]=recordingError.empty()?(empty?"no_events":"success"):"failed";receipt["state"]=recordingError.empty()?(empty?"no_events":"committed"):"failed";receipt["error"]=recordingError;receipt["undo_scope"]="Edit clips only; recorded audio files are retained";
    auto& um=edit->getUndoManager();if(!receipt["clips"].empty()){
        juce::ValueTree tx("TRANSACTION");tx.setProperty("plan_id",juce::String(receipt["plan_id"].get<std::string>()),nullptr);tx.setProperty("actor","human",nullptr);tx.setProperty("source","native-recording",nullptr);tx.setProperty("receipt",juce::String(receipt.dump()),nullptr);metadata.addChild(tx,-1,&um);
        history.resize(historyCursor);history.push_back(receipt["plan_id"]);++historyCursor;
    }else{um.undoCurrentTransactionOnly();um.beginNewTransaction();}
    lastRecording=receipt;recordingCapture=nullptr;bumpRevision();
}
}
