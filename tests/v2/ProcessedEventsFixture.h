#pragma once
#include <nativedaw/v2/AudioAnalysis.h>
#include <cmath>
namespace processed_fixture {
using ndaw::v2::Json;
inline juce::AudioBuffer<float> pcm(int rate){
    juce::AudioBuffer<float> data(2,rate*3);data.clear();
    for(const auto& pulse:std::initializer_list<std::pair<int,float>>{{rate/4,.04f},{rate,.0008f},{rate*3/2,.25f}}){
        const int duration=pulse.first==rate?rate/100:int(std::llround(rate*.005));
        for(int i=pulse.first;i<pulse.first+duration;++i){data.setSample(0,i,pulse.second);data.setSample(1,i,-pulse.second);}
    }return data;
}
inline void write(const juce::File& file,const juce::AudioBuffer<float>& data,int rate){
    juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(32));
    if(!writer||!writer->writeFromAudioSampleBuffer(data,0,data.getNumSamples()))throw std::runtime_error("real event PCM write failed");
}
inline Json intervals(const Json& receipt,const char* kind,const char* start="start_samples",const char* end="end_samples"){
    Json out=Json::array();for(const auto& e:receipt.at("events"))if(e.at("kind")==kind)out.push_back({e.at(start),e.at(end)});return out;
}
// Independent frame scan, no detector or rendering API: threshold all channels
// and minimum length against self-owned PCM, then convert only the boundaries.
inline Json silence(const juce::AudioBuffer<float>& data,int rate,int64_t begin,int64_t end,int64_t origin,int64_t clipOrigin=0,float gain=1,int64_t delay=0){
    Json out=Json::array();int64_t first=-1;const int64_t minimum=int64_t(std::ceil(rate*.1));
    const auto close=[&](int64_t last){if(first>=0&&last-first>=minimum)out.push_back({origin+std::llround((first-begin)*48000./rate),origin+std::llround((last-begin)*48000./rate)});first=-1;};
    for(auto i=begin;i<end;++i){const auto source=i-clipOrigin-delay;double peak=0;if(source>=0&&source<data.getNumSamples())for(int ch=0;ch<data.getNumChannels();++ch)peak=std::max(peak,std::abs(double(data.getSample(ch,int(source))*gain)));if(peak<=.001){if(first<0)first=i;}else close(i);}close(end);return out;
}
inline Json onsets(int rate,int64_t begin,int64_t origin,int64_t clipOrigin=0,int64_t delay=0){
    const int64_t hop=int64_t(std::llround(rate*.005));Json out=Json::array();for(auto source:{int64_t(rate/4),int64_t(rate*3/2)}){const auto at=source+clipOrigin+delay,last=begin+((at-begin)/hop+1)*hop;out.push_back({origin+std::llround((at-begin)*48000./rate),origin+std::llround((last-begin)*48000./rate)});}return out;
}
}
