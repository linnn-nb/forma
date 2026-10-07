#pragma once

class AnalysisPanel final : public juce::Component {
public:
    using Call=std::function<Json(const std::string&,const Json&)>;
    AnalysisPanel(Call call,std::function<void()> close):call(std::move(call)),close(std::move(close)){
        setComponentID("analysis.panel");
        for(auto* child:std::initializer_list<juce::Component*>{&title,&hint,&start,&end,&run,&cancel,&dismiss,&summary,&viewport})addAndMakeVisible(child);
        title.setText(text("Master 分析 / 削波风险定位"),juce::dontSendNotification);title.setFont(juce::FontOptions(22));
        hint.setText(text("离线 Master · 32-bit float · 48 kHz 工程采样位置 · 区间最多 5 分钟；播放和录音优先。"),juce::dontSendNotification);
        start.setComponentID("analysis.start_samples");end.setComponentID("analysis.end_samples");start.setInputRestrictions(16,"0123456789");end.setInputRestrictions(16,"0123456789");
        run.setComponentID("analysis.master.run");cancel.setComponentID("analysis.cancel");dismiss.setComponentID("analysis.close");
        start.setTooltip(text("起始采样位置，包含该采样"));end.setTooltip(text("结束采样位置，不包含该采样"));
        summary.setComponentID("analysis.summary");summary.setMultiLine(true);summary.setReadOnly(true);summary.setScrollbarsShown(true);
        viewport.setViewedComponent(&rows,false);viewport.setScrollBarsShown(true,false);
        run.onClick=[this]{try{
            auto a=number(start.getText()),b=number(end.getText());Json args={{"session_token",binding.at("session_token")},{"base_revision",binding.at("revision")},{"start_samples",a},{"end_samples",b},{"request_key","gui:"+juce::Uuid().toString().toStdString()}};
            update(this->call("master",args));
        }catch(const std::exception& e){error(e.what());}};
        cancel.onClick=[this]{try{update(this->call("cancel",{{"artifact_id",state["request"]["artifact_id"]}}));}catch(const std::exception& e){error(e.what());}};
        dismiss.onClick=[this]{this->close();};
    }
    void bind(const Json& facts,const Json& status){binding=facts;auto range=facts.value("time_selection",Json(nullptr));start.setText(juce::String(range.is_null()?int64_t(0):range["start_samples"].get<int64_t>()),false);end.setText(juce::String(range.is_null()?facts.value("length_samples",int64_t(0)):range["end_samples"].get<int64_t>()),false);update(status);}
    void update(const Json& value){
        state=value;const bool busy=state.value("busy",false);run.setEnabled(!busy);cancel.setEnabled(busy);start.setEnabled(!busy);end.setEnabled(!busy);
        const auto result=state.value("receipt",Json(nullptr));std::string signature=result.is_object()?result.value("artifact_id",std::string{})+":"+(result.value("current",false)?"current":"stale"):"empty";
        if(signature!=rowSignature){rowSignature=signature;buttons.clear();if(result.is_object()&&result.value("state",std::string{})=="completed")for(const auto& event:result.at("events")){
            auto button=std::make_unique<juce::TextButton>();const auto index=event["id"].get<int>();button->setComponentID("analysis.locate:"+juce::String(index));
            button->setButtonText(text("定位 ")+juce::String(event["start_samples"].get<int64_t>()/48000.,6)+" – "+juce::String(event["end_samples"].get<int64_t>()/48000.,6)+text(" 秒 · 超过满刻度"));button->setEnabled(result.value("current",false));
            button->onClick=[this,index,id=result["artifact_id"]]{try{this->call("locate",{{"artifact_id",id},{"event_id",index}});this->close();}catch(const std::exception& e){error(e.what());}};rows.addAndMakeVisible(*button);buttons.push_back(std::move(button));
        }resized();}
        juce::String display=text("状态：")+text(state.value("state",std::string("idle")));
        if(busy)display+=text(" · 渲染进度 ")+juce::String(state.value("progress",0.)*100.,0)+"%";
        if(result.is_object()&&result.value("state",std::string{})=="completed"){
            const auto& provenance=result.at("binding");display+=result.value("current",false)?text(" · 当前工程证据\n"):text(" · 历史快照：工程已变化或正在播放，请重新分析\n");
            auto measured=[&](const char* key,int digits){auto v=result.value(key,Json(nullptr));return v.is_number()?juce::String(v.get<double>(),digits):text("不可用 / 静音或区间不足");};
            display+=text("Sample Peak ")+measured("peak_dbfs",2)+" dBFS    True Peak "+measured("true_peak_dbtp",2)+" dBTP\n";
            display+="RMS "+measured("rms",6)+"    LUFS-I "+measured("lufs_i",2)+"    LUFS-M max "+measured("lufs_m_max",2)+"    LUFS-S max "+measured("lufs_s_max",2)+"\n";
            display+=text("立体声相关度 ")+measured("correlation",4)+text(" · 超过满刻度 ")+juce::String(result["over_full_scale_frames"].get<int64_t>())+text(" 帧 / ")+juce::String(result["event_count"].get<int64_t>())+text(" 段\n");
            display+=text("Tap: Master · revision ")+juce::String(provenance["revision"].get<int64_t>())+text(" · 区间 [")+juce::String(provenance["start_samples"].get<int64_t>())+", "+juce::String(provenance["end_samples"].get<int64_t>())+")\n";
            display+=text("Artifact: ")+text(result["artifact_id"].get<std::string>())+text("\n处理链 SHA256: ")+text(provenance["processing_chain_hash"].get<std::string>());
            display+=text("\n超过 0 dBFS 表示整数导出削波风险，不能据此断言原始媒体已经失真。事件为真实采样测量；仅展示前 128 段。");
        }else if(result.is_object()&&result.contains("error"))display+="\n"+text(result["error"].get<std::string>());
        else display+=text("\n输入采样区间，点击分析；结果只来自实际 Tracktion 渲染。没有测量回执就没有结论。");
        if(summary.getText()!=display)summary.setText(display,false);
    }
    void paint(juce::Graphics& g)override{g.fillAll(base());g.setColour(juce::Colour(0xffb7c7d8));g.setFont(juce::FontOptions(12));g.drawText(text("起始采样"),24,94,100,25,juce::Justification::left);g.drawText(text("结束采样"),260,94,100,25,juce::Justification::left);}
    void resized()override{title.setBounds(24,18,getWidth()-190,35);hint.setBounds(24,58,getWidth()-48,28);dismiss.setBounds(getWidth()-136,24,112,28);start.setBounds(120,94,125,28);end.setBounds(352,94,145,28);run.setBounds(514,94,140,28);cancel.setBounds(666,94,100,28);summary.setBounds(24,142,getWidth()-48,240);viewport.setBounds(24,400,getWidth()-48,std::max(60,getHeight()-424));rows.setSize(std::max(400,viewport.getWidth()-18),std::max(viewport.getHeight(),int(buttons.size())*38));for(size_t i=0;i<buttons.size();++i)buttons[i]->setBounds(0,int(i)*38,rows.getWidth(),32);}
private:
    static int64_t number(const juce::String& text){auto str=text.toStdString();if(str.empty()||str.size()>16)throw std::runtime_error("请输入整数采样位置");size_t consumed=0;auto n=std::stoll(str,&consumed);if(consumed!=str.size()||n<0)throw std::runtime_error("采样位置无效");return n;}
    void error(const std::string& message){summary.setText(text("操作未完成：")+text(message),false);}
    Call call;std::function<void()> close;Json binding=Json::object(),state=Json::object();std::string rowSignature;
    juce::Label title,hint;juce::TextEditor start,end,summary;juce::TextButton run{text("分析 Master")},cancel{text("取消分析")},dismiss{text("返回工程")};juce::Viewport viewport;juce::Component rows;std::vector<std::unique_ptr<juce::TextButton>> buttons;
};
