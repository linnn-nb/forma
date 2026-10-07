#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include <array>

namespace ndaw::v2::analysis {
Json sourceProfileSchema();
Json sourceProfile(const Json&);
// Detached, bounded PCM features. Energy rises are candidates, not musical or
// performance judgements. All positions are native source-file frame numbers.
class SourceFeatures final {
public:
    SourceFeatures(double rate,int64_t begin,const Json& profile);
    void frame(int64_t position,double maximum,double meanSquare);
    Json finish(int64_t end);
private:
    void closeSilence(int64_t end);
    void closeBin(int64_t end);
    void event(const char*,int64_t start,int64_t end,Json evidence);
    Json profile,events=Json::array();
    std::array<double,4> preceding{};
    double silenceLimit,transientLimit,binEnergy=0,binPeak=0;
    int64_t begin,quietStart=-1,minimumSilence,hop,binStart,firstAbove=-1,lastOnset=INT64_MIN/2,refractory;
    int binFrames=0,bins=0;
    int64_t silenceCount=0,transientCount=0;
};
}
