// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/PluginScanning.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace ndaw::v2
{
std::string uuid()
{
    return juce::Uuid().toString().toStdString();
}
std::string digest(const std::string& text)
{
    return juce::SHA256(text.data(), text.size()).toHexString().toStdString();
}
std::string sha256(const fs::path& path)
{
    juce::FileInputStream stream(juce::File(juce::String(path.string())));
    if (!stream.openedOk())
        throw ScanError("media_read", "Cannot read media: " + path.string());
    return juce::SHA256(stream).toHexString().toStdString();
}
Json readJson(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f)
        throw ScanError("file_read", "Cannot open " + p.string());
    try
    {
        return Json::parse(f);
    }
    catch (const std::exception&)
    {
        throw ScanError("invalid_json", "Invalid JSON: " + p.string());
    }
}
void syncFile(const fs::path& path)
{
#ifdef _WIN32
    FILE* file = _wfopen(path.c_str(), L"r+b");
    if (!file)
        throw ScanError("disk_flush", "Cannot open committed media for durability check");
    bool ok = _commit(_fileno(file)) == 0;
    std::fclose(file);
#else
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0)
        throw ScanError("disk_flush", "Cannot open staged media for durability check");
    bool ok = ::fsync(fd) == 0;
    ::close(fd);
#endif
    if (!ok)
        throw ScanError("disk_flush", "Media fsync failed");
}
void atomicWrite(const fs::path& destination, const std::string& text, bool replace)
{
    fs::create_directories(destination.parent_path());
    auto temp = destination;
    temp += "." + uuid() + ".tmp";
#ifdef _WIN32
    FILE* f = _wfopen(temp.c_str(), L"wb");
#else
    FILE* f = std::fopen(temp.c_str(), "wb");
#endif
    if (!f)
        throw ScanError("disk_write", "Cannot create staged file");
    bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size() && std::fflush(f) == 0;
#ifdef _WIN32
    ok = ok && _commit(_fileno(f)) == 0;
#else
    ok = ok && ::fsync(fileno(f)) == 0;
#endif
    ok = (std::fclose(f) == 0) && ok;
    if (!ok)
    {
        fs::remove(temp);
        throw ScanError("disk_write", "Write or flush failed");
    }
#ifdef _WIN32
    bool renamed = MoveFileExW(temp.c_str(), destination.c_str(),
                               MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0)) != 0;
#else
    bool renamed =
        replace ? (::rename(temp.c_str(), destination.c_str()) == 0) : (::link(temp.c_str(), destination.c_str()) == 0);
    if (!replace && renamed)
        ::unlink(temp.c_str());
#endif
    if (!renamed)
    {
        fs::remove(temp);
        throw ScanError("file_conflict", "Atomic commit failed; destination retained: " + destination.string());
    }
#ifndef _WIN32
    int dir = ::open(destination.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
    if (dir >= 0)
    {
        int status = ::fsync(dir);
        ::close(dir);
        if (status != 0)
            throw ScanError("disk_flush", "Directory durability could not be confirmed");
    }
#endif
}
} // namespace ndaw::v2
