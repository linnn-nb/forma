#include <nativedaw/v2/ExportVerification.h>
#include <nativedaw/v2/DeliveryCheck.h>
namespace ndaw::v2::analysis {
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
Json compact(const Json& measured){Json out;for(auto key:{"frames","sample_rate","channels","peak","rms","peak_dbfs","rms_dbfs","lufs_i","true_peak_dbtp","over_full_scale_frames","ending_window"})out[key]=measured.at(key);return out;}
std::string hash(const juce::File& file,const Control& control){
    juce::FileInputStream input(file);require(input.openedOk(),"export validation file unreadable");
    // Bounded streaming SHA256, with the same worker cancellation/pause gate.
    struct Stream:juce::InputStream {juce::FileInputStream& input;const Control& control;Stream(juce::FileInputStream& i,const Control& c):input(i),control(c){}int64_t getTotalLength()override{return input.getTotalLength();}bool isExhausted()override{return input.isExhausted();}int64_t getPosition()override{return input.getPosition();}bool setPosition(int64_t p)override{return input.setPosition(p);}int read(void* b,int n)override{require(!control.cancelled(),"export cancelled or deadline expired");control.yield();const int count=input.read(b,n);require(count>0||input.isExhausted(),"export hash read failed");return count;}} checked(input,control);
    return juce::SHA256(checked).toHexString().toStdString();
}
}
Json stageVerifiedWav(const juce::File& floating,const juce::File& stage,int64_t start,int64_t frames,const Control& control,const Json& profile){
    require(!stage.exists()&&frames>0,"invalid export stage");juce::WavAudioFormat wav;auto input=floating.createInputStream();require(input!=nullptr,"floating render unreadable");
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(input.release(),true));require(reader&&reader->sampleRate==48000&&reader->numChannels==2&&reader->usesFloatingPointData&&reader->bitsPerSample==32&&reader->lengthInSamples==frames+96000,"export float render format or continuation length mismatch");
    auto reference=measure(floating,start,control,{0,frames},nullptr,FeatureDomain::SessionSamples);
    auto continuation=measure(floating,start+frames,control,{frames,frames+96000},nullptr,FeatureDomain::SessionSamples);
    {std::unique_ptr<juce::OutputStream> stream=stage.createOutputStream();require(stream!=nullptr,"export staging file cannot be created");auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));require(writer!=nullptr,"export PCM24 writer unavailable");juce::AudioBuffer<float> buffer(2,4096);
        for(int64_t at=0;at<frames;at+=4096){require(!control.cancelled(),"export cancelled or deadline expired");control.yield();const int n=int(std::min<int64_t>(4096,frames-at));require(reader->read(&buffer,0,n,at,true,true)&&writer->writeFromAudioSampleBuffer(buffer,0,n),"export encode/read/write failure");}
        require(writer->flush(),"export flush failed");
    }
    const auto bytes=stage.getSize(),modified=stage.getLastModificationTime().toMilliseconds();const auto digest=hash(stage,control);
    auto measured=measure(stage,start,control,{},nullptr,FeatureDomain::SessionSamples);
    require(measured["frames"]==frames&&measured["channels"]==2&&measured["sample_rate"]==48000&&measured["file_bits"]==24&&!measured["file_float"].get<bool>(),"encoded WAV format/frame validation failed");
    require(bytes==stage.getSize()&&modified==stage.getLastModificationTime().toMilliseconds()&&hash(stage,control)==digest,"encoded export changed during validation");
    measured["float_reference"]=compact(reference);measured["float_reference"]["tap_point"]="master_before_pcm24_encoding";
    measured["continuation"]=compact(continuation);auto& after=measured["continuation"];after["start_samples"]=start+frames;after["end_samples"]=start+frames+96000;after["threshold_dbfs"]=-60;after["status"]=continuation["peak"].get<double>()>0.001?"needs_review":"quiet_observed_window";after["tail_truncation_certified"]=false;after["meaning"]="Two seconds of the unchanged session beyond file end; may contain later clips/MIDI and effect tails. Quiet window does not prove complete tails.";
    measured["delivery"]=delivery::evaluate(measured,profile);measured["delivery"]["scope"]="actual_decoded_pcm24_export";measured["delivery"]["export_file_certified"]=true;
    measured["file_verification"]={{"version","forma-wav-export/1"},{"status","verified_staged"},{"container","WAV"},{"encoding","PCM24"},{"sample_rate",48000},{"channels",2},{"frames",frames},{"bytes",bytes},{"modified_ms",modified},{"sha256",digest},{"dither","none; deterministic float32 to PCM24 quantisation"},{"publication","pending L1 version check and atomic no-overwrite hard link"}};
    measured["delivery"]["float_encoding_risk"]={{"status",reference["over_full_scale_frames"].get<int64_t>()>0?"failed":"passed"},{"over_full_scale_frames",reference["over_full_scale_frames"]},{"meaning","Actual floating Master frames exceeding full scale before integer encoding; decoded integer peak alone cannot reveal saturation."}};
    auto& criteria=measured["delivery"]["checks"];
    criteria.push_back({{"id","float_encoding_risk"},{"label","编码前满刻度风险"},{"status",measured["delivery"]["float_encoding_risk"]["status"]},{"actual",reference["over_full_scale_frames"]},{"limits",{{"maximum_frames",0}}},{"explanation","Integer sample peak cannot expose pre-encoding saturation; actual floating Master is measured separately."}});
    criteria.push_back({{"id","continuation_window"},{"label","文件结束后的信号"},{"status",after["status"]=="needs_review"?"review":"passed"},{"actual",after},{"limits",{{"window_seconds",2},{"peak_ceiling_dbfs",-60}}},{"explanation",after["meaning"]}});
    std::string status="passed";
    for(const auto& criterion:criteria)if(criterion["status"]=="review")status="needs_review";
    for(const auto& criterion:criteria)if(criterion["status"]=="indeterminate")status="indeterminate";
    for(const auto& criterion:criteria)if(criterion["status"]=="failed")status="failed";
    measured["delivery"]["status"]=status;
    return measured;
}
}
