// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Realtime.h"
#include <numbers>
#include <set>
#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace ndaw {
static_assert(std::atomic<int>::is_always_lock_free);
static_assert(std::atomic<Frame>::is_always_lock_free);
std::string SourceStream::stamp() const {
#if !defined(_WIN32)
    struct stat s{};
    if(::stat(path_.c_str(),&s)!=0) return {};
#if defined(__APPLE__)
    auto m=s.st_mtimespec,c=s.st_ctimespec;
#else
    auto m=s.st_mtim,c=s.st_ctim;
#endif
    return std::to_string(s.st_dev)+":"+std::to_string(s.st_ino)+":"+std::to_string(s.st_size)+":"+
        std::to_string(m.tv_sec)+":"+std::to_string(m.tv_nsec)+":"+std::to_string(c.tv_sec)+":"+std::to_string(c.tv_nsec);
#else
    std::error_code ec;auto size=fs::file_size(path_,ec);if(ec)return {};
    auto time=fs::last_write_time(path_,ec);if(ec)return {};
    return std::to_string(size)+":"+std::to_string(time.time_since_epoch().count());
#endif
}
SourceStream::SourceStream(const Json& s,const fs::path& root,std::size_t count,std::shared_ptr<Budget> budget)
    :path_(mediaPath(root,s)),frames_(s.at("frames")),channels_(s.at("channels")),pageCount_(count),budget_(std::move(budget)) {
    stamp_=stamp();if(stamp_.empty())throw Error("missing_media","Missing media retained in session: "+s.at("path").get<std::string>());
    if(sha256(path_)!=s.at("sha256").get<std::string>() || stamp()!=stamp_)throw Error("media_changed","Source checksum or file identity changed");
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    reader_.reset(formats.createReaderFor(juce::File(juce::String(path_.string()))));
    if(!reader_ || reader_->sampleRate!=s.at("sample_rate").get<int>() || reader_->lengthInSamples!=frames_ || reader_->numChannels!=channels_)
        throw Error("media_format","Source facts disagree with decoded media");
    if(pageCount_>budget_->limit/sizeof(Page))throw Error("audio_resources","Raw source cache exceeds configured memory budget");
    bytes_=pageCount_*sizeof(Page);auto used=budget_->used.load();
    while(true) {
        if(used>budget_->limit-bytes_)throw Error("audio_resources","Raw source cache exceeds configured memory budget; previous graph retained");
        if(budget_->used.compare_exchange_weak(used,used+bytes_))break;
    }
    try{pages_=std::make_unique<Page[]>(pageCount_);}catch(...){budget_->used.fetch_sub(bytes_);throw;}
    for(auto& r:requests_)r.store(-1);
}
SourceStream::~SourceStream(){if(pages_)budget_->used.fetch_sub(bytes_);}
bool SourceStream::unchanged() const {return !failed() && stamp()==stamp_;}
bool SourceStream::available(Frame frame) const noexcept {
    if(frame<0 || frame>=frames_)return true;
    Frame start=frame/sourcePageFrames*sourcePageFrames;
    for(std::size_t i=0;i<pageCount_;++i)if(pages_[i].start.load(std::memory_order_acquire)==start && pages_[i].readers.load(std::memory_order_acquire)>=0)return true;
    return false;
}
bool SourceStream::request(Frame frame) noexcept {
    if(frame<0 || frame>=frames_ || failed())return false;
    const Frame start=frame/sourcePageFrames*sourcePageFrames;
    // A pre-read window is a live cache demand, even for a clip that starts
    // just after this callback. Otherwise LRU prefers recently played, obsolete
    // pages and evicts the already-read beginning of the upcoming clip.
    for(std::size_t i=0;i<pageCount_;++i)if(pages_[i].start.load(std::memory_order_acquire)==start && pages_[i].readers.load(std::memory_order_acquire)>=0) {
        pages_[i].touch.store(clock_.fetch_add(1,std::memory_order_relaxed),std::memory_order_relaxed);return true;
    }
    if(filling_.load(std::memory_order_acquire)==start)return true;
    for(auto& r:requests_)if(r.load(std::memory_order_acquire)==start)return true;
    for(auto& r:requests_){Frame empty=-1;if(r.compare_exchange_strong(empty,start,std::memory_order_acq_rel))return true;}
    overflows_.fetch_add(1,std::memory_order_relaxed);return false;
}
bool SourceStream::pin(Frame frame,Pin& pin) noexcept {
    if(failed() || frame<0 || frame>=frames_)return false;
    const Frame start=frame/sourcePageFrames*sourcePageFrames;
    for(std::size_t i=0;i<pageCount_;++i) {
        auto& p=pages_[i];if(p.start.load(std::memory_order_acquire)!=start)continue;
        auto refs=p.readers.load(std::memory_order_acquire);
        if(refs<0 || !p.readers.compare_exchange_strong(refs,refs+1,std::memory_order_acq_rel))continue;
        if(p.start.load(std::memory_order_acquire)!=start){p.readers.fetch_sub(1,std::memory_order_release);continue;}
        pin={p.pcm[0],p.pcm[channels_==1?0:1],start,p.valid,i};p.touch.store(clock_.fetch_add(1),std::memory_order_relaxed);return true;
    }
    request(frame);return false;
}
void SourceStream::unpin(Pin& pin) noexcept {pages_[pin.index].readers.fetch_sub(1,std::memory_order_release);pin={};}
void SourceStream::service() {
    if(failed())return;
    auto now=std::chrono::steady_clock::now();
    if(now-checked_>=std::chrono::milliseconds(10)) {
        checked_=now;if(!unchanged()){failed_.store(true,std::memory_order_release);return;}
    }
    int loaded=0;
    for(auto& r:requests_) {
        if(loaded==4)break;
        Frame start=r.exchange(-1,std::memory_order_acq_rel);if(start<0 || available(start))continue;
        filling_.store(start,std::memory_order_release);
        std::size_t candidate=pageCount_;std::uint64_t oldest=UINT64_MAX;
        for(std::size_t i=0;i<pageCount_;++i)if(pages_[i].readers.load(std::memory_order_acquire)==0) {
            auto age=pages_[i].touch.load();if(age<oldest){oldest=age;candidate=i;}
        }
        if(candidate==pageCount_){filling_.store(-1);request(start);continue;}
        auto& p=pages_[candidate];int empty=0;
        if(!p.readers.compare_exchange_strong(empty,-1,std::memory_order_acq_rel)){filling_.store(-1);request(start);continue;}
        p.start.store(-1,std::memory_order_release);p.valid=static_cast<int>(std::min<Frame>(sourcePageFrames,frames_-start));
        float* channels[]{p.pcm[0],p.pcm[1]};
        bool valid=reader_->read(channels,static_cast<int>(channels_),start,p.valid);
        for(unsigned ch=0;ch<channels_ && valid;++ch)for(int i=0;i<p.valid;++i)if(!std::isfinite(p.pcm[ch][i])){valid=false;break;}
        if(valid){p.touch.store(clock_.fetch_add(1));p.start.store(start,std::memory_order_release);}
        else failed_.store(true,std::memory_order_release);
        p.readers.store(0,std::memory_order_release);filling_.store(-1,std::memory_order_release);++loaded;
        if(!valid)break;
    }
}
// The cache shares immutable PCM by file/facts, not by project Source ID.
// Admission must count every mapping that can use that shared allocation.
static std::string sourceCacheIdentity(const Json& s,const fs::path& root) {
    return mediaPath(root,s).string()+":"+s.at("sha256").get<std::string>()+":"+s.at("frames").dump()+":"+
        s.at("sample_rate").dump()+":"+s.at("channels").dump();
}
SourcePool::SourcePool(EngineConfig config,std::shared_ptr<PluginResources> plugins):budget_(std::make_shared<SourceStream::Budget>()),routing_(std::make_shared<RoutingResources>(config.routingCacheBytes)),plugins_(plugins?std::move(plugins):std::make_shared<PluginResources>(config.pluginWorkers,config.pluginIPCBytes)){budget_->limit=config.sourceCacheBytes;}
std::shared_ptr<SourceStream> SourcePool::acquire(const Json& s,const fs::path& root,std::size_t pages) {
    const auto key=sourceCacheIdentity(s,root)+":"+std::to_string(pages);
    std::shared_ptr<SourceStream> existing;
    {std::lock_guard lock(mutex_);auto it=sources_.find(key);if(it!=sources_.end())existing=it->second.lock();}
    if(existing && existing->capacity()>=pages && existing->unchanged())return existing;
    auto next=std::make_shared<SourceStream>(s,root,pages,budget_);
    {std::lock_guard lock(mutex_);sources_[key]=next;}return next;
}
void SourcePool::service() {
    std::vector<std::shared_ptr<SourceStream>> live;
    {std::lock_guard lock(mutex_);for(auto it=sources_.begin();it!=sources_.end();) {
        if(auto s=it->second.lock()){live.push_back(std::move(s));++it;}else it=sources_.erase(it);
    }}
    // IO and destruction are on this worker, outside the registry mutex.
    for(auto& s:live)s->service();
}
double RealtimeGraph::Ramp::next() noexcept {if(remaining>0){value+=step;if(--remaining==0)value=target;}return value;}
void RealtimeGraph::Ramp::from(const Ramp& old,int frames) noexcept {
    if(target==old.target){value=old.value;step=old.step;remaining=old.remaining;}
    else {value=old.value;remaining=frames;step=(target-value)/frames;}
}
RealtimeGraph::RealtimeGraph(const Json& s,const fs::path& root,SourcePool& pool,const std::vector<int>& activeInputs,const PluginPreparation* context):mixer_(validateCompiledEnvelopeBudget(s),pool.routingResources(),root,PluginProcessingMode::Realtime,pool.pluginResources(),context) {
    validateSession(s);revision=s.at("revision");rate_=s.at("sample_rate");length_=sessionLength(s)+mixer_.latency();rampFrames_=rate_/200;pendingLatency_=pendingSettle_=mixer_.latency();
    for(const auto& track:s.at("tracks"))if(track.at("kind")=="audio")playlistSelection+=track.at("id").get<std::string>()+":"+track.at("active_playlist_id").get<std::string>()+";";
    std::map<std::string,std::string> mediaKeys;
    std::set<std::string> needed;for(const auto& track:s.at("tracks"))for(const auto& clip:activeClips(track))needed.insert(clip.at("source_id"));
    for(const auto& src:s.at("sources"))if(needed.contains(src.at("id")))mediaKeys[src.at("id")]=sourceCacheIdentity(src,root);
    std::map<std::string,std::set<Frame>> offsets;
    for(const auto& t:s.at("tracks"))for(const auto& c:activeClips(t))offsets[mediaKeys.at(c.at("source_id"))].insert(c.at("source_start").get<Frame>()-c.at("start").get<Frame>());
    std::map<std::string,std::size_t> indices;
    for(const auto& src:s.at("sources")) {
        auto id=src.at("id").get<std::string>();if(!needed.contains(id))continue;indices[id]=sources_.size();
        sources_.push_back(pool.acquire(src,root,std::max<std::size_t>(16,4*offsets[mediaKeys.at(id)].size())));
    }
    for(const auto& t:s.at("tracks")) {
        Track track;track.id=t.at("id");track.armed=t.at("record_armed");auto mode=t.at("monitor_mode").get<std::string>();
        track.mode=mode=="input"?1:mode=="auto"?2:0;track.channels=static_cast<int>(t.at("input_channels").size());
        for(int ch=0;ch<track.channels;++ch){auto found=std::find(activeInputs.begin(),activeInputs.end(),t.at("input_channels")[ch].get<int>());
            track.input[ch]=found==activeInputs.end()?-1:static_cast<int>(found-activeInputs.begin());}
        auto index=tracks_.size();trackIndex_[track.id]=index;
        tracks_.push_back(std::move(track));
        for(const auto& c:activeClips(t)) {
            Clip clip;clip.id=c.at("id");clip.track=index;clip.source=indices.at(c.at("source_id"));
            clip.start=c.at("start");clip.offset=c.at("source_start");clip.length=c.at("length");clip.envelope=GainEnvelope(c);
            clip.continuity=tracks_[index].id+":"+c.at("source_id").get<std::string>()+":"+std::to_string(clip.offset-clip.start);
            clip.gain.value=clip.gain.target=std::pow(10.0,c.at("gain_db").get<double>()/20);clipIndex_[clip.id]=clips_.size();
            continuityIndex_[clip.continuity].push_back(clips_.size());clips_.push_back(std::move(clip));
        }
    }
    for(auto& [key,indices]:continuityIndex_)std::sort(indices.begin(),indices.end(),[this](auto a,auto b){return clips_[a].offset<clips_[b].offset;});
}
bool RealtimeGraph::failed() const noexcept {if(mixer_.pluginFailed())return true;for(const auto& s:sources_)if(s->failed())return true;return false;}
bool RealtimeGraph::warm(Frame at,int frames,int ahead) noexcept {
    bool ready=!failed();
    for(const auto& c:clips_) {
        auto first=std::max(at,c.start),last=std::min(at+frames+ahead,c.start+c.length);
        if(first>=last)continue;auto& s=*sources_[c.source];
        Frame sourceFirst=c.offset+first-c.start,sourceLast=c.offset+last-c.start;
        const auto needed=std::min(at+frames,c.start+c.length);
        for(Frame page=sourceFirst/sourcePageFrames*sourcePageFrames;page<sourceLast;page+=sourcePageFrames) {
            s.request(page);if(page<c.offset+needed-c.start && !s.available(page))ready=false;
        }
    }
    return ready;
}
void RealtimeGraph::releasePins() noexcept {for(auto& c:clips_)while(c.pinsHeld>0)sources_[c.source]->unpin(c.pins[--c.pinsHeld]);}
void RealtimeGraph::inherit(RealtimeGraph& old) noexcept {
    mixer_.inherit(old.mixer_);
    for(auto& c:clips_) {
        if(auto it=old.clipIndex_.find(c.id);it!=old.clipIndex_.end())c.gain.from(old.clips_[it->second].gain,rampFrames_);
        else if(auto found=old.continuityIndex_.find(c.continuity);found!=old.continuityIndex_.end()) {
            // A newly split child can inherit an in-progress gain ramp. The
            // immutable index gives O(log n) lookup, with no callback strings built.
            const auto& indices=found->second;
            auto it=std::upper_bound(indices.begin(),indices.end(),c.offset,[&old](Frame at,std::size_t i){return at<old.clips_[i].offset;});
            if(it!=indices.begin()) {
                const auto& parent=old.clips_[*--it];
                if(c.offset+c.length<=parent.offset+parent.length)c.gain.from(parent.gain,rampFrames_);
            }
        }
    }
    pendingSettle_=0;pendingLatency_=mixer_.latency()+(mixer_.parameterTransition()?pluginQuantum-1:0);pendingSettle_=pendingLatency_+remainingRampFrames();
}
void RealtimeGraph::reset() noexcept {mixer_.reset();for(auto& c:clips_)c.gain.reset();pendingLatency_=pendingSettle_=mixer_.latency();audibleOffset_=-1;}
Frame RealtimeGraph::remainingRampFrames() const noexcept {
    Frame remaining=mixer_.remainingRampFrames();
    for(const auto& c:clips_)remaining=std::max(remaining,static_cast<Frame>(c.gain.remaining));return std::max(remaining,pendingSettle_);
}
bool RealtimeGraph::render(Frame at,int frames,float* left,float* right,const float* const* input,int inputs,bool playing,bool recording,int inputOffset) noexcept {
    if(frames<0 || frames>renderBlock || at<0 || (playing && failed()))return false;
    monitorFault_=0;
    for(auto& t:tracks_)t.suppress=!playing || t.mode==1 || (t.armed && recording);
    // Pin every required page before advancing any DSP state or writing output.
    for(auto& c:clips_) {
        if(tracks_[c.track].suppress)continue;
        Frame first=std::max(at,c.start),last=std::min(at+frames,c.start+c.length);if(first>=last)continue;
        auto& s=*sources_[c.source];Frame offset=c.offset+first-c.start,end=c.offset+last-c.start;
        for(Frame page=offset/sourcePageFrames*sourcePageFrames;page<end;page+=sourcePageFrames) {
            if(c.pinsHeld==2 || !s.pin(page,c.pins[c.pinsHeld])){releasePins();return false;}++c.pinsHeld;
        }
    }
    mixer_.begin(frames);
    for(auto& c:clips_) {
        auto& t=tracks_[c.track];auto& s=*sources_[c.source];
        for(int i=0;i<frames;++i) {
            const double gain=c.gain.next();if(t.suppress)continue;Frame time=at+i-c.start;if(time<0 || time>=c.length)continue;
            Frame offset=c.offset+time;auto& pin=c.pins[(offset>=c.pins[0].start+sourcePageFrames)?1:0];int index=static_cast<int>(offset-pin.start);
            if(index<0 || index>=pin.valid){releasePins();return false;}
            const double fade=c.envelope.at(time);
            if(s.mono())mixer_.add(c.track,0,i,static_cast<double>(pin.left[index])*gain*fade);
            else {mixer_.add(c.track,1,i,static_cast<double>(pin.left[index])*gain*fade);mixer_.add(c.track,2,i,static_cast<double>(pin.right[index])*gain*fade);}
        }
    }
    for(auto& t:tracks_) {
        bool monitoring=t.mode==1 || (t.mode==2 && t.armed && (!playing || recording));if(!monitoring)continue;
        bool valid=input;for(int ch=0;ch<t.channels;++ch)if(t.input[ch]<0 || t.input[ch]>=inputs || !input || !input[t.input[ch]])valid=false;
        if(valid)for(int ch=0;ch<t.channels;++ch)for(int i=0;i<frames;++i)if(!std::isfinite(input[t.input[ch]][inputOffset+i]))valid=false;
        if(!valid){monitorFault_=1;continue;}
        const auto node=static_cast<std::size_t>(&t-tracks_.data());const auto* l=input[t.input[0]]+inputOffset;
        for(int i=0;i<frames;++i)if(t.channels==1)mixer_.add(node,0,i,l[i]);else {mixer_.add(node,1,i,l[i]);mixer_.add(node,2,i,input[t.input[1]][inputOffset+i]);}
    }
    releasePins();
    if(!mixer_.finish(frames,left,right,revision,at,playing,recording))return false;
    audibleOffset_=pendingLatency_<frames && mixer_.pluginOutputCurrent()?static_cast<int>(pendingLatency_):-1;
    pendingLatency_=std::max<Frame>(0,pendingLatency_-frames);pendingSettle_=std::max<Frame>(0,pendingSettle_-frames);
    return true;
}
}
