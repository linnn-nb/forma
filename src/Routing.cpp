// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Routing.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>
#include <limits>

namespace ndaw {
std::string mainBusId(const Json& s) {return digest(s.at("id").get<std::string>()+":main-bus").substr(0,32);}
std::string outputRouteId(const std::string& id) {return digest(id+":output-route").substr(0,32);}
RoutingPlan compileRouting(const Json& s) {
    RoutingPlan p;std::unordered_map<std::string,std::size_t> tracks,buses;
    std::set<std::string> owners;const auto main=s.at("main_bus_id").get<std::string>();bool haveMain=false;
    for(const auto& t:s.at("tracks")) {
        RouteNodeSpec node;node.id=t.at("id");node.kind=t.at("kind");node.gainDb=t.at("gain_db");node.pan=t.at("pan");node.muted=t.at("muted");
        for(const auto& fx:t.at("processors")) {
            if(fx.at("kind")=="plugin") {validateSessionPlugin(fx);ProcessorSpec spec;spec.id=fx.at("id");spec.kind="plugin";spec.plugin=fx;
                spec.lookahead=pluginPipeline+fx.at("description").at("reported_latency_frames").get<int>();node.processors.push_back(std::move(spec));}
            else if(fx.at("kind")=="lookahead_limiter")node.processors.push_back({fx.at("id"),fx.at("lookahead_frames"),fx.at("ceiling_db"),fx.at("release_ms"),"lookahead_limiter",{}});
            else throw Error("unsupported_processor","Unknown actual processor");
        }
        tracks.emplace(node.id,p.nodes.size());p.nodes.push_back(std::move(node));
    }
    for(const auto& b:s.at("buses")) {
        const auto id=b.at("id").get<std::string>(),role=b.at("role").get<std::string>();
        if(b.at("channels")!=2)throw Error("routing_layout","This graph currently processes stereo buses only");
        if(role=="main"){if(haveMain || id!=main)throw Error("routing_bus","Exactly one named main bus is required");haveMain=true;}
        else if(role!="aux")throw Error("routing_bus","Unknown bus role");
        if(b.at("owner_track_id").is_null()) {
            if(role!="main")throw Error("routing_bus","Aux bus requires its actual processing track");
            p.sink=p.nodes.size();p.nodes.push_back({id,"main_bus",0,0,false,{},0,0});buses.emplace(id,p.sink);
        } else {
            const auto owner=b.at("owner_track_id").get<std::string>();auto found=tracks.find(owner);
            if(found==tracks.end() || !owners.insert(owner).second)throw Error("routing_bus","Missing or duplicated bus owner");
            const auto& t=s.at("tracks")[found->second];
            if(t.at("input_bus_id")!=id || t.at("kind")!=(role=="main"?"master":"aux"))throw Error("routing_bus","Bus/track input ownership mismatch");
            buses.emplace(id,found->second);if(role=="main")p.sink=found->second;
        }
    }
    if(!haveMain)throw Error("routing_bus","Main bus is missing");
    for(const auto& t:s.at("tracks"))if(t.at("kind")!="audio" && !owners.contains(t.at("id").get<std::string>()))throw Error("routing_bus","Mix track has no actual input bus");
    auto edge=[&](const Json& r,std::size_t from,bool send) {
        auto to=buses.find(r.at("target_bus_id").get<std::string>());if(to==buses.end())throw Error("routing_target","Route refers to a missing bus");
        p.edges.push_back({r.at("id"),from,to->second,send,send?r.at("pre_fader").get<bool>():false,
            send?r.at("gain_db").get<double>():0,send?r.at("pan").get<double>():0,send?r.at("muted").get<bool>():false,0});
    };
    for(std::size_t i=0;i<s.at("tracks").size();++i) {
        const auto& t=s.at("tracks")[i];
        if(t.at("kind")=="master") {if(!t.at("output").is_null() || !t.at("sends").empty())throw Error("routing_master","Master fader is the final hardware-output stage");}
        else {if(t.at("output").is_null())throw Error("routing_target","Track output cannot be silently disconnected");edge(t.at("output"),i,false);for(const auto& r:t.at("sends"))edge(r,i,true);}
    }
    std::vector<unsigned> incoming(p.nodes.size());std::vector<std::vector<std::size_t>> outgoing(p.nodes.size());
    for(std::size_t i=0;i<p.edges.size();++i){++incoming[p.edges[i].to];outgoing[p.edges[i].from].push_back(i);}
    for(std::size_t i=0;i<incoming.size();++i)if(!incoming[i])p.order.push_back(i);
    for(std::size_t k=0;k<p.order.size();++k)for(auto e:outgoing[p.order[k]])if(--incoming[p.edges[e].to]==0)p.order.push_back(p.edges[e].to);
    if(p.order.size()!=p.nodes.size())throw Error("routing_cycle","Feedback/cyclic routing is rejected even when a route is muted");
    for(auto n:p.order) {
        auto& node=p.nodes[n];Frame own=0;for(const auto& fx:node.processors){if(fx.lookahead<0 || own>INT64_MAX/4-fx.lookahead)throw Error("routing_latency","Latency accumulation overflow");own+=fx.lookahead;}
        if(own>INT64_MAX/4-node.inputLatency)throw Error("routing_latency","Latency accumulation overflow");node.outputLatency=node.inputLatency+own;
        for(auto e:outgoing[n])p.nodes[p.edges[e].to].inputLatency=std::max(p.nodes[p.edges[e].to].inputLatency,node.outputLatency);
    }
    for(auto& e:p.edges)e.compensation=p.nodes[e.to].inputLatency-p.nodes[e.from].outputLatency;
    p.latency=p.nodes[p.sink].outputLatency;
    Json topology={{"sink",p.nodes[p.sink].id},{"nodes",Json::array()},{"edges",Json::array()}};
    for(const auto& n:p.nodes){Json processors=Json::array();for(const auto& fx:n.processors){Json token=Json::array({fx.id,fx.lookahead});if(fx.kind=="plugin")token.push_back(sessionPluginRuntimeToken(fx.plugin));processors.push_back(token);}topology["nodes"].push_back({n.id,n.kind,processors});}
    for(const auto& e:p.edges)topology["edges"].push_back({e.id,p.nodes[e.from].id,p.nodes[e.to].id,e.send,e.pre,e.compensation});
    p.topology=topology.dump();return p;
}
Json routingFacts(const Json& s) {
    validateSession(s);const auto p=compileRouting(s);Json nodes=Json::array(),edges=Json::array();
    for(const auto& n:p.nodes)nodes.push_back({{"id",n.id},{"kind",n.kind},{"input_latency_frames",n.inputLatency},{"output_latency_frames",n.outputLatency}});
    for(const auto& e:p.edges)edges.push_back({{"id",e.id},{"from",p.nodes[e.from].id},{"to",p.nodes[e.to].id},{"kind",e.send?"send":"output"},{"tap",e.pre?"post_fx_pre_fader_independent_pan":"post_fx_post_fader"},{"compensation_frames",e.compensation}});
    return {{"session_id",s.at("id")},{"revision",s.at("revision")},{"nodes",nodes},{"routes",edges},{"algorithmic_latency_frames",p.latency},
        {"precision","float64 routing/builtin processing; isolated SDK plugin float32; float32 device/file boundary"},
        {"plugin_boundary","first stereo plugin folds mono at equal-power centre; subsequent pan/sends use stereo balance; pipeline delay included"},{"verification","compiled topology query; not an executed audio test"}};
}
void RoutingResources::reserve(std::size_t bytes) {
    auto used=used_.load();do{if(bytes>limit_ || used>limit_-bytes)throw Error("audio_resources","Concurrent routing/processor state exceeds configured memory budget");}while(!used_.compare_exchange_weak(used,used+bytes));
}
void RoutingResources::release(std::size_t bytes) noexcept {used_.fetch_sub(bytes);}
std::shared_ptr<RouteMeter> RoutingResources::meter(const std::string& id) {
    std::lock_guard lock(mutex_);if(auto m=meters_[id].lock())return m;auto m=std::make_shared<RouteMeter>();meters_[id]=m;
    for(auto it=meters_.begin();it!=meters_.end();)if(it->second.expired())it=meters_.erase(it);else ++it;
    return m;
}
Json RoutingResources::metrics() const {
    std::lock_guard lock(mutex_);Json meters=Json::array();for(const auto& [id,weak]:meters_)if(auto m=weak.lock())meters.push_back({{"id",id},{"input_sample_peak",m->input.load()},{"pre_fader_sample_peak",m->pre.load()},{"post_fader_sample_peak",m->post.load()},{"processed_revision",m->revision.load()},{"processed_blocks",m->blocks.load()}});
    return {{"bytes",used_.load()},{"limit_bytes",limit_},{"meters",meters},{"scope","actual last processed block; sample peak, not true peak/LUFS"}};
}
double RoutingMixer::Ramp::next() noexcept {if(remaining>0){value+=step;if(--remaining==0)value=target;}return value;}
void RoutingMixer::Ramp::from(const Ramp& old,int frames) noexcept {if(target==old.target){value=old.value;step=old.step;remaining=old.remaining;}else{value=old.value;remaining=frames;step=(target-value)/frames;}}
RoutingMixer::Delay::Delay(Frame frames):data(static_cast<std::size_t>(frames)*2){}
void RoutingMixer::Delay::process(double& l,double& r) noexcept {
    if(data.empty())return;auto i=cursor*2;const auto oldL=filled>=data.size()/2?data[i]:0,oldR=filled>=data.size()/2?data[i+1]:0;
    data[i]=l;data[i+1]=r;l=oldL;r=oldR;cursor=(cursor+1)%(data.size()/2);filled=std::min(filled+1,data.size()/2);
}
void RoutingMixer::Delay::inherit(Delay& old) noexcept {if(data.size()==old.data.size()){data.swap(old.data);std::swap(cursor,old.cursor);std::swap(filled,old.filled);}}
RoutingMixer::Limiter::Limiter(const ProcessorSpec& s,int rate):spec(s),samples((s.lookahead+1)*3),ceiling(std::pow(10.0,s.ceilingDb/20)),release(-std::expm1(-1000.0/(rate*s.releaseMs))) {
    leaves=1;while(leaves<static_cast<std::size_t>(s.lookahead+1))leaves*=2;peaks.resize(2*leaves);tags.resize(2*leaves);
}
bool RoutingMixer::Limiter::process(std::array<double,3>& x) noexcept {
    if(invalid)return false;double peak=std::max(std::abs(x[0])+std::abs(x[1]),std::abs(x[0])+std::abs(x[2]));if(!std::isfinite(peak))return false;
    const auto size=static_cast<std::size_t>(spec.lookahead+1);for(int ch=0;ch<3;++ch)samples[cursor*3+ch]=x[ch];
    auto node=leaves+cursor;peaks[node]=peak;tags[node]=epoch;
    while(node>1){node/=2;const auto l=tags[node*2]==epoch?peaks[node*2]:0,r=tags[node*2+1]==epoch?peaks[node*2+1]:0;peaks[node]=std::max(l,r);tags[node]=epoch;}
    const auto maximum=peaks[1];const double target=maximum>ceiling?ceiling/maximum:1;gain=std::min(target,gain+(1-gain)*release);
    const auto read=(cursor+1)%size;for(int ch=0;ch<3;++ch)x[ch]=filled>=static_cast<std::size_t>(spec.lookahead)?samples[read*3+ch]*gain:0;
    cursor=read;filled=std::min(filled+1,size);return true;
}
void RoutingMixer::Limiter::reset() noexcept {cursor=filled=0;gain=1;if(epoch==std::numeric_limits<std::uint64_t>::max())invalid=true;else ++epoch;}
void RoutingMixer::Limiter::inherit(Limiter& old) noexcept {
    if(spec.id!=old.spec.id || spec.lookahead!=old.spec.lookahead)return;
    samples.swap(old.samples);peaks.swap(old.peaks);tags.swap(old.tags);std::swap(cursor,old.cursor);std::swap(filled,old.filled);std::swap(epoch,old.epoch);std::swap(gain,old.gain);std::swap(invalid,old.invalid);
}
RoutingMixer::RoutingMixer(const Json& s,std::shared_ptr<RoutingResources> resources,const fs::path& root,PluginProcessingMode mode,std::shared_ptr<PluginResources> plugins,const PluginPreparation* context):plan_([&]{validateSession(s);return compileRouting(s);}()),resources_(resources?std::move(resources):std::make_shared<RoutingResources>()),plugins_(plugins?std::move(plugins):std::make_shared<PluginResources>()),mode_(mode),rampFrames_(s.at("sample_rate").get<int>()/200) {
    PluginPreparation local;const auto* preparation=context?context:&local;
    std::size_t total=plan_.nodes.size()*sizeof(Node)+plan_.edges.size()*sizeof(Edge);
    auto addBytes=[&](std::size_t n){if(n>std::numeric_limits<std::size_t>::max()-total)throw Error("audio_resources","Routing memory size overflow");total+=n;};
    for(const auto& n:plan_.nodes)for(const auto& fx:n.processors)if(fx.kind=="lookahead_limiter"){std::size_t leaves=1;while(leaves<static_cast<std::size_t>(fx.lookahead+1))leaves*=2;addBytes((fx.lookahead+1)*3*sizeof(double)+leaves*2*(sizeof(double)+sizeof(std::uint64_t)));}
    for(const auto& n:plan_.nodes)for(const auto& fx:n.processors)if(fx.kind=="plugin")addBytes(fx.plugin.at("parameters").size()*sizeof(PluginControlValue));
    for(const auto& e:plan_.edges){if(static_cast<std::uint64_t>(e.compensation)>std::numeric_limits<std::size_t>::max()/(2*sizeof(double)))throw Error("audio_resources","Delay storage size overflow");addBytes(static_cast<std::size_t>(e.compensation)*2*sizeof(double));}
    resources_->reserve(total);bytes_=total;
    try {
        for(std::size_t i=0;i<plan_.nodes.size();++i){const auto& spec=plan_.nodes[i];auto n=std::make_unique<Node>();const auto angle=(spec.pan+1)*std::numbers::pi/4;
            const double pan[]{std::cos(angle),std::sin(angle),spec.pan<=0?1:1-spec.pan,spec.pan>=0?1:1+spec.pan};
            for(int ch=0;ch<4;++ch)n->pan[ch].value=n->pan[ch].target=pan[ch];n->volume.value=n->volume.target=std::pow(10.0,spec.gainDb/20);n->mute.value=n->mute.target=spec.muted?0:1;
            for(const auto& fx:spec.processors){Node::Processor processor;processor.id=fx.id;processor.latency=fx.lookahead;
                if(fx.kind=="plugin"){if(root.empty())throw Error("plugin_state","A session root is required for actual project plugins");processor.plugin=plugins_->acquire(fx.plugin,root,s.at("sample_rate"),mode,preparation);processor.control=processor.plugin->controlSnapshot(fx.plugin);}
                else processor.builtin=std::make_unique<Limiter>(fx,s.at("sample_rate"));n->processors.push_back(std::move(processor));}
            n->meter=resources_->meter(spec.id);nodeIndex_[spec.id]=i;nodes_.push_back(std::move(n));}
        for(std::size_t i=0;i<plan_.edges.size();++i){const auto& spec=plan_.edges[i];Edge e;e.delay=Delay(spec.compensation);double gain=spec.muted?0:std::pow(10.0,spec.gainDb/20);const auto angle=(spec.pan+1)*std::numbers::pi/4;
            const double pan[]{std::cos(angle),std::sin(angle),spec.pan<=0?1:1-spec.pan,spec.pan>=0?1:1+spec.pan};
            for(int ch=0;ch<4;++ch)e.pan[ch].value=e.pan[ch].target=pan[ch];e.level.value=e.level.target=gain;
            nodes_[spec.from]->outgoing.push_back(i);edgeIndex_[spec.id]=i;edges_.push_back(std::move(e));}
    }catch(...){resources_->release(bytes_);bytes_=0;throw;}
}
RoutingMixer::~RoutingMixer(){if(bytes_)resources_->release(bytes_);}
std::size_t RoutingMixer::nodeFor(const std::string& id) const {auto it=nodeIndex_.find(id);if(it==nodeIndex_.end())throw Error("routing_target","Track has no compiled node");return it->second;}
void RoutingMixer::begin(int frames) noexcept {for(auto& n:nodes_)for(auto& ch:n->raw)std::fill_n(ch,frames,0.0);}
void RoutingMixer::add(std::size_t node,int lane,int sample,double value) noexcept {nodes_[node]->raw[lane][sample]+=value;}
bool RoutingMixer::finish(int frames,float* left,float* right,std::uint64_t revision,Frame position,bool playing,bool recording,std::atomic<bool>* cancel) noexcept {
    if(frames<0 || frames>mixBlock)return false;
    std::array<double,mixBlock> preL{},preR{},postL{},postR{},fader{};double processed[3][mixBlock]{};
    for(auto index:plan_.order) {
        auto& node=*nodes_[index];const bool master=plan_.nodes[index].kind=="master";double inputPeak=0,prePeak=0,postPeak=0;
        for(int i=0;i<frames;++i){std::array<double,3> x{node.raw[0][i],node.raw[1][i],node.raw[2][i]};inputPeak=std::max(inputPeak,std::max(std::abs(x[0])+std::abs(x[1]),std::abs(x[0])+std::abs(x[2])));
            const double volume=node.volume.next(),mute=node.mute.next();if(master)for(auto& ch:x)ch*=volume;
            Frame offset=plan_.nodes[index].inputLatency;
            for(auto& fx:node.processors) {
                if(fx.plugin){double l=x[1]+x[0]*std::numbers::sqrt2/2,r=x[2]+x[0]*std::numbers::sqrt2/2;
                    if(!fx.plugin->sample(l,r,position+i-offset,playing,recording,cancel,&fx.control))return false;x={0,l,r};}
                else if(!fx.builtin->process(x))return false;offset+=fx.latency;
            }
            for(int ch=0;ch<3;++ch)processed[ch][i]=x[ch];fader[i]=volume*mute;
            const double ml=node.pan[0].next(),mr=node.pan[1].next(),sl=node.pan[2].next(),sr=node.pan[3].next();
            preL[i]=x[0]*ml+x[1]*sl;preR[i]=x[0]*mr+x[2]*sr;postL[i]=preL[i]*(master?1:volume)*mute;postR[i]=preR[i]*(master?1:volume)*mute;
            if(!std::isfinite(postL[i]) || !std::isfinite(postR[i]) || !std::isfinite(preL[i]) || !std::isfinite(preR[i]))return false;
            prePeak=std::max(prePeak,std::max(std::abs(preL[i]),std::abs(preR[i])));postPeak=std::max(postPeak,std::max(std::abs(postL[i]),std::abs(postR[i])));
        }
        node.meter->input.store(inputPeak);node.meter->pre.store(prePeak);node.meter->post.store(postPeak);node.meter->revision.store(revision);if(frames)node.meter->blocks.fetch_add(1);
        for(auto edgeIndex:node.outgoing){auto& e=edges_[edgeIndex];const auto& spec=plan_.edges[edgeIndex];auto& target=*nodes_[spec.to];
            for(int i=0;i<frames;++i){const double ml=e.pan[0].next(),mr=e.pan[1].next(),sl=e.pan[2].next(),sr=e.pan[3].next(),level=e.level.next();
                double l=postL[i],r=postR[i];if(spec.send){const double factor=spec.pre?1:fader[i];l=(processed[0][i]*ml+processed[1][i]*sl)*factor;r=(processed[0][i]*mr+processed[2][i]*sr)*factor;}
                l*=level;r*=level;e.delay.process(l,r);target.raw[1][i]+=l;target.raw[2][i]+=r;}}
        if(index==plan_.sink)for(int i=0;i<frames;++i){left[i]=static_cast<float>(postL[i]);right[i]=static_cast<float>(postR[i]);if(!std::isfinite(left[i]) || !std::isfinite(right[i]))return false;}
    }
    return true;
}
void RoutingMixer::inherit(RoutingMixer& old) noexcept {
    for(std::size_t i=0;i<nodes_.size();++i)if(auto found=old.nodeIndex_.find(plan_.nodes[i].id);found!=old.nodeIndex_.end()){
        auto& n=*nodes_[i];auto& prior=*old.nodes_[found->second];for(int ch=0;ch<4;++ch)n.pan[ch].from(prior.pan[ch],rampFrames_);n.volume.from(prior.volume,rampFrames_);n.mute.from(prior.mute,rampFrames_);
        for(auto& fx:n.processors)for(auto& existing:prior.processors)if(fx.id==existing.id){if(fx.builtin && existing.builtin)fx.builtin->inherit(*existing.builtin);if(fx.plugin && existing.plugin && fx.control.token!=existing.control.token)parameterTransition_=true;break;}
    }
    for(std::size_t i=0;i<edges_.size();++i)if(auto found=old.edgeIndex_.find(plan_.edges[i].id);found!=old.edgeIndex_.end()){
        auto& e=edges_[i];auto& previous=old.edges_[found->second];for(int ch=0;ch<4;++ch)e.pan[ch].from(previous.pan[ch],rampFrames_);e.level.from(previous.level,rampFrames_);e.delay.inherit(previous.delay);
    }
}
void RoutingMixer::reset() noexcept {for(auto& n:nodes_){for(auto& pan:n->pan)pan.reset();n->volume.reset();n->mute.reset();for(auto& fx:n->processors){if(fx.plugin)fx.plugin->reset();else fx.builtin->reset();}}for(auto& e:edges_){for(auto& pan:e.pan)pan.reset();e.level.reset();e.delay.reset();}}
bool RoutingMixer::pluginFailed() const noexcept {for(const auto& n:nodes_)for(const auto& fx:n->processors)if(fx.plugin && fx.plugin->failed())return true;return false;}
bool RoutingMixer::pluginOutputCurrent() const noexcept {for(const auto& n:nodes_)for(const auto& fx:n->processors)if(fx.plugin && !fx.plugin->outputCurrent(fx.control))return false;return true;}
Json RoutingMixer::closeOfflinePlugins() {if(mode_!=PluginProcessingMode::Offline)throw Error("plugin_mode","Live plugin teardown must follow graph retirement");Json receipts=Json::array();for(auto& n:nodes_)for(auto& fx:n->processors)if(fx.plugin)receipts.push_back(fx.plugin->close());return receipts;}
int RoutingMixer::remainingRampFrames() const noexcept {int remaining=0;for(const auto& n:nodes_){for(const auto& pan:n->pan)remaining=std::max(remaining,pan.remaining);remaining=std::max({remaining,n->volume.remaining,n->mute.remaining});}for(const auto& e:edges_){for(const auto& pan:e.pan)remaining=std::max(remaining,pan.remaining);remaining=std::max(remaining,e.level.remaining);}return remaining;}
}
