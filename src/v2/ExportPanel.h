#pragma once
class ExportPanel final:public juce::Component {
public:
    using Call=std::function<Json(const std::string&,const Json&)>;
    ExportPanel(Call call,std::function<void(Json)> choose,std::function<void()> close):call(std::move(call)),choose(std::move(choose)),close(std::move(close)){
        setComponentID("export.panel");for(auto* c:std::initializer_list<juce::Component*>{&title,&range,&hint,&postroll,&begin,&pause,&cancel,&dismiss,&summary})addAndMakeVisible(c);
        title.setText(text("导出并检查 WAV"),juce::dontSendNotification);title.setFont(juce::FontOptions(22));postroll.setText("0",false);postroll.setInputRestrictions(12,"0123456789.");postroll.setComponentID("export.postroll_seconds");begin.setComponentID("export.verified.begin");pause.setComponentID("export.pause");cancel.setComponentID("export.cancel");dismiss.setComponentID("export.close");summary.setComponentID("export.summary");summary.setMultiLine(true);summary.setReadOnly(true);
        hint.setText(text("WAV · 48 kHz · 双声道 · PCM24 · 追加后滚 0–30 秒（原工程继续回放）\n另测文件结束后 2 秒，可能含后续片段/MIDI；不能认证全部尾音。总渲染范围最多 5 分钟。"),juce::dontSendNotification);
        begin.onClick=[this]{try{const auto value=postroll.getText().toStdString();size_t used=0;const double seconds=std::stod(value,&used);if(used!=value.size()||!std::isfinite(seconds)||seconds<0||seconds>30)throw std::runtime_error("后滚必须是0–30秒");auto args=binding;args["postroll_samples"]=std::llround(seconds*48000);args["request_key"]="gui-export:"+juce::Uuid().toString().toStdString();error.clear();this->choose(args);}catch(const std::exception& e){showError(e.what());}};
        pause.onClick=[this]{perform("pause",{{"artifact_id",state["request"]["artifact_id"]},{"paused",!state.value("pause",Json::object()).value("user_requested",false)}});};cancel.onClick=[this]{perform("cancel",{{"artifact_id",state["request"]["artifact_id"]}});};dismiss.onClick=[this]{this->close();};
    }
    void bind(Json request,const Json& status){binding=std::move(request);range.setText(text(binding["mode"]=="selection"?"时间选区：":"完整工程：")+juce::String(binding["start_samples"].get<int64_t>())+" – "+juce::String(binding["end_samples"].get<int64_t>())+text(" 采样 · r")+text(binding["base_revision"].dump()),juce::dontSendNotification);error.clear();update(status);}
    void showError(const std::string& why){error=why;update(state);}
    void update(const Json& status){state=status;const bool busy=state.value("busy",false),owned=busy&&state["request"].value("purpose",std::string{})=="export";begin.setEnabled(!busy);postroll.setEnabled(!busy);pause.setEnabled(owned&&state.value("state",std::string{})!="cancelling");cancel.setEnabled(owned);pause.setButtonText(text(state.value("pause",Json::object()).value("user_requested",false)?"继续导出":"暂停导出"));
        juce::String display=error.empty()?juce::String{}:text("未完成：")+text(error)+"\n";display+=text("状态：")+text(state.value("state",std::string("idle")))+"\n";
        if(busy)display+=state.value("pause",Json::object()).value("worker_parked",false)?text("工作线程已停驻；60秒截止包含暂停。\n"):text("等待真实渲染、文件校验与发布回执。\n");
        const auto receipt=state.value("receipt",Json(nullptr));if(receipt.is_object()&&receipt.value("state",std::string{})=="completed"&&receipt["binding"]["purpose"]=="export"){
            const auto& file=receipt.at("file_verification");display+=text("已生成并校验：")+text(file.at("path").get<std::string>())+text("\nWAV / PCM24 / 48 kHz / 2声道 · ")+text(file["frames"].dump())+text(" 帧\nSHA256: ")+text(file["sha256"].get<std::string>())+"\n";
            display+=text("实际文件：Peak ")+text(receipt["peak_dbfs"].dump())+text(" dBFS · RMS ")+text(receipt["rms_dbfs"].dump())+text(" dBFS\nLUFS-I ")+text(receipt["lufs_i"].dump())+text(" · True Peak ")+text(receipt["true_peak_dbtp"].dump())+text(" dBTP\n");
            const auto& report=receipt["delivery"];const auto& profile=report["profile"];
            display+=text("交付条件：")+condition(report["status"].get<std::string>())+text(" · 目标 ")+juce::String(profile["target_lufs"].get<double>(),2)+text(" LUFS ±")+juce::String(profile["lufs_tolerance"].get<double>(),2)+text(" LU · TP ≤ ")+juce::String(profile["true_peak_ceiling_dbtp"].get<double>(),2)+text(" dBTP（示例规范）\n");
            for(const auto& criterion:report["checks"])display+=text(criterion["label"].get<std::string>())+text("：")+condition(criterion["status"].get<std::string>())+"\n";
            display+=text("浮点编码前满刻度风险 ")+text(receipt["float_reference"]["over_full_scale_frames"].dump())+text(" 帧\n");
            display+=text(receipt["continuation"]["status"]=="needs_review"?"文件结束后2秒存在信号，需复核是否遗漏尾音或后续内容。\n":"文件结束后2秒低于−60 dBFS；这不能证明全部尾音完整。\n");
            display+=text(receipt.value("current",false)?"对应当前工程；外部文件创建不属于Undo。":"历史文件回执；工程或文件已变更，定位不可用。");
        }else if(receipt.is_object()&&receipt.contains("error"))display+=text(receipt["error"].get<std::string>());
        if(summary.getText()!=display)summary.setText(display,false);
    }
    void paint(juce::Graphics& g)override{g.fillAll(base());g.setColour(juce::Colour(0xffb7c7d8));g.drawText(text("追加后滚 / 秒"),24,138,145,30,juce::Justification::left);}
    void resized()override{title.setBounds(24,18,getWidth()-170,36);dismiss.setBounds(getWidth()-136,24,112,28);range.setBounds(24,62,getWidth()-48,28);hint.setBounds(24,92,getWidth()-48,42);postroll.setBounds(170,138,104,30);begin.setBounds(294,138,200,30);pause.setBounds(512,138,132,30);cancel.setBounds(660,138,132,30);summary.setBounds(24,192,getWidth()-48,std::max(90,getHeight()-216));}
private:
    static juce::String condition(const std::string& status){return text(status=="passed"?"通过":status=="failed"?"未通过":status=="indeterminate"?"数据不足":status=="not_required"?"不要求":"需复核");}
    void perform(const std::string& command,const Json& args){try{error.clear();update(call(command,args));}catch(const std::exception& e){showError(e.what());}}
    Call call;std::function<void(Json)> choose;std::function<void()> close;Json binding,state=Json::object();std::string error;juce::Label title,range,hint;juce::TextEditor postroll,summary;juce::TextButton begin{text("选择新路径并导出…")},pause{text("暂停导出")},cancel{text("取消导出")},dismiss{text("返回工程")};
};
