#pragma once
#include <nativedaw/v2/EngineCommands.h>

namespace ndaw::v2::recovery {
constexpr int64_t maximumBytes=64*1024*1024,maximumTotalBytes=int64_t(4)*1024*1024*1024;
constexpr size_t maximumSnapshots=1024;
struct Snapshot {juce::ValueTree state;Json metadata;};
struct Loaded {juce::ValueTree state;Json metadata;};
// Detached trees and filesystem only. Never receives or reads a live Edit.
Json write(const juce::File& directory,Snapshot);
Json list(const juce::File& directory);
Loaded read(const juce::File& directory,const std::string& id,const std::string& expectedHash);
Json settings(const juce::File& directory);
void settings(const juce::File& directory,const Json&);
}
