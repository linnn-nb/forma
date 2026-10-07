#include <nativedaw/v2/McpSession.h>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>

using namespace ndaw::v2;
namespace {
class Storage final:public te::PropertyStorage {
public:explicit Storage(juce::File directory):PropertyStorage("Forma object queries"),folder(std::move(directory)){}
    juce::File getAppPrefsFolder()override{folder.createDirectory();return folder;}
private:juce::File folder;
};
struct Scratch {
    juce::File folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("forma-query-"+juce::Uuid().toString());
    Scratch(){folder.createDirectory();}~Scratch(){folder.deleteRecursively();}
};
int checks=0,requestID=0;double maximumCallMs=0;Json workloads=Json::array();
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);++checks;std::cout<<"PASS "<<reason<<std::endl;}
template<class F>void fails(F run,const char* reason){bool failed=false;try{run();}catch(const std::exception&){failed=true;}check(failed,reason);}
Json operation(const char* command,Json args){return {{"command",command},{"args",std::move(args)}};}
Json commit(Commands& c,Json operations){return c.commit(c.makePlan("human",std::move(operations)));}
void pump(){juce::MessageManager::getInstance()->runDispatchLoopUntil(1);}
Json rpc(McpSession& session,const char* method,Json params=Json::object()) {
    auto began=juce::Time::getMillisecondCounterHiRes();std::vector<Json> responses;const int id=++requestID;
    Json message{{"jsonrpc","2.0"},{"id",id},{"method",method},{"params",std::move(params)}};
    std::thread producer([&]{responses=session.receive(message.dump());});producer.join();
    while(responses.empty()) {
        if(juce::Time::getMillisecondCounterHiRes()-began>5000)throw std::runtime_error("query protocol call exceeded five seconds");
        pump();std::thread consumer([&]{responses=session.ready();});consumer.join();
    }
    const auto elapsed=juce::Time::getMillisecondCounterHiRes()-began;maximumCallMs=std::max(maximumCallMs,elapsed);
    if(elapsed>=2000)throw std::runtime_error("query protocol call exceeded fixed two-second budget");
    if(responses.size()!=1||responses[0]["id"]!=id)throw std::runtime_error("query response identity mismatch");
    return responses[0];
}
void initialize(McpSession& session) {
    rpc(session,"initialize",{{"protocolVersion","2025-11-25"},{"capabilities",Json::object()},
        {"clientInfo",{{"name","query-test"},{"version","1"}}}});
    std::thread worker([&]{session.receive(Json{{"jsonrpc","2.0"},{"method","notifications/initialized"}}.dump());});worker.join();
}
Json tool(McpSession& session,const char* name,Json args=Json::object()) {
    return rpc(session,"tools/call",{{"name",name},{"arguments",std::move(args)}});
}
Json result(const Json& response){return response.at("result").at("structuredContent");}
Json summary(McpSession& session){return result(tool(session,"query_session_summary"))["result"];}
Json arguments(const Json& snapshot,const char* collection,const std::string& target={}) {
    Json args{{"collection",collection},{"base_revision",snapshot["revision"]},{"session_token",snapshot["session_token"]}};
    if(!target.empty())args["target"]=target;return args;
}
Json all(McpSession& session,Json args,size_t* calls=nullptr) {
    Json items=Json::array();args["limit"]=32;size_t offset=0,total=0;
    do {
        args["offset"]=offset;auto response=tool(session,"query_objects",args);auto data=result(response);
        if(response["result"]["isError"]||data["status"]!="completed")throw std::runtime_error(data.dump());
        const auto& page=data["result"];if(calls)++*calls;
        if(page["items"].size()>32||page["items_bytes"]!=page["items"].dump().size()
            ||page["items_bytes"].get<size_t>()>Commands::maximumQueryPageBytes)throw std::runtime_error("page count or byte budget failed");
        if(offset==0)total=page["total"];if(total!=page["total"].get<size_t>()||page["offset"]!=offset)throw std::runtime_error("page position drift");
        for(const auto& item:page["items"])items.push_back(item);
        if(page["next_offset"].is_null())break;
        const auto next=page["next_offset"].get<size_t>();if(next<=offset)throw std::runtime_error("page made no progress");offset=next;
    }while(true);
    if(items.size()!=total)throw std::runtime_error("page enumeration lost objects");return items;
}
void knownAudio(const juce::File& path) {
    juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> out=path.createOutputStream();
    auto writer=wav.createWriterFor(out,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    if(!writer)throw std::runtime_error("query fixture writer failed");
    juce::AudioBuffer<float> signal(2,4800);for(int channel=0;channel<2;++channel)for(int n=0;n<4800;++n)
        signal.setSample(channel,n,.05f*std::sin(2*juce::MathConstants<double>::pi*1000*n/48000));
    if(!writer->writeFromAudioSampleBuffer(signal,0,4800))throw std::runtime_error("query fixture write failed");
}
}
int main(int argc,char** argv) {juce::ScopedJuceInitialiser_GUI gui;try {
    Scratch scratch;Commands c(false,std::make_unique<Storage>(scratch.folder.getChildFile("prefs")));
    CommandQueue queue(c);Scope readOnly;readOnly.mode=Permission::ReadOnly;
    auto client=queue.connect("agent:query-test",readOnly);McpSession session(client,Commands::registry());initialize(session);
    auto empty=summary(session);check(empty["counts"]["tracks"]==0&&all(session,arguments(empty,"tracks")).empty(),"empty native Edit has an explicit complete empty page");
    check(empty["native_limits"]["track_index_capacity"].get<int>()>512,"production Edit does not inherit the SDK 400-track product default");
    auto tools=McpSession::tools(Commands::registry());bool reflected=false;
    for(const auto& entry:Commands::registry())if(entry["id"]=="query.objects")
        for(const auto& definition:tools)if(definition["name"]=="query_objects")reflected=definition["inputSchema"]==entry["schema"]&&definition["annotations"]["readOnlyHint"];
    check(reflected,"read-only object tool schema is generated by the L1 registry");
    fails([&]{c.makePlan("human",Json::array({operation("query.summary",Json::object())}));},"query registry entries cannot enter an editing Plan");
    auto setupBegan=juce::Time::getMillisecondCounterHiRes();
    for(int count=0;count<512;) {
        Json ops=Json::array();for(int n=0;n<64;++n,++count)
            ops.push_back(operation("track.create",{{"name","Query track "+std::to_string(count+1)},{"ref","$t"+std::to_string(n)}}));
        commit(c,std::move(ops));
        if(count==128||count==256||count==512) {
            const auto snapshot=summary(session);const auto revision=snapshot["revision"];size_t calls=0;
            const auto began=juce::Time::getMillisecondCounterHiRes();auto pages=all(session,arguments(snapshot,"tracks"),&calls);
            const auto elapsed=juce::Time::getMillisecondCounterHiRes()-began;
            std::set<std::string> ids;bool facts=true;
            for(size_t i=0;i<pages.size();++i){ids.insert(pages[i]["id"]);facts&=pages[i]["name"]=="Query track "+std::to_string(i+1)&&!pages[i].contains("clips")&&!pages[i].contains("plugins")&&pages[i]["output"]["target"]=="master";}
            check(pages.size()==size_t(count)&&ids.size()==size_t(count)&&facts,"native track pages preserve every actual object and order without expanding details");
            check(snapshot["counts"]["tracks"]==count&&summary(session)["revision"]==revision,"large read-only enumeration preserves revision and reports actual totals");
            workloads.push_back({{"tracks",count},{"pages",calls},{"total_query_ms",elapsed}});
        }
    }
    auto native=c.query();const std::string audio=native["tracks"][0]["id"];queue.setSelection(audio);
    check(summary(session)["selection"]["track"]==audio,"compact snapshot carries verified GUI selection even outside a later page");
    queue.setSelection("missing","missing");check(summary(session)["selection"]["track"].is_null()&&summary(session)["selection"]["clip"].is_null(),"stale selection IDs are reported as missing facts");queue.setSelection(audio);
    auto source=scratch.folder.getChildFile("known.wav");knownAudio(source);const auto mediaHash=Commands::mediaHash(source);
    auto created=commit(c,Json::array({operation("clip.import",{{"track",audio},{"path",source.getFullPathName().toStdString()},{"position_samples",24000}}),
        operation("track.create",{{"name","Query MIDI"},{"type","midi"},{"ref","$m"}}),
        operation("midi.clip.create",{{"track","$m"},{"name","Dense editable MIDI"},{"ref","$c"},{"position_samples",12000},{"length_samples",384000}}),
        operation("track.create",{{"name","Query Reverb"},{"type","aux"},{"ref","$a"}}),
        operation("plugin.insert",{{"track","$a"},{"type",te::ReverbPlugin::xmlTypeName},{"wet_only",true}}),
        operation("send.create",{{"track",audio},{"target","$a"},{"db",-12},{"position","post"}}),
        operation("plugin.insert",{{"track",audio},{"type",te::EqualiserPlugin::xmlTypeName}})}));
    (void)created;native=c.query();std::string midi,clip,plugin;
    for(const auto& t:native["tracks"]){if(t["name"]=="Query MIDI"){midi=t["id"];clip=t["clips"][0]["id"];}if(t["id"]==audio)plugin=t["plugins"][0]["id"];}
    for(int batch=0;batch<8;++batch) {
        Json notes=Json::array();for(int n=0;n<64;++n){const int index=batch*64+n;
            notes.push_back(operation("midi.note.add",{{"clip",clip},{"pitch",48+index%24},{"velocity",30+index%90},
                {"position_samples",12000+index*256},{"length_samples",128}}));}
        commit(c,std::move(notes));
    }
    Json points=Json::array();for(int n=0;n<64;++n)points.push_back(operation("automation.point.add",{{"track",audio},{"parameter","volume"},
        {"ref","$p"+std::to_string(n)},{"position_samples",n*1024},{"value",-6+(n%6)},{"curve",0}}));commit(c,std::move(points));
    const auto setupMs=juce::Time::getMillisecondCounterHiRes()-setupBegan;
    check(setupMs<120000,"representative native fixture initialization meets the preset 120-second budget");
    auto snapshot=summary(session);native=c.query();const auto preserved=native;
    auto notes=all(session,arguments(snapshot,"midi_notes",clip));Json expectedNotes;
    for(const auto& t:native["tracks"])if(t["id"]==midi)expectedNotes=t["clips"][0]["notes"];
    check(notes.size()==512&&notes==expectedNotes,"dense MIDI pages preserve stable IDs, pitch, velocity, source beats and exact sample mapping");
    auto clipPage=all(session,arguments(snapshot,"clips",midi));check(clipPage.size()==1&&clipPage[0]["note_count"]==512&&!clipPage[0].contains("notes"),"MIDI clip summary declares omitted note details with the exact count");
    check(all(session,arguments(snapshot,"midi_controllers",clip)).empty(),"empty controller collection is explicit rather than invented MIDI data");
    auto sends=all(session,arguments(snapshot,"sends",audio));check(sends==native["tracks"][0]["sends"],"send pages report actual Aux target, bus, gain and position");
    auto plugins=all(session,arguments(snapshot,"plugins",audio));check(plugins.size()==1&&plugins[0]["id"]==plugin&&!plugins[0].contains("parameters"),"plugin summary identifies an actual editable instance without expanding parameters");
    check(all(session,arguments(snapshot,"parameters",plugin))==native["tracks"][0]["plugins"][0]["parameters"],"paged parameter IDs, ranges, units and live values match the native L1 facts");
    check(all(session,arguments(snapshot,"tempos"))==native["music"]["tempos"]&&all(session,arguments(snapshot,"meters"))==native["music"]["meters"],"Tempo and meter pages retain original IDs and musical/sample positions");
    auto expectedAutomation=c.automationQuery(audio);auto lanes=all(session,arguments(snapshot,"automation_lanes",audio));bool laneFacts=lanes.size()==expectedAutomation["lanes"].size();
    for(size_t n=0;n<lanes.size();++n){auto expected=expectedAutomation["lanes"][n];auto count=expected["points"].size();expected.erase("points");auto pageLane=lanes[n];laneFacts&=pageLane["point_count"]==count;pageLane.erase("point_count");laneFacts&=pageLane==expected;}
    check(laneFacts,"automation summaries preserve actual lane units and counts without serializing curves");
    auto curveArgs=arguments(snapshot,"automation_points",audio);curveArgs["parameter"]="volume";Json expectedPoints;
    for(const auto& lane:expectedAutomation["lanes"])if(lane["parameter"]=="volume")expectedPoints=lane["points"];
    check(all(session,curveArgs)==expectedPoints&&expectedPoints.size()==64,"automation point pages preserve stable IDs, fader dB conversion and curves");
    auto clips=all(session,arguments(snapshot,"clips",audio));check(clips[0]["path"]==source.getFullPathName().toStdString()&&clips[0]["source_offset_seconds"]==0&&clips[0]["start_samples"]==24000,"audio summaries keep source time separate from session position");
    check(c.query()==preserved&&Commands::mediaHash(source)==mediaHash,"all read-only detail pages preserve actual Edit, revision, history and original media");
    auto invalid=[&](Json args,const char* reason){auto r=tool(session,"query_objects",std::move(args));check(r["result"]["isError"]&&result(r)["status"]=="failed",reason);};
    auto base=arguments(snapshot,"tracks");for(const Json& bad:{Json(-1),Json(1.5),Json("1"),Json(uint64_t(-1))}){auto args=base;args["offset"]=bad;invalid(args,"invalid page offsets fail as tools without inventing successful data");}
    for(int limit:{0,65}){auto args=base;args["limit"]=limit;invalid(args,"page limits outside 1..64 are rejected");}
    auto last=base;last["offset"]=snapshot["counts"]["tracks"];auto end=result(tool(session,"query_objects",last))["result"];
    check(end["items"].empty()&&end["next_offset"].is_null(),"exact-end offset returns a complete empty page");last["offset"]=snapshot["counts"]["tracks"].get<int>()+1;invalid(last,"offset beyond collection fails without a phantom page");
    auto injected=base;injected["actor"]="human";invalid(injected,"query parameters cannot impersonate a human");
    auto badTarget=base;badTarget["target"]=audio;invalid(badTarget,"global collections reject a misleading target");
    invalid(arguments(snapshot,"clips","missing"),"missing track fails explicitly");invalid(arguments(snapshot,"parameters","warmth"),"invented plugin parameter owner fails explicitly");
    invalid(arguments(snapshot,"midi_notes",clips[0]["id"]),"audio clips cannot masquerade as MIDI");
    auto missingLane=curveArgs;missingLane["parameter"]="air";invalid(missingLane,"invented automation lane is not returned as a successful empty curve");
    auto old=snapshot;commit(c,Json::array({operation("track.gain",{{"track",audio},{"db",-3}})}));invalid(base,"human editing between pages rejects the old revision");c.undo();invalid(base,"Undo does not make old pages current again");snapshot=summary(session);
    auto parameter=native["tracks"][0]["plugins"][0]["parameters"][0]["id"];
    c.parameterControl("parameter.gesture.begin",{{"plugin",plugin},{"parameter",parameter}});
    check(!summary(session)["object_pages_available"].get<bool>(),"summary reports paging unavailable during a real human parameter gesture");
    invalid(arguments(summary(session),"tracks"),"active gesture cannot produce a mixed-state page");
    c.parameterControl("parameter.gesture.end",{{"plugin",plugin},{"parameter",parameter}});
    check(summary(session)["object_pages_available"],"paging becomes available after the actual gesture ends");
    auto threadArgs=arguments(summary(session),"tracks");bool wrongThread=false;std::thread wrong([&]{try{c.queryObjects(threadArgs);}catch(const std::exception&){wrongThread=true;}});wrong.join();
    check(wrongThread,"mutable SDK owner rejects object queries from a worker thread");
    auto saved=scratch.folder.getChildFile("session.tracktionedit");snapshot=summary(session);c.save(saved);c.open(saved);
    fails([&]{c.queryObjects(arguments(snapshot,"tracks"));},"old session token cannot follow a reopened Edit");
    check(tool(session,"query_session_summary")["result"]["isError"],"stale locally granted client is rejected after session replacement");
    auto renewed=queue.connect("agent:query-renewed",readOnly);McpSession resumed(renewed,Commands::registry());initialize(resumed);
    auto reopenedNotes=all(resumed,arguments(summary(resumed),"midi_notes",clip));bool notesPreserved=reopenedNotes.size()==expectedNotes.size();
    for(size_t n=0;n<reopenedNotes.size()&&n<expectedNotes.size();++n){auto actual=reopenedNotes[n],expected=expectedNotes[n];
        for(const char* key:{"source_beat","length_beats","start_beat"}){notesPreserved&=std::abs(actual[key].get<double>()-expected[key].get<double>())<1e-10;actual.erase(key);expected.erase(key);}notesPreserved&=actual==expected;}
    check(notesPreserved,"new grant preserves exact MIDI IDs/samples and source beats within XML precision after reopen");
    check(Commands::mediaHash(source)==mediaHash,"save/reopen and queries retain original source bytes");
    {
        // Fault fixture uses a real saved Edit with oversized imported metadata.
        // It does not modify a live Edit outside L1 or substitute a fake query.
        Commands large(false,std::make_unique<Storage>(scratch.folder.getChildFile("byte-prefs")));
        Json ops=Json::array();for(int n=0;n<6;++n)ops.push_back(operation("track.create",{{"name","Metadata "+std::to_string(n)},{"ref","$t"+std::to_string(n)}}));commit(large,std::move(ops));
        auto savedLarge=scratch.folder.getChildFile("metadata.tracktionedit");large.save(savedLarge);
        auto xml=juce::XmlDocument::parse(savedLarge);check(xml!=nullptr,"byte-budget fixture is an actual readable native Edit document");
        int found=0;forEachXmlChildElement(*xml,child)if(child->hasTagName("TRACK")){child->setAttribute("name",juce::String(std::string(70000,'x')));++found;}
        check(found==6&&xml->writeTo(savedLarge),"oversized imported metadata is stored only in the generated test document");large.open(savedLarge);
        auto args=arguments(large.querySummary(),"tracks");args["limit"]=64;auto first=large.queryObjects(args);
        check(first["byte_limited"]&&first["items"].size()==3&&first["next_offset"]==3&&first["items_bytes"]==first["items"].dump().size(),"byte-limited page returns a truthful continuation without truncating an object");
        args["offset"]=first["next_offset"];auto last=large.queryObjects(args);bool exact=last["items"].size()==3&&last["next_offset"].is_null();
        for(const auto& item:first["items"])exact&=item["name"].get<std::string>().size()==70000;for(const auto& item:last["items"])exact&=item["name"].get<std::string>().size()==70000;
        check(exact,"byte paging enumerates all imported metadata exactly with no hidden truncation");
        forEachXmlChildElement(*xml,child)if(child->hasTagName("TRACK")){child->setAttribute("name",juce::String(std::string(Commands::maximumQueryPageBytes+1,'x')));break;}
        check(xml->writeTo(savedLarge),"single oversized object fault fixture written");large.open(savedLarge);
        fails([&]{large.queryObjects(arguments(large.querySummary(),"tracks"));},"single oversized object fails explicitly instead of returning a fake empty page");
    }
    Json report{{"result","passed"},{"checks",checks},{"workloads",workloads},{"fixture_setup_ms",setupMs},
        {"maximum_mcp_call_ms",maximumCallMs},{"scope","real Tracktion Edit and registered MCP queries; generated known MIDI/audio; not desktop audition, a model result or engine realtime qualification"}};
    if(argc>1){std::ofstream file(argv[1]);file<<report.dump(2);}std::cout<<report.dump(2)<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
