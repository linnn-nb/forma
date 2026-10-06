#include <nativedaw/v2/EngineCommands.h>
#include <iostream>
#include <fstream>
#include <thread>
using namespace ndaw::v2;
namespace {
int checks=0;
void check(bool condition,const char* name) { if (!condition) throw std::runtime_error(name); ++checks; std::cout<<"PASS "<<name<<"\n"; }
template<class F> void fails(F f,const char* name) { bool failed=false; try {f();} catch(const std::exception&){failed=true;} check(failed,name); }
Json createOps(const juce::File& audio,double gain=-6) {
    return Json::array({
        {{"command","track.create"},{"args",{{"name","Known signal"},{"ref","$signal"}}}},
        {{"command","clip.import"},{"args",{{"track","$signal"},{"path",audio.getFullPathName().toStdString()},{"position_samples",0}}}},
        {{"command","track.gain"},{"args",{{"track","$signal"},{"db",gain}}}}
    });
}
void writeSignal(const juce::File& audio) {
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream=audio.createOutputStream();
    auto options=juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24);
    auto writer=wav.createWriterFor(stream,options);
    if (!writer) throw std::runtime_error("fixture writer failed");
    juce::AudioBuffer<float> block(2,240000);
    for (int i=0;i<240000;++i) { const auto v=float(0.2*std::sin(2*juce::MathConstants<double>::pi*1000*i/48000)); for (int c=0;c<2;++c) block.setSample(c,i,v); }
    if(!writer->writeFromAudioSampleBuffer(block,0,240000)) throw std::runtime_error("fixture write failed");
}
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-m0-"+juce::Uuid().toString());
        folder.createDirectory(); auto audio=folder.getChildFile("known-sine.wav"); writeSignal(audio);
        const auto originalHash=Commands::mediaHash(audio);
        Commands c(false); const auto start=juce::Time::getMillisecondCounterHiRes();
        auto p=c.makePlan("human",createOps(audio));
        check(c.preview(p).at("changes").size()==3,"M0-T01 dry-run contains three operations");
        check(c.query().at("tracks").empty(),"preview is read-only");
        auto receipt=c.commit(p); auto q=c.query();
        check(q.at("tracks").size()==1 && q.at("tracks")[0].at("clips").size()==1,"three operations commit to real Edit");
        check(std::abs(q.at("tracks")[0].at("gain_db").get<double>()+6)<0.001,"M0-T02 actual plugin gain is -6 dB");
        check(juce::Time::getMillisecondCounterHiRes()-start<1000,"Plan under predeclared 1 s budget");
        auto id=q.at("tracks")[0].at("id").get<std::string>();
        // Exercise real SDK timers/async updates, rather than Undo only in the committing call stack.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(1100);
        auto undoStarted=juce::Time::getMillisecondCounterHiRes();
        c.undo(p.at("plan_id")); const auto undoMs=juce::Time::getMillisecondCounterHiRes()-undoStarted;
        check(c.query().at("tracks").empty(),"one delayed Undo removes complete Plan after SDK async updates");
        check(undoMs<1000,"Undo under predeclared 1 s budget");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(450);
        check(c.commit(p).at("state")=="undone" && c.query().at("tracks").empty(),"retry after Undo does not resurrect edit");
        auto redoStarted=juce::Time::getMillisecondCounterHiRes();
        c.redo(); const auto redoMs=juce::Time::getMillisecondCounterHiRes()-redoStarted;
        juce::MessageManager::getInstance()->runDispatchLoopUntil(450);
        q=c.query(); check(q.at("tracks")[0].at("id")==id && std::abs(q.at("tracks")[0].at("gain_db").get<double>()+6)<0.001,"Redo preserves IDs and plugin gain");
        check(redoMs<1000,"Redo under predeclared 1 s budget");
        auto gain=c.makePlan("human",Json::array({{{"command","track.gain"},{"args",{{"track",id},{"db",-12}}}}})); c.commit(gain);
        fails([&]{c.undo(p.at("plan_id"));},"selective Undo refuses to erase a later human transaction");
        c.undo();
        check(std::abs(c.query().at("tracks")[0].at("gain_db").get<double>()+6)<0.001,"separate gain Undo restores -6 dB");
        auto stale=c.makePlan("human",Json::array({{{"command","track.gain"},{"args",{{"track",id},{"db",-9}}}}})); c.redo(); fails([&]{c.commit(stale);},"M0-T03 stale revision rejected"); c.undo();
        auto collision=p; collision["operations"][0]["args"]["name"]="collision"; fails([&]{c.commit(collision);},"idempotency content collision rejected");
        fails([&]{c.makePlan("human",Json::array({{{"command","track.gain"},{"args",{{"track","absent"},{"db",0}}}}}));},"nonexistent object rejected");
        auto invalid=createOps(audio); invalid[2]["args"]["db"]=100; fails([&]{c.makePlan("human",invalid);},"out of range parameter rejected atomically");
        auto unknown=createOps(audio); unknown[2]["args"]["warmth"]=1; fails([&]{c.makePlan("human",unknown);},"invented plugin parameter rejected");
        auto agent=c.makePlan("agent:codex",Json::array({{{"command","track.gain"},{"args",{{"track",id},{"db",-6}}}}})); fails([&]{c.commit(agent);},"M0-T04 agent commit requires acceptance");
        check(c.commit(agent,true).at("actor")=="agent:codex","accepted structured agent Plan uses the same executor");
        c.undo(agent.at("plan_id"));
        bool threadFailed=false; std::thread worker([&]{try{c.query();}catch(const std::exception&){threadFailed=true;}}); worker.join(); check(threadFailed,"background Edit access rejected");
        std::cout<<c.query().dump(2)<<"\n"; auto exportFile=folder.getChildFile("render.wav"); auto measured=c.render(exportFile,0,240000); std::cout<<measured.dump(2)<<"\n";
        check(measured.at("frames")==240000 && measured.at("channels")==2 && measured.at("pcm_bits")==24,"M0-T05 Tracktion render format and range verified");
        check(std::abs(measured.at("peak_dbfs").get<double>()-(-19.9794))<0.02,"rendered -6 dB gain verified numerically");
        check(std::abs(measured.at("lufs_i").get<double>()-(-20.0))<0.15,"known stereo 1 kHz loudness within 0.15 LU");
        check(std::abs(measured.at("true_peak_dbtp").get<double>()-(-19.9794))<0.08,"true peak within 0.08 dB reference");
        check(measured.at("render_ms").get<double>()<10000,"render under predeclared 10 s budget");
        fails([&]{c.render(exportFile,0,240000);},"existing export protected");
        auto saved=folder.getChildFile("session.tracktionedit"); c.save(saved); Commands reopened(false); reopened.open(saved);
        auto restored=reopened.query(); check(restored.at("tracks")==c.query().at("tracks"),"M0-T06 save and reopen preserve track clip gain and stable IDs");
        auto corrupt=folder.getChildFile("corrupt.tracktionedit"); corrupt.replaceWithText("broken"); fails([&]{reopened.open(corrupt);},"corrupt Edit rejected without replacing session");
        check(restored.at("tracks")==reopened.query().at("tracks"),"failed open keeps current Edit");
        fails([&]{c.save(saved);},"existing save protected"); check(Commands::mediaHash(audio)==originalHash,"original PCM unchanged after render and Undo");
        auto changedPlan=c.makePlan("human",createOps(audio)); audio.appendText("tampered"); fails([&]{c.commit(changedPlan);},"media hash conflict rejected before mutation");
        Json summary{{"checks",checks},{"result","passed"},{"render",measured},{"undo_ms",undoMs},{"redo_ms",redoMs},{"evidence_type","deterministic signal and structured Plan tests; no model invocation or microphone audition"}};
        if (argc>1) { std::ofstream report(argv[1]); report<<summary.dump(2); report.close(); if (!report) throw std::runtime_error("test report write failed"); }
        std::cout<<summary.dump(2)<<"\n";
        folder.deleteRecursively(); return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<"\n"; return 1; }
}
