// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/PluginHost.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <thread>
#ifndef _WIN32
#include <unistd.h>
#endif

// A stalled SDK call cannot keep an orphan alive indefinitely. This guardian
// is outside all SDK/audio paths; normal parent supervision still owns exits.
class ParentGuardian {
public:
    explicit ParentGuardian(int owner) {
#ifndef _WIN32
        if(owner<=1 || ::getppid()!=owner)throw ndaw::Error("plugin_owner","Request owner is not the actual spawning parent");
        thread_=std::thread([this,owner]{while(!stop_.load()){if(::getppid()!=owner)::_exit(121);std::this_thread::sleep_for(std::chrono::milliseconds(20));}});
#else
        (void)owner; // production supervisor assigns kill-on-parent-job-close before resuming the child; Windows unexecuted
#endif
    }
    ~ParentGuardian(){stop_.store(true);if(thread_.joinable())thread_.join();}
private:std::atomic<bool> stop_{false};std::thread thread_;
};

static int runOwnedRequest(int argc,char** argv) {
    if(argc!=3 || std::string(argv[1])!="--request")return 2;
    const auto requestFile=ndaw::fs::absolute(argv[2]);const auto job=requestFile.parent_path();
    ndaw::Json request,response;std::unique_ptr<ParentGuardian> guardian;
    try {
#ifndef _WIN32
        const auto actualParent=static_cast<int>(::getppid());guardian=std::make_unique<ParentGuardian>(actualParent);
#endif
        if(!ndaw::fs::is_regular_file(requestFile) || ndaw::fs::file_size(requestFile)>16*1024*1024)throw ndaw::Error("plugin_request","Missing/oversized actual job request");
        request=ndaw::readJson(requestFile);
        if(request.at("protocol")!=ndaw::pluginProtocol || request.at("job_id")!=job.filename().string() ||
           ndaw::fs::path(request.at("response_path").get<std::string>())!=job/"response.json")throw ndaw::Error("plugin_protocol","Invalid job identity/destination");
#ifndef _WIN32
        if(request.at("owner_pid").get<int>()!=actualParent)throw ndaw::Error("plugin_owner","Request owner differs from the spawning parent");
#else
        guardian=std::make_unique<ParentGuardian>(request.at("owner_pid"));
#endif
        {juce::ScopedJuceInitialiser_GUI ui;response=ndaw::runPluginJob(request,job);}
        // Response only after plugin/UI destruction. Parent also requires a
        // normally exited, reaped child; a later signal cannot be success.
    } catch(const ndaw::Error& e) {
        if(request.is_null())return 2;
        response={{"protocol",ndaw::pluginProtocol},{"job_id",request.value("job_id","")},{"action",request.value("action","")},
            {"status","failed"},{"code",e.code},{"message",e.what()}};
    } catch(const std::exception& e) {
        if(request.is_null())return 2;
        response={{"protocol",ndaw::pluginProtocol},{"job_id",request.value("job_id","")},{"action",request.value("action","")},
            {"status","failed"},{"code","plugin_exception"},{"message",e.what()}};
    }
    try{const auto payload=response.dump();if(payload.size()>request.at("limits").at("response_bytes").get<std::size_t>())return 3;
        ndaw::atomicWrite(job/"response.json",payload,false);return 0;}catch(...){return 4;}
}
#if JUCE_MAC
// Finish native application startup before entering the bounded SDK job loop.
// A console main with manual event polling does not provide a complete Cocoa
// application lifecycle for AU views/accessibility/focus.
class PluginWorkerApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override{return "NativeDAW Plugin Host";}
    const juce::String getApplicationVersion() override{return "0.1.0";}
    bool moreThanOneInstanceAllowed() override{return true;}
    void initialise(const juce::String& args) override{
        juce::Process::setDockIconVisible(false);
        const auto tokens=juce::StringArray::fromTokens(args,true);
        if(tokens.size()!=2 || tokens[0]!="--request"){setApplicationReturnValue(2);quit();return;}
        const auto path=tokens[1].unquoted().toStdString();
        juce::MessageManager::callAsync([this,path]{std::string exe="ndaw_plugin_worker",option="--request",request=path;char* arguments[]{exe.data(),option.data(),request.data()};setApplicationReturnValue(runOwnedRequest(3,arguments));quit();});
    }
    void shutdown() override{}
    // Explicitly quitting the isolated host is a failure, never a successful
    // state capture. The owning supervisor preserves/reloads project state.
    void systemRequestedQuit() override{std::_Exit(119);}
};
START_JUCE_APPLICATION(PluginWorkerApplication)
#else
int main(int argc,char** argv){return runOwnedRequest(argc,argv);}
#endif
