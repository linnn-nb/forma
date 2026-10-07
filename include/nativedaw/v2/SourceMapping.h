#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2::analysis {
// Mapping is an observation of the CURRENT clip, never part of source evidence.
Json projectSourceEvent(const Json& event,const Json& mapping);
}
