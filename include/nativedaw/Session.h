// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <json.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <filesystem>
#include <stdexcept>

namespace ndaw {
using Json = nlohmann::json;
using Frame = std::int64_t;
namespace fs = std::filesystem;
struct Error : std::runtime_error {
    std::string code;
    Error(std::string c, std::string message) : runtime_error(std::move(message)), code(std::move(c)) {}
};
std::string uuid();
std::string sha256(const fs::path&);
std::string digest(const std::string&);
Json newSession(std::string name, int sampleRate = 48000);
std::string mainPlaylistId(const Json& trackId);
Json& playlist(Json& track, const std::string& id);
const Json& playlist(const Json& track, const std::string& id);
Json& activeClips(Json& track);
const Json& activeClips(const Json& track);
void initialisePlaylists(Json& track); // NRT schema migration/new track only
void validateSession(const Json&);
Json migrate(Json);
Json readJson(const fs::path&);
void atomicWrite(const fs::path&, const std::string&, bool replace);
void syncFile(const fs::path&);
void saveSession(const fs::path&, const Json&, bool replace = true);
Json loadSession(const fs::path&, bool recover = false);
Json inspectMedia(const fs::path&);
fs::path mediaPath(const fs::path& root, const Json& source);
Frame sessionLength(const Json&);
Json analysisForSource(const fs::path&, const Json& source, std::atomic<bool>* cancelled = nullptr);
Json mapSourceRange(const Json& session, const std::string& sourceId, Frame start, Frame end);
}
