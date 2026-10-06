// SPDX-License-Identifier: AGPL-3.0-only
#include "NativePluginStates.h"
namespace ndaw::v2 {
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::string id(te::Plugin& p){return p.itemID.toString().toStdString();}
std::string hash(const juce::MemoryBlock& b){return juce::SHA256(b.getData(),b.getSize()).toHexString().toStdString();}
int nativeIndex(juce::AudioPluginInstance& instance,const juce::String& param){
 const auto& values=instance.getParameters();
 for(int i=0;i<values.size();++i){auto* withID=dynamic_cast<juce::AudioProcessorParameterWithID*>(values[i]);if((withID?withID->paramID:juce::String(i))==param)return i;}
 return -1;
}
static_assert(std::atomic<uint32_t>::is_always_lock_free);
}
struct NativePluginStates::Snapshot {
 std::shared_ptr<const juce::MemoryBlock> blob;
 std::string digest;
 int program=0;double sampleRate=0;double captureMs=0;
 std::map<juce::String,float> overrides;
};
struct NativePluginStates::Watch final:juce::AudioProcessorListener {
 explicit Watch(te::ExternalPlugin& p):plugin(&p),instance(*p.getAudioPluginInstance()),mainThread(juce::Thread::getCurrentThreadId()){
  publicIndices.resize(size_t(instance.getParameters().size()),false);
  for(auto* a:p.getAutomatableParameters()){int index=nativeIndex(instance,a->paramID);if(index>=0)publicIndices[size_t(index)]=true;}
  instance.addListener(this);
 }
 ~Watch()override{instance.removeListener(this);}
 bool isOwned()const{return suppress.load(std::memory_order_relaxed)>0&&juce::Thread::getCurrentThreadId()==mainThread;}
 void audioProcessorParameterChanged(juce::AudioProcessor*,int index,float)override{
  // Native program selectors and non-automatable parameters must not disappear.
  bool unmapped=index<0||size_t(index)>=publicIndices.size()||!publicIndices[size_t(index)];
  if(isOwned()) {if(selectingProgram)programCandidate=programCandidate==-1||programCandidate==index?index:-2;return;}
  if(unmapped)dirty.fetch_or(index==programSelector.load(std::memory_order_relaxed)?1u:4u,std::memory_order_relaxed);
 }
 void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& d)override{
  if(!isOwned()){uint32_t bits=(d.programChanged?1u:0u)|(d.nonParameterStateChanged?2u:0u);if(bits)dirty.fetch_or(bits,std::memory_order_relaxed);}
 }
 void setProgram(int index){
  // Learn the one callback produced by the actual owned SDK call.
  // Multiple different callbacks are ambiguous and are never classified.
  juce::ScopedValueSetter<bool> selecting(selectingProgram,true);programCandidate=-1;
  instance.setCurrentProgram(index);if(programCandidate>=0)programSelector.store(programCandidate,std::memory_order_relaxed);
 }
 te::Plugin::Ptr plugin;juce::AudioPluginInstance& instance;
 const juce::Thread::ThreadID mainThread;
 std::vector<bool> publicIndices;
 std::atomic<uint32_t> dirty{0},suppress{0};std::atomic<int> programSelector{-1};
 bool selectingProgram=false;int programCandidate=-1;
 std::shared_ptr<Snapshot> checkpoint;
 std::map<juce::String,float> knownOverrides;
 Json error=nullptr;bool invalidated=false,seenPlayback=false;
};
struct NativePluginStates::Action final:juce::UndoableAction {
 Action(NativePluginStates& service,std::string id,std::shared_ptr<Snapshot> before,std::shared_ptr<Snapshot> after)
 :service(service),target(std::move(id)),before(std::move(before)),after(std::move(after)){}
 bool perform()override{if(first){first=false;service.persist(target,*after);}else service.apply(target,*after);return true;}
 bool undo()override{service.apply(target,*before);return true;}
 int getSizeInUnits()override{return int((before->blob->getSize()+after->blob->getSize())/1024+1);}
 NativePluginStates& service;std::string target;std::shared_ptr<Snapshot> before,after;bool first=true;
};
NativePluginStates::NativePluginStates(Commands& c):owner(c){}
NativePluginStates::~NativePluginStates(){reset();}
void NativePluginStates::reset(){parameterBefore.clear();watches.clear();last=nullptr;}
void NativePluginStates::owned(bool begin){for(auto& [_,w]:watches)if(begin)w->suppress.fetch_add(1,std::memory_order_relaxed);else w->suppress.fetch_sub(1,std::memory_order_relaxed);}
std::shared_ptr<NativePluginStates::Snapshot> NativePluginStates::read(Watch& w,bool restoring){
 auto started=juce::Time::getMillisecondCounterHiRes();auto blob=std::make_shared<juce::MemoryBlock>();
 ++reads;w.instance.getStateInformation(*blob);lastReadMs=juce::Time::getMillisecondCounterHiRes()-started;maxReadMs=std::max(maxReadMs,lastReadMs);
 auto configured=juce::SystemStats::getEnvironmentVariable("NATIVEDAW_V2_PLUGIN_STATE_LIMIT",{}).getLargeIntValue();
 size_t limit=restoring||configured<=0?16*1024*1024:size_t(std::min<int64_t>(configured,16*1024*1024));
 require(blob->getSize()>0&&blob->getSize()<=limit,"native state checkpoint is empty or exceeds configured byte budget (maximum 16 MiB)");
 auto s=std::make_shared<Snapshot>();s->digest=hash(*blob);s->blob=std::move(blob);s->program=w.instance.getCurrentProgram();s->sampleRate=w.instance.getSampleRate();
 for(auto* a:w.plugin->getAutomatableParameters()){
  int index=nativeIndex(w.instance,a->paramID);if(index>=0)s->overrides[a->paramID]=a->getCurve().getNumPoints()==0?w.instance.getParameters()[index]->getValue():a->getCurrentExplicitValue();
 }
 double ms=juce::Time::getMillisecondCounterHiRes()-started;s->captureMs=ms;lastReadMs=ms;maxReadMs=std::max(maxReadMs,ms);
 require(ms<=500,"native state capture exceeded measured 500 ms budget; plugin state is not qualified");
 return s;
}
std::shared_ptr<NativePluginStates::Snapshot> NativePluginStates::before(Watch& w){
 require(w.checkpoint!=nullptr,"native state has no reversible checkpoint");
 auto s=std::make_shared<Snapshot>(*w.checkpoint);
 for(const auto& [key,value]:w.knownOverrides)s->overrides[key]=value;
 for(auto* a:w.plugin->getAutomatableParameters())if(a->getCurve().getNumPoints()>0&&nativeIndex(w.instance,a->paramID)>=0)s->overrides[a->paramID]=a->getCurrentExplicitValue();
 return s;
}
void NativePluginStates::apply(const std::string& target,const Snapshot& s){
 owner.checkThread();Commands::ParameterWriteGuard guard(owner);auto* p=dynamic_cast<te::ExternalPlugin*>(owner.processor(target));
 require(p&&p->getAudioPluginInstance(),"native state target unavailable");
 auto& instance=*p->getAudioPluginInstance();
 if(s.program>=0&&s.program<instance.getNumPrograms()&&s.program!=instance.getCurrentProgram()){
  auto it=watches.find(target);if(it!=watches.end() && &it->second->instance==&instance)it->second->setProgram(s.program);else instance.setCurrentProgram(s.program);
 }
 instance.setStateInformation(s.blob->getData(),int(s.blob->getSize()));
 for(const auto& [key,value]:s.overrides){int index=nativeIndex(instance,key);require(index>=0,"native state parameter identity changed");if(s.sampleRate==instance.getSampleRate())instance.getParameters()[index]->setValue(value);}
 persist(target,s);
 if(auto it=watches.find(target);it!=watches.end()){auto& w=*it->second;w.checkpoint=read(w,true);w.knownOverrides.clear();w.dirty.store(0,std::memory_order_relaxed);w.invalidated=false;w.error=nullptr;}
}
void NativePluginStates::persist(const std::string& target,const Snapshot& s){
 owner.checkThread();Commands::ParameterWriteGuard guard(owner);auto* p=dynamic_cast<te::ExternalPlugin*>(owner.processor(target));require(p&&p->getAudioPluginInstance(),"native state target unavailable");
 auto& instance=*p->getAudioPluginInstance();
 // Adopt actual mapped values in this same transaction before queued host echoes.
 // Preserve explicit automation bases; no curve points are written here.
 for(auto* a:p->getAutomatableParameters()){int index=nativeIndex(instance,a->paramID);if(index>=0){auto v=s.overrides.find(a->paramID);a->setParameter(a->getCurve().getNumPoints()>0&&v!=s.overrides.end()?v->second:instance.getParameters()[index]->getValue(),juce::dontSendNotification);}}
 p->state.setProperty(te::IDs::state,s.blob->toBase64Encoding(),nullptr);
 p->state.setProperty(te::IDs::programNum,instance.getCurrentProgram(),nullptr);
 // The first perform records already-applied native state without reloading DSP.
 juce::MemoryOutputStream explicitValues;
 for(const auto& [key,value]:s.overrides){explicitValues.writeString(key);explicitValues.writeFloat(value);}
 explicitValues.flush();
 if(explicitValues.getDataSize()>0)p->state.setProperty(te::IDs::parameters,explicitValues.getMemoryBlock(),nullptr);else p->state.removeProperty(te::IDs::parameters,nullptr);
}

void NativePluginStates::sync(bool checkpoint){
 owner.checkThread();
 std::erase_if(watches,[&](const auto& item){return owner.processor(item.first)!=item.second->plugin.get();});
 for(auto* p:te::getAllPlugins(*owner.edit,true))if(auto* ext=dynamic_cast<te::ExternalPlugin*>(p);ext&&ext->getAudioPluginInstance()){
  auto key=id(*ext);auto found=watches.find(key);
  if(found==watches.end()){
   auto w=std::make_unique<Watch>(*ext);w->suppress.store(uint32_t(owner.ownedParameterWrites),std::memory_order_relaxed);
   try{w->checkpoint=read(*w);}catch(const std::exception& e){fail(*w,e);}
   watches.emplace(key,std::move(w));
   }else if(checkpoint&&!owner.edit->getTransport().isPlaying()){
   if(!found->second->error.is_null())continue;
   const auto bits=found->second->dirty.load(std::memory_order_relaxed);
   if(bits!=0){if(bits==1&&found->second->checkpoint&&found->second->instance.getCurrentProgram()==found->second->checkpoint->program)found->second->dirty.store(0,std::memory_order_relaxed);else continue;}
   try{found->second->checkpoint=read(*found->second);found->second->knownOverrides.clear();found->second->invalidated=false;found->second->error=nullptr;}
   catch(const std::exception& e){fail(*found->second,e);}
  }
 }
}
void NativePluginStates::fail(Watch& w,const std::exception& e){
 w.invalidated=true;w.error={{"state","failed"},{"plugin",id(*w.plugin)},{"error",e.what()},{"revision",owner.revision},{"audio_verified",false}};
}
void NativePluginStates::noteParameter(te::Plugin& p,te::AutomatableParameter& a,float value){
 auto it=watches.find(id(p));if(it!=watches.end()&&nativeIndex(it->second->instance,a.paramID)>=0)it->second->knownOverrides[a.paramID]=value;
}
bool NativePluginStates::beforeParameter(te::AutomatableParameter& a){
 if(draining||owner.ownedParameterWrites>0)return false;
 auto it=watches.find(a.getOwnerID().toString().toStdString());if(it==watches.end())return false;
 auto& w=*it->second;bool pending=w.invalidated||!w.checkpoint||w.dirty.load(std::memory_order_relaxed)!=0||(w.checkpoint&&w.instance.getCurrentProgram()!=w.checkpoint->program);
 if(!pending)return false;
 drain();
 require(w.checkpoint&&w.dirty.load(std::memory_order_relaxed)==0&&!w.invalidated,"native plugin state pending; stop or recover before parameter edits");return false;
}
void NativePluginStates::beginParameters(te::AutomatableParameter& a){
 if(owner.edit->getTransport().isPlaying())return;
 auto it=watches.find(a.getOwnerID().toString().toStdString());if(it!=watches.end()&&!parameterBefore.contains(it->first))parameterBefore[it->first]=before(*it->second);
}
void NativePluginStates::finishParameters(bool committed){
 if(!committed){parameterBefore.clear();return;}
 if(parameterBefore.empty())return;
 Commands::ParameterWriteGuard guard(owner);
 for(const auto& [target,old]:parameterBefore)if(auto it=watches.find(target);it!=watches.end()){
  try{auto next=read(*it->second);
  if(next->program!=old->program||(it->second->dirty.load(std::memory_order_relaxed)&2u)!=0)require(owner.edit->getUndoManager().perform(new Action(*this,target,old,next)),"native human state Undo setup failed");
  it->second->checkpoint=next;it->second->knownOverrides.clear();it->second->dirty.store(0,std::memory_order_relaxed);it->second->invalidated=false;it->second->error=nullptr;}
  catch(const std::exception& e){fail(*it->second,e);}
 }
 parameterBefore.clear();
}
void NativePluginStates::publish(Watch& w,std::shared_ptr<Snapshot> old,std::shared_ptr<Snapshot> next,const char* source){
 auto key=id(*w.plugin);auto txid=juce::Uuid().toString().toStdString();
 Commands::ParameterWriteGuard guard(owner);
 owner.edit->getUndoManager().beginNewTransaction("human:"+juce::String(txid));
 require(owner.edit->getUndoManager().perform(new Action(*this,key,old,next)),"native state Undo setup failed");
 last={{"state","committed"},{"actor","human"},{"source",source},{"plan_id",txid},{"plugin",key},{"before_hash",old->digest},{"state_hash",next->digest},{"before_program",old->program},{"program",next->program},{"state_bytes",next->blob->getSize()},{"capture_ms",next->captureMs},{"private_state_interpreted",false},{"audio_verified",false}};
 juce::ValueTree tx("TRANSACTION");tx.setProperty("plan_id",juce::String(txid),nullptr);tx.setProperty("actor","human",nullptr);tx.setProperty("source",source,nullptr);tx.setProperty("capture",juce::String(last.dump()),nullptr);owner.metadata.addChild(tx,-1,&owner.edit->getUndoManager());
 owner.history.resize(owner.historyCursor);owner.history.push_back(txid);++owner.historyCursor;owner.bumpRevision();last["revision"]=owner.revision;
 w.checkpoint=next;w.knownOverrides.clear();w.dirty.store(0,std::memory_order_relaxed);w.invalidated=false;w.error=nullptr;
}
void NativePluginStates::drain(){
 owner.checkThread();if(draining||owner.ownedParameterWrites>0)return;
 juce::ScopedValueSetter<bool> busy(draining,true);
 const bool active=owner.edit->getTransport().isPlaying()||!owner.recordingCapture.is_null()||!owner.capture.is_null();
 for(auto& [target,wp]:watches){auto& w=*wp;
  auto flags=w.dirty.load(std::memory_order_relaxed);
  bool programDrift=w.checkpoint&&w.instance.getCurrentProgram()!=w.checkpoint->program;
  // AU program notifications can be delayed echoes of an owned state restore.
  if(!programDrift&&(flags&~1u)==0){w.dirty.store(0,std::memory_order_relaxed);flags=0;}
  bool signalled=flags!=0||programDrift;
  if(active){w.seenPlayback=true;if(signalled&&!w.invalidated){owner.bumpRevision();w.invalidated=true;}continue;}
  if(!owner.parameterCapture.is_null())continue;
  if(!w.error.is_null())continue;
  if(!signalled&&!w.seenPlayback)continue;
  if(w.seenPlayback&&!signalled&&!w.knownOverrides.empty()){
   // Known public writes and automation are already in their own human pass.
   // No opaque read is made while playing; accept their new stopped checkpoint.
   try{w.checkpoint=read(w);w.knownOverrides.clear();w.seenPlayback=false;}catch(const std::exception& e){fail(w,e);}
   continue;
  }
  w.seenPlayback=false;if(!signalled)continue;
  try {
   auto next=read(w);auto prior=before(w);
   if(next->digest!=prior->digest||next->program!=prior->program)publish(w,prior,next,signalled?"plugin_state":"plugin_state_poll");
   else {w.dirty.store(0,std::memory_order_relaxed);w.invalidated=false;w.error=nullptr;}
  }catch(const std::exception& e){if(!w.invalidated)owner.bumpRevision();fail(w,e);}
 }
}
void NativePluginStates::setProgram(const std::string& target,int index){
 owner.checkThread();auto it=watches.find(target);require(it!=watches.end(),"no native state checkpoint for program change");
 Commands::ParameterWriteGuard guard(owner);auto& w=*it->second;
 require(index>=0&&index<w.instance.getNumPrograms(),"program index outside actual native range");
 auto prior=read(w);try{
  w.setProgram(index);auto next=read(w);
  require(w.instance.getCurrentProgram()==index,"native plugin did not accept program index");
  require(owner.edit->getUndoManager().perform(new Action(*this,target,prior,next)),"native program Undo setup failed");
  w.checkpoint=next;w.knownOverrides.clear();w.dirty.store(0,std::memory_order_relaxed);w.invalidated=false;
 }catch(...){apply(target,*prior);throw;}
}
void NativePluginStates::historyState(const std::string& id,const char* state){if(!last.is_null()&&last.value("plan_id",std::string{})==id)last["state"]=state;}
Json NativePluginStates::query()const{
 Json states=Json::array(),failures=Json::array();bool pending=false;
 for(const auto& [target,w]:watches){bool dirty=w->dirty.load(std::memory_order_relaxed)!=0||w->invalidated||(w->checkpoint&&w->instance.getCurrentProgram()!=w->checkpoint->program);pending|=dirty;if(!w->error.is_null())failures.push_back(w->error);states.push_back({{"plugin",target},{"state_hash",w->checkpoint?w->checkpoint->digest:""},{"program",w->checkpoint?w->checkpoint->program:0},{"pending",dirty},{"failure",w->error},{"private_state_interpreted",false}});}
 return {{"state_reads",reads},{"last_read_ms",lastReadMs},{"max_read_ms",maxReadMs},{"capture_policy","program index or actual non-parameter/unmapped notification; unreported opaque changes unqualified"},{"pending",pending},{"checkpoints",states},{"last_capture",last},{"failure",failures.empty()?Json(nullptr):failures[0]},{"failures",failures},{"opaque_capture_deferred",owner.edit->getTransport().isPlaying()}};
}
Json NativePluginStates::control(const std::string& cmd,const Json& args){
 owner.checkThread();require(cmd=="plugin.state.retry"||cmd=="plugin.state.restore_checkpoint","unknown native state control");
 require(args.is_object()&&args.size()==1&&args.at("plugin").is_string(),"invalid native state arguments");
 auto target=args.at("plugin").get<std::string>();auto it=watches.find(target);require(it!=watches.end(),"native state target unavailable");
 require(!owner.edit->getTransport().isPlaying()&&owner.capture.is_null()&&owner.recordingCapture.is_null()&&owner.parameterCapture.is_null(),"stop playback and recording before native state recovery");
 if(cmd=="plugin.state.retry"){it->second->error=nullptr;it->second->dirty.fetch_or(2,std::memory_order_relaxed);drain();auto result=query();result["plugin"]=target;result["state"]=!it->second->error.is_null()?"failed":it->second->invalidated?"pending":"captured";return result;}
 require(it->second->checkpoint!=nullptr,"no qualified native checkpoint to restore; remove unavailable plugin instead");
 // This is an explicit recovery action. The uncaptured state has no Undo image.
 auto original=before(*it->second);owner.edit->getTransport().freePlaybackContext();apply(target,*original);owner.bumpRevision();
 auto txid=juce::Uuid().toString().toStdString();owner.edit->getUndoManager().beginNewTransaction("human:"+juce::String(txid));
 juce::ValueTree tx("TRANSACTION");tx.setProperty("plan_id",juce::String(txid),nullptr);tx.setProperty("actor","human",nullptr);tx.setProperty("source","plugin_state_recovery",nullptr);tx.setProperty("reversible",false,nullptr);tx.setProperty("plugin",juce::String(target),nullptr);owner.metadata.addChild(tx,-1,&owner.edit->getUndoManager());
 owner.history.resize(owner.historyCursor);owner.history.push_back(txid);++owner.historyCursor;
 last={{"plan_id",txid},{"state","restored_checkpoint"},{"actor","human"},{"source","plugin_state_recovery"},{"plugin",target},{"revision",owner.revision},{"reversible",false},{"audio_verified",false}};
 return last;
}
Json Commands::nativeStateControl(const std::string& cmd,const Json& args){checkThread();require(nativeStates!=nullptr,"native state service unavailable");return nativeStates->control(cmd,args);}
void Commands::nativeStateOwned(bool begin){if(nativeStates)nativeStates->owned(begin);}
void Commands::captureNativeStates()const{if(nativeStates)nativeStates->drain();}
}
