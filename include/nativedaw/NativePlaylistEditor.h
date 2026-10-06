// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Audio.h"
#include <juce_gui_extra/juce_gui_extra.h>
namespace ndaw {
class NativePlaylistEditor final : public juce::Component,private juce::ListBoxModel,private juce::ChangeListener {
public:
    explicit NativePlaylistEditor(std::function<void(Json)> submit):submit_(std::move(submit)),cache_(32) {
        formats_.registerBasicFormats();setSize(1040,760);setTitle("Playlists and candidate Comp");lanes_.setModel(this);lanes_.setRowHeight(112);
        for(auto* c:std::vector<juce::Component*>{&tracks_,&lanes_,&title_,&facts_,&range_,&boundary_,&name_,&begin_,&end_,&fade_,&create_,&duplicate_,&rename_,&use_,&target_,&take_,&copy_,&comp_,&audition_,&stop_,&lock_,&stage_,&linked_,&clear_,&restore_,&curve_,&plan_})addAndMakeVisible(*c);
        name_.setTitle("Playlist name");name_.setText("Candidate Comp");begin_.setTitle("Selection start samples");end_.setTitle("Selection end samples");fade_.setTitle("Comp fade length samples");fade_.setTooltip("Single-track candidate: edge fade length. Linked Comp: centered crossfade length, including real source handles.");fade_.setText("0");
        range_.setText("Source selection (samples)       Start                         End                         Fade",juce::dontSendNotification);
        boundary_.setText("Playback and target Playlists are independent. Copies preserve source clips and media. Audition changes the playback Playlist with Undo. Stop to switch; finish recording first.",juce::dontSendNotification);
        curve_.addItem("Equal gain (linear)",1);curve_.addItem("Equal power",2);curve_.setSelectedId(1);curve_.setTitle("Linked Comp crossfade curve");
        curve_.setTooltip("Equal power can raise correlated signal levels; verify the actual mix.");
        stage_.onClick=[this]{stageSelection(trackId_,selectedPlaylist().at("id"),clipId_,value(begin_),value(end_));};
        linked_.onClick=[this]{if(submit_)submit_(Json::array({linkedPlan()}));};clear_.onClick=[this]{staged_.clear();refreshPlan();};
        restore_.onClick=[this]{const auto& p=selectedPlaylist();if(!p.contains("comp_set_id"))throw Error("comp_members","Select a linked candidate to restore its prior playback Playlists");if(submit_)submit_(Json::array({{{"command","restore_comp_set"},{"comp_set_id",p.at("comp_set_id")}}}));};
        refreshPlan();
        tracks_.onChange=[this]{if(!updating_ && tracks_.getSelectedId()>0){trackId_=trackIds_.at(tracks_.getSelectedId()-1);selected_=0;if(selectTrack)selectTrack(trackId_);rebuild();}};
        create_.onClick=[this]{send({{"command","create_playlist"},{"name",name_.getText().toStdString()}});};
        duplicate_.onClick=[this]{send({{"command","create_playlist"},{"name",name_.getText().toStdString()},{"source_playlist_id",selectedPlaylist().at("id")}});};
        rename_.onClick=[this]{send({{"command","rename_playlist"},{"playlist_id",selectedPlaylist().at("id")},{"name",name_.getText().toStdString()}});};
        use_.onClick=[this]{send({{"command","select_playlist"},{"playlist_id",selectedPlaylist().at("id")}});};
        target_.onClick=[this]{send({{"command","set_target_playlist"},{"playlist_id",selectedPlaylist().at("id")}});};
        lock_.onClick=[this]{send({{"command","set_playlist_lock"},{"playlist_id",selectedPlaylist().at("id")},{"locked",!selectedPlaylist().at("locked").get<bool>()}});};
        take_.onClick=[this]{if(!clipId_.empty())send({{"command","create_take"},{"playlist_id",selectedPlaylist().at("id")},{"clip_id",clipId_},{"name",name_.getText().toStdString()}});};
        copy_.onClick=[this]{auto op=selection();op["command"]="copy_range_to_playlist";op["playlist_id"]=track().at("target_playlist_id");send(op);};
        comp_.onClick=[this]{send({{"command","create_comp_playlist"},{"name",name_.getText().toStdString()},{"segments",Json::array({selection()})}});};
        audition_.onClick=[this]{if(audition)audition(trackId_,selectedPlaylist().at("id"),value(begin_),value(end_));};stop_.onClick=[this]{if(stop)stop();};
        for(auto* button:{&create_,&duplicate_,&rename_,&use_,&target_,&take_,&copy_,&comp_,&audition_,&stop_,&lock_,&stage_,&linked_,&clear_,&restore_}){auto action=button->onClick;button->onClick=[this,action]{try{action();}catch(const std::exception& e){facts_.setText(std::string("Not executed: ")+e.what(),juce::dontSendNotification);}};}
    }
    ~NativePlaylistEditor() override {lanes_.setModel(nullptr);for(auto& [id,t]:thumbnails_)t->removeChangeListener(this);}
    std::function<void(std::string,std::string,Frame,Frame)> audition;
    std::function<void()> stop;
    std::function<void(std::string)> selectTrack;
    void update(const Json& session,const fs::path& root,const std::string& selectedTrack,Frame begin,Frame end) {
        if(!session_.is_null() && session_.at("id")!=session.at("id")){staged_.clear();first_=true;refreshPlan();}
        updating_=true;session_=session;root_=root;trackIds_.clear();tracks_.clear();
        if(!selectedTrack.empty() && selectedTrack!=trackId_){trackId_=selectedTrack;selected_=0;clipId_.clear();}
        for(const auto& t:session_.at("tracks"))if(t.at("kind")=="audio"){trackIds_.push_back(t.at("id"));tracks_.addItem(t.at("name").get<std::string>(),static_cast<int>(trackIds_.size()));}
        auto found=std::find(trackIds_.begin(),trackIds_.end(),trackId_);if(found==trackIds_.end() && !trackIds_.empty()){trackId_=trackIds_[0];found=trackIds_.begin();}
        tracks_.setSelectedId(found==trackIds_.end()?0:static_cast<int>(found-trackIds_.begin())+1,juce::dontSendNotification);
        for(const auto& source:session_.at("sources")) {auto key=root_.string()+":"+source.at("id").get<std::string>();if(!thumbnails_.contains(key)){
            auto thumbnail=std::make_unique<juce::AudioThumbnail>(256,formats_,cache_);thumbnail->addChangeListener(this);auto path=mediaPath(root_,source);
            if(fs::is_regular_file(path))thumbnail->setSource(new juce::FileInputSource(juce::File(juce::String(path.string()))));thumbnails_[key]=std::move(thumbnail);}}
        if(first_){begin_.setText(juce::String(static_cast<juce::int64>(begin)));end_.setText(juce::String(static_cast<juce::int64>(end)));first_=false;}
        updating_=false;rebuild();
    }
    void stageSelection(const std::string& tid,const std::string& pid,const std::string& cid,Frame a,Frame b) {
        if(staged_.empty())stagedRevision_=session_.at("revision");else if(stagedRevision_!=session_.at("revision"))throw Error("version_conflict","Project changed after Comp staging; clear the plan and choose current sources");
        if(staged_.size()>=64ull*1024*1024/4096)throw Error("audio_resources","Staged Comp exceeds the64 MiB plan estimate");
        const Json* t=nullptr;for(const auto& item:session_.at("tracks"))if(item.at("id")==tid)t=&item;
        if(!t || cid.empty() || a<0 || a>=b)throw Error("comp_range","Choose an actual clip and a nonempty shared sample range");
        const auto& p=playlist(*t,pid);bool found=false;for(const auto& c:p.at("clips"))if(c.at("id")==cid && a>=c.at("start").get<Frame>() && b<=c.at("start").get<Frame>()+c.at("length").get<Frame>())found=true;
        if(!found)throw Error("comp_range","The staged range must belong to the selected source clip");
        Json item{{"track_id",tid},{"source_playlist_id",pid},{"clip_id",cid},{"begin",a},{"end",b}};
        for(auto& row:staged_)if(row.at("track_id")==tid && row.at("begin")==a && row.at("end")==b){row=item;refreshPlan();return;}
        staged_.push_back(std::move(item));refreshPlan();
    }
    Json linkedPlan() const {
        if(stagedRevision_!=session_.at("revision"))throw Error("version_conflict","Project changed after Comp staging; clear the plan and choose current sources");
        std::map<std::pair<Frame,Frame>,std::map<std::string,Json>> rows;std::set<std::string> members;
        for(const auto& item:staged_){std::string tid=item.at("track_id");members.insert(tid);rows[{item.at("begin"),item.at("end")}][tid]={{"source_playlist_id",item.at("source_playlist_id")},{"clip_id",item.at("clip_id")}};}
        if(members.size()<2 || rows.empty())throw Error("comp_members","Stage at least two tracks with explicit sources for the same ranges");
        Json ranges=Json::array(),tracks=Json::array();for(const auto& [range,sources]:rows){if(sources.size()!=members.size())throw Error("comp_members","Every staged range needs an explicit source for every member track");ranges.push_back({{"begin",range.first},{"end",range.second}});}
        for(const auto& tid:members){Json segments=Json::array();for(const auto& [range,sources]:rows)segments.push_back(sources.at(tid));tracks.push_back({{"track_id",tid},{"segments",segments}});}
        return {{"command","create_linked_comp"},{"source_revision",stagedRevision_},{"name",name_.getText().toStdString()},{"ranges",ranges},{"members",tracks},{"crossfade_frames",value(fade_)},{"curve",curve_.getSelectedId()==2?"equal_power":"linear"}};
    }
    void resized() override {
        auto w=getWidth();title_.setBounds(18,8,w-36,26);tracks_.setBounds(18,40,300,28);facts_.setBounds(334,40,w-352,32);
        lanes_.setBounds(18,86,w-36,getHeight()-390);range_.setBounds(18,getHeight()-194,w-36,22);
        begin_.setBounds(210,getHeight()-167,165,28);end_.setBounds(388,getHeight()-167,165,28);fade_.setBounds(568,getHeight()-167,110,28);name_.setBounds(18,getHeight()-167,180,28);
        juce::TextButton* row1[]{&create_,&duplicate_,&rename_,&use_,&target_,&lock_};for(int i=0;i<6;++i)row1[i]->setBounds(18+i*(w-36)/6,getHeight()-127,(w-48)/6,30);
        juce::TextButton* row2[]{&take_,&copy_,&comp_,&audition_,&stop_};for(int i=0;i<5;++i)row2[i]->setBounds(18+i*(w-36)/5,getHeight()-89,(w-48)/5,30);
        curve_.setBounds(18,getHeight()-260,220,28);juce::TextButton* row3[]{&stage_,&linked_,&clear_,&restore_};for(int i=0;i<4;++i)row3[i]->setBounds(250+i*(w-270)/4,getHeight()-260,(w-286)/4,28);
        plan_.setBounds(18,getHeight()-226,w-36,28);
        boundary_.setBounds(18,getHeight()-48,w-36,40);
    }
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff161b22));}
private:
    void refreshPlan() {
        juce::String text="Linked plan: ";for(const auto& item:staged_){juce::String name="Unavailable source";for(const auto& t:session_.is_null()?Json::array():session_.at("tracks"))if(t.at("id")==item.at("track_id")){name=t.at("name").get<std::string>();for(const auto& p:t.at("playlists"))if(p.at("id")==item.at("source_playlist_id")){name+=" / "+juce::String(p.at("name").get<std::string>());for(const auto& c:p.at("clips"))if(c.at("id")==item.at("clip_id"))name+=" / "+juce::String(c.at("name").get<std::string>());}}text+=name+" "+juce::String(static_cast<juce::int64>(item.at("begin").get<Frame>()))+"–"+juce::String(static_cast<juce::int64>(item.at("end").get<Frame>()))+"; ";}
        if(staged_.empty())text+="choose sources on each track, then create the inactive candidate";plan_.setText(text,juce::dontSendNotification);plan_.setTooltip(text);
    }
    const Json& track() const {for(const auto& t:session_.at("tracks"))if(t.at("id")==trackId_)return t;throw Error("unknown_object","Select an audio track");}
    const Json& selectedPlaylist() const {return track().at("playlists").at(static_cast<std::size_t>(selected_));}
    Frame value(const juce::TextEditor& e) const {const auto text=e.getText();if(text.isEmpty() || text.containsAnyOf(".-+eE") || !text.containsOnly("0123456789"))throw Error("units","Enter nonnegative integer sample positions");auto result=text.getLargeIntValue();if(result<0 || result>INT64_MAX/4)throw Error("range","Sample range is too large");return result;}
    Json selection() const {if(clipId_.empty())throw Error("comp_range","Select a source clip in a Playlist lane");return {{"source_playlist_id",selectedPlaylist().at("id")},{"clip_id",clipId_},{"begin",value(begin_)},{"end",value(end_)},{"fade_in",value(fade_)},{"fade_out",value(fade_)}};}
    void send(Json operation){operation["track_id"]=trackId_;if(submit_)submit_(Json::array({operation}));}
    void rebuild() {
        const bool hasTrack=std::find(trackIds_.begin(),trackIds_.end(),trackId_)!=trackIds_.end();
        for(auto* b:{&duplicate_,&rename_,&use_,&target_,&take_,&copy_,&comp_,&audition_,&lock_})b->setEnabled(hasTrack);create_.setEnabled(hasTrack);clipId_.clear();
        if(hasTrack){selected_=std::clamp(selected_,0,static_cast<int>(track().at("playlists").size())-1);const auto& p=selectedPlaylist();if(!p.at("clips").empty())clipId_=p.at("clips")[0].at("id");
            title_.setText(track().at("name").get<std::string>()+" | Playlists",juce::dontSendNotification);facts_.setText("Playback: "+playlist(track(),track().at("active_playlist_id")).at("name").get<std::string>()+"    Target: "+playlist(track(),track().at("target_playlist_id")).at("name").get<std::string>(),juce::dontSendNotification);
            lock_.setButtonText(p.at("locked").get<bool>()?"Unlock Playlist":"Lock Playlist");copy_.setEnabled(!clipId_.empty() && p.at("id")!=track().at("target_playlist_id"));comp_.setEnabled(!clipId_.empty());take_.setEnabled(!clipId_.empty());}
        else {title_.setText("Playlists | create an audio track first",juce::dontSendNotification);facts_.setText("",juce::dontSendNotification);}
        lanes_.updateContent();lanes_.selectRow(hasTrack?selected_:-1);lanes_.repaint();
    }
    int getNumRows() override {if(session_.is_null() || trackIds_.empty())return 0;return static_cast<int>(track().at("playlists").size());}
    juce::String getNameForRow(int row) override {const auto& p=track().at("playlists").at(static_cast<std::size_t>(row));return p.at("name").get<std::string>()+(p.at("id")==track().at("active_playlist_id")?" | playback":"")+(p.at("id")==track().at("target_playlist_id")?" | target":"");}
    Frame extent() const {Frame length=session_.at("sample_rate").get<int>()*5;for(const auto& p:track().at("playlists"))for(const auto& c:p.at("clips"))length=std::max(length,c.at("start").get<Frame>()+c.at("length").get<Frame>());return length;}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected) override {
        const auto& p=track().at("playlists").at(static_cast<std::size_t>(row));g.fillAll(selected?juce::Colour(0xff273746):juce::Colour(0xff20252d));g.setColour(juce::Colour(0xff718799));g.drawHorizontalLine(height-1,0,static_cast<float>(width));
        juce::String labels=p.at("name").get<std::string>();if(p.at("id")==track().at("active_playlist_id"))labels+=" [PLAY]";if(p.at("id")==track().at("target_playlist_id"))labels+=" [TARGET]";
        if(p.at("locked").get<bool>())labels+=" [LOCK]";g.setColour(juce::Colours::white);g.setFont(13);g.drawFittedText(labels,10,5,200,42,juce::Justification::topLeft,2);
        g.setColour(juce::Colour(0xffa4b6c5));g.setFont(11);g.drawText(juce::String(p.at("clips").size())+" clips",10,53,200,18,juce::Justification::left);
        const double scale=double(width-222)/extent(),rate=session_.at("sample_rate");
        for(const auto& clip:p.at("clips")) {auto bounds=juce::Rectangle<int>(218+static_cast<int>(clip.at("start").get<Frame>()*scale),6,std::max(2,static_cast<int>(clip.at("length").get<Frame>()*scale)),height-14);
            g.setColour(p.at("role")=="comp"?juce::Colour(0xff588f71):juce::Colour(0xff3b6c8a));g.fillRect(bounds);g.setColour(juce::Colour(0xffc5dae5));g.drawText(clip.at("name").get<std::string>(),bounds.removeFromTop(20),juce::Justification::left);
            auto key=root_.string()+":"+clip.at("source_id").get<std::string>();auto found=thumbnails_.find(key);if(found!=thumbnails_.end() && found->second->getTotalLength()>0)found->second->drawChannels(g,bounds,clip.at("source_start").get<Frame>()/rate,(clip.at("source_start").get<Frame>()+clip.at("length").get<Frame>())/rate,static_cast<float>(std::pow(10.0,clip.at("gain_db").get<double>()/20.)));
        }
        if(selected){try{auto a=value(begin_),b=value(end_);if(b>a){g.setColour(juce::Colour(0x4473c8ff));g.fillRect(218+static_cast<int>(a*scale),0,std::max(1,static_cast<int>((b-a)*scale)),height);}}catch(const Error&){} }
    }
    void selectedRowsChanged(int row) override {if(row<0 || updating_ || row==selected_)return;selected_=row;rebuild();}
    void listBoxItemClicked(int row,const juce::MouseEvent& e) override {
        selected_=row;const auto frame=std::clamp<Frame>(static_cast<Frame>((e.x-218)*double(extent())/std::max(1,lanes_.getWidth()-222)),0,extent());
        for(const auto& c:selectedPlaylist().at("clips"))if(frame>=c.at("start").get<Frame>() && frame<c.at("start").get<Frame>()+c.at("length").get<Frame>()){
            clipId_=c.at("id");begin_.setText(juce::String(static_cast<juce::int64>(c.at("start").get<Frame>())));end_.setText(juce::String(static_cast<juce::int64>(c.at("start").get<Frame>()+c.at("length").get<Frame>())));break;}lanes_.repaint();
    }
    void changeListenerCallback(juce::ChangeBroadcaster*) override {lanes_.repaint();}
    std::function<void(Json)> submit_;juce::AudioFormatManager formats_;juce::AudioThumbnailCache cache_;std::map<std::string,std::unique_ptr<juce::AudioThumbnail>> thumbnails_;
    Json session_;Json staged_=Json::array();std::uint64_t stagedRevision_{};fs::path root_;std::string trackId_,clipId_;std::vector<std::string> trackIds_;int selected_=0;bool updating_=false,first_=true;
    juce::ComboBox tracks_,curve_;juce::ListBox lanes_;juce::Label title_,facts_,range_,boundary_,plan_;juce::TextEditor name_,begin_,end_,fade_;
    juce::TextButton stage_{"Stage this source"},linked_{"Create linked Comp"},clear_{"Clear plan"},restore_{"Restore linked playback"};
    juce::TextButton create_{"New empty"},duplicate_{"Duplicate source"},rename_{"Rename"},use_{"Use for playback"},target_{"Set target"},take_{"Register source Take"},copy_{"Copy selection to target"},comp_{"New candidate Comp"},audition_{"Audition selection"},stop_{"Stop audition"},lock_{"Lock Playlist"};
};
class NativePlaylistWindow final : public juce::DocumentWindow {
public:
    explicit NativePlaylistWindow(std::function<void(Json)> submit):DocumentWindow("NativeDAW | Playlists",juce::Colour(0xff161b22),closeButton){setUsingNativeTitleBar(true);setContentOwned(new NativePlaylistEditor(std::move(submit)),true);setResizable(true,false);setResizeLimits(900,720,1600,1000);centreWithSize(1040,760);}
    NativePlaylistEditor& editor(){return *static_cast<NativePlaylistEditor*>(getContentComponent());}
    void closeButtonPressed() override {setVisible(false);}
};
}
