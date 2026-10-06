// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Plugins.h"
namespace ndaw {
// Only linked into the child. These functions instantiate third-party code.
// Never link an instance or processing wait into the device callback.
Json runPluginJob(const Json& request,const fs::path& jobDirectory);
}
