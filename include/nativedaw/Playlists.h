// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"

namespace ndaw {
bool isPlaylistCommand(const std::string&);
void applyPlaylistCommand(Json& session, Json& operation);
void addPlaylistCommands(Json& registry);
}
