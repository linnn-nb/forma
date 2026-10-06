#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2 {
namespace {
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
std::string kind(te::MidiInputDevice& d){return d.getName()=="NativeDAW Keyboard"?"keyboard":te::HostedAudioDeviceInterface::isHostedMidiInputDevice(d)?"hosted_test":d.getDeviceType()==te::InputDevice::physicalMidiDevice?"system_midi_port":"virtual";}
}
Json Commands::midiDevices() const {
    auto& dm=engine.getDeviceManager();Json inputs=Json::array(),outputs=Json::array();
    for(auto d:dm.getMidiInDevices())if(!d->isTrackDevice())inputs.push_back({{"id",d->getDeviceID().toStdString()},{"name",d->getName().toStdString()},{"enabled",d->isEnabled()},{"kind",kind(*d)}});
    for(int n=0;n<dm.getNumMidiOutDevices();++n){auto* d=dm.getMidiOutDevice(n);outputs.push_back({{"id",d->getDeviceID().toStdString()},{"name",d->getName().toStdString()},{"enabled",d->isEnabled()},{"kind",dm.isHostedAudioDeviceInterfaceInUse()?"hosted_test":"system_midi_port"}});}
    return {{"midi_inputs",inputs},{"midi_outputs",outputs},{"midi_configuration",midiConfiguration}};
}
Json Commands::configureMidiDevice(const std::string& direction,const std::string& device,bool enabled){
    checkThread();require(!audioConfigurationPending(),"wait for audio device preparation");require(parameterCapture.is_null(),"finish native parameter gesture first");ParameterWriteGuard parameterGuard(*this);require(!edit->getTransport().isPlaying()&&recordingCapture.is_null(),"stop before MIDI device configuration");require(direction=="input"||direction=="output","unknown MIDI device direction");require(midiConfiguration.is_null()||midiConfiguration.value("state",std::string{})!="requested","MIDI configuration already pending");
    auto& dm=engine.getDeviceManager();auto in=dm.findMidiInputDeviceForID(juce::String(device));te::MidiOutputDevice* out=nullptr;for(int n=0;n<dm.getNumMidiOutDevices();++n)if(dm.getMidiOutDevice(n)->getDeviceID().toStdString()==device)out=dm.getMidiOutDevice(n);
    require(direction=="input"?in&&!in->isTrackDevice():out!=nullptr,"MIDI device not found");releaseMidiKeys();edit->getTransport().freePlaybackContext();
    if(direction=="input")in->setEnabled(enabled);else out->setEnabled(enabled);
    // SDK device opening is deferred to its message-thread rescan. Do not report
    // completion from setEnabled; the poll checks the actual rescan result.
    midiConfiguration={{"state","requested"},{"direction",direction},{"device",device},{"enabled",enabled}};midiConfigurationStarted=juce::Time::getMillisecondCounterHiRes();bumpRevision();startTimerHz(20);return midiConfiguration;
}
void Commands::finishMidiConfiguration(){
    if(midiConfiguration.is_null()||midiConfiguration.value("state",std::string{})!="requested"||juce::Time::getMillisecondCounterHiRes()-midiConfigurationStarted<150)return;
    auto& dm=engine.getDeviceManager();const std::string id=midiConfiguration["device"],direction=midiConfiguration["direction"];bool found=false,enabled=false;
    if(direction=="input"){auto d=dm.findMidiInputDeviceForID(juce::String(id));found=bool(d);enabled=d&&d->isEnabled();}
    else for(int n=0;n<dm.getNumMidiOutDevices();++n)if(auto* d=dm.getMidiOutDevice(n);d->getDeviceID().toStdString()==id){found=true;enabled=d->isEnabled();}
    bool success=found&&enabled==midiConfiguration["enabled"].get<bool>();midiConfiguration["state"]=success?"applied":"failed";midiConfiguration["error"]=success?"":"MIDI port disappeared or could not open";restoreInputAssignments();bumpRevision();if(recordingCapture.is_null())stopTimer();
}
Json Commands::midiKeyboard(const std::string& target,int pitch,int velocity,bool on){
    checkThread();require(!audioConfigurationPending(),"wait for audio device preparation");require(midiConfiguration.is_null()||midiConfiguration.value("state",std::string{})!="requested","wait for MIDI device configuration");require(pitch>=0&&pitch<=127&&velocity>=1&&velocity<=127,"invalid keyboard note");auto* t=track(target);require(t&&(trackType(*t)=="midi"||trackType(*t)=="instrument"),"keyboard requires MIDI or instrument track");
    auto q=recordingQuery(*t);auto d=engine.getDeviceManager().findMidiInputDeviceForID(juce::String(q["device"].get<std::string>()));require(d&&d->isEnabled()&&(kind(*d)=="keyboard"||kind(*d)=="hosted_test"),"select NativeDAW Keyboard input before playing screen keys");
    require(q["monitoring"].get<bool>()||q["recording"].get<bool>(),"enable MIDI monitoring or recording before playing screen keys");
    auto i=std::find_if(heldMidiKeys.begin(),heldMidiKeys.end(),[&](const auto& k){return k.device==d&&k.pitch==pitch;});
    if(on){if(i==heldMidiKeys.end()){heldMidiKeys.push_back({d,pitch});d->keyboardState.noteOn(1,pitch,velocity/127.f);}}
    else if(i!=heldMidiKeys.end()){d->keyboardState.noteOff(1,pitch,0);heldMidiKeys.erase(i);}
    return {{"state","sent"},{"input",d->getDeviceID().toStdString()},{"pitch",pitch},{"note_on",on},{"audio_verified",false},{"history","captured by native recording transaction only while recording"}};
}
void Commands::releaseMidiKeys(){for(auto& k:heldMidiKeys)k.device->keyboardState.noteOff(1,k.pitch,0);heldMidiKeys.clear();}
}
