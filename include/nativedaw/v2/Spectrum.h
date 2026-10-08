#pragma once
#include <nativedaw/v2/AudioAnalysis.h>

namespace ndaw::v2::analysis {
// One offline worker owns the bounded ring and cached FFT. No Edit access.
class Spectrum final {
public:
    static constexpr int size=4096,hop=size/2,bins=size/2+1;
    Spectrum(double rate,int channels,int64_t sourceBegin,int64_t sessionBegin,FeatureDomain,const Control&);
    void frame(const float* samples);
    Json finish();
private:
    struct Sum{double value=0,error=0;void add(double x){const double y=x-error,next=value+y;error=(next-value)-y;value=next;}};
    void window();
    double rate,windowSquare=0;int channels;int64_t sourceBegin,sessionBegin,frames=0,lastEnd=0,windows=0;FeatureDomain domain;const Control& control;
    juce::dsp::FFT fft{12};std::array<std::array<float,size>,2> ring{};
    std::array<float,size> weights{};std::array<float,2*size> transformed{};
    std::array<std::array<Sum,bins>,2> powers{};
};
}
