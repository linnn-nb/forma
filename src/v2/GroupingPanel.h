#pragma once
// Included in ndaw::desktop. Facts are read-only; every gesture uses L1 Writer.
class GroupingPanel final : public juce::Component {
public:
    explicit GroupingPanel(Writer write,std::function<void(const std::string&)> previewDelete):write(std::move(write)),previewDelete(std::move(previewDelete)) {
        for(auto* c:std::initializer_list<juce::Component*>{&nameLabel,&name,&rename,&up,&down,&colourLabel,&colour,&deleteTrack,&parentLabel,&parent,&collapsed,&memberLabel,&candidate,&add,&member,&remove,&explanation})addAndMakeVisible(c);
        up.setComponentID("track.order.up");down.setComponentID("track.order.down");colour.setComponentID("track.colour");deleteTrack.setComponentID("track.delete.preview");
        colourLabel.setText(text("COLOUR · Edit / Mix 颜色"),juce::dontSendNotification);
        for(const auto& label:{"默认","青绿","蓝色","紫色","玫红","琥珀","草绿"})colour.addItem(text(label),colour.getNumItems()+1);
        up.onClick=[this]{reorder(-1);};down.onClick=[this]{reorder(1);};
        colour.onChange=[this]{int i=colour.getSelectedId()-1;if(!facts.is_null()&&i>=0&&i<int(palette.size())&&facts.value("colour",Json(nullptr))!=(i?Json(palette[i]):Json(nullptr)))this->write("track.colour",{{"track",facts["id"]},{"colour",palette[i]}});};
        deleteTrack.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff683e43));deleteTrack.onClick=[this]{if(!facts.is_null())this->previewDelete(facts["id"]);};
        deleteTrack.setTooltip(text("先显示删除范围、片段、插件与外部连接；接受后整体可撤销，媒体文件保留。Folder / VCA 同时删除后代。"));
        nameLabel.setText(text("NAME · 轨道名称"),juce::dontSendNotification);parentLabel.setText(text("PARENT · 所属 Folder / VCA"),juce::dontSendNotification);memberLabel.setText(text("MEMBERS · 直属成员"),juce::dontSendNotification);
        name.setComponentID("group.name");rename.setComponentID("track.rename");parent.setComponentID("group.parent");collapsed.setComponentID("track.collapsed");candidate.setComponentID("group.candidate");add.setComponentID("group.add");member.setComponentID("group.member");remove.setComponentID("group.remove");
        rename.onClick=[this]{if(!facts.is_null()&&name.getText().toStdString()!=facts["name"].get<std::string>())this->write("track.rename",{{"track",facts["id"]},{"name",name.getText().toStdString()}});};name.onReturnKey=[this]{rename.triggerClick();};
        parent.onChange=[this]{int i=parent.getSelectedId()-1;if(!facts.is_null()&&i>=0&&i<int(parents.size())&&parents[i]!=facts["parent"].get<std::string>())this->write("track.parent",{{"track",facts["id"]},{"parent",parents[i]}});};
        collapsed.setClickingTogglesState(true);collapsed.onClick=[this]{this->write("track.collapsed",{{"track",facts["id"]},{"enabled",!facts["collapsed"].get<bool>()}});};
        candidate.onChange=[this]{refreshButtons();};member.onChange=[this]{refreshButtons();};
        add.onClick=[this]{int i=candidate.getSelectedId()-1;if(i>=0&&i<int(candidates.size()))this->write("track.parent",{{"track",candidates[i]},{"parent",facts["id"]}});};
        remove.onClick=[this]{int i=member.getSelectedId()-1;if(i>=0&&i<int(members.size()))this->write("track.parent",{{"track",members[i]},{"parent","root"}});};
        candidate.setTextWhenNothingSelected(text("没有可添加的轨道"));member.setTextWhenNothingSelected(text("尚无成员"));explanation.setFont(juce::FontOptions(12));
    }
    void update(const Json& selected,const Json& tracks,bool playing) {
        bool selectionChanged=facts.is_null()||selected.is_null()||facts.value("id",std::string{})!=selected.value("id",std::string{});std::string signature;facts=selected;this->playing=playing;
        auto descends=[&](std::string track,const std::string& ancestor){std::set<std::string> visited;while(track!="root"&&visited.insert(track).second){if(track==ancestor)return true;std::string next="root";for(const auto& t:tracks)if(t["id"]==track)next=t["parent"];track=next;}return false;};
        // Only rebuild when names/hierarchy change; avoid interrupting an open menu
        // on the 20 Hz UI refresh (gain and transport are irrelevant here).
        Json structure=Json::array();for(const auto& t:tracks)structure.push_back({t["id"],t["name"],t["type"],t["parent"]});signature=Json::array({facts.is_null()?Json(nullptr):facts["id"],structure}).dump();
        if(signature!=lastStructure){lastStructure=signature;parents={"root"};candidates.clear();members.clear();parent.clear(juce::dontSendNotification);candidate.clear(juce::dontSendNotification);member.clear(juce::dontSendNotification);parent.addItem(text("Root · 顶层"),1);
            if(!facts.is_null())for(const auto& t:tracks){std::string id=t["id"],current=facts["id"];bool group=t["type"]=="folder"||t["type"]=="vca";
                if(group&&!descends(id,current)){parents.push_back(id);parent.addItem(text(t["name"].get<std::string>()),int(parents.size()));}
                if(id!=current&&t["parent"]!=current&&!descends(current,id)){candidates.push_back(id);candidate.addItem(text(t["name"].get<std::string>()),int(candidates.size()));}
                if(t["parent"]==current){members.push_back(id);member.addItem(text(t["name"].get<std::string>()),int(members.size()));}
            }
            candidate.setSelectedId(candidates.empty()?0:1,juce::dontSendNotification);member.setSelectedId(members.empty()?0:1,juce::dontSendNotification);
        }
        siblings.clear();if(!facts.is_null())for(const auto& t:tracks)if(t["parent"]==facts["parent"])siblings.push_back(t["id"]);
        bool enabled=!facts.is_null()&&!playing;colour.setEnabled(enabled);deleteTrack.setEnabled(enabled);auto current=std::find(siblings.begin(),siblings.end(),facts.is_null()?std::string{}:facts["id"].get<std::string>());up.setEnabled(enabled&&current!=siblings.begin()&&current!=siblings.end());down.setEnabled(enabled&&current!=siblings.end()&&current+1!=siblings.end());
        if(!facts.is_null()){int choice=1;if(facts["colour"].is_string()){choice=0;for(size_t i=1;i<palette.size();++i)if(palette[i]==facts["colour"].get<std::string>())choice=int(i)+1;}colour.setSelectedId(choice,juce::dontSendNotification);if(!choice)colour.setText(text(facts["colour"].get<std::string>()),juce::dontSendNotification);}
        name.setEnabled(enabled);rename.setEnabled(enabled);parent.setEnabled(enabled);
        if(!facts.is_null()){auto actualName=facts["name"].get<std::string>();if(selectionChanged||actualName!=lastName)name.setText(text(actualName),false);lastName=actualName;for(size_t i=0;i<parents.size();++i)if(parents[i]==facts["parent"].get<std::string>())parent.setSelectedId(int(i)+1,juce::dontSendNotification);collapsed.setToggleState(facts["collapsed"],juce::dontSendNotification);}
        const bool group=!facts.is_null()&&facts["capabilities"]["group"].get<bool>();collapsed.setEnabled(group&&!playing);candidate.setEnabled(group&&!playing);member.setEnabled(group&&!playing);refreshButtons();
        explanation.setText(facts.is_null()?text("选择一条轨道以组织工程"):facts["type"]=="vca"?text("VCA 控制所有后代的推子。\n成员输出与自身增益保持。\n使用 Tracktion 推子位置叠加法，\n并非简单的 dB 相加。"):facts["type"]=="folder"?text("Folder 仅组织成员，不汇总音频。\nMute / Solo 作用于后代。\n折叠只改变 Edit 显示；Mix 保留成员。"):text("选择所属 Folder / VCA。\n移入或移出不改变输出路由。\n结构修改前需停止播放。"),juce::dontSendNotification);
    }
    void resized() override {int w=getWidth()-20;nameLabel.setBounds(10,0,w,22);name.setBounds(10,28,w-72,28);rename.setBounds(16+w-72,28,66,28);up.setBounds(10,68,(w-8)/2,28);down.setBounds(18+(w-8)/2,68,(w-8)/2,28);colourLabel.setBounds(10,108,w,22);colour.setBounds(10,136,w,28);parentLabel.setBounds(10,181,w,22);parent.setBounds(10,207,w,28);collapsed.setBounds(10,248,w,28);memberLabel.setBounds(10,295,w,22);candidate.setBounds(10,326,w-68,28);add.setBounds(16+w-68,326,62,28);member.setBounds(10,369,w-68,28);remove.setBounds(16+w-68,369,62,28);deleteTrack.setBounds(10,414,w,32);explanation.setBounds(10,464,w,110);}

private:
    void reorder(int offset){if(facts.is_null())return;auto it=std::find(siblings.begin(),siblings.end(),facts["id"].get<std::string>());if(it!=siblings.end()){auto index=it-siblings.begin()+offset;if(index>=0&&index<int(siblings.size()))write("track.order",{{"track",facts["id"]},{"index",index}});}}
    void refreshButtons(){const bool enabled=!facts.is_null()&&!playing&&facts["capabilities"]["group"].get<bool>();add.setEnabled(enabled&&candidate.getSelectedId()>0);remove.setEnabled(enabled&&member.getSelectedId()>0);}
    const std::vector<std::string> palette{"default","#55A7A0","#598BCA","#A761C2","#C8708D","#C6A153","#82AD71"};
    Writer write;std::function<void(const std::string&)> previewDelete;Json facts=nullptr;bool playing=false;std::string lastStructure,lastName;std::vector<std::string> parents,candidates,members,siblings;
    juce::Label nameLabel,parentLabel,memberLabel,colourLabel,explanation;juce::TextEditor name;juce::ComboBox parent,candidate,member,colour;juce::TextButton up{text("↑ 上移")},down{text("↓ 下移")},deleteTrack{text("预览删除轨道…")},rename{text("改名")},add{text("添加")},remove{text("移出")};juce::ToggleButton collapsed{text("折叠 Folder / VCA")};
};
