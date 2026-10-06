// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
#include <set>
#include <algorithm>
namespace ndaw {
// Presentation selection refers to facts, never duplicates mutable project objects.
struct EditSelection {
    std::set<std::string> tracks,clips;
    std::string focusTrack,focusClip;
    Frame begin{},end{};
    void single(std::string t,std::string c,Frame a,Frame b,bool additive=false) {
        if(!additive){tracks.clear();clips.clear();}focusTrack=std::move(t);focusClip=std::move(c);
        if(!focusTrack.empty())tracks.insert(focusTrack);if(!focusClip.empty())clips.insert(focusClip);begin=std::max<Frame>(0,a);end=std::max(begin,b);
    }
    void range(const Json& s,const std::string& first,const std::string& last,Frame a,Frame b,bool additive=false) {
        if(!additive){tracks.clear();clips.clear();}int start=-1,finish=-1,index=0;
        for(const auto& t:s.at("tracks")){if(t.at("id")==first)start=index;if(t.at("id")==last)finish=index;++index;}
        if(start<0 || finish<0)throw Error("unknown_object","Range track is no longer in the session");begin=std::max<Frame>(0,std::min(a,b));end=std::max(begin,std::max(a,b));
        for(int i=std::min(start,finish);i<=std::max(start,finish);++i){const auto& t=s.at("tracks")[i];tracks.insert(t.at("id"));for(const auto& c:activeClips(t)){Frame at=c.at("start"),length=c.at("length");if(at<end && at+length>begin)clips.insert(c.at("id"));}}
        focusTrack=last;focusClip.clear();for(const auto& t:s.at("tracks"))if(t.at("id")==last)for(const auto& c:activeClips(t))if(clips.contains(c.at("id"))){focusClip=c.at("id");break;}
    }
    void retain(const Json& s) {
        std::set<std::string> validTracks,validClips;for(const auto& t:s.at("tracks")){validTracks.insert(t.at("id"));for(const auto& c:activeClips(t))validClips.insert(c.at("id"));}
        std::erase_if(tracks,[&](const auto& id){return !validTracks.contains(id);});std::erase_if(clips,[&](const auto& id){return !validClips.contains(id);});
        // Every selected clip retains its real parent. IDs sort lexically, not
        // in timeline order, so a fallback must never borrow another row's clip.
        for(const auto& t:s.at("tracks"))for(const auto& c:activeClips(t))if(clips.contains(c.at("id")))tracks.insert(t.at("id"));
        if(!tracks.contains(focusTrack)) {
            focusTrack.clear();for(const auto& t:s.at("tracks"))if(tracks.contains(t.at("id"))){focusTrack=t.at("id");break;}
        }
        bool owned=false;std::string fallback;
        for(const auto& t:s.at("tracks"))if(t.at("id")==focusTrack)for(const auto& c:activeClips(t))if(clips.contains(c.at("id"))){if(fallback.empty())fallback=c.at("id");if(c.at("id")==focusClip)owned=true;}
        if(!owned)focusClip=std::move(fallback);
    }
    Json operation(const std::string& action) const {if(clips.empty())throw Error("selection","Select actual active clips first");return {{"command","edit_clip_selection"},{"clip_ids",clips},{"action",action}};}
};
}
