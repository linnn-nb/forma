// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectFileStore.h"
#if JUCE_WINDOWS
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
namespace ndaw::v2
{
// Shared durable storage primitive already used by plugin/media publication.
void syncFile(const fs::path&);
namespace project_files
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
fs::path path(const juce::File& f)
{
#if JUCE_WINDOWS
    return fs::path(f.getFullPathName().toWideCharPointer());
#else
    return fs::path(f.getFullPathName().toStdString());
#endif
}
void privateDirectory(const fs::path& p)
{
    const bool created = fs::create_directory(p);
    require(!fs::is_symlink(fs::symlink_status(p)) && fs::is_directory(p), "工程备份路径必须是真实目录");
#if !JUCE_WINDOWS
    if (created)
        fs::permissions(p, fs::perms::owner_all);
    require((fs::status(p).permissions() & (fs::perms::group_all | fs::perms::others_all)) == fs::perms::none,
            "工程备份目录权限过宽；请另存工程或使用仅本人可访问的备份目录");
#else
    (void)created; // Windows ACL inheritance requires separate platform qualification.
#endif
}
struct Lock
{
#if JUCE_WINDOWS
    HANDLE handle = INVALID_HANDLE_VALUE;
    explicit Lock(const fs::path& p)
    {
        handle = CreateFileW(p.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                             nullptr);
        require(handle != INVALID_HANDLE_VALUE, "另一进程正在保存此工程；请稍后重试");
    }
    ~Lock()
    {
        CloseHandle(handle);
    }
#else
    int fd = -1;
    explicit Lock(const fs::path& p)
    {
        fd = ::open(p.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd < 0 || ::flock(fd, LOCK_EX | LOCK_NB) != 0)
        {
            if (fd >= 0)
                ::close(fd);
            throw std::runtime_error("另一进程正在保存此工程，或锁文件不可用；请稍后重试");
        }
    }
    ~Lock()
    {
        ::close(fd);
    }
#endif
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
};
struct Stage
{
    juce::File directory;
    explicit Stage(const juce::File& parent) : directory(parent.getChildFile(".forma-save-" + juce::Uuid().toString()))
    {
        require(fs::create_directory(path(directory)), "无法建立独立的工程暂存目录");
#if !JUCE_WINDOWS
        fs::permissions(path(directory), fs::perms::owner_all);
#endif
    }
    ~Stage()
    {
        directory.deleteRecursively();
    }
};
void checkOriginal(const juce::File& f, const std::string& expected)
{
    require(fs::is_regular_file(fs::symlink_status(path(f))) && !f.isSymbolicLink(),
            "当前工程文件缺失、被替换为链接或不是普通文件；请重新打开或另存为");
    require(Commands::mediaHash(f) == expected, "工程文件已被外部修改；未覆盖，请重新打开或另存为");
}
} // namespace
Json replace(const juce::File& file, const std::string& expected,
             const std::function<Json(const juce::File&)>& producer)
{
    require(!expected.empty(), "没有当前工程的文件版本；请先另存为");
    checkOriginal(file, expected);
    require(file.hasWriteAccess(), "当前工程文件只读；请另存为");
    const auto parent = file.getParentDirectory();
    const auto backups = parent.getChildFile(".forma-backups");
    privateDirectory(path(backups));
    const auto canonical = fs::canonical(path(file)).generic_string();
    Lock lock(path(backups.getChildFile(juce::String(digest(canonical)) + ".lock")));
    checkOriginal(file, expected);
    Stage stage(parent);
    const auto staged = stage.directory.getChildFile("next.tracktionedit");
    auto result = producer(staged);
    checkOriginal(file, expected);
    result["path"] = file.getFullPathName().toStdString();
    result["operation"] = "save_current";
    result["reversible"] = false;
    result["backup"] = nullptr;
    if (result.at("sha256") == expected)
    {
        result["state"] = "unchanged";
        return result;
    }
    juce::MemoryBlock previous;
    require(file.loadFileAsData(previous) &&
                juce::SHA256(previous.getData(), previous.getSize()).toHexString().toStdString() == expected,
            "备份读取期间工程文件已改变；未覆盖，请重新打开或另存为");
    const auto backup = backups.getChildFile(juce::String(expected) + "-" + juce::Uuid().toString() + ".tracktionedit");
    atomicWrite(path(backup), std::string(static_cast<const char*>(previous.getData()), previous.getSize()), false);
    require(Commands::mediaHash(backup) == expected, "旧工程备份校验失败；未覆盖当前文件");
#if !JUCE_WINDOWS
    fs::permissions(path(staged), fs::status(path(file)).permissions());
#endif
    syncFile(path(staged));
    checkOriginal(file, expected); // Cooperating Forma writers hold the same nonblocking lock.
#if JUCE_WINDOWS
    require(MoveFileExW(path(staged).c_str(), path(file).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
                0,
            "工程原子发布失败；原文件与备份保留");
#else
    fs::rename(path(staged), path(file)); // Same filesystem, one atomic namespace replacement.
    const int dir = ::open(path(parent).c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    const bool synced = dir >= 0 && ::fsync(dir) == 0;
    if (dir >= 0)
        ::close(dir);
    require(synced, "工程已发布但目录同步未确认；旧版在.forma-backups，请核对后重新打开或另存");
#endif
    require(Commands::mediaHash(file) == result.at("sha256").get<std::string>(),
            "发布后的工程文件校验不一致；旧版在.forma-backups，尚未确认保存成功");
    result["state"] = "saved";
    result["backup"] = {{"path", backup.getFullPathName().toStdString()}, {"sha256", expected}};
    return result;
}
} // namespace project_files
} // namespace ndaw::v2
