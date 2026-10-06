// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
namespace ndaw {
bool isCompSetCommand(const std::string&);
void applyCompSetCommand(Json&,Json&);
void addCompSetCommands(Json&);
void validateCompSets(const Json&);
void selectPlaylistLinkedAware(Json&,const std::string& trackId,const std::string& playlistId);
}
