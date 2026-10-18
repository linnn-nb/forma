// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
// Non-real-time provenance. It never invalidates or rewrites an existing print.
Json renderImplementationIdentity();
Json renderBinding(const std::string& editXml);
} // namespace ndaw::v2
