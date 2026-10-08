#pragma once
#include <nativedaw/v2/AudioAnalysis.h>
#include <ebur128.h>
#include <iostream>

namespace loudness_fixture {
using ndaw::v2::Json;
inline juce::AudioBuffer<float> pcm(int rate){
    juce::AudioBuffer<float> data(2,rate*8+77);data.clear();
    for(int n=0;n<data.getNumSamples();++n){const double seconds=n/double(rate),amplitude=seconds<2?.02:seconds<5?.2:0;const float sample=float(amplitude*std::sin(2*juce::MathConstants<double>::pi*1000*seconds));data.setSample(0,n,sample);data.setSample(1,n,sample);}
    return data;
}
inline void write(const juce::File& file,const juce::AudioBuffer<float>& data,int rate){
    juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(data.getNumChannels()).withBitsPerSample(32));
    if(!writer||!writer->writeFromAudioSampleBuffer(data,0,data.getNumSamples()))throw std::runtime_error("loudness PCM fixture write failed");
}
// Independent client of the pinned library. Different (257-frame) ingress,
// explicit hop endpoints and input PCM; never calls production measure/point.
inline Json reference(const juce::AudioBuffer<float>& data,int rate,int begin,int end,double gain=1.){
    const juce::ScopedNoDenormals noDenormals;
    const int hop=(rate+5)/10;auto* meter=ebur128_init(data.getNumChannels(),rate,EBUR128_MODE_M|EBUR128_MODE_S);if(!meter)throw std::runtime_error("reference meter failed");
    struct Cleanup{ebur128_state* p;~Cleanup(){ebur128_destroy(&p);}} cleanup{meter};Json result=Json::array();std::vector<float> samples(size_t(257*data.getNumChannels()));int cursor=begin;
    for(int stop=begin+hop;stop<=end;stop+=hop){
        while(cursor<stop){const int frames=std::min(257,stop-cursor);for(int n=0;n<frames;++n)for(int ch=0;ch<data.getNumChannels();++ch)samples[size_t(n*data.getNumChannels()+ch)]=float(data.getSample(ch,cursor+n)*gain);
            if(ebur128_add_frames_float(meter,samples.data(),frames)!=EBUR128_SUCCESS)throw std::runtime_error("reference sample ingress failed");cursor+=frames;}
        if(stop-begin<4*hop)continue;double m,s=-INFINITY;if(ebur128_loudness_momentary(meter,&m)!=EBUR128_SUCCESS)throw std::runtime_error("reference M failed");if(stop-begin>=30*hop&&ebur128_loudness_shortterm(meter,&s)!=EBUR128_SUCCESS)throw std::runtime_error("reference S failed");
        result.push_back(Json::array({stop-begin,std::isfinite(m)?Json(m):Json(nullptr),std::isfinite(s)?Json(s):Json(nullptr)}));
    }return result;
}
inline bool matches(const Json& points,const Json& ref){if(points.size()!=ref.size())return false;for(size_t i=0;i<ref.size();++i){if(points[i][0]!=ref[i][0])return false;for(int c:{1,2}){if(points[i][c].is_null()!=ref[i][c].is_null()){std::cerr<<"LUFS_NULL row="<<i<<" actual="<<points[i]<<" reference="<<ref[i]<<std::endl;return false;}if(ref[i][c].is_number()&&std::abs(points[i][c].get<double>()-ref[i][c].get<double>())>1e-6){std::cerr<<"LUFS_DIFF row="<<i<<" column="<<c<<" actual="<<points[i]<<" reference="<<ref[i]<<std::endl;return false;}}}return true;}
}
