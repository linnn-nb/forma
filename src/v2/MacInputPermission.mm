#include <nativedaw/v2/EngineCommands.h>
#import <AVFoundation/AVFoundation.h>
namespace ndaw::v2
{
std::string Commands::inputPermission()
{
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio])
    {
    case AVAuthorizationStatusAuthorized:
        return "authorized";
    case AVAuthorizationStatusDenied:
        return "denied";
    case AVAuthorizationStatusRestricted:
        return "restricted";
    case AVAuthorizationStatusNotDetermined:
        return "not_determined";
    }
    return "unknown";
}
void Commands::requestInputPermission(std::function<void(bool)> callback)
{
    if (inputPermission() != "not_determined")
    {
        callback(inputPermission() == "authorized");
        return;
    }
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                             completionHandler:^(BOOL granted) {
                               juce::MessageManager::callAsync([callback, granted] { callback(bool(granted)); });
                             }];
}
} // namespace ndaw::v2
