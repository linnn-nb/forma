#pragma once
#include <nativedaw/v2/EngineCommands.h>

namespace ndaw::v2 {
// Caller-generated identity for one intention, not a permission or a transport ID.
inline std::string requestKey(const Json& value) {
    if(!value.is_string())throw std::runtime_error("request_key must be a string");
    auto key=value.get<std::string>();
    if(key.empty()||key.size()>128||key.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._:-")!=std::string::npos)
        throw std::runtime_error("request_key must be 1..128 bytes of letters, digits or ._:-");
    return key;
}
inline std::string requestDigest(const Json& value) {
    const auto bytes=value.dump();
    return juce::SHA256(bytes.data(),bytes.size()).toHexString().toStdString();
}
}
