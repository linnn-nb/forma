#pragma once
// The panel holds a versioned preview; only its L1 callback can replace Edit.
class NewSessionPanel final:public juce::Component {
public:
    using Control=std::function<Json(const std::string&,const Json&)>;
    NewSessionPanel(Control control,std::function<void()> close,std::function<void()> save)
        :control(std::move(control)),close(std::move(close)),save(std::move(save)){
        setComponentID("session.new.panel");setWantsKeyboardFocus(true);
        for(auto* c:std::initializer_list<juce::Component*>{&title,&nameLabel,&name,&detail,&outcome,&create,&cancel,&saveButton,&preview})addAndMakeVisible(c);
        title.setText(text("新建工程"),juce::dontSendNotification);title.setFont(juce::FontOptions(24,juce::Font::bold));
        nameLabel.setText(text("工程名称"),juce::dontSendNotification);name.setComponentID("session.new.name");name.setInputRestrictions(128);name.setText("Untitled",false);
        detail.setComponentID("session.new.preview");detail.setMultiLine(true);detail.setReadOnly(true);detail.setFont(juce::FontOptions(14));
        outcome.setComponentID("session.new.result");create.setComponentID("session.new.confirm");cancel.setComponentID("session.new.cancel");saveButton.setComponentID("session.new.save");preview.setComponentID("session.new.refresh");
        create.onClick=[this]{try{auto args=binding;args["name"]=name.getText().toStdString();update(lastFacts,this->control("session.new",args));}catch(const std::exception& e){outcome.setText(text("未新建：")+text(e.what()),juce::dontSendNotification);}};
        cancel.onClick=[this]{cancelAndClose();};saveButton.onClick=[this]{this->close();this->save();};
        preview.onClick=[this]{bind(lastFacts,lastRecovery);};
    }
    void bind(const Json& facts,const Json& recovery){
        binding={{"session_token",recovery["session_token"]},{"base_revision",facts["revision"]}};
        detail.setText(text("当前工程：r")+text(facts["revision"].dump())+text(" · ")+juce::String(facts["tracks"].size())+text(" 个轨道\n\n确认后先保存当前工程的恢复副本，取得真实校验回执后，再创建独立的空白工程。\n\n新工程：120 BPM · 4/4 · 无轨道 · 监听关闭。\n当前音频设备设置保持；Agent 写权限撤回。\n\n原工程文件和媒体不覆盖。恢复副本只保存工程状态，继续引用原媒体路径。\n切换不属于 Undo；可从「工程恢复副本」找回刚才的工程。\n需要普通工程文件时，请先「另存当前工程」。"));
        update(facts,recovery);
    }
    void update(const Json& facts,const Json& recovery){
        lastFacts=facts;lastRecovery=recovery;const bool busy=recovery.value("busy",false);
        const bool current=!binding.is_null()&&binding["session_token"]==recovery["session_token"]&&binding["base_revision"]==facts["revision"];
        const bool stopped=!facts.value("playing",false)&&facts["parameter_capture"].is_null()&&facts["recording_capture"].is_null()&&facts["automation_capture"].is_null();
        create.setEnabled(!busy&&current&&stopped&&!name.getText().trim().isEmpty());name.setEnabled(!busy);saveButton.setEnabled(!busy&&stopped);preview.setEnabled(!busy);cancel.setEnabled(true);
        const auto phase=recovery.value("state",std::string{});
        outcome.setText(text(busy?"正在保存当前工程的恢复副本；尚未切换":!current?"工程已改变，请重新预览。":!stopped?"请先停止播放、录音或参数手势。":phase=="failed"?"未新建："+recovery.value("error",std::string{}):"已预览；确认后才新建工程。"),juce::dontSendNotification);
    }
    void cancelAndClose(){try{if(lastRecovery.value("busy",false))control("session.recovery.cancel",Json::object());close();}catch(const std::exception& e){outcome.setText(text(e.what()),juce::dontSendNotification);}}
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colours::black.withAlpha(.8f));g.setColour(base());g.fillRoundedRectangle(card.toFloat(),8);g.setColour(juce::Colour(0xff45505e));g.drawRoundedRectangle(card.toFloat(),8,1);}
    void resized()override{card=juce::Rectangle<int>(std::min(720,getWidth()-24),std::min(600,getHeight()-24)).withCentre(getLocalBounds().getCentre());const int x=card.getX()+24,y=card.getY()+20,w=card.getWidth()-48;title.setBounds(x,y,w,34);nameLabel.setBounds(x,y+48,100,30);name.setBounds(x+106,y+48,w-106,30);detail.setBounds(x,y+96,w,std::max(100,card.getHeight()-240));outcome.setBounds(x,card.getBottom()-116,w,40);preview.setBounds(x,card.getBottom()-68,106,31);saveButton.setBounds(x+114,card.getBottom()-68,138,31);create.setBounds(x+260,card.getBottom()-68,w-340,31);cancel.setBounds(x+w-72,card.getBottom()-68,72,31);}
private:
    Control control;std::function<void()> close,save;Json binding=nullptr,lastFacts,lastRecovery;juce::Rectangle<int> card;
    juce::Label title,nameLabel,outcome;juce::TextEditor name,detail;
    juce::TextButton create{text("保留副本并新建")},cancel{text("取消")},saveButton{text("另存当前工程…")},preview{text("重新预览")};
};
