#pragma once
#include "LoudnessFixture.h"
#include <complex>
namespace spectrum_fixture {
using ndaw::v2::Json;
inline juce::AudioBuffer<float> pcm(int frames=48077,int channels=2){
    juce::AudioBuffer<float> data(channels,frames);
    for(int n=0;n<frames;++n)for(int ch=0;ch<channels;++ch){const double x=2*juce::MathConstants<double>::pi*n/4096.;data.setSample(ch,n,float((ch?-.17:.17)*std::sin(37*x)+(ch?.12:.07)*std::cos((ch?789.:241.375)*x)+(ch?0:.03)+.04*(n%2?-1:1)));}
    return data;
}
inline std::vector<int> ends(int length){std::vector<int> result;for(int end=4096;end<=length;end+=2048)result.push_back(end);if(length>=4096&&result.back()!=length)result.push_back(length);return result;}
inline double bin(const juce::AudioBuffer<float>& data,int begin,int end,int k,int channel=-1,double gain=1.){
    const auto windows=ends(end-begin);double power=0;const int first=channel<0?0:channel,last=channel<0?data.getNumChannels():channel+1;
    for(int stop:windows)for(int ch=first;ch<last;++ch){std::complex<double> sum{};double norm=0;for(int n=0;n<4096;++n){const double w=.5-.5*std::cos(2*juce::MathConstants<double>::pi*n/4096),phase=-2*juce::MathConstants<double>::pi*k*n/4096;sum+=double(data.getSample(ch,begin+stop-4096+n))*gain*w*std::complex<double>(std::cos(phase),std::sin(phase));norm+=w*w;}power+=std::norm(sum)*(k==0||k==2048?1.:2.)/(4096*norm);}
    return power/(windows.size()*(last-first));
}
inline double windowPower(const juce::AudioBuffer<float>& data,int begin,int end,double gain=1.){double total=0;const auto windows=ends(end-begin);for(int stop:windows)for(int ch=0;ch<data.getNumChannels();++ch){double sum=0,norm=0;for(int n=0;n<4096;++n){const double w=.5-.5*std::cos(2*juce::MathConstants<double>::pi*n/4096),v=data.getSample(ch,begin+stop-4096+n)*gain;sum+=v*v*w*w;norm+=w*w;}total+=sum/norm;}return total/(windows.size()*data.getNumChannels());}
inline bool close(double actual,double ref){return std::abs(actual-ref)<=std::max(1e-10,std::abs(ref)*2e-5);}
inline bool matches(const Json& spectrum,const juce::AudioBuffer<float>& data,int begin,int end,double gain=1.){for(int k:{0,1,36,37,38,240,241,242,788,789,790,2047,2048})if(!close(spectrum["bin_power"][k],bin(data,begin,end,k,-1,gain)))return false;return std::abs(spectrum["total_power"].get<double>()-windowPower(data,begin,end,gain))<=2e-6;}
}
