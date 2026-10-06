// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Permissions.h"
#import <AVFoundation/AVFoundation.h>
namespace ndaw {
MicrophonePermission microphonePermission() {
    switch([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]) {
        case AVAuthorizationStatusAuthorized:return MicrophonePermission::Granted;
        case AVAuthorizationStatusNotDetermined:return MicrophonePermission::NotDetermined;
        default:return MicrophonePermission::Denied;
    }
}
void requestMicrophonePermission(std::function<void(bool)> callback) {
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted){callback(granted==YES);}];
}
}
