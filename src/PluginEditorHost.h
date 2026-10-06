// SPDX-License-Identifier: AGPL-3.0-only
// Included only in PluginHost.cpp after SDK metadata/state helpers, inside ndaw.
// All calls, view ownership and destruction run on the SDK child's message thread.
class SDKEditorPreview {
    class Content final : public juce::Component {
    public:
        Content(SDKEditorPreview& owner,std::unique_ptr<juce::AudioProcessorEditor> editor)
            :owner_(owner),editor_(std::move(editor)) {
            title_.setText("Native plugin preview | Audio paused | Review to commit",juce::dontSendNotification);
            addAndMakeVisible(title_);addAndMakeVisible(programs_);addAndMakeVisible(review_);addAndMakeVisible(cancel_);addAndMakeVisible(*editor_);
            programs_.setTextWhenNothingSelected("No SDK programs exposed");
            for(const auto& program:owner_.programs_){const int index=program.at("index");const auto name=program.at("name").get<std::string>();programs_.addItem(name.empty()?"Program "+std::to_string(index)+" (SDK name empty)":name,index+1);}
            programs_.setEnabled(!owner_.programs_.empty());programs_.setSelectedId(owner_.programs_.empty()?0:owner_.p_.getCurrentProgram()+1,juce::dontSendNotification);
            programs_.onChange=[this]{try{owner_.setProgram(programs_.getSelectedId()-1);}catch(const std::exception& e){owner_.error_=e.what();title_.setText("Program failed; cancel or review the failure",juce::dontSendNotification);}};
            review_.onClick=[this]{owner_.shared_.editorDecision.store(1,std::memory_order_release);};
            cancel_.onClick=[this]{owner_.shared_.editorDecision.store(2,std::memory_order_release);};
            setSize(std::max(600,editor_->getWidth()),editor_->getHeight()+100);
        }
        ~Content() override {owner_.p_.editorBeingDeleted(editor_.get());editor_.reset();}
        void resized() override {
            title_.setBounds(10,5,getWidth()-20,24);programs_.setBounds(10,35,getWidth()-300,27);
            review_.setBounds(getWidth()-280,35,140,27);cancel_.setBounds(getWidth()-130,35,120,27);
            editor_->setBounds(0,80,getWidth(),std::max(1,getHeight()-80));
        }
    private:
        SDKEditorPreview& owner_;std::unique_ptr<juce::AudioProcessorEditor> editor_;
        juce::Label title_;juce::ComboBox programs_;juce::TextButton review_{"Review changes"},cancel_{"Cancel preview"};
    };
    class Window final : public juce::DocumentWindow {
    public:
        Window(SDKEditorPreview& owner,Content* content):DocumentWindow("NativeDAW plugin | "+owner.p_.getName(),juce::Colour(0xff252a32),closeButton),owner_(owner) {
            setUsingNativeTitleBar(true);setContentOwned(content,true);setResizable(false,false);
            centreWithSize(content->getWidth(),content->getHeight());setVisible(true);toFront(true);
        }
        void closeButtonPressed() override {owner_.shared_.editorDecision.store(2,std::memory_order_release);setVisible(false);}
    private:SDKEditorPreview& owner_;
    };
public:
    SDKEditorPreview(juce::AudioPluginInstance& p,const PluginLimits& limits,PluginShared& shared,const fs::path& job,const std::string& pluginId,int latency)
        :p_(p),limits_(limits),shared_(shared),job_(job),pluginId_(pluginId),latency_(latency) {}
    ~SDKEditorPreview(){window_.reset();}
    bool active() const {return static_cast<bool>(window_);}
    void serve() {
        const auto sequence=shared_.editorRequest.load(std::memory_order_acquire);
        if(sequence==lastRequest_)return;
        const auto command=shared_.editorCommand.load(std::memory_order_relaxed);Json response;
        try {
            if(command==1){
                if(active())throw Error("plugin_editor_busy","Another preview is open");
                lease_=sequence;shared_.editorLease.store(lease_);shared_.editorDecision.store(0);error_.clear();
                before_=snapshot(p_,limits_,job_,100000+static_cast<int>(sequence)*3);beforeParameters_=parameters(p_,limits_);
                if(!p_.hasEditor())throw Error("plugin_editor_unavailable","This actual SDK instance exposes no native editor");
                const int count=p_.getNumPrograms();if(count<0 || count>4096)throw Error("plugin_program_resources","Actual SDK program count exceeds4096");
                programs_=Json::array();for(int i=0;i<count;++i)programs_.push_back({{"index",i},{"name",p_.getProgramName(i).substring(0,512).toStdString()}});
                std::unique_ptr<juce::AudioProcessorEditor> editor(p_.createEditorIfNeeded());
                if(!editor)throw Error("plugin_editor_unavailable","SDK returned no actual native editor");
#if JUCE_MAC
                // An SDK child normally has no foreground UI. Its actual editor
                // needs native focus/Dock identity while this stopped lease owns it.
                juce::Process::setDockIconVisible(true);
                juce::Process::makeForegroundProcess();
#endif
                window_=std::make_unique<Window>(*this,new Content(*this,std::move(editor)));
                response={{"event","opened"},{"native_sdk_editor",true},{"before_state",before_},{"before_parameters",beforeParameters_},{"programs",programs_}};
                shared_.editorStatus.store(2);
            }else if(command==2 || command==3){
                if(!active())throw Error("plugin_editor_closed","Native preview is not open");
                response=finish(command==2);shared_.editorStatus.store(command==2?4:5);
            }else if(command==4){
                if(!active())throw Error("plugin_editor_closed","Open the actual editor before a program preview");
                setProgram(shared_.editorProgram.load());response={{"event","program_previewed"},{"program_index",p_.getCurrentProgram()},{"parameters",parameters(p_,limits_)}};
            }else throw Error("plugin_editor_command","Unknown maintenance command");
            response["status"]="succeeded";
        }catch(const std::exception& e){
            bool restored=false;try{restore();restored=true;}catch(...){std::uint32_t none=0;shared_.fault.compare_exchange_strong(none,static_cast<std::uint32_t>(PluginFault::Parameter));}
            const auto* domain=dynamic_cast<const Error*>(&e);response={{"status","failed"},{"code",domain?domain->code:"plugin_editor_exception"},{"message",e.what()},{"prior_state_restored",restored}};shared_.editorStatus.store(6);
        }
        response["protocol"]=1;response["job_id"]=job_.filename().string();response["worker_pid"]=pluginOwnerPid();response["plugin_id"]=pluginId_;response["request_sequence"]=sequence;response["editor_lease"]=lease_;
        if(response.dump().size()>limits_.responseBytes)throw Error("plugin_editor_resources","Native editor receipt exceeds16MiB");
        atomicWrite(job_/("editor-"+std::to_string(sequence)+".json"),response.dump(),false);
        lastRequest_=sequence;shared_.editorAck.store(sequence,std::memory_order_release);
    }
    void shutdown(){if(active())finish(false);}
private:
    void setProgram(int index) {
        if(index<0 || index>=static_cast<int>(programs_.size()))throw Error("plugin_program","Use an actually enumerated SDK program index");
        p_.setCurrentProgram(index);if(p_.getCurrentProgram()!=index)throw Error("plugin_program","Actual SDK did not select the requested program");
        if(!stereoMainOnly(p_) || p_.getLatencySamples()!=latency_)throw Error("plugin_editor_configuration","Program changed admitted layout or latency; cannot commit this preview");
    }
    void restore() {
        window_.reset();
#if JUCE_MAC
        juce::Process::setDockIconVisible(false);
#endif
        if(before_.is_null())return;
        juce::MemoryBlock bytes;const auto path=fs::path(before_.at("path").get<std::string>());
        if(sha256(path)!=before_.at("sha256").get<std::string>() || !juce::File(juce::String(path.string())).loadFileAsData(bytes))throw Error("plugin_editor_restore","Retained pre-editor state changed");
        p_.setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));
        if(!stereoMainOnly(p_) || p_.getLatencySamples()!=latency_)throw Error("plugin_editor_restore","SDK did not restore layout/latency");
        const auto current=parameters(p_,limits_);if(current.size()!=beforeParameters_.size())throw Error("plugin_editor_restore","SDK parameter structure changed");
        for(std::size_t i=0;i<current.size();++i)if(current[i].at("id")!=beforeParameters_[i].at("id") || current[i].at("index")!=beforeParameters_[i].at("index") || std::abs(current[i].at("value").get<double>()-beforeParameters_[i].at("value").get<double>())>1e-6)throw Error("plugin_editor_restore","SDK failed actual pre-editor parameter restoration");
        juce::MemoryBlock restored;p_.getStateInformation(restored);
        if(restored.getSize()!=bytes.getSize() || juce::SHA256(restored.getData(),restored.getSize()).toHexString().toStdString()!=before_.at("sha256").get<std::string>())throw Error("plugin_editor_restore","Opaque pre-editor state did not round trip; SDK candidate cannot be accepted");
    }
    Json finish(bool capture) {
        Json result{{"event",capture?"captured_and_restored":"cancelled_and_restored"},{"native_sdk_editor",true},{"before_state",before_},{"before_parameters",beforeParameters_}};
        window_.reset(); // SDK editor destruction precedes its candidate snapshot
        if(capture){
            if(!error_.empty())throw Error("plugin_program",error_);
            if(!stereoMainOnly(p_) || p_.getLatencySamples()!=latency_)throw Error("plugin_editor_configuration","Native editor changed layout/latency; requalification required");
            result["candidate_state"]=snapshot(p_,limits_,job_,100001+static_cast<int>(lease_)*3);result["candidate_parameters"]=parameters(p_,limits_);
        }
        restore();result["prior_state_restored"]=true;result["restored_parameters"]=parameters(p_,limits_);return result;
    }
    juce::AudioPluginInstance& p_;const PluginLimits& limits_;PluginShared& shared_;fs::path job_;std::string pluginId_,error_;int latency_;
    Json before_,beforeParameters_,programs_;std::uint64_t lease_{},lastRequest_{};std::unique_ptr<Window> window_;
};
