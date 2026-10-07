#pragma once
#include <nativedaw/v2/EngineCommands.h>

namespace ndaw::v2::delivery {
// Pure rules over verified measurement receipts; no Edit or file access.
// Callers cannot supply evidence through the production command API.
Json profileSchema();
Json normaliseProfile(const Json&);
Json evaluate(const Json& measurement,const Json& profile);
}
