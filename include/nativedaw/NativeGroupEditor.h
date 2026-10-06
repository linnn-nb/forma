// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "TrackGroups.h"
#include <juce_gui_extra/juce_gui_extra.h>
namespace ndaw {
class NativeGroupEditor final : public juce::Component,private juce::ListBoxModel {
public:
    explicit NativeGroupEditor(std::function<void(Json,std::uint64_t,std::string)> submit):submit_(std::move(submit)),members_("Group members",this) {
        setSize(680,620);for(auto* c:std::vector<juce::Component*>{&groups_,&name_,&type_,&enabled_,&members_,&volume_,&pan_,&mute_,&record_,&monitor_,&new_,&create_,&apply_,&erase_,&up_,&down_,&suspend_,&facts_})addAndMakeVisible(*c);
        groups_.setTitle("Existing track group");name_.setTitle("Group name");members_.setTitle("Actual track membership");type_.addItem("Edit",1);type_.addItem("Mix",2);type_.addItem("Edit + Mix",3);type_.setSelectedId(3);
        members_.setMultipleSelectionEnabled(true);members_.setRowHeight(28);name_.setText("New group");
        groups_.onChange=[this]{if(updating_)return;groupId_=groups_.getSelectedId()>0?groupIds_.at(groups_.getSelectedId()-1):"";load();};
        new_.onClick=[this]{groupId_.clear();groups_.setSelectedId(0,juce::dontSendNotification);name_.setText("New group");enabled_.setToggleState(true,juce::dontSendNotification);};
        create_.onClick=[this]{send(plan(true));};apply_.onClick=[this]{send(plan(false));};erase_.onClick=[this]{if(groupId_.empty())throw Error("unknown_object","Choose an existing group");send({{"command","delete_track_group"},{"group_id",groupId_}});};
        up_.onClick=[this]{reorder(-1);};down_.onClick=[this]{reorder(1);};suspend_.onClick=[this]{send({{"command","set_groups_suspended"},{"suspended",!session_.at("groups_suspended").get<bool>()}});};
        for(auto* b:{&new_,&create_,&apply_,&erase_,&up_,&down_,&suspend_}){auto action=b->onClick;b->onClick=[this,action]{try{action();}catch(const std::exception& e){facts_.setText(std::string("Not executed: ")+e.what(),juce::dontSendNotification);}};}
    }
    ~NativeGroupEditor() override {members_.setModel(nullptr);}
    void update(const Json& s,const std::set<std::string>& selected) {
        // Keep a visible staged edit pinned. Reopening/Refresh is the explicit rebase.
        session_=s;revision_=s.at("revision");updating_=true;groups_.clear();groupIds_.clear();
        for(const auto& g:s.at("track_groups")){groupIds_.push_back(g.at("id"));groups_.addItem(g.at("name").get<std::string>(),static_cast<int>(groupIds_.size()));}
        auto it=std::find(groupIds_.begin(),groupIds_.end(),groupId_);if(it==groupIds_.end())groupId_.clear();groups_.setSelectedId(groupId_.empty()?0:static_cast<int>(it-groupIds_.begin())+1,juce::dontSendNotification);
        members_.updateContent();juce::SparseSet<int> rows;int i=0;for(const auto& t:s.at("tracks")){if(selected.contains(t.at("id")))rows.addRange({i,i+1});++i;}members_.setSelectedRows(rows,juce::dontSendNotification);updating_=false;
        if(!groupId_.empty())load();else {volume_.setToggleState(true,juce::dontSendNotification);pan_.setToggleState(true,juce::dontSendNotification);mute_.setToggleState(true,juce::dontSendNotification);enabled_.setToggleState(true,juce::dontSendNotification);}
        suspend_.setButtonText(s.at("groups_suspended").get<bool>()?"Resume all groups":"Suspend all groups");facts_.setText("Mix uses the top enabled group for each attribute. Edit follows connected members. Control-click isolates a track control. All changes have Undo.",juce::dontSendNotification);
    }
    Json plan(bool create) const {
        Json ids=Json::array();for(int i=0;i<static_cast<int>(session_.at("tracks").size());++i)if(members_.isRowSelected(i))ids.push_back(session_.at("tracks")[i].at("id"));
        Json op{{"command",create?"create_track_group":"set_track_group"},{"name",name_.getText().toStdString()},{"type",type_.getSelectedId()==1?"edit":type_.getSelectedId()==2?"mix":"edit_mix"},{"track_ids",ids},{"enabled",enabled_.getToggleState()},
            {"links",{{"volume",volume_.getToggleState()},{"pan",pan_.getToggleState()},{"mute",mute_.getToggleState()},{"record",record_.getToggleState()},{"monitor",monitor_.getToggleState()}}}};
        if(!create){if(groupId_.empty())throw Error("unknown_object","Choose an existing group");op["group_id"]=groupId_;}return op;
    }
    void chooseGroup(const std::string& id){groupId_=id;auto it=std::find(groupIds_.begin(),groupIds_.end(),id);if(it==groupIds_.end())throw Error("unknown_object","Group is no longer in this revision");groups_.setSelectedId(static_cast<int>(it-groupIds_.begin())+1,juce::dontSendNotification);load();}
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff292e35));g.setColour(juce::Colour(0xff9fa9b8));g.setFont(juce::FontOptions(12));g.drawText("TRACK GROUPS",16,12,220,20,juce::Justification::centredLeft);g.drawText("Members — Shift/Command selects multiple tracks",16,118,getWidth()-32,22,juce::Justification::centredLeft);g.drawText("Mix attributes",16,getHeight()-172,200,22,juce::Justification::centredLeft);}
    void resized() override {const int w=getWidth(),h=getHeight();groups_.setBounds(16,40,w-152,28);new_.setBounds(w-126,40,110,28);name_.setBounds(16,80,w-306,28);type_.setBounds(w-280,80,160,28);enabled_.setBounds(w-110,80,96,28);members_.setBounds(16,144,w-32,std::max(100,h-334));
        juce::ToggleButton* links[]{&volume_,&pan_,&mute_,&record_,&monitor_};for(int i=0;i<5;++i)links[i]->setBounds(16+i*(w-32)/5,h-148,(w-32)/5,28);
        juce::TextButton* buttons[]{&create_,&apply_,&erase_,&up_,&down_};for(int i=0;i<5;++i)buttons[i]->setBounds(16+i*(w-32)/5,h-106,(w-42)/5,28);suspend_.setBounds(16,h-68,180,28);facts_.setBounds(208,h-72,w-224,58);}
private:
    int getNumRows() override {return session_.is_null()?0:static_cast<int>(session_.at("tracks").size());}
    void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool selected) override {if(row<0 || row>=getNumRows())return;if(selected)g.fillAll(juce::Colour(0xff48718a));g.setColour(juce::Colour(0xffe4e7ec));g.setFont(juce::FontOptions(12));const auto& t=session_.at("tracks")[row];g.drawText(t.at("name").get<std::string>()+"  ["+t.at("kind").get<std::string>()+"]",8,0,w-16,h,juce::Justification::centredLeft);}
    void load(){if(groupId_.empty())return;updating_=true;for(const auto& g:session_.at("track_groups"))if(g.at("id")==groupId_) {
        name_.setText(g.at("name").get<std::string>());type_.setSelectedId(g.at("type")=="edit"?1:g.at("type")=="mix"?2:3,juce::dontSendNotification);enabled_.setToggleState(g.at("enabled"),juce::dontSendNotification);const auto& l=g.at("links");volume_.setToggleState(l.at("volume"),juce::dontSendNotification);pan_.setToggleState(l.at("pan"),juce::dontSendNotification);mute_.setToggleState(l.at("mute"),juce::dontSendNotification);record_.setToggleState(l.at("record"),juce::dontSendNotification);monitor_.setToggleState(l.at("monitor"),juce::dontSendNotification);
        juce::SparseSet<int> rows;int i=0;for(const auto& t:session_.at("tracks")){if(std::find(g.at("track_ids").begin(),g.at("track_ids").end(),t.at("id"))!=g.at("track_ids").end())rows.addRange({i,i+1});++i;}members_.setSelectedRows(rows,juce::dontSendNotification);
    }updating_=false;}
    void reorder(int delta){auto found=std::find(groupIds_.begin(),groupIds_.end(),groupId_);if(found==groupIds_.end())throw Error("unknown_object","Choose a group to reorder");int index=static_cast<int>(found-groupIds_.begin()),next=index+delta;if(next<0 || next>=static_cast<int>(groupIds_.size()))throw Error("range","Group is already at this boundary");auto order=groupIds_;std::swap(order[index],order[next]);send({{"command","reorder_track_groups"},{"group_ids",order}});}
    void send(Json op){if(submit_)submit_(Json::array({op}),revision_,session_.at("id"));}
    std::function<void(Json,std::uint64_t,std::string)> submit_;Json session_;std::uint64_t revision_{};std::string groupId_;std::vector<std::string> groupIds_;bool updating_=false;
    juce::ComboBox groups_,type_;juce::TextEditor name_;juce::ListBox members_;juce::Label facts_;
    juce::ToggleButton enabled_{"Enabled"},volume_{"Volume"},pan_{"Pan"},mute_{"Mute"},record_{"Record"},monitor_{"Monitor"};
    juce::TextButton new_{"New selection"},create_{"Create"},apply_{"Apply"},erase_{"Delete"},up_{"Move up"},down_{"Move down"},suspend_{"Suspend all groups"};
};
class NativeGroupWindow final : public juce::DocumentWindow {
public:
    explicit NativeGroupWindow(std::function<void(Json,std::uint64_t,std::string)> submit):DocumentWindow("NativeDAW | Track groups",juce::Colour(0xff292e35),closeButton){setUsingNativeTitleBar(true);setContentOwned(new NativeGroupEditor(std::move(submit)),true);setResizable(true,false);setResizeLimits(640,560,1000,1000);centreWithSize(680,620);}
    NativeGroupEditor& editor(){return *static_cast<NativeGroupEditor*>(getContentComponent());}
    void closeButtonPressed() override {setVisible(false);}
};
class NativeGroupList final : public juce::Component,private juce::ListBoxModel {
public:
    NativeGroupList():list_("Track groups",this){addAndMakeVisible(list_);addAndMakeVisible(create_);list_.setRowHeight(28);create_.setButtonText("+");create_.setTooltip("Create or modify track groups");create_.onClick=[this]{if(edit)edit("");};setTitle("Edit and Mix groups");}
    ~NativeGroupList() override {list_.setModel(nullptr);}
    std::function<void(std::string)> edit;std::function<void(std::vector<std::string>)> selectMembers;std::function<void(Json,std::uint64_t,std::string)> execute;
    void update(const Json& s){session_=s;list_.updateContent();repaint();}
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff2a2e35));g.setColour(juce::Colour(0xff9fa9b8));g.setFont(juce::FontOptions(11));g.drawText(session_.is_null()?"GROUPS":session_.at("groups_suspended").get<bool>()?"GROUPS (suspended)":"GROUPS",12,3,getWidth()-48,26,juce::Justification::centredLeft);}
    void resized() override {create_.setBounds(getWidth()-36,4,28,24);list_.setBounds(0,32,getWidth(),std::max(0,getHeight()-32));}
private:
    int getNumRows() override {return session_.is_null()?0:static_cast<int>(session_.at("track_groups").size());}
    void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool selected) override {if(row<0 || row>=getNumRows())return;const auto& item=session_.at("track_groups")[row];if(selected)g.fillAll(juce::Colour(0xff343e4b));g.setColour(item.at("enabled").get<bool>() && !session_.at("groups_suspended").get<bool>()?juce::Colour(0xff79b5d6):juce::Colour(0xff454c57));g.fillRoundedRectangle(8,9,10,10,2);g.setColour(juce::Colour(0xffe4e7ec));g.setFont(juce::FontOptions(11));g.drawText(item.at("name").get<std::string>(),24,0,w-52,h,juce::Justification::centredLeft);g.setColour(juce::Colour(0xff9fa9b8));g.drawText(item.at("type")=="edit"?"E":item.at("type")=="mix"?"M":"EM",w-28,0,25,h,juce::Justification::centred);}
    void listBoxItemClicked(int row,const juce::MouseEvent& e) override {if(row<0 || row>=getNumRows())return;auto g=session_.at("track_groups")[row];if(e.mods.isPopupMenu()){if(edit)edit(g.at("id"));}
        else if(e.x<22){if(execute)execute(Json::array({{{"command","set_track_group"},{"group_id",g.at("id")},{"enabled",!g.at("enabled").get<bool>()}}}),session_.at("revision"),session_.at("id"));}
        else if(selectMembers)selectMembers(g.at("track_ids").get<std::vector<std::string>>());}
    void listBoxItemDoubleClicked(int row,const juce::MouseEvent&) override {if(row>=0 && row<getNumRows() && edit)edit(session_.at("track_groups")[row].at("id"));}
    Json session_;juce::ListBox list_;juce::TextButton create_;
};
}
