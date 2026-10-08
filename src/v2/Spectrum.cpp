#include <nativedaw/v2/Spectrum.h>

namespace ndaw::v2::analysis {
namespace {void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}Json level(double power){return power>0?Json(10*std::log10(power)):Json(nullptr);}}
Spectrum::Spectrum(double r,int c,int64_t begin,int64_t origin,FeatureDomain d,const Control& ct):rate(r),channels(c),sourceBegin(begin),sessionBegin(origin),domain(d),control(ct){
    require(std::isfinite(rate)&&rate>=8000&&rate<=192000&&channels>=1&&channels<=2,"invalid spectrum domain");
    for(int i=0;i<size;++i){weights[size_t(i)]=float(.5-.5*std::cos(2*juce::MathConstants<double>::pi*i/size));const double w=weights[size_t(i)];windowSquare+=w*w;}
}
void Spectrum::frame(const float* samples){
    for(int c=0;c<channels;++c){require(std::isfinite(samples[c]),"nonfinite spectrum sample");ring[size_t(c)][size_t(frames%size)]=samples[c];}
    ++frames;if(frames>=size&&(frames-size)%hop==0)window();
}
void Spectrum::window(){
    require(!control.cancelled(),"spectrum cancelled or deadline expired");control.yield();require(windows<30000,"spectrum exceeds 30000 window budget");
    const auto first=frames-size;
    for(int c=0;c<channels;++c){
        std::fill(transformed.begin(),transformed.end(),0.f);
        for(int i=0;i<size;++i)transformed[size_t(i)]=ring[size_t(c)][size_t((first+i)%size)]*weights[size_t(i)];
        fft.performRealOnlyForwardTransform(transformed.data(),true);
        for(int k=0;k<bins;++k){const double re=transformed[size_t(2*k)],im=transformed[size_t(2*k+1)];const double power=(re*re+im*im)*(k==0||k==size/2?1.:2.)/(size*windowSquare);require(std::isfinite(power)&&power>=0,"spectrum FFT overflow or invalid power");powers[size_t(c)][size_t(k)].add(power);}
    }
    ++windows;lastEnd=frames;
}
Json Spectrum::finish(){
    require(!control.cancelled(),"spectrum cancelled or deadline expired");
    const bool finalWindow=frames>=size&&lastEnd!=frames;if(finalWindow)window();
    Json out={{"version","forma-spectrum/1"},{"status",windows?"measured":"insufficient_window"},{"fft_size",size},{"hop_frames",hop},{"bin_count",bins},{"bin_width_hz",rate/size},{"sample_rate",rate},{"channels",channels},{"window","periodic Hann"},{"window_square_sum",windowSquare},{"window_count",windows},{"final_end_aligned_window",finalWindow},{"frames",frames},{"decoded_start_frame",sourceBegin},{"decoded_end_frame",sourceBegin+frames},{"session_start_samples",domain==FeatureDomain::SessionSamples?Json(sessionBegin):Json(nullptr)},{"time_domain",domain==FeatureDomain::SourceFrames?"native source-file frames":"session samples at 48000 Hz"},
        {"power_rule","equal-weight mean of complete windows and channels; one-sided abs(DFT)^2/(N*sum(window^2)); factor two applies only to interior bins; DC/Nyquist undoubled"},{"frequency_rule","bin centre k*sample_rate/N; DC and Nyquist included; bands partition centres, not ideal filters"},{"coverage_rule","regular complete windows plus a distinct full window ending at range end; no padding; overlapping windows are equally weighted"},{"scope","selected-range window-weighted spectrum; not unwindowed RMS, PSD per Hz, live meter, event location or sound-quality judgement"}};
    if(!windows){out["bin_power"]=Json::array();out["bands"]=Json::array();out["total_power"]=nullptr;out["rms_dbfs"]=nullptr;out["dominant_bin"]=nullptr;return out;}
    out["bin_power"]=Json::array();Sum total;int dominant=0;
    for(int k=0;k<bins;++k){Sum mean;for(int c=0;c<channels;++c)mean.add(powers[size_t(c)][size_t(k)].value/(windows*channels));out["bin_power"].push_back(mean.value);total.add(mean.value);if(mean.value>out["bin_power"][size_t(dominant)].get<double>())dominant=k;}
    out["total_power"]=total.value;out["rms_dbfs"]=level(total.value);out["dominant_bin"]=total.value>0?Json(dominant):Json(nullptr);
    std::vector<double> edges{0};for(double edge:{80.,250.,2000.,6000.,20000.})if(edge<rate/2)edges.push_back(edge);edges.push_back(rate/2);
    out["bands"]=Json::array();int firstBin=0;
    for(size_t i=1;i<edges.size();++i){const int endBin=i+1==edges.size()?bins:int(std::ceil(edges[i]*size/rate));Sum mean;Json perChannel=Json::array();for(int c=0;c<channels;++c){Sum band;for(int k=firstBin;k<endBin;++k)band.add(powers[size_t(c)][size_t(k)].value/windows);perChannel.push_back(band.value);mean.add(band.value/channels);}
        out["bands"].push_back({{"id",i-1},{"lower_hz",edges[i-1]},{"upper_hz",edges[i]},{"upper_inclusive",i+1==edges.size()},{"first_bin",firstBin},{"end_bin",endBin},{"power",mean.value},{"rms_dbfs",level(mean.value)},{"fraction",total.value>0?Json(mean.value/total.value):Json(nullptr)},{"channel_power",std::move(perChannel)}});firstBin=endBin;
    }
    require(out.dump().size()<=64*1024,"spectrum exceeds 64 KiB budget");return out;
}
}
