// SPDX-License-Identifier: AGPL-3.0-only
#include "AppPreferences.h"
#include "TimelineState.h"
#include <filesystem>
namespace ndaw::v2
{
namespace
{
constexpr int64_t limit = 1024 * 1024;
void require(bool b, const char* s)
{
    if (!b)
        throw std::runtime_error(s);
}
std::string preferencesDigest(const std::string& s)
{
    return juce::SHA256(s.data(), s.size()).toHexString().toStdString();
}
Json defaults(bool legacy)
{
    auto folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    if (!folder.isDirectory())
        folder = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    const auto path = folder.getFullPathName().toStdString();
    return {{"media_directory", path},           {"export_directory", path},    {"recording_directory", path},
            {"scrub_insertion_follows", legacy}, {"default_track_height", 144}, {"default_grid_beats", .25}};
}
void validate(const Json& v)
{
    require(v.is_object() && v.size() == 6, "偏好字段不完整");
    for (const char* k : {"media_directory", "export_directory", "recording_directory"})
    {
        require(v.at(k).is_string(), "偏好目录必须是路径文本");
        const auto s = v.at(k).get<std::string>();
        require(s.size() <= 4096 && juce::CharPointer_UTF8::isValidString(s.data(), int(s.size())) &&
                    s.find('\0') == std::string::npos && juce::File::isAbsolutePath(juce::String(s)),
                "偏好目录必须是有效绝对路径");
    }
    require(v.at("scrub_insertion_follows").is_boolean(), "Scrub偏好必须是布尔值");
    const auto& h = v.at("default_track_height");
    require(h.is_number_integer() && (h == 96 || h == 144 || h == 216 || h == 288), "默认轨道高度无效");
    const auto& g = v.at("default_grid_beats");
    require(g.is_number() && (g == .125 || g == .25 || g == .5 || g == 1.), "默认网格无效");
}
std::string read(const juce::File& f)
{
    require(!std::filesystem::is_symlink(std::filesystem::symlink_status(f.getFullPathName().toStdString())),
            "偏好文件不能是符号链接");
    if (!f.exists())
        return {};
    require(f.existsAsFile() && f.getSize() > 0 && f.getSize() <= limit, "偏好文件损坏或超出1MiB预算");
    return f.loadFileAsString().toStdString();
}
Json unpack(const std::string& text)
{
    const auto e = Json::parse(text);
    require(e.is_object() && e.size() == 2 && e.at("sha256").is_string() && e.at("data").is_object() &&
                preferencesDigest(e.at("data").dump()) == e.at("sha256").get<std::string>(),
            "偏好校验和损坏");
    const auto d = e.at("data");
    require(d.size() == 4 && d.at("schema") == 1 && d.at("revision").is_number_unsigned() &&
                d.at("cursor").is_number_integer() && d.at("cursor") >= 0 && d.at("states").is_array() &&
                d.at("states").size() > 0 && d.at("states").size() <= 65 &&
                d.at("cursor").get<size_t>() < d.at("states").size(),
            "偏好历史结构损坏");
    for (const auto& v : d.at("states"))
        validate(v);
    return d;
}
} // namespace
AppPreferences::AppPreferences(juce::File f, bool legacy) : file(std::move(f))
{
    document = {{"schema", 1}, {"revision", uint64_t(0)}, {"cursor", 0}, {"states", Json::array({defaults(legacy)})}};
    try
    {
        const auto text = read(file);
        if (!text.empty())
            document = unpack(text);
        diskHash = preferencesDigest(text);
    }
    catch (const std::exception& e)
    {
        error = e.what();
    }
}
Json AppPreferences::view() const
{
    const auto cursor = document.at("cursor").get<size_t>();
    const auto& value = document.at("states")[cursor];
    Json availability = Json::object();
    for (const char* k : {"media_directory", "export_directory", "recording_directory"})
        availability[k] = juce::File(juce::String(value.at(k).get<std::string>())).isDirectory();
    return {{"available", error.empty()},
            {"error", error.empty() ? Json(nullptr) : Json(error)},
            {"revision", document.at("revision")},
            {"values", value},
            {"directories_available", availability},
            {"can_undo", error.empty() && cursor > 0},
            {"can_redo", error.empty() && cursor + 1 < document.at("states").size()},
            {"retained_steps", document.at("states").size() - 1},
            {"maximum_steps", 64},
            {"path", file.getFullPathName().toStdString()},
            {"history_scope", "application preferences only"}};
}
void AppPreferences::publish(Json next)
{
    require(error.empty(), "偏好原文件损坏，保留文件，未覆盖");
    juce::InterProcessLock lock("Forma.Preferences." +
                                juce::String(preferencesDigest(file.getFullPathName().toStdString())));
    require(lock.enter(0), "另一实例正在保存偏好，请重试");
    require(preferencesDigest(read(file)) == diskHash, "偏好被另一实例修改，请重启后重新查看");
    next["revision"] = document.at("revision").get<uint64_t>() + 1;
    require(next.at("revision") != 0, "偏好版本耗尽");
    const std::string encoded = Json{{"data", next}, {"sha256", preferencesDigest(next.dump())}}.dump(2);
    require(encoded.size() <= size_t(limit), "偏好历史超出1MiB预算");
    require(file.getParentDirectory().createDirectory().wasOk(), "偏好目录创建失败");
    juce::TemporaryFile stage(file);
    {
        auto stream = stage.getFile().createOutputStream();
        require(stream && stream->write(encoded.data(), encoded.size()), "偏好暂存写盘失败");
        stream->flush();
        require(stream->getStatus().wasOk(), "偏好暂存flush失败");
    }
    require(read(stage.getFile()) == encoded && preferencesDigest(read(file)) == diskHash, "偏好暂存校验或版本冲突");
    require(stage.overwriteTargetFileWithTemporary(), "偏好原子保存失败");
    document = std::move(next);
    diskHash = preferencesDigest(encoded);
}
Json AppPreferences::update(const Json& patch, uint64_t revision)
{
    require(error.empty() && document.at("revision") == revision, "偏好版本已失效或不可写");
    require(patch.is_object() && !patch.empty(), "偏好更改为空");
    auto next = document, value = view().at("values");
    for (const auto& item : patch.items())
    {
        require(value.contains(item.key()), "未知偏好字段");
        value[item.key()] = item.value();
    }
    validate(value);
    for (const char* k : {"media_directory", "export_directory", "recording_directory"})
        if (patch.contains(k))
            require(juce::File(juce::String(value.at(k).get<std::string>())).isDirectory(), "选择的偏好目录不存在");
    if (value == view().at("values"))
        return view();
    auto& states = next.at("states");
    states.erase(states.begin() + next.at("cursor").get<size_t>() + 1, states.end());
    states.push_back(value);
    if (states.size() > 65)
        states.erase(states.begin());
    next["cursor"] = states.size() - 1;
    publish(std::move(next));
    return view();
}
Json AppPreferences::history(bool redo, uint64_t revision)
{
    require(error.empty() && document.at("revision") == revision, "偏好历史版本已失效或不可写");
    require(view().at(redo ? "can_redo" : "can_undo"), "没有可用偏好历史");
    auto next = document;
    next["cursor"] = document.at("cursor").get<size_t>() + (redo ? 1 : size_t(-1));
    publish(std::move(next));
    return view();
}
Json Commands::preferences() const
{
    checkThread();
    if (!appPreferences)
        appPreferences = std::make_unique<AppPreferences>(
            engine.getPropertyStorage().getAppPrefsFolder().getChildFile("Forma-Preferences.json"),
            engine.getPropertyStorage().getPropertiesFile().getBoolValue("formaScrubInsertionFollows", false));
    return appPreferences->view();
}
Json Commands::configurePreferences(const Json& patch, uint64_t expected)
{
    checkThread();
    require(!scrubPlayback, "先停止Scrub再修改偏好");
    (void)preferences();
    return appPreferences->update(patch, expected);
}
Json Commands::preferenceHistory(bool redo, uint64_t expected)
{
    checkThread();
    require(!scrubPlayback, "先停止Scrub再恢复偏好");
    (void)preferences();
    return appPreferences->history(redo, expected);
}
void Commands::applyPreferenceDefaults(te::Edit& target)
{
    const auto value = preferences().at("values");
    auto meta = target.state.getOrCreateChildWithName("NATIVEDAW", nullptr);
    auto ui = readUiState(meta);
    ui["row_height"] = value.at("default_track_height");
    ui["grid_beats"] = value.at("default_grid_beats");
    meta.getOrCreateChildWithName("UI", nullptr).setProperty("json", juce::String(ui.dump()), nullptr);
}
} // namespace ndaw::v2
