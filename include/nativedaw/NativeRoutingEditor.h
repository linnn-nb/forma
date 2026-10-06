// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Routing.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <functional>

namespace ndaw {
// Native controls submit exactly the same stable-ID commands as CLI/AI.
// No editor widget owns or writes audio-graph state.
class NativeRoutingEditor final : public juce::Component {
public:
    std::function<void(std::string,std::string)> requestStateCapture;
    std::function<void(std::string,std::string)> requestNativeEditor;
    explicit NativeRoutingEditor(std::function<void(Json)> submit,fs::path catalog=defaultPluginCatalogDirectory()):submit_(std::move(submit)),catalog_(std::move(catalog)) {
        setSize(760,800);
        for(auto* c:{&tracks_,&outputs_,&sends_,&sendTargets_,&processors_,&availablePlugins_,&pluginParameters_})addAndMakeVisible(*c);
        for(auto* b:{&addAux_,&addMaster_,&deleteTrack_,&addSend_,&removeSend_,&applySend_,&insert_,&remove_,&applyFx_,&lockFx_,&refreshPlugins_,&insertPlugin_,&applyPlugin_,&captureState_})addAndMakeVisible(*b);
        for(auto* b:{&pre_,&sendMute_})addAndMakeVisible(*b);
        for(auto* l:{&title_,&routeFacts_,&meters_,&sendLabel_,&fxLabel_,&boundary_,&pluginValue_})addAndMakeVisible(*l);
        title_.setText("Routing and processing",juce::dontSendNotification);
        sendLabel_.setText("Sends | independent pan; post insert",juce::dontSendNotification);
        fxLabel_.setText("Linked sample-peak limiter",juce::dontSendNotification);
        boundary_.setText("Stop before changing routes, taps or inserts. Plugin values can change during playback, and remain pinned during recording. Changed latency stops processing. AU/VST3 adds 1024 frames + SDK latency.\nStereo main I/O; normalized SDK values. Scan in Plugins, then refresh here. Low-latency monitoring is not qualified.",juce::dontSendNotification);
        auto slider=[this](juce::Slider& s,double lo,double hi,double step,const char* suffix){s.setRange(lo,hi,step);s.setTextValueSuffix(suffix);s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,120,26);addAndMakeVisible(s);};
        slider(sendGain_,-120,24,0.1," dB");slider(sendPan_,-1,1,0.01," pan");
        slider(lookahead_,0,8192,1," frames");slider(ceiling_,-60,0,0.1," dB ceiling");slider(release_,0.1,5000,0.1," ms release");
        slider(pluginNormalized_,0,1,0.0001," normalized");
        tracks_.onChange=[this]{const auto id=tracks_.getSelectedId();if(id<1)return;track_=trackIds_.at(static_cast<std::size_t>(id-1));send_.clear();processor_.clear();populate();};
        outputs_.onChange=[this]{if(!track_.empty() && outputs_.getSelectedId()>0)submit_(Json::array({{{"command","set_track_output"},{"track_id",track_},{"target_bus_id",busIds_.at(outputs_.getSelectedId()-1)}}}));};
        sends_.onChange=[this]{const auto id=sends_.getSelectedId();if(id<1)return;send_=sendIds_.at(id-1);populateSend();};
        processors_.onChange=[this]{const auto id=processors_.getSelectedId();if(id<1)return;processor_=processorIds_.at(id-1);populateFx();};
        addAux_.onClick=[this]{submit_(Json::array({{{"command","add_aux_track"},{"name","Aux "+std::to_string(snapshot_.at("buses").size())}}}));};
        addMaster_.onClick=[this]{submit_(Json::array({{{"command","add_master_track"},{"name","Main master"}}}));};
        deleteTrack_.onClick=[this]{submit_(Json::array({{{"command","delete_mix_track"},{"track_id",track_}}}));};
        addSend_.onClick=[this]{const auto i=sendTargets_.getSelectedId();if(i>0)submit_(Json::array({{{"command","add_send"},{"track_id",track_},{"target_bus_id",busIds_.at(i-1)},{"gain_db",-12}}}));};
        removeSend_.onClick=[this]{submit_(Json::array({{{"command","remove_send"},{"track_id",track_},{"send_id",send_}}}));};
        applySend_.onClick=[this]{const auto i=sendTargets_.getSelectedId();if(i>0)submit_(Json::array({{{"command","set_send"},{"track_id",track_},{"send_id",send_},{"target_bus_id",busIds_.at(i-1)},
            {"gain_db",sendGain_.getValue()},{"pan",sendPan_.getValue()},{"pre_fader",pre_.getToggleState()},{"muted",sendMute_.getToggleState()}}}));};
        insert_.onClick=[this]{submit_(Json::array({{{"command","insert_limiter"},{"track_id",track_}}}));};
        remove_.onClick=[this]{submit_(Json::array({{{"command","remove_processor"},{"track_id",track_},{"processor_id",processor_}}}));};
        applyFx_.onClick=[this]{submit_(Json::array({{{"command","set_limiter"},{"track_id",track_},{"processor_id",processor_},{"lookahead_frames",static_cast<int>(lookahead_.getValue())},
            {"ceiling_db",ceiling_.getValue()},{"release_ms",release_.getValue()}}}));};
        lockFx_.onClick=[this]{if(const auto* fx=selectedFx())submit_(Json::array({{{"command","set_processor_lock"},{"track_id",track_},{"processor_id",processor_},{"locked",!fx->at("locked").get<bool>()}}}));};
        refreshPlugins_.onClick=[this]{refreshCatalog();};
        availablePlugins_.onChange=[this]{insertPlugin_.setEnabled(selectedTrack() && !selectedTrack()->at("locked").get<bool>() && availablePlugins_.getSelectedId()>0);};
        insertPlugin_.onClick=[this]{const auto id=availablePlugins_.getSelectedId();if(id>0)submit_(Json::array({{{"command","insert_plugin"},{"track_id",track_},{"plugin_id",catalogIds_.at(id-1)}}}));};
        pluginParameters_.onChange=[this]{populateParameter();};
        applyPlugin_.onClick=[this]{const auto id=pluginParameters_.getSelectedId();if(id>0)submit_(Json::array({{{"command","set_plugin_parameter"},{"track_id",track_},{"processor_id",processor_},{"parameter_id",parameterIds_.at(id-1)},{"normalized",pluginNormalized_.getValue()}}}));};
        captureState_.onClick=[this]{if(requestStateCapture)requestStateCapture(track_,processor_);};
        nativeEditor_.onClick=[this]{if(requestNativeEditor)requestNativeEditor(track_,processor_);};addAndMakeVisible(nativeEditor_);
        refreshCatalog();
    }
    void update(Json snapshot,const std::string& selection={}) {
        snapshot_=std::move(snapshot);if(!selection.empty())track_=selection;
        tracks_.clear(juce::dontSendNotification);trackIds_.clear();
        for(const auto& t:snapshot_.at("tracks")){trackIds_.push_back(t.at("id"));tracks_.addItem(t.at("name").get<std::string>()+" | "+t.at("kind").get<std::string>(),static_cast<int>(trackIds_.size()));}
        if(!selectedTrack()){track_=trackIds_.empty()?std::string{}:trackIds_.front();send_.clear();processor_.clear();}
        tracks_.setSelectedId(indexOf(trackIds_,track_),juce::dontSendNotification);populate();
    }
    void measured(const Json& metrics) {
        juce::String text="No processed block for this track";
        for(const auto& m:metrics.at("routing").at("meters"))if(m.at("id")==track_ && m.at("processed_blocks").get<std::uint64_t>()>0) {
            auto db=[](double peak){return peak>0?juce::String(20*std::log10(peak),1)+" dBFS":"-inf dBFS";};
            text="Actual block sample peak | input "+db(m.at("input_sample_peak"))+" | output "+db(m.at("post_fader_sample_peak"))+" | revision "+juce::String(m.at("processed_revision").get<juce::int64>());
        }
        meters_.setText(text,juce::dontSendNotification);
        if(const auto* fx=selectedFx();fx && fx->at("kind")=="plugin" && pluginParameters_.getSelectedId()>0) {
            const auto id=parameterIds_.at(pluginParameters_.getSelectedId()-1);
            juce::String value="Desired value; awaiting plugin processing";
            for(const auto& instance:metrics.at("routing").at("plugins").at("instances"))if(instance.at("instance_id")==processor_ && instance.at("runtime_identity_token")==sessionPluginRuntimeToken(*fx)) {
                const auto fault=instance.at("fault").get<int>();const auto& control=instance.at("parameter_control");const auto token=sessionPluginAudioToken(*fx);
                if(fault)value="Plugin failed | fault "+juce::String(fault)+" | desired value retained";
                else if(control.at("sdk_snapshot_verified")==true && control.at("acknowledged_token")==token)
                    for(const auto& p:control.at("actual_values"))if(p.at("sdk_id")==id)value="Plugin readback: "+juce::String(p.at("actual").get<double>(),6)+" normalized | "+
                        (control.at("output_snapshot_verified")==true && control.at("returned_output_token")==token?"output verified":"output pending");
            }
            pluginValue_.setText(value,juce::dontSendNotification);
        }
    }
    void resized() override {
        title_.setBounds(16,10,720,28);tracks_.setBounds(16,48,355,28);outputs_.setBounds(390,48,354,28);
        addAux_.setBounds(16,85,135,28);addMaster_.setBounds(162,85,140,28);deleteTrack_.setBounds(312,85,170,28);
        routeFacts_.setBounds(16,121,728,45);meters_.setBounds(16,170,728,30);
        sendLabel_.setBounds(16,208,728,25);sends_.setBounds(16,240,300,28);sendTargets_.setBounds(330,240,414,28);
        sendGain_.setBounds(16,280,345,28);sendPan_.setBounds(390,280,354,28);pre_.setBounds(16,317,185,28);sendMute_.setBounds(212,317,185,28);
        addSend_.setBounds(16,352,135,28);removeSend_.setBounds(163,352,135,28);applySend_.setBounds(312,352,170,28);
        fxLabel_.setBounds(16,395,325,25);processors_.setBounds(330,394,414,28);
        lookahead_.setBounds(16,432,355,28);ceiling_.setBounds(390,432,354,28);release_.setBounds(16,469,355,28);
        insert_.setBounds(16,508,135,28);remove_.setBounds(163,508,135,28);applyFx_.setBounds(312,508,135,28);lockFx_.setBounds(461,508,180,28);
        refreshPlugins_.setBounds(16,550,150,28);availablePlugins_.setBounds(178,550,410,28);insertPlugin_.setBounds(602,550,142,28);
        pluginParameters_.setBounds(16,590,355,28);pluginNormalized_.setBounds(390,590,354,28);
        applyPlugin_.setBounds(16,630,190,28);pluginValue_.setBounds(216,627,528,42);captureState_.setBounds(16,670,205,28);nativeEditor_.setBounds(240,670,245,28);boundary_.setBounds(16,707,728,80);
    }
private:
    static int indexOf(const std::vector<std::string>& ids,const std::string& id){const auto it=std::find(ids.begin(),ids.end(),id);return it==ids.end()?0:static_cast<int>(it-ids.begin()+1);}
    const Json* selectedTrack() const {if(snapshot_.is_null())return nullptr;for(const auto& t:snapshot_.at("tracks"))if(t.at("id")==track_)return &t;return nullptr;}
    const Json* selectedFx() const {if(const auto* t=selectedTrack())for(const auto& fx:t->at("processors"))if(fx.at("id")==processor_)return &fx;return nullptr;}
    void populate() {
        const auto* t=selectedTrack();const bool writable=t && !t->at("locked").get<bool>(),output=writable && t->at("kind")!="master";
        outputs_.clear(juce::dontSendNotification);sendTargets_.clear(juce::dontSendNotification);busIds_.clear();
        for(const auto& b:snapshot_.at("buses")){busIds_.push_back(b.at("id"));outputs_.addItem(juce::String(b.at("name").get<std::string>()),static_cast<int>(busIds_.size()));sendTargets_.addItem(juce::String(b.at("name").get<std::string>()),static_cast<int>(busIds_.size()));}
        outputs_.setSelectedId(t && !t->at("output").is_null()?indexOf(busIds_,t->at("output").at("target_bus_id")):0,juce::dontSendNotification);outputs_.setTextWhenNothingSelected("Main master output stage");outputs_.setEnabled(output);
        addMaster_.setEnabled(std::all_of(snapshot_.at("buses").begin(),snapshot_.at("buses").end(),[](const Json& b){return b.at("role")!="main" || b.at("owner_track_id").is_null();}));deleteTrack_.setEnabled(writable && t->at("kind")!="audio");
        sends_.clear(juce::dontSendNotification);sendIds_.clear();processors_.clear(juce::dontSendNotification);processorIds_.clear();
        if(t){for(const auto& r:t->at("sends")){sendIds_.push_back(r.at("id"));sends_.addItem("Send "+std::to_string(sendIds_.size())+" | "+r.at("id").get<std::string>().substr(0,8),static_cast<int>(sendIds_.size()));}
            for(const auto& fx:t->at("processors")){processorIds_.push_back(fx.at("id"));processors_.addItem((fx.at("kind")=="plugin"?fx.at("description").at("name").get<std::string>():"Limiter")+" | "+fx.at("id").get<std::string>().substr(0,8),static_cast<int>(processorIds_.size()));}}
        if(!indexOf(sendIds_,send_))send_=sendIds_.empty()?std::string{}:sendIds_.front();
        if(!indexOf(processorIds_,processor_))processor_=processorIds_.empty()?std::string{}:processorIds_.front();
        sends_.setSelectedId(indexOf(sendIds_,send_),juce::dontSendNotification);processors_.setSelectedId(indexOf(processorIds_,processor_),juce::dontSendNotification);
        addSend_.setEnabled(output);sendTargets_.setEnabled(output);insert_.setEnabled(writable);populateSend();populateFx();
        insertPlugin_.setEnabled(writable && availablePlugins_.getSelectedId()>0);
        auto facts=routingFacts(snapshot_);juce::String text="System processing delay: "+juce::String(facts.at("algorithmic_latency_frames").get<juce::int64>())+" frames";
        for(const auto& n:facts.at("nodes"))if(n.at("id")==track_)text+=" | input "+juce::String(n.at("input_latency_frames").get<juce::int64>())+" | output "+juce::String(n.at("output_latency_frames").get<juce::int64>())+" frames";
        routeFacts_.setText(text+"\n"+(t?t->at("id").get<std::string>():"Select a track"),juce::dontSendNotification);
    }
    void populateSend() {
        const auto* t=selectedTrack();const Json* send=nullptr;if(t)for(const auto& r:t->at("sends"))if(r.at("id")==send_)send=&r;
        const bool enabled=send && !t->at("locked").get<bool>();
        for(juce::Component* c:std::vector<juce::Component*>{&applySend_,&removeSend_,&sendGain_,&sendPan_,&pre_,&sendMute_})c->setEnabled(enabled);
        if(send){sendTargets_.setSelectedId(indexOf(busIds_,send->at("target_bus_id")),juce::dontSendNotification);sendGain_.setValue(send->at("gain_db"),juce::dontSendNotification);sendPan_.setValue(send->at("pan"),juce::dontSendNotification);pre_.setToggleState(send->at("pre_fader"),juce::dontSendNotification);sendMute_.setToggleState(send->at("muted"),juce::dontSendNotification);}
        else sendTargets_.setSelectedId(busIds_.size()>1?2:busIds_.empty()?0:1,juce::dontSendNotification);
    }
    void populateFx() {
        const auto* fx=selectedFx();const auto* t=selectedTrack();const bool enabled=fx && !t->at("locked").get<bool>() && !fx->at("locked").get<bool>();
        const bool plugin=fx && fx->at("kind")=="plugin";
        captureState_.setEnabled(enabled && plugin && static_cast<bool>(requestStateCapture));
        nativeEditor_.setEnabled(enabled && plugin && static_cast<bool>(requestNativeEditor) && fx->at("description").value("has_editor",false));
        fxLabel_.setText(plugin?"AU/VST3 effect | SDK parameters below":"Linked sample-peak limiter",juce::dontSendNotification);
        for(juce::Component* c:std::vector<juce::Component*>{&applyFx_,&lookahead_,&ceiling_,&release_})c->setEnabled(enabled && !plugin);remove_.setEnabled(enabled);
        lockFx_.setEnabled(fx && !t->at("locked").get<bool>());lockFx_.setButtonText(fx && fx->at("locked").get<bool>()?"Unlock processor":"Lock processor");
        if(fx && !plugin){lookahead_.setValue(fx->at("lookahead_frames"),juce::dontSendNotification);ceiling_.setValue(fx->at("ceiling_db"),juce::dontSendNotification);release_.setValue(fx->at("release_ms"),juce::dontSendNotification);}
        pluginParameters_.clear(juce::dontSendNotification);parameterIds_.clear();if(plugin)for(const auto& p:fx->at("parameters"))if(!p.at("sdk_id").is_null()){
            parameterIds_.push_back(p.at("sdk_id"));pluginParameters_.addItem(p.at("name").get<std::string>()+" | ID "+p.at("sdk_id").get<std::string>(),static_cast<int>(parameterIds_.size()));}
        pluginParameters_.setEnabled(enabled && plugin);pluginParameters_.setSelectedId(parameterIds_.empty()?0:1,juce::dontSendNotification);populateParameter();
    }
    void populateParameter() {
        const auto* fx=selectedFx();const auto* t=selectedTrack();const int index=pluginParameters_.getSelectedId();const bool enabled=fx && fx->at("kind")=="plugin" && t && !t->at("locked").get<bool>() && !fx->at("locked").get<bool>() && index>0;
        pluginNormalized_.setEnabled(enabled);applyPlugin_.setEnabled(enabled);pluginValue_.setText(enabled?"Desired value; awaiting plugin processing":"Select an enumerated plugin parameter",juce::dontSendNotification);
        if(enabled)for(const auto& p:fx->at("parameters"))if(p.at("sdk_id")==parameterIds_.at(index-1))pluginNormalized_.setValue(p.at("value"),juce::dontSendNotification);
    }
    void refreshCatalog() {
        availablePlugins_.clear(juce::dontSendNotification);catalogIds_.clear();
        try {const auto catalog=readPluginCatalog(catalog_);for(const auto& [key,e]:catalog.at("entries").items())if(e.at("status")=="verified" && !e.value("blacklisted",false))for(const auto& p:e.at("plugins"))if(p.at("instrument")==false){catalogIds_.push_back(p.at("id"));availablePlugins_.addItem(p.at("name").get<std::string>()+" | "+p.at("format").get<std::string>()+" "+p.at("version").get<std::string>(),static_cast<int>(catalogIds_.size()));}}
        catch(const std::exception& e){pluginValue_.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification);}
        availablePlugins_.setSelectedId(catalogIds_.empty()?0:1,juce::dontSendNotification);insertPlugin_.setEnabled(selectedTrack() && !selectedTrack()->at("locked").get<bool>() && !catalogIds_.empty());
    }
    std::function<void(Json)> submit_;fs::path catalog_;Json snapshot_;std::string track_,send_,processor_;
    std::vector<std::string> trackIds_,busIds_,sendIds_,processorIds_,catalogIds_,parameterIds_;
    juce::ComboBox tracks_,outputs_,sends_,sendTargets_,processors_,availablePlugins_,pluginParameters_;
    juce::TextButton addAux_{"Add Aux"},addMaster_{"Add main master"},deleteTrack_{"Delete mix track"},addSend_{"Add send"},removeSend_{"Remove send"},applySend_{"Apply send"},insert_{"Insert limiter"},remove_{"Remove processor"},applyFx_{"Apply parameters"},lockFx_{"Lock processor"};
    juce::ToggleButton pre_{"Pre fader"},sendMute_{"Mute send"};juce::Slider sendGain_,sendPan_,lookahead_,ceiling_,release_;
    juce::TextButton refreshPlugins_{"Refresh plugins"},insertPlugin_{"Insert AU/VST3"},applyPlugin_{"Apply plugin value"},captureState_{"Capture DSP state..."},nativeEditor_{"Open native editor preview..."};juce::Slider pluginNormalized_;
    juce::Label title_,routeFacts_,meters_,sendLabel_,fxLabel_,boundary_,pluginValue_;
};
class NativeRoutingWindow final : public juce::DocumentWindow {
public:
    explicit NativeRoutingWindow(std::function<void(Json)> submit):DocumentWindow("NativeDAW | Routing",juce::Colour(0xff171c24),closeButton) {
        setUsingNativeTitleBar(true);setContentOwned(new NativeRoutingEditor(std::move(submit)),true);setResizable(false,false);centreWithSize(760,800);
    }
    NativeRoutingEditor& editor(){return *static_cast<NativeRoutingEditor*>(getContentComponent());}
    std::function<void()> onClosed;
    void closeButtonPressed() override {setVisible(false);if(onClosed)onClosed();}
};
}
