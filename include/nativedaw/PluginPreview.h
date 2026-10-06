// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
namespace ndaw {
// Text is from the actual before/candidate SDK readback, never a guessed unit
// conversion or an interpretation of the plugin's opaque internals.
inline std::string describeNativePluginPreview(const std::string& plugin,const Json& editor) {
    std::string text=plugin+"\n\nAudio is paused. Accept commits one edit; Undo restores the previous project. Keep project state rejects this candidate.\n\n";
    const auto& before=editor.at("before_parameters");const auto& after=editor.at("candidate_parameters");bool changes=false;
    for(std::size_t i=0;i<after.size();++i){const auto& a=after[i];const auto& b=before.at(i);if(a.at("id")!=b.at("id") || a.at("index")!=b.at("index"))throw Error("plugin_capture_parameters","Native preview SDK parameter identity changed");
        if(std::abs(a.at("value").get<double>()-b.at("value").get<double>())<=1e-6)continue;
        changes=true;const auto unit=a.at("unit_label").get<std::string>();
        text+=a.at("name").get<std::string>()+": "+b.at("display").get<std::string>()+(unit.empty()?"":" "+unit)+" -> "+a.at("display").get<std::string>()+(unit.empty()?"":" "+unit)+"\n";
    }
    if(!changes)text+="No exposed parameter changes.\n";
    if(editor.at("candidate_state").at("sha256")!=editor.at("before_state").at("sha256"))text+="\nThe plugin's private state also changed and is included. Its internal contents remain opaque.\n";
    return text+"\nThe actual plugin restored its prior state before this preview. Other resident instances were retained.\n";
}
}
