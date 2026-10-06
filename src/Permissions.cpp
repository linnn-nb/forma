// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Permissions.h"
namespace ndaw {
// On other desktop backends actual open/capture errors are authoritative.
// This is deliberately distinct from asserting that OS privacy permission was granted.
MicrophonePermission microphonePermission(){return MicrophonePermission::ManagedByBackend;}
void requestMicrophonePermission(std::function<void(bool)> callback){callback(false);}
}
