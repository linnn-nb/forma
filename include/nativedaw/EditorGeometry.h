// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
#include <cmath>

namespace ndaw {
// UI coordinates are presentation state, never a replacement for sample positions.
struct EditorGeometry {
    int sampleRate=48000;
    double pixelsPerSecond=80;
    Frame gridFrames=0;
    Frame frameAt(double pixel) const {
        return std::max<Frame>(0,static_cast<Frame>(std::llround(pixel*sampleRate/pixelsPerSecond)));
    }
    double pixelAt(Frame frame) const { return static_cast<double>(frame)*pixelsPerSecond/sampleRate; }
    Frame snap(Frame frame) const {
        frame=std::max<Frame>(0,frame);
        return gridFrames>0?static_cast<Frame>(std::llround(static_cast<double>(frame)/gridFrames))*gridFrames:frame;
    }
    // Left trim moves both timeline and source boundaries; right trim preserves both.
    // Returning one batch makes the gesture a single common-domain transaction.
    static Json trim(const Json& clip,Frame boundary,bool left,Frame sourceFrames) {
        const auto start=clip.at("start").get<Frame>(),offset=clip.at("source_start").get<Frame>(),length=clip.at("length").get<Frame>();
        const auto id=clip.at("id").get<std::string>();
        if(left) {
            const auto delta=std::clamp(boundary-start,std::max(-offset,-start),length-1);
            return Json::array({{{"command","move_clip"},{"clip_id",id},{"position",start+delta}},
                {{"command","trim_clip"},{"clip_id",id},{"source_start",offset+delta},{"length",length-delta}}});
        }
        const auto next=std::clamp(boundary-start,Frame{1},sourceFrames-offset);
        return Json::array({{{"command","trim_clip"},{"clip_id",id},{"source_start",offset},{"length",next}}});
    }
};
}
