#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2 {
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::string id(te::Track& t){return t.itemID.toString().toStdString();}
void validName(const std::string& name){auto utf=juce::String::fromUTF8(name.data(),int(name.size()));require(!name.empty() && utf.length()<=64 && utf.toStdString()==name && name.find('\0')==std::string::npos,"track name must contain 1..64 valid characters, no NUL (SDK limit)");}
juce::Colour parseColour(const std::string& value){
    if(value=="default")return {};
    require(value.size()==7&&value[0]=='#'&&value.substr(1).find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,"colour requires #RRGGBB or default");
    return juce::Colour::fromString(juce::String("ff"+value.substr(1)));
}
bool groupType(const std::string& type){return type=="folder" || type=="vca";}
}
te::Track* Commands::domainTrack(const std::string& target) const {
    for(auto* t:te::getAllTracks(*edit))if((dynamic_cast<te::AudioTrack*>(t)||dynamic_cast<te::FolderTrack*>(t)) && id(*t)==target)return t;
    return nullptr;
}
void Commands::registerHierarchyCommands(Json& registry) {
    const Json string={{"type","string"}};
    auto add=[&](const char* command,Json properties){Json required=Json::array();for(auto it=properties.begin();it!=properties.end();++it)required.push_back(it.key());
        registry.push_back({{"id",command},{"schema",{{"type","object"},{"required",required},{"properties",properties},{"additionalProperties",false}}},
            {"risk","low"},{"permission","edit"},{"reversible",true},{"live",false},{"test","M1-GROUP-01"}});};
    add("track.order",{{"track",string},{"index",{{"type","integer"},{"minimum",0}}}});
    registry.back()["test"]="M1-TRACK-01";
    registry.back()["units"]={{"index","zero-based position among siblings; moves entire subtree, preserves parent and routing"}};
    add("track.colour",{{"track",string},{"colour",string}});
    registry.back()["test"]="M1-TRACK-01";
    registry.back()["units"]={{"colour","opaque #RRGGBB or default; shared by Edit and Mix"}};
    add("track.delete",{{"track",string},{"connections",{{"type","string"},{"enum",{"reject","disconnect"}}}}});
    registry.back()["risk"]="medium";registry.back()["test"]="M1-TRACK-01";
    registry.back()["units"]={{"connections","reject incoming references, or explicitly disconnect outputs to None and remove incoming sends; deletes descendants; never deletes media"}};
    add("track.parent",{{"track",string},{"parent",string}});
    registry.back()["units"]={{"parent","folder/VCA stable ID or preceding local reference; root detaches; preserves audio output and member faders"}};
    add("track.rename",{{"track",string},{"name",string}});
    add("track.collapsed",{{"track",string},{"enabled",{{"type","boolean"}}}});
}
Json Commands::hierarchyQuery(te::Track& t) const {
    int depth=0;bool hidden=false;for(auto* p=t.getParentTrack();p;p=p->getParentTrack()){++depth;hidden|=bool(p->state.getProperty("ndaw_collapsed",false));}
    Json members=Json::array();for(auto* child:t.getAllSubTracks(false))members.push_back(id(*child));
    Json facts={{"automation_mode",te::toString(t.automationMode.get()).toStdString()},{"parent",t.getParentTrack()?id(*t.getParentTrack()):"root"},{"depth",depth},{"children",members},{"edit_hidden",hidden},{"collapsed",bool(t.state.getProperty("ndaw_collapsed",false))}};
    int order=0;for(auto* sibling:te::getAllTracks(*edit))if(sibling!=&t&&sibling->getParentTrack()==t.getParentTrack()&&(dynamic_cast<te::AudioTrack*>(sibling)||dynamic_cast<te::FolderTrack*>(sibling))){if(sibling->state.getParent().indexOf(sibling->state)<t.state.getParent().indexOf(t.state))++order;}
    facts["order_index"]=order;facts["colour"]=t.getColour().isTransparent()?Json(nullptr):Json("#"+t.getColour().toDisplayString(false).toStdString());
    if(auto* a=dynamic_cast<te::AudioTrack*>(&t)) {facts["gain_db"]=a->getVolumePlugin()->getVolumeDb();facts["base_gain_db"]=te::volumeFaderPositionToDB(a->getVolumePlugin()->volParam->getCurrentExplicitValue());facts["automation_volume_points"]=a->getVolumePlugin()->volParam->getCurve().getNumPoints();facts["capabilities"]={{"gain",true},{"audio_routing",true},{"clips",true},{"group",false}};return facts;}
    auto* f=dynamic_cast<te::FolderTrack*>(&t);require(f!=nullptr,"unsupported domain track");auto* v=f->getVCAPlugin();
    // Organisational folders contain no audio processor. VCA uses the SDK's real
    // parent-to-member fader linkage; it does not rewrite member parameters.
    facts.update(Json{{"id",id(t)},{"name",t.getName().toStdString()},{"type",v?"vca":"folder"},{"gain_db",v?Json(v->getVolumeDb()):Json(nullptr)},
        {"base_gain_db",v?Json(te::volumeFaderPositionToDB(v->volParam->getCurrentExplicitValue())):Json(nullptr)},{"automation_volume_points",v?v->volParam->getCurve().getNumPoints():0},{"pan_law",nullptr},{"mute",t.isMuted(false)},{"solo",t.isSolo(false)},{"solo_safe",t.isSoloIsolate(false)},{"audible",t.shouldBePlayed()},
        {"clips",Json::array()},{"plugins",Json::array()},{"sends",Json::array()},{"output",{{"kind","control"},{"target","none"},{"name",v?"VCA · member faders":"Folder · organisation"}}},
        {"capabilities",{{"gain",v!=nullptr},{"audio_routing",false},{"clips",false},{"group",true}}}});
    if(v)facts["vca"]={{"id",v->itemID.toString().toStdString()},{"law","Tracktion fader-position offset"},{"membership","hierarchical; descendants"}};
    return facts;
}
Json Commands::validateHierarchyPlan(const Json& operations) const {
    struct Node {std::string type,parent;Json facts;std::set<std::string> objects;};std::map<std::string,Node> nodes;
    std::vector<std::string> order;std::map<std::string,std::string> outputs;std::map<std::string,std::pair<std::string,std::string>> sends;
    std::set<std::string> dead;Json diff=Json::array();auto q=query();bool hasDeletion=std::any_of(operations.begin(),operations.end(),[](const auto& op){return op.at("command")=="track.delete";});
    for(const auto& t:q["tracks"]){std::string key=t["id"];nodes[key]={t["type"],t["parent"],t,{}};if(hasDeletion)nodes[key].facts["automation"]=automationQuery(key);order.push_back(key);if(t["capabilities"]["audio_routing"]){outputs[key]=t["output"]["target"];for(const auto& send:t["sends"])if(send["target"].is_string())sends[send["id"]]={key,send["target"]};}
        auto* native=domainTrack(key);std::function<void(juce::ValueTree)> collect=[&](juce::ValueTree state){for(const auto& field:{te::IDs::id,juce::Identifier("ndaw_id")})if(state.hasProperty(field))nodes[key].objects.insert(state.getProperty(field).toString().toStdString());for(auto child:state)collect(child);};collect(native->state);
    }
    auto subtree=[&](const std::string& root){std::set<std::string> affected{root};bool changed=true;while(changed){changed=false;for(const auto& [key,n]:nodes)if(!affected.contains(key)&&affected.contains(n.parent)){affected.insert(key);changed=true;}}return affected;};
    size_t serial=0;
    for(const auto& op:operations){const std::string cmd=op.at("command");const auto& a=op.at("args");
        for(const char* key:{"track","parent","target","clip","plugin","send","note","point","parameter"})if(a.contains(key)&&a[key].is_string()){auto value=a[key].get<std::string>();require(!dead.contains(value)&&!dead.contains(value.substr(0,value.find("::"))),"target deleted earlier in Plan");}
        if(cmd=="track.create"){validName(a.at("name"));std::string key=a.at("ref");nodes[key]={a.value("type",std::string("audio")),"root",{{"id",key},{"name",a.at("name")},{"clips",Json::array()},{"plugins",Json::array()}},{key}};order.push_back(key);outputs[key]="master";++serial;continue;}
        // Keep ownership of newly planned objects so later deletion cannot leave
        // a seemingly valid clip / note reference that will disappear at commit.
        if(a.contains("ref")&&cmd!="track.create"){
            std::string owner=a.value("track",std::string{});if(owner.empty()&&a.contains("clip"))for(const auto& [key,n]:nodes)if(n.objects.contains(a["clip"]))owner=key;
            if(nodes.contains(owner))nodes.at(owner).objects.insert(a.at("ref"));
        }
        if(cmd=="track.output")outputs[a.at("track")]=a.at("target");
        if(cmd=="send.create")sends["#planned-send-"+std::to_string(serial)]={a.at("track"),a.at("target")};
        if(cmd=="send.remove")sends.erase(a.at("send"));
        if(!a.contains("track")){++serial;continue;}const std::string target=a.at("track");require(nodes.contains(target),"domain track not found");auto& n=nodes.at(target);
        if(cmd=="track.parent"){
            const std::string parent=a.at("parent");require(parent=="root"||(nodes.contains(parent)&&groupType(nodes.at(parent).type)),"parent requires a Folder or VCA");
            require(target!=parent,"track cannot contain itself");std::string ancestor=parent;std::set<std::string> visited;
            while(ancestor!="root"){require(ancestor!=target && visited.insert(ancestor).second,"track hierarchy cycle rejected");require(nodes.contains(ancestor),"unresolved parent");ancestor=nodes.at(ancestor).parent;}
            n.parent=parent;std::erase(order,target);order.push_back(target);
        } else if(cmd=="track.order"){
            std::vector<std::string> siblings;for(const auto& key:order)if(nodes.at(key).parent==n.parent)siblings.push_back(key);
            int64_t index=a.at("index");require(index>=0&&index<int64_t(siblings.size()),"track order outside sibling range");
            int before=int(std::find(siblings.begin(),siblings.end(),target)-siblings.begin());auto anchor=siblings[size_t(index)];auto from=std::find(order.begin(),order.end(),target),to=std::find(order.begin(),order.end(),anchor);std::rotate(index<before?to:from,index<before?from:from+1,index<before?from+1:to+1);
            diff.push_back({{"command",cmd},{"track",target},{"parent",n.parent},{"before_index",before},{"after_index",index}});
        } else if(cmd=="track.colour"){
            auto colour=parseColour(a.at("colour"));auto after=colour.isTransparent()?Json(nullptr):Json("#"+colour.toDisplayString(false).toStdString());
            diff.push_back({{"command",cmd},{"track",target},{"before",n.facts.value("colour",Json(nullptr))},{"after",after}});n.facts["colour"]=after;
        } else if(cmd=="track.delete"){
            const std::string policy=a.at("connections");require(policy=="reject"||policy=="disconnect","unknown deletion connections policy");auto affected=subtree(target);
            Json disconnected=Json::array(),removedSends=Json::array(),tracks=Json::array();
            for(auto& [owner,output]:outputs)if(!affected.contains(owner)&&affected.contains(output)){disconnected.push_back({{"track",owner},{"before",output},{"after","none"}});output="none";}
            for(auto it=sends.begin();it!=sends.end();)if(affected.contains(it->second.first)||affected.contains(it->second.second)){if(!affected.contains(it->second.first)){removedSends.push_back({{"id",it->first},{"track",it->second.first},{"target",it->second.second}});dead.insert(it->first);}it=sends.erase(it);}else ++it;
            require(policy=="disconnect"||(disconnected.empty()&&removedSends.empty()),"track has incoming routes or sends; preview explicit disconnect policy");
            for(const auto& key:order)if(affected.contains(key)){auto& deleted=nodes.at(key);tracks.push_back({{"id",key},{"name",deleted.facts.at("name")},{"type",deleted.type},{"parent",deleted.parent},{"original_facts",deleted.facts}});dead.insert(key);dead.insert(deleted.objects.begin(),deleted.objects.end());outputs.erase(key);}
            diff.push_back({{"command",cmd},{"operation_index",serial},{"track",target},{"deleted_tracks",tracks},{"disconnected_outputs",disconnected},{"removed_incoming_sends",removedSends},{"media_files_deleted",false},{"snapshot","original facts; all preceding planned changes listed in changes"}});
            for(const auto& key:affected)nodes.erase(key);std::erase_if(order,[&](const auto& key){return affected.contains(key);});
        } else if(cmd=="track.rename"){validName(a.at("name"));n.facts["name"]=a.at("name");}
        else if(cmd=="track.collapsed")require(groupType(n.type),"only Folder/VCA can collapse");
        else if(cmd=="track.gain")require(n.type!="folder","organisational Folder has no gain control");
        else if(cmd=="clip.import" || (cmd=="plugin.insert"||cmd=="plugin.external.insert") || cmd=="track.output" || cmd=="send.create" || cmd=="midi.clip.create")require(!groupType(n.type),"Folder/VCA has no audio routing, inserts or clips");
        ++serial;
    }
    return diff;
}
void Commands::executeHierarchyOperation(const std::string& cmd,const Json& a) {
    auto* t=domainTrack(a.at("track"));require(t!=nullptr,"hierarchy target disappeared");
    if(cmd=="track.rename")t->setName(juce::String(a.at("name").get<std::string>()));
    else if(cmd=="track.collapsed")t->state.setProperty("ndaw_collapsed",bool(a.at("enabled")),&edit->getUndoManager());
    else if(cmd=="track.colour")t->setColour(parseColour(a.at("colour")));
    else if(cmd=="track.order"){
        std::vector<te::Track*> siblings;for(auto* candidate:te::getAllTracks(*edit))if(candidate->getParentTrack()==t->getParentTrack()&&(dynamic_cast<te::AudioTrack*>(candidate)||dynamic_cast<te::FolderTrack*>(candidate)))siblings.push_back(candidate);
        auto parent=t->state.getParent();auto index=a.at("index").get<size_t>();require(index<siblings.size(),"order target disappeared");
        parent.moveChild(parent.indexOf(t->state),parent.indexOf(siblings[index]->state),&edit->getUndoManager());
        require(hierarchyQuery(*domainTrack(a.at("track")))["order_index"]==index,"SDK track order failed");
    } else if(cmd=="track.delete"){
        std::set<std::string> affected{id(*t)};for(auto* child:t->getAllSubTracks(true))affected.insert(id(*child));
        std::vector<std::string> incoming;for(auto* source:te::getAudioTracks(*edit))if(!affected.contains(id(*source))){
            if(auto* dest=source->getOutput().getDestinationTrack();dest&&affected.contains(id(*dest))){require(a.at("connections")=="disconnect","incoming output requires disconnect");source->state.setProperty("ndaw_output_target","none",&edit->getUndoManager());source->getOutput().setOutputToNone();}
            auto routes=routingQuery(*source);for(const auto& s:routes["sends"])for(const auto& destination:s["targets"])if(affected.contains(destination)){require(a.at("connections")=="disconnect","incoming send requires disconnect");incoming.push_back(s["id"]);break;}
        }
        for(const auto& sid:incoming){auto* plugin=send(sid);require(plugin!=nullptr,"incoming send disappeared");plugin->flushPluginStateToValueTree();plugin->deleteFromParent();}
        t->flushStateToValueTree();for(auto* child:t->getAllSubTracks(true))child->flushStateToValueTree();
        edit->deleteTrack(t);require(domainTrack(a.at("track"))==nullptr,"SDK track deletion failed");restoreInputAssignments();
    } else {
        const std::string parent=a.at("parent");if((t->getParentTrack()?id(*t->getParentTrack()):"root")==parent)return;
        auto* dest=parent=="root"?nullptr:domainTrack(parent);require(parent=="root" || dynamic_cast<te::FolderTrack*>(dest),"parent disappeared");
        te::Track* last=nullptr;for(auto* candidate:te::getAllTracks(*edit))if(candidate!=t && candidate->getParentTrack()==dest && !candidate->isAChildOf(*t))last=candidate;
        // SDK ValueTree reparenting recreates Track objects. Never retain a raw
        // pointer across this operation; all subsequent targets resolve by ID.
        edit->moveTrack(t,te::TrackInsertPoint(dest,last));
        auto* moved=domainTrack(a.at("track"));require(moved && (moved->getParentTrack()?id(*moved->getParentTrack()):"root")==parent,"SDK track move failed");
    }
}
}
