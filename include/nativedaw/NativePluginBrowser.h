// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Plugins.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <thread>

namespace ndaw {
class NativePluginBrowser final:public juce::Component {
public:
    explicit NativePluginBrowser(std::function<bool()> audioPriority,fs::path directory=defaultPluginCatalogDirectory())
        :audioPriority_(std::move(audioPriority)),directory_(std::move(directory)) {
        for(auto* b:{&discover_,&scan_,&rescan_,&cancel_,&technical_})addAndMakeVisible(*b);
        addAndMakeVisible(candidates_);addAndMakeVisible(details_);details_.setMultiLine(true);details_.setReadOnly(true);
        details_.setText("Discover installed AU/VST3 candidates, then scan a selected plugin. Successful scan means instance/parameter/state inspection. Realtime project inserts are not available in this increment.");
        discover_.onClick=[this]{start([](PluginCatalog& c,std::atomic<bool>& cancel,const std::function<bool()>& priority){return c.discover(&cancel,priority);});};
        scan_.onClick=[this]{scan(false);};rescan_.onClick=[this]{scan(true);};cancel_.onClick=[this]{cancel();};
        technical_.setClickingTogglesState(true);technical_.onClick=[this]{showReceipt();};
    }
    ~NativePluginBrowser() override {cancel();if(worker_.joinable())worker_.join();}
    void cancel(){cancelled_.store(true);}
    const Json& lastReceipt() const{return lastReceipt_;} // message-thread query; never a plugin/audio pointer
    void resized() override {discover_.setBounds(12,12,110,28);scan_.setBounds(132,12,110,28);rescan_.setBounds(252,12,110,28);cancel_.setBounds(372,12,110,28);
        technical_.setBounds(492,12,170,28);candidates_.setBounds(12,52,getWidth()-24,30);details_.setBounds(12,94,getWidth()-24,getHeight()-106);}
private:
    void showReceipt() {
        if(lastReceipt_.is_null())return;
        if(!technical_.getToggleState()){
            juce::String text;const auto status=lastReceipt_.value("status",std::string{});
            if(status=="cancelled" || lastReceipt_.value("code",std::string{})=="plugin_cancelled")text="Inspection cancelled. Retained inventory remains available.\n";
            else if(status=="verified" || status=="cached_verified_scan")text="Scan verified: actual instance, parameters and captured state.\n";
            else if(status=="succeeded" && lastReceipt_.contains("response") && lastReceipt_["response"].contains("candidates"))
                text="Discovery complete: "+juce::String(static_cast<int>(lastReceipt_["response"]["candidates"].size()))+" installed candidates. Select one and scan.\n";
            else if(status=="running")text="Inspecting installed plugins...\n";
            else text="Scan "+juce::String(status)+": "+juce::String(lastReceipt_.value("error",lastReceipt_.value("code",std::string{"See retained diagnostic receipt"})))+"\n";
            if(lastReceipt_.contains("entry")){
                const auto& e=lastReceipt_["entry"];text+="\nCandidate: "+juce::String(e.value("candidate",std::string{}))+"\n";
                if(e.value("blacklisted",false))text+="Blacklisted after failure. Rescan explicitly retries; prior inspection/state remains retained.\n";
                if(e.contains("plugins"))for(const auto& p:e["plugins"]){
                    text+="\n"+juce::String(p.at("name").get<std::string>())+" | "+juce::String(p.at("format").get<std::string>())+" | "+juce::String(p.at("version").get<std::string>())+"\n";
                    text+=juce::String(p.at("manufacturer").get<std::string>())+" | "+(p.value("instrument",false)?"Instrument":"Audio effect")+"\n";
                    text+="Reported latency: "+juce::String(p.value("reported_latency_frames",0))+" samples\n";
                    text+="Parameters: "+juce::String(static_cast<int>(p.at("parameters").size()))+" (up to 16 shown)\n";int count=0;
                    for(const auto& param:p.at("parameters")){if(count++==16)break;
                        text+="  "+juce::String(param.at("name").get<std::string>())+": "+juce::String(param.at("display").get<std::string>())+" "+juce::String(param.at("unit_label").get<std::string>())+"\n";}
                }
            }
            text+="\nRealtime project insertion and plugin editor opening are still unavailable in this development increment.\n";
            details_.setText(text);return;
        }
        auto shown=lastReceipt_;
        if(shown.contains("entry"))for(const char* key:{"last_success","last_attempt"})shown["entry"].erase(key);
        if(shown.contains("entry") && shown["entry"].contains("plugins"))for(auto& p:shown["entry"]["plugins"]){p["parameter_count"]=p["parameters"].size();
            while(p["parameters"].size()>16)p["parameters"].erase(p["parameters"].end()-1);p["display_scope"]="first 16 parameters shown; complete inventory retained";}
        shown.erase("result");details_.setText(juce::String(shown.dump(2)));
    }
    using Work=std::function<Json(PluginCatalog&,std::atomic<bool>&,const std::function<bool()>&)>;
    void start(Work work) {
        if(busy_)return;if(audioPriority_ && audioPriority_()){details_.setText("Stop playback/recording before plugin discovery or scanning.");return;}
        if(worker_.joinable())worker_.join();busy_=true;cancelled_.store(false);discover_.setEnabled(false);scan_.setEnabled(false);rescan_.setEnabled(false);
        lastReceipt_={{"status","running"}};showReceipt();
        auto safe=juce::Component::SafePointer<NativePluginBrowser>(this);const auto priority=audioPriority_;const auto directory=directory_;
        try{worker_=std::thread([this,safe,priority,directory,work=std::move(work)]{
            Json result;try{PluginCatalog catalog(directory);result=work(catalog,cancelled_,priority);}catch(const std::exception& e){result={{"status","failed"},{"error",e.what()}};}
            juce::MessageManager::callAsync([safe,result=std::move(result)]{if(!safe)return;safe->busy_=false;safe->discover_.setEnabled(true);safe->scan_.setEnabled(true);safe->rescan_.setEnabled(true);
                if(result.contains("response") && result["response"].contains("candidates")){
                    safe->candidateFacts_=result["response"]["candidates"];safe->candidates_.clear();int id=1;
                    for(const auto& c:safe->candidateFacts_)safe->candidates_.addItem(juce::String(c.at("format").get<std::string>()+" | "+c.at("candidate").get<std::string>()),id++);
                    if(id>1)safe->candidates_.setSelectedId(1);}
                safe->lastReceipt_=result;safe->showReceipt();
            });
        });}catch(const std::exception& e){busy_=false;discover_.setEnabled(true);scan_.setEnabled(true);rescan_.setEnabled(true);
            lastReceipt_={{"status","failed"},{"code","plugin_worker_start"},{"error",e.what()}};showReceipt();}
    }
    void scan(bool force){const auto index=candidates_.getSelectedId()-1;if(index<0 || static_cast<std::size_t>(index)>=candidateFacts_.size()){details_.setText("Discover and select an actual plugin candidate first.");return;}
        const auto c=candidateFacts_[index];start([c,force](PluginCatalog& catalog,std::atomic<bool>& cancel,const std::function<bool()>& priority){return catalog.scan(c.at("format"),c.at("candidate"),force,&cancel,priority);});}
    std::function<bool()> audioPriority_;fs::path directory_;std::thread worker_;std::atomic<bool> cancelled_{false};bool busy_=false;
    Json candidateFacts_=Json::array(),lastReceipt_;juce::TextButton discover_{"Discover"},scan_{"Scan selected"},rescan_{"Rescan"},cancel_{"Cancel"},technical_{"Technical details"};
    juce::ComboBox candidates_;juce::TextEditor details_;
};
class NativePluginWindow final:public juce::DocumentWindow {
public:
    explicit NativePluginWindow(std::function<bool()> priority):DocumentWindow("NativeDAW | Plugin inventory",juce::Colour(0xff10151e),closeButton){
        setUsingNativeTitleBar(true);setContentOwned(new NativePluginBrowser(std::move(priority)),true);setResizable(true,true);centreWithSize(920,650);}
    void closeButtonPressed() override {static_cast<NativePluginBrowser*>(getContentComponent())->cancel();setVisible(false);}
};
}
