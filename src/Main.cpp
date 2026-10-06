// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Audio.h"
#include "nativedaw/AI.h"
#include "nativedaw/Permissions.h"
#include "nativedaw/NativeAudioConfig.h"
#include "nativedaw/NativeRoutingEditor.h"
#include "nativedaw/NativePlaylistEditor.h"
#include "nativedaw/NativeGroupEditor.h"
#include "nativedaw/NativeRecordingSetup.h"
#include "nativedaw/NativePluginBrowser.h"
#include "nativedaw/PluginPreview.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include "nativedaw/NativeWorkstation.h"

using namespace ndaw::desktop;

using namespace ndaw;
class MainComponent final : public juce::Component,private juce::Timer,private juce::MenuBarModel {
public:
    explicit MainComponent(const fs::path& initial) : timeline_(engine_),ruler_(timeline_),headers_(true),mixer_(false),tracks_(false),clips_(true),menu_(this) {
        setLookAndFeel(&look_);
        setSize(1440,900); setWantsKeyboardFocus(true);
        viewport_.setViewedComponent(&timeline_,false); addAndMakeVisible(viewport_);
        headerViewport_.setViewedComponent(&headers_,false);headerViewport_.setScrollBarsShown(false,false);
        mixViewport_.setViewedComponent(&mixer_,false);mixViewport_.setScrollBarsShown(false,true);
        for(auto* c:std::vector<juce::Component*>{&menu_,&ruler_,&headerViewport_,&tracks_,&clips_,&groups_,&counter_,&selectionStart_,&selectionEnd_,&selectionLength_,&grid_,&timeFormat_,&workspaceTitle_,&selectionTitle_})addAndMakeVisible(*c);addChildComponent(mixViewport_);
        viewport_.changed=[this](juce::Rectangle<int> area){headerViewport_.setViewPosition(0,area.getY());ruler_.scroll=area.getX();ruler_.repaint();};
        headerViewport_.changed=[this](juce::Rectangle<int> area){if(viewport_.getViewPositionY()!=area.getY())viewport_.setViewPosition(viewport_.getViewPositionX(),area.getY());};
        addButton("New",[this]{newProject();}); addButton("Open",[this]{openProject();}); addButton("Save",[this]{save();});
        addButton("Import",[this]{import();}); addButton("Play",[this]{play();}); addButton("Stop",[this]{if(engine_.recordState()!=RecordState::Idle)record();else stopPlayback();});
        addButton("Add Audio",[this]{auto id=uuid();track_=id;clip_.clear();apply(Json::array({{{"command","add_audio_track"},{"name","Audio "+std::to_string(commands_->query().at("tracks").size()+1)},{"id",id}}}));});
        addButton("Record",[this]{record();}); addButton("Undo",[this]{editHistory(false);}); addButton("Redo",[this]{editHistory(true);});
        addButton("Record mode",[this]{showRecordSetup();});
        addButton("Split",[this]{split();}); addButton("Delete",[this]{removeClip();}); addButton("Export WAV",[this]{exportAudio();});
        addButton("Audio devices",[this]{configureAudio();}); addButton("AI panel",[this]{aiVisible_=!aiVisible_; resized();});
        addButton("Routing",[this]{showRouting(track_);});
        addButton("Plugins",[this]{if(!pluginWindow_)pluginWindow_=std::make_unique<NativePluginWindow>([this]{return engine_.state()==PlaybackState::Playing || engine_.state()==PlaybackState::Priming || engine_.recordState()==RecordState::Recording;});pluginWindow_->setVisible(true);pluginWindow_->toFront(true);});
        addAndMakeVisible(status_); status_.setColour(juce::Label::textColourId,juce::Colour(0xffb9c9dc));
        auto slider=[this](juce::Slider& s,double lo,double hi,double step,const juce::String& suffix){
            s.setRange(lo,hi,step); s.setTextValueSuffix(suffix); s.setSliderStyle(juce::Slider::LinearHorizontal);
            s.setTextBoxStyle(juce::Slider::TextBoxRight,false,86,25); addAndMakeVisible(s);
        };
        slider(gain_,-120,24,0.1," dB"); slider(pan_,-1,1,0.01,""); slider(clipGain_,-120,24,0.1," dB");
        gainLabel_.setText("Track gain",juce::dontSendNotification); panLabel_.setText("Pan / balance",juce::dontSendNotification); clipLabel_.setText("Clip gain",juce::dontSendNotification);
        addAndMakeVisible(gainLabel_); addAndMakeVisible(panLabel_); addAndMakeVisible(clipLabel_);
        gain_.onDragEnd=[this]{if(!track_.empty()) apply(Json::array({{{"command","set_track_gain"},{"track_id",track_},{"gain_db",gain_.getValue()}}}));};
        pan_.onDragEnd=[this]{if(!track_.empty()) apply(Json::array({{{"command","set_track_pan"},{"track_id",track_},{"pan",pan_.getValue()}}}));};
        clipGain_.onDragEnd=[this]{if(selection_.clips.empty() || clip_.empty())return;for(const auto& t:displaySession_.at("tracks"))for(const auto& c:activeClips(t))if(c.at("id")==clip_){auto op=selection_.operation("gain");op["delta_db"]=clipGain_.getValue()-c.at("gain_db").get<double>();apply(Json::array({op}),displaySession_.at("revision"),displaySession_.at("id"));return;}};
        for(auto* s:{&gain_,&pan_,&clipGain_}) {
            s->onDragStart=[this]{sliderGesture_=true;};auto finish=s->onDragEnd;
            s->onDragEnd=[this,finish]{sliderGesture_=false;finish();};
            s->onValueChange=[this,finish]{if(!sliderGesture_)finish();};
        }
        mute_.setButtonText("Mute track"); lock_.setButtonText("Lock track"); addAndMakeVisible(mute_); addAndMakeVisible(lock_);
        mute_.onClick=[this]{if(!track_.empty()) apply(Json::array({{{"command","set_track_mute"},{"track_id",track_},{"muted",mute_.getToggleState()}}}));};
        lock_.onClick=[this]{if(!track_.empty()) apply(Json::array({{{"command","set_track_lock"},{"track_id",track_},{"locked",lock_.getToggleState()}}}));};
        arm_.setButtonText("Record arm");addAndMakeVisible(arm_);
        arm_.onClick=[this]{if(!track_.empty())apply(Json::array({{{"command","set_track_arm"},{"track_id",track_},{"record_armed",arm_.getToggleState()}}}));};
        monitor_.addItem("Monitor off",1);monitor_.addItem("Input monitoring",2);monitor_.addItem("Auto monitoring",3);addAndMakeVisible(monitor_);
        monitor_.onChange=[this]{if(!track_.empty())apply(Json::array({{{"command","set_track_monitor"},{"track_id",track_},{"monitor_mode",monitor_.getSelectedId()==2?"input":monitor_.getSelectedId()==3?"auto":"off"}}}));};
        inputButton_.setButtonText("Input...");addAndMakeVisible(inputButton_);inputButton_.onClick=[this]{selectInput();};
        timeline_.execute=[this](Json ops){apply(std::move(ops));};
        timeline_.select=[this](std::string track,std::string clip,Frame begin,Frame end){selectObjects(track,clip,begin,end);};
        timeline_.selectionChanged=[this](EditSelection selection,std::uint64_t revision){if(displaySession_.at("revision")!=revision){note_="Selection cancelled: session changed";return;}selectMany(std::move(selection));};
        timeline_.executePinned=[this](Json ops,std::uint64_t revision,std::string sessionId){apply(std::move(ops),revision,sessionId);};
        timeline_.sourceChosen=[this](std::string track,std::string pl,std::string clip,Frame begin,Frame end,std::uint64_t revision){if(displaySession_.at("revision")!=revision)return;selection_.single(track,"",begin,end);sourceSelection_={{"track_id",track},{"playlist_id",pl},{"clip_id",clip},{"begin",begin},{"end",end},{"revision",revision}};track_=track;clip_.clear();cursor_=begin;selectionEndFrame_=end;updateSelection();note_="Source Playlist selected | Right-click to audition or copy to target";};
        timeline_.auditionSource=[this](std::string track,std::string pl,Frame begin,Frame end,std::uint64_t revision){auditionPlaylist(track,pl,begin,end,revision);};
        groups_.edit=[this](std::string id){showGroups(id);};groups_.execute=timeline_.executePinned;
        groups_.selectMembers=[this](std::vector<std::string> ids){EditSelection next;next.begin=cursor_;next.end=selectionEndFrame_;for(const auto& id:ids){next.tracks.insert(id);for(const auto& t:displaySession_.at("tracks"))if(t.at("id")==id)for(const auto& c:activeClips(t))if(c.at("start").get<Frame>()<next.end && c.at("start").get<Frame>()+c.at("length").get<Frame>()>next.begin)next.clips.insert(c.at("id"));}next.focusTrack=ids.empty()?"":ids.front();next.focusClip=next.clips.empty()?"":*next.clips.begin();selectMany(std::move(next));};
        timeline_.contextAI=[this]{aiVisible_=true;resized();intent_.grabKeyboardFocus();};
        ruler_.seek=[this](Frame frame){selectObjects(track_,clip_,frame,frame);};
        auto selectTrack=[this](std::string id){selectObjects(id,"",cursor_,selectionEndFrame_);};
        for(auto* bank:{&headers_,&mixer_}){bank->toggleLanes=[this](std::string id){timeline_.toggleSourceLanes(id);headers_.setRowHeights(timeline_.trackHeights());resized();};bank->selectModified=[this](std::string id,juce::ModifierKeys mods){auto next=selection_;if(mods.isShiftDown() && !next.focusTrack.empty())next.range(displaySession_,next.focusTrack,id,cursor_,selectionEndFrame_,true);else next.single(id,"",cursor_,selectionEndFrame_,mods.isCommandDown());selectMany(std::move(next));};bank->execute=[this](Json ops){apply(std::move(ops));};bank->select=selectTrack;bank->routing=[this](std::string id){showRouting(id);};bank->playlists=[this](std::string id){selectObjects(id,"",cursor_,selectionEndFrame_);showPlaylists();};}
        tracks_.selectionChanged=[this](EditSelection next){next.begin=cursor_;next.end=selectionEndFrame_;selectMany(std::move(next));};clips_.selectionChanged=[this](EditSelection next){selectMany(std::move(next));};
        tracks_.select=timeline_.select;clips_.select=[this](std::string track,std::string clip,Frame begin,Frame end){selectObjects(track,clip,begin,end);viewport_.setViewPosition(std::max(0,static_cast<int>(timeline_.geometry.pixelAt(begin))-30),viewport_.getViewPositionY());};
        addButton("Edit",[this]{setMix(false);});addButton("Mix",[this]{setMix(true);});
        addButton("Select",[this]{setTool(Timeline::Tool::Select);});addButton("Grab",[this]{setTool(Timeline::Tool::Grab);});addButton("Trim",[this]{setTool(Timeline::Tool::Trim);});
        addButton("Zoom -",[this]{zoom(.5);});addButton("Zoom +",[this]{zoom(2);});addButton("Fit",[this]{fit();});
        addButton("Return",[this]{if(engine_.recordState()!=RecordState::Idle){note_="Stop recording before locating";return;}stopPlayback();selectObjects(track_,clip_,0,0);viewport_.setViewPosition(0,viewport_.getViewPositionY());});
        addButton("Add Aux",[this]{apply(Json::array({{{"command","add_aux_track"},{"name","Aux "+std::to_string(commands_->query().at("tracks").size()+1)}}}));});
        addButton("Add Master",[this]{apply(Json::array({{{"command","add_master_track"},{"name","Master"}}}));});
        addButton("Clips",[this]{clipsVisible_=!clipsVisible_;resized();});
        grid_.addItem("Slip",1);grid_.addItem("Grid: 0.1 s",2);grid_.addItem("Grid: 0.5 s",3);grid_.addItem("Grid: 1 s",4);grid_.setSelectedId(1);grid_.onChange=[this]{const double seconds=grid_.getSelectedId()==2?.1:grid_.getSelectedId()==3?.5:grid_.getSelectedId()==4?1.:0;timeline_.geometry.gridFrames=static_cast<Frame>(seconds*timeline_.geometry.sampleRate);timeline_.repaint();};
        timeFormat_.addItem("Min:Secs",1);timeFormat_.addItem("Samples",2);timeFormat_.setSelectedId(1);timeFormat_.onChange=[this]{ruler_.samples=timeFormat_.getSelectedId()==2;ruler_.repaint();};
        selectionTitle_.setText("SELECTION  /  SAMPLES",juce::dontSendNotification);
        selectionStart_.setTitle("Selection start in samples");selectionEnd_.setTitle("Selection end in samples");selectionLength_.setTitle("Selection length in samples");selectionLength_.setReadOnly(true);
        selectionStart_.setInputRestrictions(18,"0123456789");selectionEnd_.setInputRestrictions(18,"0123456789");
        selectionStart_.onReturnKey=[this]{auto begin=selectionStart_.getText().getLargeIntValue();selectObjects(track_,clip_,begin,std::max<Frame>(begin,selectionEndFrame_));};
        selectionEnd_.onReturnKey=[this]{auto end=selectionEnd_.getText().getLargeIntValue();selectObjects(track_,clip_,std::min<Frame>(cursor_,end),end);};
        selectionStart_.onFocusLost=selectionStart_.onReturnKey;selectionEnd_.onFocusLost=selectionEnd_.onReturnKey;
        setTool(Timeline::Tool::Select);
        aiTitle_.setText("AI task | local project",juce::dontSendNotification); addChildComponent(aiTitle_);
        intent_.setMultiLine(true); intent_.setTextToShowWhenEmpty("Describe an edit or query. Audio stays local.",juce::Colours::grey); addChildComponent(intent_);
        model_.setTextToShowWhenEmpty("Installed Ollama model name",juce::Colours::grey); model_.setText(ProviderConfig::environment().model); addChildComponent(model_);
        permissions_.addItem("Read-only analysis",1); permissions_.addItem("Preview before commit",2); permissions_.addItem("Low-risk | selected objects",3);
        permissions_.setSelectedId(2); addChildComponent(permissions_);
        aiRun_.setButtonText("Plan / query"); aiCancel_.setButtonText("Cancel task"); aiAccept_.setButtonText("Accept changes"); aiReject_.setButtonText("Reject preview");
        for(auto* b:{&aiRun_,&aiCancel_,&aiAccept_,&aiReject_}) addChildComponent(*b);
        aiRun_.onClick=[this]{runAI();}; aiCancel_.onClick=[this]{cancellation_.cancelled.store(true); note_="AI cancellation requested";};
        aiAccept_.onClick=[this]{acceptAI();}; aiReject_.onClick=[this]{aiPlan_.reset(); aiOutput_.setText("Preview rejected. Session unchanged.");};
        aiOutput_.setMultiLine(true); aiOutput_.setReadOnly(true); aiOutput_.setText("No model execution yet. Configure a local tool-capable model. Normal DAW operations work offline."); addChildComponent(aiOutput_);
        if(!initial.empty()) {
            try { file_=fs::absolute(initial); commands_=std::make_shared<Commands>(loadSession(file_),file_.parent_path()); }
            catch(const std::exception& e) { note_=e.what(); }
        }
        if(!commands_) makeEmpty();
        refresh(); auto error=engine_.openDevice(commands_->query().at("sample_rate"),256);
        if(!error.empty()) note_="Audio device unavailable: "+error;
        setMix(false); // Lay out children only after actions and controls exist.
        startTimerHz(20);
    }
    ~MainComponent() override { stopTimer();pluginWindow_.reset(); cancellation_.cancelled.store(true); if(aiThread_.joinable()) aiThread_.join(); if(work_.joinable()) work_.join(); engine_.closeDevice();setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override {
        g.fillAll(background);g.setColour(panel);g.fillRect(0,28,getWidth(),98);g.fillRect(0,128,getWidth(),42);
        g.setColour(line);g.drawHorizontalLine(126,0,static_cast<float>(getWidth()));g.drawHorizontalLine(170,0,static_cast<float>(getWidth()));
        if(!mixVisible_){label(g,"TRACK CONTROLS",{sidebarWidth_+10,172,210,24},10,text);label(g,"MARKERS",{sidebarWidth_+10,196,210,24},10,muted);}
        label(g,"INSPECTOR",{12,inspectorY_,sidebarWidth_-20,28},11,text);
        label(g,"START",selectionStart_.getBounds().translated(0,-17).withHeight(16),9,muted);label(g,"END",selectionEnd_.getBounds().translated(0,-17).withHeight(16),9,muted);label(g,"LENGTH",selectionLength_.getBounds().translated(0,-17).withHeight(16),9,muted);
    }
    void resized() override {
        if(buttons_.empty())return;
        const int w=getWidth(),h=getHeight();menu_.setBounds(0,0,w,28);
        place("New",12,40,48,23);place("Open",64,40,48,23);place("Save",116,40,48,23);place("Import",12,72,73,25);place("Export WAV",91,72,73,25);
        counter_.setBounds(180,37,std::max(320,w-790),76);
        int selectX=counter_.getRight()+16;selectionTitle_.setBounds(selectX,35,260,20);selectionStart_.setBounds(selectX,73,100,27);selectionEnd_.setBounds(selectX+107,73,100,27);selectionLength_.setBounds(selectX+214,73,100,27);
        int tx=w-278;place("Return",tx,48,62,36);place("Stop",tx+67,48,62,36);place("Play",tx+134,48,62,36);place("Record",tx+201,48,64,36);
        place("Record mode",tx,91,265,23);
        place("Edit",12,135,70,27);place("Mix",87,135,70,27);place("Select",180,135,59,27);place("Grab",244,135,57,27);place("Trim",306,135,57,27);grid_.setBounds(378,135,127,27);
        place("Split",519,135,50,27);place("Undo",574,135,50,27);place("Redo",629,135,50,27);place("Zoom -",692,135,58,27);place("Zoom +",755,135,58,27);place("Fit",818,135,42,27);
        place("Routing",w-354,135,74,27);place("Clips",w-273,135,65,27);place("Plugins",w-201,135,72,27);place("AI panel",w-122,135,110,27);
        button("Add Audio").setVisible(false);button("Add Aux").setVisible(false);button("Add Master").setVisible(false);button("Delete").setVisible(false);button("Audio devices").setVisible(false);
        const int top=172,bottom=h-30,bodyHeight=bottom-top;int right=aiVisible_?340:(!mixVisible_ && clipsVisible_)?190:0;
        inspectorY_=std::max(top+150,bottom-270);tracks_.setBounds(0,top,sidebarWidth_,std::max(60,inspectorY_-top-140));groups_.setBounds(0,std::max(top+60,inspectorY_-140),sidebarWidth_,140);
        gainLabel_.setBounds(12,inspectorY_+30,sidebarWidth_-20,20);gain_.setBounds(10,inspectorY_+52,sidebarWidth_-20,25);
        panLabel_.setBounds(12,inspectorY_+81,sidebarWidth_-20,20);pan_.setBounds(10,inspectorY_+103,sidebarWidth_-20,25);
        clipLabel_.setBounds(12,inspectorY_+132,sidebarWidth_-20,20);clipGain_.setBounds(10,inspectorY_+154,sidebarWidth_-20,25);
        lock_.setBounds(9,inspectorY_+183,sidebarWidth_-15,26);mute_.setVisible(false);arm_.setVisible(false);monitor_.setVisible(false);inputButton_.setBounds(10,inspectorY_+218,sidebarWidth_-20,28);
        workspaceTitle_.setBounds(sidebarWidth_+8,174,225,22);workspaceTitle_.setVisible(mixVisible_);
        const int header=222,canvasX=sidebarWidth_+header,canvasW=std::max(250,w-canvasX-right);
        timeFormat_.setBounds(sidebarWidth_+8,220,header-16,24);timeFormat_.setVisible(!mixVisible_);
        ruler_.setBounds(canvasX,top,canvasW,76);ruler_.setVisible(!mixVisible_);
        viewport_.setBounds(canvasX,top+76,canvasW,std::max(100,bodyHeight-76));viewport_.setVisible(!mixVisible_);
        headerViewport_.setBounds(sidebarWidth_,top+76,header,std::max(100,bodyHeight-76));headerViewport_.setVisible(!mixVisible_);
        timeline_.extent(viewport_.getMaximumVisibleWidth(),viewport_.getMaximumVisibleHeight());headers_.setRowHeights(timeline_.trackHeights());headers_.extent(header,timeline_.getHeight(),timeline_.rowHeight);
        mixViewport_.setBounds(sidebarWidth_,top+28,w-sidebarWidth_-right,std::max(100,bodyHeight-28));mixViewport_.setVisible(mixVisible_);mixer_.extent(mixViewport_.getMaximumVisibleWidth(),mixViewport_.getMaximumVisibleHeight());
        clips_.setBounds(w-right,top,right,bodyHeight);clips_.setVisible(clipsVisible_ && !aiVisible_ && !mixVisible_);
        status_.setBounds(12,h-27,w-24,24);
        std::vector<juce::Component*> ai{&aiTitle_,&intent_,&model_,&permissions_,&aiRun_,&aiCancel_,&aiAccept_,&aiReject_,&aiOutput_};for(auto* c:ai)c->setVisible(aiVisible_);
        int left=w-330;aiTitle_.setBounds(left,top+8,318,25);model_.setBounds(left,top+39,318,28);permissions_.setBounds(left,top+75,318,28);
        intent_.setBounds(left,top+113,318,100);aiRun_.setBounds(left,top+224,153,28);aiCancel_.setBounds(left+162,top+224,156,28);aiAccept_.setBounds(left,top+261,153,28);aiReject_.setBounds(left+162,top+261,156,28);aiOutput_.setBounds(left,top+301,318,std::max(60,bodyHeight-313));
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(intent_.hasKeyboardFocus(true) || model_.hasKeyboardFocus(true) || selectionStart_.hasKeyboardFocus(true) || selectionEnd_.hasKeyboardFocus(true)) return false;
        if(key.getKeyCode()==juce::KeyPress::spaceKey) {play();return true;}
        if(key.getModifiers().isCommandDown()) {
            // macOS command events intentionally carry textCharacter == 0.
            const auto code=juce::CharacterFunctions::toLowerCase(static_cast<juce::juce_wchar>(key.getKeyCode()));
            if(code=='s'){save();return true;}
            if(code=='i'){import();return true;}
            if(code=='e'){split();return true;}
            if(code=='=' || code=='+'){setMix(!mixVisible_);return true;}
            if(code=='c'){copyClip();return true;}
            if(code=='v'){pasteClip();return true;}
            if(code=='o'){openProject();return true;}
            if(code=='z'){editHistory(key.getModifiers().isShiftDown());return true;}
        }
        if(key.getModifiers().isAltDown() && (key.getKeyCode()==juce::KeyPress::leftKey || key.getKeyCode()==juce::KeyPress::rightKey) && !selection_.clips.empty()){auto op=selection_.operation("move");op["delta"]=key.getKeyCode()==juce::KeyPress::leftKey?-1:1;apply(Json::array({op}),displaySession_.at("revision"),displaySession_.at("id"));return true;}
        if(key.getKeyCode()==juce::KeyPress::backspaceKey || key.getKeyCode()==juce::KeyPress::deleteKey){removeClip();return true;}
        if(key.getKeyCode()==juce::KeyPress::returnKey){stopPlayback();selectObjects(track_,clip_,0,0);return true;}
        if(key.getTextCharacter()=='+'){zoom(2);return true;}if(key.getTextCharacter()=='-'){zoom(.5);return true;}
        if(key.getTextCharacter()=='1'){setTool(Timeline::Tool::Select);return true;}if(key.getTextCharacter()=='2'){setTool(Timeline::Tool::Grab);return true;}if(key.getTextCharacter()=='3'){setTool(Timeline::Tool::Trim);return true;}
        if(key.getTextCharacter()=='s'){split();return true;} return false;
    }
private:
    void showRecordSetup() {
        if(busy_ || editorPreview_ || engine_.recordState()!=RecordState::Idle){note_="Finish the operation and recording before changing recording setup";return;}
        if(!recordWindow_){auto safe=juce::Component::SafePointer<MainComponent>(this);recordWindow_=std::make_unique<NativeRecordingWindow>([safe](Json ops){if(safe)safe->apply(std::move(ops));});}
        recordWindow_->editor().update(commands_->query(),cursor_,selectionEndFrame_);recordWindow_->setVisible(true);recordWindow_->toFront(true);
    }
    juce::TextButton& button(const juce::String& name) {
        for(auto& b:buttons_)if(b->getName()==name)return *b;
        throw Error("desktop_control","Unknown workstation action");
    }
    void place(const juce::String& name,int x,int y,int width,int height) {button(name).setBounds(x,y,width,height);}
    void selectObjects(std::string track,std::string clip,Frame begin,Frame end) {
        selection_.single(std::move(track),std::move(clip),begin,end);selectMany(selection_);
    }
    void selectMany(EditSelection selection) {
        sourceSelection_=nullptr;selection_=std::move(selection);selection_.retain(displaySession_);track_=selection_.focusTrack;clip_=selection_.focusClip;cursor_=selection_.begin;selectionEndFrame_=selection_.end;updateSelection();
    }
    void showGroups(const std::string& id="") {
        if(!groupWindow_){auto safe=juce::Component::SafePointer<MainComponent>(this);groupWindow_=std::make_unique<NativeGroupWindow>([safe](Json ops,std::uint64_t revision,std::string sessionId){if(safe)safe->apply(std::move(ops),revision,sessionId);});}
        groupWindow_->editor().update(displaySession_,selection_.tracks);if(!id.empty())groupWindow_->editor().chooseGroup(id);groupWindow_->setVisible(true);groupWindow_->toFront(true);
    }
    void auditionPlaylist(const std::string& track,const std::string& pl,Frame begin,Frame end,std::uint64_t revision) {
        if(end<=begin){note_="Select an explicit audition range";return;}if(!commands_->captureLease().is_null()){note_="Finish recording before Playlist audition";return;}auto commands=commands_;auto safe=juce::Component::SafePointer<MainComponent>(this);
        task("Preparing source Playlist audition...",[safe,commands,revision,track,pl,begin,end]{if(!safe)return std::string("Audition cancelled");if(commands->query().at("revision")!=revision)throw Error("version_conflict","Source selection changed before audition");safe->engine_.stop();const auto state=commands->query();bool selected=false;for(const auto& t:state.at("tracks"))if(t.at("id")==track && t.at("active_playlist_id")==pl)selected=true;if(!selected){auto p=commands->dryRun(Json::array({{{"command","select_playlist"},{"track_id",track},{"playlist_id",pl}}}),revision,Actor::Gui,uuid());commands->commit(p);}safe->engine_.publishSession(commands->query(),commands->root());safe->engine_.play(begin,end);return std::string("Auditioning actual source Playlist | Switch has Undo");});
    }
    void showRouting(const std::string& track) {
        if(!track.empty())selectObjects(track,"",cursor_,selectionEndFrame_);
        if(!routingWindow_){auto safe=juce::Component::SafePointer<MainComponent>(this);routingWindow_=std::make_unique<NativeRoutingWindow>([safe](Json ops){if(safe)safe->apply(std::move(ops));});routingWindow_->onClosed=[safe]{if(safe){if(auto* window=safe->findParentComponentOfClass<juce::DocumentWindow>())window->toFront(true);safe->grabKeyboardFocus();}};routingWindow_->editor().requestStateCapture=[safe](std::string track,std::string instance){if(safe)safe->capturePluginState(track,instance);};routingWindow_->editor().requestNativeEditor=[safe](std::string track,std::string instance){if(safe)safe->openNativePluginEditor(track,instance);};}
        routingWindow_->editor().update(commands_->query(),track_);routingWindow_->setVisible(true);routingWindow_->toFront(true);
    }
    void showPlaylists() {
        if(!playlistWindow_){auto safe=juce::Component::SafePointer<MainComponent>(this);playlistWindow_=std::make_unique<NativePlaylistWindow>([safe](Json ops){if(safe)safe->apply(std::move(ops));});
            playlistWindow_->editor().selectTrack=[safe](std::string id){if(safe)safe->selectObjects(id,"",safe->cursor_,safe->selectionEndFrame_);};
            playlistWindow_->editor().stop=[safe]{if(safe){safe->stopPlayback();}};
            playlistWindow_->editor().audition=[safe](std::string track,std::string playlist,Frame begin,Frame end){if(!safe)return;if(end<=begin){safe->note_="Select an explicit audition range";return;}if(!safe->commands_->captureLease().is_null()){safe->note_="Finish recording before Playlist audition";return;}auto commands=safe->commands_;auto revision=commands->query().at("revision").get<std::uint64_t>();safe->task("Preparing Playlist audition...",[safe,commands,revision,track,playlist,begin,end]{if(!safe)return std::string("Audition cancelled");safe->engine_.stop();const auto state=commands->query();if(state.at("revision")!=revision)throw Error("version_conflict","Project changed before audition");bool alreadySelected=false;for(const auto& t:state.at("tracks"))if(t.at("id")==track && t.at("active_playlist_id")==playlist)alreadySelected=true;if(!alreadySelected){auto p=commands->dryRun(Json::array({{{"command","select_playlist"},{"track_id",track},{"playlist_id",playlist}}}),revision,Actor::Gui,uuid());commands->commit(p);}safe->engine_.publishSession(commands->query(),commands->root());safe->engine_.play(begin,end);return std::string("Auditioning source selection | Playlist switches have Undo");});};}
        playlistWindow_->editor().update(commands_->query(),commands_->root(),track_,cursor_,selectionEndFrame_);playlistWindow_->setVisible(true);playlistWindow_->toFront(true);
    }
    void setMix(bool visible) {
        mixVisible_=visible;button("Edit").setToggleState(!visible,juce::dontSendNotification);button("Mix").setToggleState(visible,juce::dontSendNotification);
        workspaceTitle_.setText("MIX",juce::dontSendNotification);resized();
    }
    void setTool(Timeline::Tool tool) {
        timeline_.tool=tool;button("Select").setToggleState(tool==Timeline::Tool::Select,juce::dontSendNotification);button("Grab").setToggleState(tool==Timeline::Tool::Grab,juce::dontSendNotification);button("Trim").setToggleState(tool==Timeline::Tool::Trim,juce::dontSendNotification);
    }
    void zoom(double factor) {
        auto anchor=timeline_.geometry.frameAt(viewport_.getViewPositionX());timeline_.geometry.pixelsPerSecond=juce::jlimit(2.,2000.,timeline_.geometry.pixelsPerSecond*factor);timeline_.extent();viewport_.setViewPosition(static_cast<int>(timeline_.geometry.pixelAt(anchor)),viewport_.getViewPositionY());ruler_.repaint();timeline_.repaint();
    }
    void fit() {
        const auto session=commands_->query();auto frames=std::max<Frame>(session.at("sample_rate").get<int>()*5,sessionLength(session));timeline_.geometry.pixelsPerSecond=std::max(2.,(viewport_.getMaximumVisibleWidth()-30)*session.at("sample_rate").get<double>()/frames);timeline_.extent();viewport_.setViewPosition(0,viewport_.getViewPositionY());ruler_.repaint();timeline_.repaint();
    }
    void copyClip() {if(!clip_.empty()){clipboardClip_=clip_;clipboardSession_=displaySession_.at("id");note_="Clip copied | Choose a track and position to paste";}}
    void pasteClip() {
        if(clipboardClip_.empty() || clipboardSession_!=displaySession_.at("id").get<std::string>()){note_="Copy a clip from this session first";return;}
        Json op{{"command","duplicate_clip"},{"clip_id",clipboardClip_},{"position",cursor_}};if(!track_.empty())op["track_id"]=track_;apply(Json::array({op}));
    }
    void renameObject(bool clip) {
        if(busy_ || (clip?clip_.empty():track_.empty()))return;std::string name;
        for(const auto& t:displaySession_.at("tracks")){if(!clip && t.at("id")==track_)name=t.at("name");if(clip)for(const auto& c:activeClips(t))if(c.at("id")==clip_)name=c.at("name");}
        if(name.empty())return;
        auto* dialog=new juce::AlertWindow(clip?"Rename clip":"Rename track","Enter a display name.",juce::MessageBoxIconType::NoIcon);dialog->addTextEditor("name",name,"Name");dialog->addButton("Rename",1,juce::KeyPress(juce::KeyPress::returnKey));dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
        auto safe=juce::Component::SafePointer<MainComponent>(this);auto session=commands_;const auto id=clip?clip_:track_;const auto revision=displaySession_.at("revision");
        dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog,session,id,clip,revision](int result){if(result!=1 || !safe)return;if(safe->commands_!=session || session->query().at("revision")!=revision){safe->note_="Rename cancelled: session changed while the dialog was open";return;}safe->apply(Json::array({{{"command",clip?"rename_clip":"rename_track"},{clip?"clip_id":"track_id",id},{"name",dialog->getTextEditorContents("name").toStdString()}}}));}),true);
    }
    void moveTrack(int direction) {
        if(engine_.state()==PlaybackState::Playing || engine_.state()==PlaybackState::Priming || engine_.recordState()!=RecordState::Idle){note_="Stop transport before reordering tracks";return;}
        Json ids=Json::array();int index=-1;for(const auto& track:displaySession_.at("tracks")){if(track.at("id")==track_)index=static_cast<int>(ids.size());ids.push_back(track.at("id"));}
        const int target=index+direction;if(index>=0 && target>=0 && target<static_cast<int>(ids.size())){std::swap(ids[index],ids[target]);apply(Json::array({{{"command","reorder_tracks"},{"track_ids",ids}}}));}
    }
    juce::StringArray getMenuBarNames() override {return {"File","Edit","Track","Clip","View","Setup"};}
    juce::PopupMenu getMenuForIndex(int index,const juce::String&) override {
        juce::PopupMenu m;
        if(index==0){m.addItem(1,"New session");m.addItem(2,"Open session...  Cmd+O");m.addItem(3,"Save session  Cmd+S");m.addSeparator();m.addItem(4,"Import audio...  Cmd+I");m.addItem(5,"Bounce WAV / BWF...");}
        if(index==1){m.addItem(6,"Undo  Cmd+Z");m.addItem(7,"Redo  Shift+Cmd+Z");m.addSeparator();m.addItem(29,"Copy clip  Cmd+C",!clip_.empty());m.addItem(30,"Paste clip  Cmd+V",!clipboardClip_.empty());m.addSeparator();m.addItem(8,"Split at cursor  Cmd+E",!clip_.empty());m.addItem(9,"Delete selected clips",!selection_.clips.empty());m.addItem(39,"Nudge selection earlier by1 sample",!selection_.clips.empty());m.addItem(40,"Nudge selection later by1 sample",!selection_.clips.empty());}
        if(index==2){m.addItem(10,"New audio track");m.addItem(11,"New Aux input");m.addItem(12,"New Master fader");m.addSeparator();m.addItem(31,"Rename track...",!track_.empty());m.addItem(32,"Move track up",!track_.empty());m.addItem(33,"Move track down",!track_.empty());m.addSeparator();m.addItem(13,"Routing and inserts...");m.addItem(35,"Playlists and Comp...",!track_.empty());m.addItem(37,"Track groups...");m.addItem(38,"Show / hide source Playlist lanes",!track_.empty());}
        if(index==3){m.addItem(34,"Rename clip...",!clip_.empty());m.addItem(8,"Split at cursor",!clip_.empty());m.addItem(9,"Delete clip",!clip_.empty());m.addSeparator();m.addItem(22,"5 ms fades",!clip_.empty());m.addItem(23,"20 ms fades",!clip_.empty());m.addItem(24,"Remove fades",!clip_.empty());}
        if(index==4){m.addItem(14,"Edit window  Cmd+=",true,!mixVisible_);m.addItem(15,"Mix window  Cmd+=",true,mixVisible_);m.addSeparator();m.addItem(16,"Zoom in");m.addItem(17,"Zoom out");m.addItem(18,"Fit session");m.addItem(19,"Clip list",true,clipsVisible_);m.addItem(20,"AI task panel",true,aiVisible_);m.addSeparator();m.addItem(25,"Small track height");m.addItem(26,"Medium track height");m.addItem(27,"Large track height");}
        if(index==5){m.addItem(21,"Audio devices...");m.addItem(36,"Recording setup...",engine_.recordState()==RecordState::Idle);m.addItem(28,"Plugin manager...");}
        return m;
    }
    void menuItemSelected(int id,int) override {
        const std::map<int,juce::String> actions{{1,"New"},{2,"Open"},{3,"Save"},{4,"Import"},{5,"Export WAV"},{6,"Undo"},{7,"Redo"},{8,"Split"},{9,"Delete"},{10,"Add Audio"},{11,"Add Aux"},{12,"Add Master"},{13,"Routing"},{14,"Edit"},{15,"Mix"},{16,"Zoom +"},{17,"Zoom -"},{18,"Fit"},{19,"Clips"},{20,"AI panel"},{21,"Audio devices"},{28,"Plugins"}};
        if(auto i=actions.find(id);i!=actions.end()){button(i->second).onClick();return;}
        if(id==39 || id==40){auto op=selection_.operation("move");op["delta"]=id==39?-1:1;apply(Json::array({op}),displaySession_.at("revision"),displaySession_.at("id"));return;}if(id==37){showGroups();return;}if(id==38){timeline_.toggleSourceLanes(track_);headers_.setRowHeights(timeline_.trackHeights());resized();return;}if(id==36){showRecordSetup();return;}if(id==35){showPlaylists();return;}if(id==29){copyClip();return;}if(id==30){pasteClip();return;}if(id==31 || id==34){renameObject(id==34);return;}if(id==32 || id==33){moveTrack(id==32?-1:1);return;}
        if(id>=22 && id<=24 && !selection_.clips.empty()){const Frame fade=id==24?0:static_cast<Frame>(displaySession_.at("sample_rate").get<double>()*(id==22?.005:.02));auto op=selection_.operation("fades");op["fade_in"]=fade;op["fade_out"]=fade;apply(Json::array({op}),displaySession_.at("revision"),displaySession_.at("id"));return;}
        if(id>=25 && id<=27){timeline_.rowHeight=id==25?116:id==26?156:220;timeline_.update(displaySession_,commands_->root());resized();timeline_.repaint();}
    }
    void addButton(const juce::String& name,std::function<void()> callback) {
        auto b=std::make_unique<juce::TextButton>(name);b->setName(name); b->onClick=[this,callback=std::move(callback)]{grabKeyboardFocus();try{callback();}catch(const std::exception& e){note_=std::string("Failed: ")+e.what();}}; addAndMakeVisible(*b); buttons_.push_back(std::move(b));
    }
    void makeEmpty() {
        auto root=fs::path(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getFullPathName().toStdString())/"NativeDAW"/"Sessions"/uuid();
        file_=root/"session.ndaw"; commands_=std::make_shared<Commands>(newSession("Untitled"),root); track_.clear(); clip_.clear(); selection_={};sourceSelection_=nullptr;cursor_=selectionEndFrame_=0;
        commands_->save(file_); // A first-save baseline exists before a crash/recovery can occur.
    }
    void refresh() { displaySession_=commands_->query();auto& selectedSession=displaySession_;if(track_.empty() && !selectedSession.at("tracks").empty())track_=selectedSession.at("tracks")[0].at("id");engine_.publishSession(displaySession_,commands_->root()); timeline_.update(displaySession_,commands_->root()); selection_.retain(displaySession_);if(selection_.focusTrack.empty())selection_.single(track_,"",cursor_,selectionEndFrame_);track_=selection_.focusTrack;clip_=selection_.focusClip;updateSelection();if(routingWindow_)routingWindow_->editor().update(commands_->query(),track_);if(playlistWindow_)playlistWindow_->editor().update(displaySession_,commands_->root(),track_,cursor_,selectionEndFrame_);resized(); }
    void updateSelection() {
        const auto& snapshot=displaySession_; bool trackFound=false,clipFound=false,audio=false,master=false;
        for(const auto& t:snapshot["tracks"]) if(t.at("id")==track_) {
            trackFound=true;audio=t.at("kind")=="audio";master=t.at("kind")=="master";gain_.setValue(t.at("gain_db").get<double>(),juce::dontSendNotification); pan_.setValue(t.at("pan").get<double>(),juce::dontSendNotification);
            mute_.setToggleState(t.at("muted").get<bool>(),juce::dontSendNotification); lock_.setToggleState(t.at("locked").get<bool>(),juce::dontSendNotification);
            arm_.setToggleState(t.at("record_armed").get<bool>(),juce::dontSendNotification);
            auto mode=t.at("monitor_mode").get<std::string>();monitor_.setSelectedId(mode=="input"?2:mode=="auto"?3:1,juce::dontSendNotification);
            juce::String label="Input ";for(const auto& n:t.at("input_channels"))label+=juce::String(n.get<int>()+1)+" ";inputButton_.setButtonText(label);
            for(const auto& c:activeClips(t)) if(c.at("id")==clip_) {clipFound=true;clipGain_.setValue(c.at("gain_db").get<double>(),juce::dontSendNotification);}
        }
        gain_.setEnabled(trackFound);pan_.setEnabled(trackFound && !master);mute_.setEnabled(trackFound && !master);lock_.setEnabled(trackFound);clipGain_.setEnabled(clipFound);arm_.setEnabled(audio && engine_.recordState()==RecordState::Idle);inputButton_.setEnabled(audio && engine_.recordState()==RecordState::Idle);monitor_.setEnabled(audio);
        selection_.focusTrack=track_;selection_.focusClip=clipFound?clip_:std::string{};selection_.begin=cursor_;selection_.end=selectionEndFrame_;timeline_.selected(selection_);
        headers_.update(snapshot,track_,selection_.tracks);mixer_.update(snapshot,track_,selection_.tracks);tracks_.update(snapshot,track_,selection_.tracks);clips_.update(snapshot,clip_,selection_.clips);groups_.update(snapshot);
        ruler_.begin=cursor_;ruler_.end=selectionEndFrame_;ruler_.markers=snapshot.at("markers");ruler_.recording=recordSettings(snapshot);
        ruler_.setDescription(ruler_.recording.punch?"Selection Punch "+juce::String(ruler_.recording.begin)+" to "+juce::String(ruler_.recording.end)+" samples; continuous capture "+juce::String(ruler_.recording.captureBegin())+" to "+juce::String(ruler_.recording.captureEnd())+" samples. Configure in Recording setup.":ruler_.recording.loop?"Loop recording "+juce::String(ruler_.recording.begin)+" to "+juce::String(ruler_.recording.end)+" samples. Configure in Recording setup.":"Timeline seconds/samples, markers and edit selection");ruler_.repaint();
        if(!selectionStart_.hasKeyboardFocus(true))selectionStart_.setText(juce::String(static_cast<juce::int64>(cursor_)),false);if(!selectionEnd_.hasKeyboardFocus(true))selectionEnd_.setText(juce::String(static_cast<juce::int64>(selectionEndFrame_)),false);selectionLength_.setText(juce::String(static_cast<juce::int64>(selectionEndFrame_-cursor_)),false);
    }
    void task(std::string description,std::function<std::string()> action,bool refreshAfter=true) {
        if(busy_ || editorPreview_){note_="Finish or cancel the current plugin preview/operation first";return;}
        if(work_.joinable()) work_.join(); busy_=true; note_=description;
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        work_=std::thread([safe,action=std::move(action),refreshAfter]{
            std::string message; try{message=action();}catch(const Error& e){message=e.code=="no_progress"?"No change | current values and relative offsets retained":std::string("Failed: ")+e.what();}catch(const std::exception& e){message=std::string("Failed: ")+e.what();}
            juce::MessageManager::callAsync([safe,message,refreshAfter]{if(safe){safe->busy_=false;safe->note_=message;if(refreshAfter)safe->refresh();}});
        });
    }
    void apply(Json operations,std::optional<std::uint64_t> pinnedRevision=std::nullopt,const std::string& pinnedSession="") {
        if(!pinnedSession.empty() && commands_->query().at("id")!=pinnedSession){note_="Not executed: a different session is now open";return;}
        if(busy_ || editorPreview_){note_="Finish the current operation first";return;}
        if(pinnedRevision && commands_->query().at("revision")!=*pinnedRevision){note_="Not executed: session changed after selection or preview";return;}
        for(const auto& op:operations)if((op.at("command")=="select_playlist" || op.at("command")=="select_comp_set" || op.at("command")=="restore_comp_set" || op.at("command")=="move_comp_set" || (op.at("command")=="edit_clip_selection" && op.at("action")!="gain" && op.at("action")!="fades") || (op.contains("resolved_clip_ids") && (op.at("command")=="move_clip" || op.at("command")=="trim_clip" || op.at("command")=="split_clip" || op.at("command")=="delete_clip")))){if(!commands_->captureLease().is_null()){note_="Finish recording before switching Playlists";return;}engine_.stop();}
        auto commands=commands_; auto revision=commands->query().at("revision").get<std::uint64_t>();
        task("Editing...",[commands,operations,revision]{auto plan=commands->dryRun(operations,revision,Actor::Gui,uuid());commands->commit(plan);return "Edit committed | Undo available";});
    }
    void capturePluginState(const std::string& track,const std::string& instance) {
        if(busy_ || aiBusy_ || editorPreview_){note_="Finish the current task before capturing DSP state";return;}
        if(!commands_->captureLease().is_null()){note_="Finalize recording before capturing DSP state";return;}
        if(work_.joinable())work_.join();busy_=true;note_="Capturing actual plugin DSP state...";auto commands=commands_;const auto snapshot=commands->query();
        auto safe=juce::Component::SafePointer<MainComponent>(this);work_=std::thread([this,safe,commands,snapshot,track,instance]{
            try{const auto capture=engine_.capturePluginState(snapshot,commands->root(),track,instance);const auto facts=commands->retainPluginState(capture);
                auto plan=commands->dryRun(Json::array({{{"command","adopt_plugin_state"},{"track_id",track},{"processor_id",instance},{"capture_id",facts.at("capture_id")}}}),snapshot.at("revision"),Actor::Gui,uuid());
                juce::MessageManager::callAsync([safe,commands,plan,facts]{if(!safe)return;
                    auto options=juce::MessageBoxOptions{}.withIconType(juce::MessageBoxIconType::QuestionIcon).withTitle("Captured plugin DSP state")
                        .withMessage("Actual resident plugin state was captured and its process exited normally.\n\nAudio is paused until you decide. Adopt commits one reversible project edit. Keep project state restores the previous project state.\n\nState SHA256: "+facts.at("state_sha256").get<std::string>()+"\n\nChanges:\n"+plan.diff.dump(2))
                        .withButton("Adopt state").withButton("Keep project state");
                    juce::AlertWindow::showAsync(options,[safe,commands,plan](int result){if(!safe)return;safe->busy_=false;
                        safe->task("Resolving DSP-state capture...",[safe,commands,plan,result]{std::string message;
                            try{if(result==1){commands->commit(plan);message="DSP state adopted | Undo available";}else message="DSP capture rejected | Previous project state restored";}
                            catch(const std::exception& e){message=std::string("DSP adoption failed: ")+e.what();}
                            if(safe)safe->engine_.resumeAfterPluginCapture(commands->query(),commands->root());return message;});});
                });
            }catch(const std::exception& e){const auto message=std::string("DSP capture failed: ")+e.what();
                try{if(engine_.metrics().at("plugin_state_capture_suspended").get<bool>())engine_.resumeAfterPluginCapture(commands->query(),commands->root());}catch(...){}
                juce::MessageManager::callAsync([safe,message]{if(safe){safe->busy_=false;safe->note_=message;safe->refresh();}});
            }
        });
    }
    void openNativePluginEditor(const std::string& track,const std::string& instance) {
        if(busy_ || aiBusy_ || editorPreview_){note_="Finish the current task before plugin preview";return;}
        const auto commands=commands_;const auto snapshot=commands->query();auto safe=juce::Component::SafePointer<MainComponent>(this);
        task("Opening actual resident plugin editor...",[this,safe,commands,snapshot,track,instance]{
            engine_.openPluginEditor(snapshot,commands->root(),track,instance);
            juce::MessageManager::callAsync([safe]{if(safe)safe->editorPreview_=true;});
            return "Native plugin preview | Audio paused | Review or cancel in plugin window";
        },false);
    }
    void resolveNativePluginEditor(bool review) {
        if(busy_)return;editorPreview_=false;auto commands=commands_;auto safe=juce::Component::SafePointer<MainComponent>(this);
        if(!review){const bool failed=engine_.pluginEditorStatus().value("failed",false);task("Restoring plugin preview...",[this,failed]{engine_.cancelPluginEditor();return failed?"Plugin editor process failed | Original project reload requested | No edit adopted":"Plugin preview cancelled | Prior SDK state restored";});return;}
        if(work_.joinable())work_.join();busy_=true;note_="Capturing native editor candidate...";
        work_=std::thread([this,safe,commands]{try{
            const auto capture=engine_.finishPluginEditor(true);const auto facts=commands->retainPluginState(capture);
            auto plan=commands->dryRun(Json::array({{{"command","adopt_plugin_state"},{"track_id",facts.at("track_id")},{"processor_id",facts.at("processor_id")},{"capture_id",facts.at("capture_id")}}}),facts.at("base_revision"),Actor::Gui,uuid());
            const auto preview=describeNativePluginPreview(std::string("Native plugin changes"),capture.facts().at("editor"));
            juce::MessageManager::callAsync([safe,commands,plan,facts,preview]{if(!safe)return;
                const auto options=juce::MessageBoxOptions{}.withIconType(juce::MessageBoxIconType::QuestionIcon).withTitle("Review native plugin changes")
                    .withMessage(juce::String(preview)).withButton("Accept changes").withButton("Keep project state");
                juce::AlertWindow::showAsync(options,[safe,commands,plan](int result){if(!safe)return;safe->busy_=false;
                    safe->task("Resolving native plugin changes...",[safe,commands,plan,result]{std::string note;
                            try{if(result==1){commands->commit(plan);note="Plugin state transaction committed | Audio readback in Routing | Undo available";}else note="Plugin preview rejected | Prior project state retained";}
                        catch(const std::exception& e){note=std::string("Plugin edit failed: ")+e.what();}
                        if(safe)safe->engine_.resumeAfterPluginCapture(commands->query(),commands->root());return note;});});
            });
        }catch(const std::exception& e){const auto message=std::string("Plugin editor failed: ")+e.what();try{engine_.cancelPluginEditor();}catch(...){}
            juce::MessageManager::callAsync([safe,message]{if(safe){safe->busy_=false;safe->note_=message;if(!safe->engine_.metrics().at("plugin_state_capture_suspended").get<bool>())safe->refresh();}});
        }});
    }
    void editHistory(bool redo) {if(!commands_->captureLease().is_null()){note_="Finish recording before Undo/Redo";return;}engine_.stop();auto commands=commands_;task(redo?"Redo...":"Undo...",[commands,redo]{if(redo)commands->redo();else commands->undo();return redo?"Redo committed":"Undo committed";});}
    void choose(const juce::String& title,int flags,const juce::String& pattern,std::function<void(fs::path)> chosen) {
        chooser_=std::make_unique<juce::FileChooser>(title,juce::File(juce::String(file_.parent_path().string())),pattern);
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        chooser_->launchAsync(flags,[safe,chosen=std::move(chosen)](const juce::FileChooser& chooser){if(safe && chooser.getResult()!=juce::File{})chosen(fs::path(chooser.getResult().getFullPathName().toStdString()));});
    }
    void newProject(){if(busy_ || editorPreview_)return;if(engine_.recordState()!=RecordState::Idle){note_="Stop and finalize recording before changing projects";return;}engine_.stop();cancellation_.cancelled.store(true);makeEmpty();refresh();note_="New local session";}
    void openProject() {if(engine_.recordState()!=RecordState::Idle){note_="Stop and finalize recording before opening another project";return;}
        if(busy_ || editorPreview_)return;
        choose("Open NativeDAW session",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,"*.ndaw",[this](fs::path path){
            try{std::error_code pathError;if(fs::equivalent(path,file_,pathError)){note_="Session already open | Current edits retained";return;}
                auto session=loadSession(path);engine_.stop();cancellation_.cancelled.store(true);commands_=std::make_shared<Commands>(session,path.parent_path());file_=path;track_.clear();clip_.clear();selection_={};sourceSelection_=nullptr;cursor_=selectionEndFrame_=0;refresh();
                auto error=engine_.openDevice(session.at("sample_rate"),256); note_=error.empty()?"Session reopened":error;}
            catch(const std::exception& e){note_=e.what();}
        });
    }
    void save(){auto commands=commands_;auto file=file_;task("Saving...",[commands,file]{commands->save(file);return "Saved | "+file.string();},false);}
    void import() {
        choose("Import audio (session sample rate)",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,"*.wav;*.aiff;*.aif;*.flac",[this](fs::path path){
            Json operations=Json::array(); auto track=track_;
            const auto snapshot=commands_->query();for(const auto& t:snapshot.at("tracks"))if(t.at("id")==track && t.at("kind")!="audio")track.clear();
            if(track.empty()){track=uuid();operations.push_back({{"command","add_audio_track"},{"id",track},{"name",path.stem().string()}});}
            operations.push_back({{"command","import_audio"},{"track_id",track},{"path",path.string()},{"position",cursor_}});track_=track;apply(operations);
        });
    }
    void stopPlayback(){if(editorPreview_){note_="Audio is paused for the plugin preview; review or cancel it first";return;}engine_.cancelPreparation();engine_.stop();if(engine_.metrics().at("routing_requires_stop").get<bool>())refresh();}
    void play(){if(busy_ || editorPreview_){note_="Finish the current operation before playback";return;}if(engine_.recordState()!=RecordState::Idle){record();return;}if(engine_.state()==PlaybackState::Playing || engine_.state()==PlaybackState::Priming)stopPlayback();else engine_.play(cursor_);}
    void split(){if(!selection_.clips.empty()){auto op=selection_.operation("split");op["position"]=cursor_;apply(Json::array({op}),displaySession_.at("revision"));}}
    void removeClip(){if(!selection_.clips.empty())apply(Json::array({selection_.operation("delete")}),displaySession_.at("revision"));}
    void exportAudio() {
        choose("Export new WAV/BWF",juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,"*.wav",[this](fs::path path){
            if(path.extension().empty())path+=".wav";auto session=commands_->query();auto root=commands_->root();
            task("Rendering and checking file...",[session,root,path]{auto result=renderToFile(session,root,path,0,sessionLength(session));return "Export verified | "+result.at("path").get<std::string>();},false);
        });
    }
    void configureAudio() {if(busy_ || editorPreview_ || engine_.recordState()!=RecordState::Idle){note_="Finish the operation and stop recording before changing devices";return;}
        try {
#if defined(__APPLE__)
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        auto* selector=new NativeAudioConfig(deviceInventory(),commands_->query().at("sample_rate"),engine_.metrics().at("native_device"),[safe](DeviceSetup setup){
            if(!safe)return;
            auto apply=[safe,setup]{if(!safe || safe->busy_ || safe->engine_.recordState()!=RecordState::Idle)return;
                safe->task("Opening selected native audio devices...",[safe,setup]{auto error=safe->engine_.configureDevice(setup);if(!error.empty())throw Error("device",error);return std::string("Native audio device configured");},false);};
            if(!setup.inputs.empty() && microphonePermission()==MicrophonePermission::NotDetermined){requestMicrophonePermission([safe,apply](bool allowed){juce::MessageManager::callAsync([safe,apply,allowed]{if(allowed)apply();else if(safe)safe->note_="Microphone access denied; input device was not opened";});});}
            else apply();
        });
#else
        auto* selector=new juce::AudioDeviceSelectorComponent(engine_.devices(),0,INT_MAX,0,2,false,false,true,false);
#endif
        selector->setSize(650,480); juce::DialogWindow::LaunchOptions options;options.content.setOwned(selector);options.dialogTitle="Audio devices";
        options.dialogBackgroundColour=juce::Colour(0xff202630);options.useNativeTitleBar=true;options.resizable=true;options.launchAsync();
        }catch(const std::exception& e){note_=std::string("Audio device discovery failed: ")+e.what();}
    }
    void selectInput() {
        if(track_.empty() || busy_)return;auto* device=engine_.currentDevice();
        if(!device || device->getInputChannelNames().isEmpty()){note_="Choose an input device in Audio devices first";configureAudio();return;}
        auto names=device->getInputChannelNames();juce::PopupMenu menu;
        for(int i=0;i<names.size();++i)menu.addItem(i+1,"Mono "+juce::String(i+1)+" | "+names[i]);
        menu.addSeparator();for(int i=0;i+1<names.size();++i)menu.addItem(names.size()+i+1,"Stereo "+juce::String(i+1)+"/"+juce::String(i+2)+" | "+names[i]+" / "+names[i+1]);
        auto safe=juce::Component::SafePointer<MainComponent>(this);auto id=track_;const auto count=names.size();
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&inputButton_),[safe,id,count](int result){if(safe && result>0){Json channels=result<=count?Json::array({result-1}):Json::array({result-count-1,result-count});
            safe->apply(Json::array({{{"command","set_track_input"},{"track_id",id},{"input_channels",channels}}}));}});
    }
    void record() {
        if(busy_ || editorPreview_){note_="Finish the current operation first";return;}
        cancellation_.cancelled.store(true);
        if(engine_.recordState()!=RecordState::Idle) {
            auto commands=commands_;task("Finalizing captured files...",[this,commands]{auto result=finishSessionCapture(*commands,engine_);commands->save(file_);
                return "Recorded "+std::to_string(result.at("capture").at("files").size())+" tracks | "+std::to_string(result.at("capture").at("frames").get<Frame>())+" frames | Undo removes clips, retains audio";});
            return;
        }
        auto snapshot=commands_->query();bool armed=false;for(const auto& t:snapshot.at("tracks"))armed|=t.at("record_armed").get<bool>();
        if(!armed){note_="Add/select an audio track, choose its input and enable Record arm";return;}
        auto permission=microphonePermission();
        if(permission==MicrophonePermission::NotDetermined) {
            auto safe=juce::Component::SafePointer<MainComponent>(this);note_="Microphone permission is required for recording";
            requestMicrophonePermission([safe](bool allowed){juce::MessageManager::callAsync([safe,allowed]{if(safe){if(allowed)safe->record();else safe->note_="Microphone access denied; no recording started";}});});return;
        }
        if(permission==MicrophonePermission::Denied){note_="Enable NativeDAW microphone access in macOS Privacy & Security";return;}
        auto* device=engine_.currentDevice();
        if(!device || device->getActiveInputChannels().isZero()){note_="Enable the selected inputs in Audio devices before recording";configureAudio();return;}
        const auto settings=recordSettings(snapshot);auto commands=commands_;auto at=settings.loop || settings.punch?settings.captureBegin():cursor_;
        task("Preparing armed tracks...",[this,commands,at,loop=settings.loop,punch=settings.punch]{startSessionCapture(*commands,engine_,at);return punch?"Selection Punch | continuous handles retained | automatic stop after post-roll | physical placement is uncalibrated":loop?"Loop recording | continuous disk capture | Stop creates take Playlists | physical placement is uncalibrated":"Recording armed tracks | Stop/Record finalizes | physical placement is uncalibrated";},false);
    }
    static std::string previewText(const Plan& plan) {
        std::string text="Changes at session revision "+std::to_string(plan.base)+":\n\n";
        for(const auto& op:plan.operations) {
            auto name=op.at("command").get<std::string>();std::string target;
            for(const auto* state:{&plan.before,&plan.after})for(const auto& t:state->at("tracks")) {
                if(op.contains("track_id") && t.at("id")==op.at("track_id"))target=t.at("name");
                if(op.contains("id") && t.at("id")==op.at("id"))target=t.at("name");
                for(const auto& c:activeClips(t))if(op.contains("clip_id") && c.at("id")==op.at("clip_id"))target=c.at("name");
            }
            for(const char* key:{"affected_track_ids","resolved_clip_ids"})if(op.contains(key)){text+="Affected objects: ";for(const auto& id:op.at(key)){std::string display=id.get<std::string>();for(const auto& t:plan.before.at("tracks")){if(t.at("id")==id)display=t.at("name");for(const auto& c:activeClips(t))if(c.at("id")==id)display=t.at("name").get<std::string>()+" / "+c.at("name").get<std::string>();}text+=display+"; ";}text+="\n";}
            if(name=="edit_clip_selection")text+="Selection action: "+op.at("action").get<std::string>()+"\n";
            else if(name=="move_clip")text+="Move “"+target+"” to "+std::to_string(op.at("position").get<Frame>())+" samples\n";
            else if(name.find("gain")!=std::string::npos)text+="Set “"+target+"” gain to "+std::to_string(op.at("gain_db").get<double>())+" dB\n";
            else if(name=="add_audio_track")text+="Add audio track “"+op.at("name").get<std::string>()+"”\n";
            else if(name=="set_track_pan")text+="Set “"+target+"” pan/balance to "+std::to_string(op.at("pan").get<double>())+"\n";
            else if(name=="set_track_mute")text+=(op.at("muted").get<bool>()?"Mute “":"Unmute “")+target+"”\n";
            else text+=name+" | “"+target+"”\n";
        }
        return text+"\nCanonical operations and parameter values:\n"+plan.operations.dump(2)+"\n\nActual state differences:\n"+plan.diff.dump(2)+"\n\nAccept commits one session transaction. Audio application is tracked separately. Original media is retained. Undo is available.";
    }
    void runAI() {
        if(busy_ || editorPreview_){note_="Finish the plugin preview/operation before starting AI";return;}
        if(aiBusy_)return;if(aiThread_.joinable())aiThread_.join();aiBusy_=true;cancellation_.cancelled.store(false);aiPlan_.reset();
        cancellation_.audioPriority=[this]{return engine_.state()==PlaybackState::Playing || engine_.state()==PlaybackState::Priming || engine_.recordState()==RecordState::Recording;};
        auto commands=commands_;auto config=ProviderConfig::environment();config.model=model_.getText().toStdString();
        auto intent=intent_.getText().toStdString();auto permission=permissions_.getSelectedId()==1?Permission::ReadOnly:Permission::Preview;
        aiScope_={};if(permissions_.getSelectedId()==3){aiScope_.mode=Permission::ScopedLowRisk;if(!clip_.empty())aiScope_.targets.insert(clip_);if(!track_.empty())aiScope_.targets.insert(track_);}
        aiOutput_.setText("Querying local model capabilities and pinned session facts...");
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        aiThread_=std::thread([this,safe,commands,config,intent,permission]{
            std::optional<AITaskResult> result;std::string error;
            try{AIPlanner planner(config);result=planner.plan(*commands,intent,permission,cancellation_);}catch(const std::exception& e){error=e.what();}
            juce::MessageManager::callAsync([safe,commands,result=std::move(result),error]{
                if(!safe)return;safe->aiBusy_=false;
                if(safe->commands_!=commands){safe->aiOutput_.setText("Task cancelled: a different session is now open.");return;}
                if(safe->cancellation_.cancelled.load()){safe->aiOutput_.setText("Task cancelled. No AI edit was committed.");return;}
                if(!error.empty()){safe->aiOutput_.setText("Task failed: "+juce::String(error));return;}
                if(result->plan){safe->aiPlan_=result->plan;safe->aiOutput_.setText(previewText(*result->plan));if(safe->aiScope_.mode==Permission::ScopedLowRisk)safe->acceptAI(false);}
                else safe->aiOutput_.setText(juce::String(result->answer)+"\n\nRead-only response; no edit was committed.");
            });
        });
    }
    void acceptAI(bool human=true) {
        if(!aiPlan_)return;if(busy_){note_="Finish the current file operation before accepting changes";return;}
        auto plan=*aiPlan_;auto commands=commands_;auto scope=aiScope_;
        if(human)commands->approve(plan);
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        task("Committing AI transaction...",[safe,commands,plan,scope]{
            try {
                if(safe && safe->cancellation_.cancelled.load())throw Error("cancelled","AI commit cancelled before execution");
                if(safe)for(const auto& op:plan.operations)if((op.at("command")=="select_playlist" || op.at("command")=="select_comp_set" || op.at("command")=="restore_comp_set" || op.at("command")=="move_comp_set" || (op.at("command")=="edit_clip_selection" && op.at("action")!="gain" && op.at("action")!="fades") || (op.contains("resolved_clip_ids") && (op.at("command")=="move_clip" || op.at("command")=="trim_clip" || op.at("command")=="split_clip" || op.at("command")=="delete_clip"))))safe->engine_.stop();
                auto receipt=commands->commit(plan,scope);
                juce::MessageManager::callAsync([safe,commands,plan,receipt]{if(safe && safe->commands_==commands && safe->aiPlan_ && safe->aiPlan_->id==plan.id){safe->aiPlan_.reset();safe->aiOutput_.setText("Session transaction committed:\n"+receipt.dump(2)+"\n\nAudio application and failures are shown in the transport status. Undo remains available.");}});
                return std::string("AI session transaction committed | Undo available");
            } catch(const Error& error) {
                const auto explanation=std::string(error.what());
                juce::MessageManager::callAsync([safe,commands,plan,explanation]{if(safe && safe->commands_==commands && safe->aiPlan_ && safe->aiPlan_->id==plan.id)safe->aiOutput_.setText("No AI transaction committed: "+explanation+"\n\n"+previewText(plan));});throw;
            }
        });
    }
    void timerCallback() override {
        if(!busy_ && !editorPreview_ && engine_.recordState()==RecordState::Finishing)record();
        if(auto* window=findParentComponentOfClass<juce::DocumentWindow>()){
            const auto title=juce::String("NativeDAW | ")+juce::String(displaySession_.at("name").get<std::string>());
            if(window->getName()!=title)window->setName(title);
        }
        if(editorPreview_ && !busy_){const auto editor=engine_.pluginEditorStatus();if(editor.at("active")==true && (editor.at("decision").get<int>()!=0 || editor.at("failed")==true))resolveNativePluginEditor(editor.at("decision")==1 && editor.at("failed")==false);}
        auto metrics=engine_.metrics();const auto& session=displaySession_;
        juce::String text=juce::String(session.at("name").get<std::string>())+"  |  "+juce::String(session.at("sample_rate").get<int>())+" Hz | revision "+juce::String(session.at("revision").get<Frame>())+
            "  |  Position "+juce::String(static_cast<double>(engine_.presentationPosition())/session.at("sample_rate").get<double>(),3)+" s  |  Peak "+juce::String(metrics.at("output_peak").get<double>(),4)+
            "  |  Device "+juce::String(metrics.at("device").get<std::string>())+"\n"+juce::String(note_);
        if(engine_.state()==PlaybackState::Failed)text+=" | PLAYBACK FAILED / underrun or device/media error";
        if(metrics.at("error")!="")text+=" | "+juce::String(metrics.at("error").get<std::string>());
        if(engine_.state()==PlaybackState::Playing && metrics.at("audible_session_revision")!=session.at("revision"))
            text+=" | Audio update pending (playing revision "+juce::String(metrics.at("audible_session_revision").get<Frame>())+")";
        if(metrics.at("record_state")==static_cast<int>(RecordState::Failed))text+=" | RECORDING FAILED / partial media retained";
        if(metrics.at("monitor_input_unavailable").get<bool>())text+=" | MONITOR INPUT UNAVAILABLE / invalid input";
        if(engine_.recordState()==RecordState::Recording)text+=" | Captured "+juce::String(metrics.at("captured_frames").get<Frame>())+" / written "+juce::String(metrics.at("written_frames").get<Frame>());
        juce::String critical;
        if(metrics.at("plugin_state_capture_suspended").get<bool>())critical+="PLUGIN PREVIEW / audio paused  |  ";
        if(metrics.at("record_state")==static_cast<int>(RecordState::Failed))critical+="RECORDING FAILED / partial media retained  |  ";
        if(engine_.state()==PlaybackState::Failed)critical+="PLAYBACK FAILED  |  ";
        if(metrics.at("monitor_input_unavailable").get<bool>())critical+="MONITOR INPUT UNAVAILABLE  |  ";
        if(metrics.at("error")!="")critical+=juce::String(metrics.at("error").get<std::string>())+"  |  ";
        if(engine_.state()==PlaybackState::Playing && metrics.at("audible_session_revision")!=session.at("revision"))critical+="Audio update pending / playing r"+juce::String(metrics.at("audible_session_revision").get<Frame>())+"  |  ";
        if(engine_.recordState()==RecordState::Recording)critical+="Recording / captured "+juce::String(metrics.at("captured_frames").get<Frame>())+" / written "+juce::String(metrics.at("written_frames").get<Frame>())+"  |  ";
        const auto settings=recordSettings(session);auto& recordModeButton=button("Record mode");recordModeButton.setButtonText(settings.punch?"Selection Punch":settings.loop?"Loop recording":"Normal recording");
        recordModeButton.setTooltip(settings.punch?"Punch samples "+juce::String(settings.begin)+" to "+juce::String(settings.end)+" | pre-roll "+juce::String(settings.preRoll)+" / post-roll "+juce::String(settings.postRoll)+" samples | click to configure":settings.loop?"Loop samples "+juce::String(settings.begin)+" to "+juce::String(settings.end)+" | click to configure":"Click to configure normal, loop or selection Punch recording");
        recordModeButton.setEnabled(!busy_ && !editorPreview_ && engine_.recordState()==RecordState::Idle);
        if(engine_.recordState()==RecordState::Recording && settings.loop)critical+="Loop pass "+juce::String(metrics.at("capture").at("completed_loop_passes").get<Frame>()+1)+"  |  ";
        if(engine_.recordState()==RecordState::Recording && settings.punch)critical+="Punch capture / "+juce::String(metrics.at("capture").at("punch_phase").get<std::string>())+"  |  ";
        counter_.activity=engine_.recordState()==RecordState::Finishing?"FINALIZING CAPTURE":engine_.recordState()==RecordState::Recording && settings.punch?"PUNCH  /  "+juce::String(metrics.at("capture").at("punch_phase").get<std::string>()).toUpperCase():juce::String{};
        status_.setColour(juce::Label::textColourId,metrics.at("record_state")==static_cast<int>(RecordState::Failed) || engine_.state()==PlaybackState::Failed?juce::Colour(0xffef9890):juce::Colour(0xffb9c9dc));
        status_.setText(critical+juce::String(note_)+"  |  "+juce::String(session.at("sample_rate").get<int>())+" Hz  |  r"+juce::String(session.at("revision").get<Frame>())+"  |  Peak "+juce::String(metrics.at("output_peak").get<double>(),4),juce::dontSendNotification);status_.setTooltip(text);timeline_.repaint();headers_.measured(metrics);mixer_.measured(metrics);counter_.position=engine_.state()==PlaybackState::Playing || engine_.state()==PlaybackState::Priming?engine_.presentationPosition():cursor_;counter_.rate=session.at("sample_rate");counter_.sessionName=session.at("name").get<std::string>();counter_.device=metrics.at("device").get<std::string>();counter_.playing=engine_.state()==PlaybackState::Playing;counter_.recording=engine_.recordState()==RecordState::Recording;counter_.setDescription(clock(counter_.position,counter_.rate)+"; position "+juce::String(static_cast<juce::int64>(counter_.position))+" samples; "+(counter_.recording?"recording":counter_.playing?"playing":"stopped"));counter_.repaint();button("Play").setToggleState(counter_.playing,juce::dontSendNotification);button("Record").setToggleState(counter_.recording,juce::dontSendNotification);if(routingWindow_ && routingWindow_->isVisible())routingWindow_->editor().measured(metrics);
        aiRun_.setEnabled(!aiBusy_ && !busy_);aiAccept_.setEnabled(aiPlan_.has_value() && !busy_);
    }
    WorkstationLook look_;AudioEngine engine_;Timeline timeline_;TimelineRuler ruler_;ChannelBank headers_,mixer_;SessionList tracks_,clips_;NativeGroupList groups_;SyncedViewport viewport_,headerViewport_;juce::Viewport mixViewport_;CounterPanel counter_;juce::MenuBarComponent menu_;juce::TextEditor selectionStart_,selectionEnd_,selectionLength_;juce::ComboBox grid_,timeFormat_;juce::Label workspaceTitle_,selectionTitle_;std::vector<std::unique_ptr<juce::TextButton>> buttons_;
    juce::Label status_,gainLabel_,panLabel_,clipLabel_,aiTitle_;juce::Slider gain_,pan_,clipGain_;juce::ToggleButton mute_,lock_,arm_;juce::ComboBox monitor_;juce::TextButton inputButton_;
    juce::TextEditor intent_,model_,aiOutput_;juce::ComboBox permissions_;juce::TextButton aiRun_,aiCancel_,aiAccept_,aiReject_;
    std::shared_ptr<Commands> commands_;Json displaySession_,sourceSelection_;EditSelection selection_;fs::path file_;std::string track_,clip_,note_,clipboardClip_,clipboardSession_;Frame cursor_=0,selectionEndFrame_=0,recordAt_=0;int sidebarWidth_=168,inspectorY_=0;bool mixVisible_=false,clipsVisible_=true;
    std::unique_ptr<juce::FileChooser> chooser_;std::thread work_,aiThread_;bool busy_=false,aiBusy_=false,aiVisible_=false,sliderGesture_=false,editorPreview_=false;
    std::unique_ptr<NativeGroupWindow> groupWindow_;std::unique_ptr<NativeRoutingWindow> routingWindow_;std::unique_ptr<NativePlaylistWindow> playlistWindow_;std::unique_ptr<NativeRecordingWindow> recordWindow_;
    std::unique_ptr<NativePluginWindow> pluginWindow_;
    Cancellation cancellation_;std::optional<Plan> aiPlan_;Scope aiScope_;
};
class NativeDAWApp final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override{return "NativeDAW";} const juce::String getApplicationVersion() override{return "0.1.0";}
    void initialise(const juce::String& args) override {
        fs::path initial;auto tokens=juce::StringArray::fromTokens(args,true);
        for(int i=0;i+1<tokens.size();++i)if(tokens[i]=="--session")initial=tokens[i+1].unquoted().toStdString();
        window_=std::make_unique<Window>(initial);
    }
    void shutdown() override{window_.reset();}
private:
    class Window final : public juce::DocumentWindow {
    public:
        explicit Window(const fs::path& initial):DocumentWindow("NativeDAW",juce::Colour(0xff10151e),allButtons){
            setUsingNativeTitleBar(true);setContentOwned(new MainComponent(initial),true);setResizable(true,true);setResizeLimits(1320,740,7680,4320);centreWithSize(1440,900);setVisible(true);
        }
        void closeButtonPressed() override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
    };
    std::unique_ptr<Window> window_;
};
START_JUCE_APPLICATION(NativeDAWApp)
