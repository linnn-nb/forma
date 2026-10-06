#pragma once
// Read-only facts and revision-bound Plans. No mutable Edit escapes L1.
using ClipWriter=std::function<void(const std::string&,Json,uint64_t)>;
class ClipPanel final:public juce::Component {
public:
    explicit ClipPanel(ClipWriter writer):write(std::move(writer)){
        for(auto* c:std::initializer_list<juce::Component*>{&title,&split,&copy,&remove,&lock,&close,&start,&end,&moveTo,&gain,&fadeIn,&fadeOut,&inCurve,&outCurve,&trim,&move,&applyGain,&applyFade,&startLabel,&endLabel,&moveLabel,&gainLabel,&inLabel,&outLabel})addAndMakeVisible(c);
        const std::pair<Field*,const char*> fields[]={{&start,"clip.start"},{&end,"clip.end"},{&moveTo,"clip.position"},{&gain,"clip.gain.db"},{&fadeIn,"clip.fade.in"},{&fadeOut,"clip.fade.out"}};
        for(auto [field,id]:fields){field->setComponentID(id);field->setInputRestrictions(20);field->onBegin=[this]{if(!dirty){dirty=true;editRevision=revision;}};field->onTextChange=[this]{if(!dirty){dirty=true;editRevision=revision;}};}
        for(auto* combo:{&inCurve,&outCurve}){combo->addItem(text("线性"),1);combo->addItem(text("凸形"),2);combo->addItem(text("凹形"),3);combo->addItem("S",4);combo->setSelectedId(1,juce::dontSendNotification);combo->onChange=[this]{if(!dirty){dirty=true;editRevision=revision;}};}
        inCurve.setComponentID("clip.fade.in_curve");outCurve.setComponentID("clip.fade.out_curve");title.setComponentID("clip.detail");
        split.setComponentID("clip.split");copy.setComponentID("clip.copy");remove.setComponentID("clip.delete");lock.setComponentID("clip.lock");trim.setComponentID("clip.trim");move.setComponentID("clip.move");applyGain.setComponentID("clip.gain");applyFade.setComponentID("clip.fade");close.setComponentID("clip.close");
        auto send=[this](const std::string& cmd,Json a){if(facts.is_null())return;auto rev=dirty?editRevision:revision;write(cmd,std::move(a),rev);dirty=false;};
        split.onClick=[this,send]{send("clip.split",{{"clip",facts["id"]},{"position_samples",playhead},{"ref","$right"}});};
        copy.onClick=[this,send]{send("clip.copy",{{"clip",facts["id"]},{"track",track},{"position_samples",facts["start_samples"].get<int64_t>()+facts["length_samples"].get<int64_t>()},{"ref","$copy"}});};
        remove.onClick=[this,send]{send("clip.delete",{{"clip",facts["id"]}});};lock.onClick=[this,send]{send("clip.lock",{{"clip",facts["id"]},{"locked",!facts["locked"].get<bool>()}});};
        trim.onClick=[this,send]{guard([&]{send("clip.trim",{{"clip",facts["id"]},{"start_samples",integer(start)},{"end_samples",integer(end)}});});};
        move.onClick=[this,send]{guard([&]{send("clip.move",{{"clip",facts["id"]},{"position_samples",integer(moveTo)}});});};
        applyGain.onClick=[this,send]{guard([&]{send("clip.gain",{{"clip",facts["id"]},{"db",decimal(gain)}});});};
        applyFade.onClick=[this,send]{guard([&]{send("clip.fade",{{"clip",facts["id"]},{"in_samples",integer(fadeIn)},{"out_samples",integer(fadeOut)},{"in_curve",curve(inCurve)},{"out_curve",curve(outCurve)}});});};
        title.setFont(juce::FontOptions(12));startLabel.setText(text("起点 samples"),juce::dontSendNotification);endLabel.setText(text("终点 samples"),juce::dontSendNotification);moveLabel.setText(text("移至 samples"),juce::dontSendNotification);gainLabel.setText("Clip dB",juce::dontSendNotification);inLabel.setText(text("淡入 samples"),juce::dontSendNotification);outLabel.setText(text("淡出 samples"),juce::dontSendNotification);
    }
    std::function<void()> onClose;
    std::function<void(const std::string&)> onError;
    void update(Json c,std::string owner,uint64_t rev,int64_t position,bool playing){
        auto id=c.is_null()?std::string{}:c["id"].get<std::string>();bool changed=id!=selected;selected=id;facts=std::move(c);track=std::move(owner);revision=rev;playhead=position;if(changed)dirty=false;
        bool editable=!facts.is_null()&&facts.value("editable_audio",false)&&!facts.value("locked",false)&&!playing;
        for(auto* b:{&copy,&remove,&trim,&move,&applyGain,&applyFade})b->setEnabled(editable);
        for(auto* field:{&start,&end,&moveTo,&gain,&fadeIn,&fadeOut})field->setEnabled(editable);inCurve.setEnabled(editable);outCurve.setEnabled(editable);
        lock.setEnabled(!facts.is_null()&&!playing&&facts.value("editable_audio",false));lock.setButtonText(!facts.is_null()&&facts.value("locked",false)?text("解锁"):text("锁定"));close.onClick=[this]{if(onClose)onClose();};
        split.setEnabled(editable&&playhead>facts["start_samples"].get<int64_t>()&&playhead<facts["start_samples"].get<int64_t>()+facts["length_samples"].get<int64_t>());
        if(facts.is_null()){title.setText(text("选择一个音频片段"),juce::dontSendNotification);return;}
        title.setText(text(facts["name"].get<std::string>())+text(" · 源偏移 ")+juce::String(facts["source_offset_samples"].get<int64_t>())+" @48k · "+juce::String(facts["source_sample_rate"].get<double>()/1000.,1)+text(" kHz 文件 · 拖动移动，两边修剪"),juce::dontSendNotification);
        if(!dirty){start.setText(juce::String(facts["start_samples"].get<int64_t>()),false);end.setText(juce::String(facts["start_samples"].get<int64_t>()+facts["length_samples"].get<int64_t>()),false);moveTo.setText(start.getText(),false);gain.setText(juce::String(facts["gain_db"].get<double>(),2),false);fadeIn.setText(juce::String(facts["fade_in_samples"].get<int64_t>()),false);fadeOut.setText(juce::String(facts["fade_out_samples"].get<int64_t>()),false);inCurve.setSelectedId(curveID(facts["fade_in_curve"]),juce::dontSendNotification);outCurve.setSelectedId(curveID(facts["fade_out_curve"]),juce::dontSendNotification);}
    }
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff232d38));g.setColour(accent());g.drawHorizontalLine(0,0,float(getWidth()));}
    void resized()override{
        int x=10;for(auto* b:{&split,&copy,&remove,&lock}){b->setBounds(x,8,100,26);x+=106;}close.setBounds(getWidth()-38,8,28,26);title.setBounds(10,39,getWidth()-20,24);
        const int w=std::max(68,(getWidth()-62)/5);startLabel.setBounds(10,68,w,20);start.setBounds(10,91,w,25);endLabel.setBounds(20+w,68,w,20);end.setBounds(20+w,91,w,25);trim.setBounds(30+2*w,91,w,25);moveLabel.setBounds(40+3*w,68,w,20);moveTo.setBounds(40+3*w,91,w,25);move.setBounds(50+4*w,91,w,25);
        const int small=std::max(55,(getWidth()-88)/8);x=10;gainLabel.setBounds(x,122,small,20);gain.setBounds(x,145,small,25);x+=small+8;applyGain.setBounds(x,145,small,25);x+=small+8;inLabel.setBounds(x,122,small*2+8,20);fadeIn.setBounds(x,145,small,25);x+=small+8;inCurve.setBounds(x,145,small,25);x+=small+8;outLabel.setBounds(x,122,small*2+8,20);fadeOut.setBounds(x,145,small,25);x+=small+8;outCurve.setBounds(x,145,small,25);x+=small+8;applyFade.setBounds(x,145,small*2+8,25);
    }
private:
    struct Field:juce::TextEditor{std::function<void()> onBegin;void focusGained(FocusChangeType reason)override{juce::TextEditor::focusGained(reason);if(onBegin)onBegin();}};
    template<class F>void guard(F f){try{f();}catch(const std::exception& e){if(onError)onError(e.what());}}
    static int64_t integer(const juce::TextEditor& f){size_t n=0;auto s=f.getText().toStdString();if(s.empty()||s.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("sample position requires a non-negative integer");auto value=std::stoll(s,&n);if(n!=s.size())throw std::runtime_error("invalid sample position");return value;}
    static double decimal(const juce::TextEditor& f){size_t n=0;auto s=f.getText().toStdString();auto value=std::stod(s,&n);if(n!=s.size()||!std::isfinite(value))throw std::runtime_error("invalid clip gain");return value;}
    static std::string curve(const juce::ComboBox& c){return c.getSelectedId()==2?"convex":c.getSelectedId()==3?"concave":c.getSelectedId()==4?"s_curve":"linear";}
    static int curveID(std::string s){return s=="convex"?2:s=="concave"?3:s=="s_curve"?4:1;}
    ClipWriter write;Json facts=nullptr;std::string selected,track;uint64_t revision=0,editRevision=0;int64_t playhead=0;bool dirty=false;
    juce::TextButton split{text("光标处分割")},copy{text("接续复制")},remove{text("删除片段")},lock{text("锁定")},close{text("×")},trim{text("应用修剪")},move{text("应用移动")},applyGain{text("增益")},applyFade{text("应用淡化")};
    Field start,end,moveTo,gain,fadeIn,fadeOut;juce::ComboBox inCurve,outCurve;juce::Label title,startLabel,endLabel,moveLabel,gainLabel,inLabel,outLabel;
};
