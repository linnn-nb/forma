// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <functional>
namespace ndaw {
enum class MicrophonePermission { Granted, NotDetermined, Denied, ManagedByBackend };
MicrophonePermission microphonePermission();
void requestMicrophonePermission(std::function<void(bool)>);
}
