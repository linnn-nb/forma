// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
#include <set>

namespace ndaw {
// All group resolution is control-thread work. The graph consumes actual fields only.
void validateTrackGroups(const Json&);
void validateGroupValues(const Json& track);
bool isTrackGroupCommand(const std::string&);
void applyTrackGroupCommand(Json&,Json&);
void applyGroupedTrackControl(Json&,Json&);
bool applyGroupedClipCommand(Json&,Json&); // false: existing single-object command
std::vector<std::string> resolveEditClips(const Json&,const std::vector<std::string>&,bool respect,bool linkedMove=false);
void addTrackGroupCommands(Json&);
}
