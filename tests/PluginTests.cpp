// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Plugins.h"
#include "nativedaw/NativePluginBrowser.h"
#include <iostream>
#include <thread>
#include <fstream>
using namespace ndaw;
static void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F> static void fails(const std::string& code,F f){try{f();}catch(const Error& e){check(e.code==code,"Unexpected error code");return;}throw std::runtime_error("Expected failure was absent");}
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI gui;
    if(argc!=2)return 2;
    const fs::path worker=fs::absolute(argv[1]),root=fs::temp_directory_path()/("ndaw-plugin-tests-"+uuid());
    fs::create_directories(root);Json checks=Json::array();int failures=0;PluginLimits budget;budget.timeoutMs=300;budget.pollMs=5;
    auto run=[&](const char* id,auto f){try{auto receipt=f();checks.push_back({{"id",id},{"passed",true},{"receipt",receipt}});}
        catch(const std::exception& e){++failures;checks.push_back({{"id",id},{"passed",false},{"error",e.what()}});}};
    auto process=[&](const std::string& mode,std::atomic<bool>* cancel=nullptr,const std::function<bool()>& yield=std::function<bool()>{}){
        const auto job=root/uuid();fs::create_directories(job);const auto response=job/"response.json";
        atomicWrite(job/"request.json",Json{{"protocol",pluginProtocol},{"job_id",job.filename().string()},{"action","scan"},
            {"test_mode",mode},{"limits",budget.facts()},{"response_path",response.string()}}.dump(),false);
        return supervisePluginWorker(worker,job/"request.json",response,budget,cancel,yield);};
    run("PLUGIN-PROC-00",[&]{const auto old=budget.timeoutMs;budget.timeoutMs=PluginLimits{}.timeoutMs;const auto p=process("normal");budget.timeoutMs=old;
        check(p.status=="exited" && p.exitCode==0 && p.reaped,"Cold worker startup failed under production scan deadline");
        auto result=p.facts();result["scope"]="test-only freshly linked worker cold startup under unchanged production 10 s deadline; subsequent 300 ms fault tests are warm";return result;});
    run("PLUGIN-PROC-01",[&]{Json results=Json::array();for(const auto& mode:{"normal","nonzero_after_response","crash","crash_after_response"}){
        const auto p=process(mode);check(p.reaped,"Worker not reaped");
        const auto expected=(std::string(mode)=="normal"?"exited":std::string(mode)=="nonzero_after_response"?"abnormal_exit":
#ifdef _WIN32
            "abnormal_exit"
#else
            "crashed"
#endif
            );if(p.status!=expected)throw std::runtime_error(std::string(mode)+" expected "+expected+", actual "+p.facts().dump());
        if(std::string(mode)=="nonzero_after_response")check(p.exitCode==7,"Nonzero exit discarded");
        results.push_back(p.facts());}return results;});
    run("PLUGIN-PROC-02",[&]{const auto timeout=process("hang");check(timeout.status=="timeout" && timeout.reaped && timeout.wallMs<=1000,"Deadline/reaping budget failed");
        std::atomic<bool> cancel{false};std::jthread trigger([&]{std::this_thread::sleep_for(std::chrono::milliseconds(75));cancel.store(true);});
        const auto cancelled=process("hang",&cancel);trigger.join();check(cancelled.status=="cancelled" && cancelled.reaped && cancelled.wallMs<=750,"Cancel budget failed");
        const auto yielded=process("hang",nullptr,[]{return true;});check(yielded.status=="cancelled" && yielded.reaped,"Audio-priority yield failed");
        return Json::array({timeout.facts(),cancelled.facts(),yielded.facts()});});
    run("PLUGIN-PROC-03",[&]{const auto old=budget.responseBytes;budget.responseBytes=1024;const auto p=process("oversized");budget.responseBytes=old;
        check(p.status=="response_limit" && p.reaped,"Output limit failed");return p.facts();});
#if JUCE_MAC
    run("PLUGIN-PROC-05",[&]{const auto old=budget.rssBytes;budget.rssBytes=32*1024*1024;const auto p=process("memory");budget.rssBytes=old;
        check(p.status=="memory_limit" && p.reaped && p.peakRss>32*1024*1024,"Observed RSS limit failed");return p.facts();});
#endif
    run("PLUGIN-PROC-04",[&]{Json results=Json::array();for(const auto& mode:{"malformed","stale","protocol","crash_after_response","nonzero_after_response","failed"}){
        const auto dir=root/uuid(),candidate=root/(uuid()+".vst3");atomicWrite(candidate,Json{{"mode",mode}}.dump(),false);
        PluginCatalog catalog(dir,worker,budget);auto result=catalog.scan("VST3",candidate.string());
        check(result.at("status")=="failed" && result.at("entry").at("blacklisted").get<bool>(),"Invalid worker success accepted");
        if(std::string(mode)=="crash_after_response")check(result.at("result").at("process").at("status")!="exited","Crash lost after success JSON");
        results.push_back({{"mode",mode},{"code",result.at("result").at("code")}});}return results;});
    run("PLUGIN-SCAN-01",[&]{const auto dir=root/uuid(),candidate=root/(uuid()+".vst3");atomicWrite(candidate,"{\"mode\":\"normal\"}",false);Json lastSuccess;
        {PluginCatalog catalog(dir,worker,budget);const auto valid=catalog.scan("VST3",candidate.string());check(valid.at("status")=="verified","Fixture transaction failed");
            lastSuccess=valid.at("entry").at("last_success");check(catalog.scan("VST3",candidate.string()).at("status")=="cached_verified_scan","Unchanged file not cached");
            atomicWrite(candidate,"{\"mode\":\"crash\"}",true);const auto failure=catalog.scan("VST3",candidate.string(),true);check(failure.at("status")=="failed","Failed rescan accepted");
            check(failure.at("entry").at("last_success")==lastSuccess && failure.at("entry").at("plugins").size()==1,"Prior success erased");
            check(catalog.scan("VST3",candidate.string()).at("status")=="blocked","Blacklist bypassed");}
        {PluginCatalog catalog(dir,worker,budget);check(catalog.scan("VST3",candidate.string()).at("status")=="blocked","Blacklist not persistent");
            atomicWrite(candidate,"{\"mode\":\"normal\"}",true);check(catalog.scan("VST3",candidate.string(),true).at("status")=="verified","Explicit rescan did not recover");}
        return Json{{"fixture_transactions_only",true},{"last_success_retained",true}};});
    run("PLUGIN-SCAN-02",[&]{const auto dir=root/uuid(),candidate=root/(uuid()+".vst3");atomicWrite(candidate,"{\"mode\":\"normal\"}",false);
        PluginCatalog catalog(dir,worker,budget);catalog.scan("VST3",candidate.string());const auto before=catalog.query();
        fails("plugin_catalog_owned",[&]{PluginCatalog other(dir,worker,budget);});
        fs::rename(dir/"catalog.json",dir/"before.json");fs::create_directory(dir/"catalog.json");
        fails("file_conflict",[&]{catalog.scan("VST3",candidate.string(),true);});check(catalog.query()==before,"Failed save leaked uncommitted state");
        fs::remove(dir/"catalog.json");fs::rename(dir/"before.json",dir/"catalog.json");
        check(readPluginCatalog(dir)==before,"Persisted inventory changed");return Json{{"save_failure_rollback",true},{"exclusive_owner",true}};});
    run("PLUGIN-SCAN-03",[&]{const auto dir=root/uuid();fs::create_directories(dir);auto body=Json{{"schema_version",1},{"revision",4},{"entries",{{"fixture",{{"status","scanning"},{"last_success",{{"retained",true}}}}}}}};
        atomicWrite(dir/"catalog.json",Json{{"body",body},{"checksum",digest(body.dump())}}.dump(),false);
        {PluginCatalog catalog(dir,worker,budget);const auto e=catalog.query().at("entries").at("fixture");check(e.at("status")=="interrupted" && e.at("blacklisted").get<bool>() && e.at("last_success").at("retained").get<bool>(),"Interrupted recovery lost prior state");}
        const auto original=sha256(dir/"catalog.json");auto envelope=readJson(dir/"catalog.json");envelope["checksum"]="bad";atomicWrite(dir/"catalog.json",envelope.dump(),true);const auto corrupted=sha256(dir/"catalog.json");
        fails("plugin_catalog_corrupt",[&]{PluginCatalog catalog(dir,worker,budget);});check(sha256(dir/"catalog.json")==corrupted && original!=corrupted,"Corrupt inventory was overwritten");
        return Json{{"interrupted_retained",true},{"corrupt_original_preserved",true}};});
    run("PLUGIN-ADMISSION-01",[&]{PluginCatalog catalog(root/uuid(),worker,budget);const auto input=root/"original.wav",output=root/"existing.wav";atomicWrite(input,"source",false);atomicWrite(output,"retained",false);
        const auto hash=sha256(output);fails("export_exists",[&]{catalog.processFile("missing",input,output);});check(sha256(output)==hash,"Existing output overwritten");
        fails("plugin_unavailable",[&]{catalog.processFile("invented-warmth",input,root/"new.wav");});
        fails("plugin_parameters",[&]{catalog.processFile("missing",input,root/"new.wav",Json::object());});
        fails("plugin_format",[&]{catalog.scan("AAX",input.string());});fails("plugin_missing",[&]{catalog.scan("VST3",(root/"absent.vst3").string());});
        return Json{{"unknown_id_rejected",true},{"original_and_output_preserved",true}};});
#if JUCE_MAC
    run("GUI-PLUGIN-01",[&]{std::atomic<bool> priority{false};NativePluginBrowser browser([&]{return priority.load();},root/"native-widget-catalog");
        juce::TextButton *discover=nullptr,*scan=nullptr,*technical=nullptr;juce::ComboBox* candidates=nullptr;juce::TextEditor* details=nullptr;
        for(auto* child:browser.getChildren()){
            if(auto* button=dynamic_cast<juce::TextButton*>(child)){if(button->getButtonText()=="Discover")discover=button;if(button->getButtonText()=="Scan selected")scan=button;if(button->getButtonText()=="Technical details")technical=button;}
            if(auto* combo=dynamic_cast<juce::ComboBox*>(child))candidates=combo;if(auto* editor=dynamic_cast<juce::TextEditor*>(child))details=editor;}
        check(discover && scan && candidates && details && technical,"Native plugin controls missing");
        auto finish=[&]{const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(12);while(!discover->isEnabled() && std::chrono::steady_clock::now()<until)
                juce::MessageManager::getInstance()->runDispatchLoopUntil(5);check(discover->isEnabled(),"Native plugin callback did not finish");};
        discover->onClick();finish();int selected=0;for(int i=0;i<candidates->getNumItems();++i)if(candidates->getItemText(i).contains("AudioUnit:Effects/aufx,lpas,appl"))selected=candidates->getItemId(i);
        check(selected>0,"Actual AU was not discovered by native callback");candidates->setSelectedId(selected,juce::dontSendNotification);scan->onClick();finish();
        const auto result=browser.lastReceipt();check(result.at("status")=="verified" && result.at("entry").at("plugins")[0].at("name")=="AULowpass" && details->getText().contains("AULowpass"),"Actual native scan failed");
        technical->setToggleState(true,juce::dontSendNotification);technical->onClick();const auto diagnostic=Json::parse(details->getText().toStdString());
        check(diagnostic.at("status")=="verified" && diagnostic.at("entry").at("plugins")[0].at("parameter_count")==2,"Technical view lost actual parameter receipt");
        technical->setToggleState(false,juce::dontSendNotification);technical->onClick();check(details->getText().contains("Cutoff Frequency"),"Readable view lost actual parameter display");
        priority.store(true);discover->onClick();check(details->getText().contains("Stop playback/recording"),"Audio priority did not prevent scan");
        priority.store(false);discover->onClick();priority.store(true);finish();const auto cancelled=browser.lastReceipt();
        check(cancelled.at("status")=="failed" && cancelled.at("code")=="plugin_cancelled" && cancelled.at("process").at("reaped").get<bool>() && details->getText().contains("cancelled"),"Native worker did not yield/reap");
        return Json{{"actual_plugin","Apple AULowpass"},{"actual_sdk_child",true},{"widget_callbacks_executed",true},{"manual_mouse_workflow",false},
            {"audio_priority_prevents_and_cancels",true},{"scope","native component callbacks/message dispatch only; no desktop/install/editor/plugin audio acceptance"}};});
#endif
    const auto report=Json{{"status",failures?"failed":"passed"},{"checks",checks},{"failures",failures},{"limits",budget.facts()},{"test_directory",root.string()},
        {"test_worker",true},{"real_plugin_audio_acceptance",false},{"scope","lifecycle/protocol/inventory tests use explicitly synthetic metadata; macOS widget check separately uses actual AU SDK inspection; no plugin audio/model/manual desktop acceptance"}};
    std::cout<<report.dump(2)<<'\n';if(!failures)fs::remove_all(root);return failures?1:0;
}
