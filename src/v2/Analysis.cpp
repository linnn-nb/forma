#include <nativedaw/v2/EngineCommands.h>
#include <ebur128.h>
namespace ndaw::v2 {
Json Commands::analyse(const juce::File& source) {
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(source));
    if (!r || r->numChannels<1 || r->numChannels>2) throw std::runtime_error("M0 analysis requires mono/stereo PCM");
    auto* raw=ebur128_init(r->numChannels,static_cast<unsigned long>(r->sampleRate),EBUR128_MODE_I|EBUR128_MODE_TRUE_PEAK);
    if (!raw) throw std::runtime_error("libebur128 initialisation failed");
    struct Cleanup { ebur128_state* s; ~Cleanup(){ebur128_destroy(&s);} } cleanup{raw};
    juce::AudioBuffer<float> block(r->numChannels,4096); std::vector<float> interleaved(4096*r->numChannels);
    double peak=0,sum=0;
    for (int64_t offset=0;offset<r->lengthInSamples;offset+=4096) {
        const int n=static_cast<int>(std::min<int64_t>(4096,r->lengthInSamples-offset));
        if (!r->read(&block,0,n,offset,true,true)) throw std::runtime_error("PCM read failed");
        for (int i=0;i<n;++i) for (unsigned ch=0;ch<r->numChannels;++ch) {
            const auto v=block.getSample(ch,i); if (!std::isfinite(v)) throw std::runtime_error("nonfinite audio");
            interleaved[i*r->numChannels+ch]=v; peak=std::max(peak,std::abs(double(v))); sum+=double(v)*v;
        }
        if (ebur128_add_frames_float(raw,interleaved.data(),n)!=EBUR128_SUCCESS) throw std::runtime_error("loudness analysis failed");
    }
    double lufs=-std::numeric_limits<double>::infinity(),tp=0;
    if (r->lengthInSamples>=std::ceil(r->sampleRate*.4) && ebur128_loudness_global(raw,&lufs)!=EBUR128_SUCCESS) throw std::runtime_error("integrated loudness unavailable");
    for (unsigned ch=0;ch<r->numChannels;++ch) { double p=0; if (ebur128_true_peak(raw,ch,&p)!=EBUR128_SUCCESS) throw std::runtime_error("true peak unavailable"); tp=std::max(tp,p); }
    auto db=[](double value)->Json { if (value<=0) return nullptr; return 20*std::log10(value); };
    return {{"path",source.getFullPathName().toStdString()},{"media_hash",mediaHash(source)},
        {"frames",r->lengthInSamples},{"channels",r->numChannels},{"sample_rate",r->sampleRate},{"pcm_bits",r->bitsPerSample},
        {"peak",peak},{"peak_dbfs",db(peak)},{"rms",std::sqrt(sum/(r->lengthInSamples*r->numChannels))},
        {"lufs_i",std::isfinite(lufs)?Json(lufs):Json(nullptr)},{"true_peak_dbtp",db(tp)},{"analyser","libebur128 1.2.6"},{"audio_verified",true}};
}
}
