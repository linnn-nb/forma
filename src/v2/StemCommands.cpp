// SPDX-License-Identifier: AGPL-3.0-only
#include "StemExport.h"
namespace ndaw::v2
{
namespace
{
void require(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
}
Json saved(juce::ValueTree meta)
{
    const auto node = meta.getChildWithName("STEM_EXPORT");
    if (!node.isValid())
        return nullptr;
    require(node.getNumChildren() == 0 && node.getNumProperties() == 3 && node["schema"].toString() == "1",
            "保存的Stems记录结构损坏");
    const auto text = node["record"].toString();
    require(text.getNumBytesAsUTF8() <= 4 * 1024 * 1024, "保存的Stems记录超过4MiB预算");
    auto r = Json::parse(text.toStdString());
    require(r.is_object() && r.value("schema", 0) == 1 &&
                r.at("batch_id") == node["batch_id"].toString().toStdString() && r.at("files").is_array() &&
                r.at("configuration").is_object(),
            "保存的Stems引用损坏");
    return r;
}
} // namespace
Json readStemExportState(juce::ValueTree meta)
{
    return saved(meta);
}
void Commands::registerStemCommands(Json& r)
{
    r.push_back({{"id", "export.stems.publish"},
                 {"schema",
                  {{"type", "object"},
                   {"required", {"batch_id"}},
                   {"properties", {{"batch_id", {{"type", "string"}}}}},
                   {"additionalProperties", false}}},
                 {"permission", "edit"},
                 {"risk", "external_write"},
                 {"live", false},
                 {"reversible", true},
                 {"undo_effect", "project configuration and export record only; generated files retained"},
                 {"tool_visibility", "local_gui"},
                 {"test", "P2-STEMS-01"},
                 {"units", {{"batch_id", "opaque L1 fully verified staged batch; local file grant required"}}}});
}
Json Commands::stemExportPreset() const
{
    checkThread();
    const auto record = saved(metadata);
    if (record.is_null())
        return {{"configuration", nullptr}, {"record", nullptr}};
    auto observed = record;
    observed["audio_verified"] = false;
    observed["verification_state"] = "saved_reference_not_reverified";
    for (auto& file : observed["files"])
        file["available"] = juce::File(juce::String(file.at("path").get<std::string>())).existsAsFile();
    return {{"configuration", record["configuration"]}, {"record", observed}};
}
Json Commands::stemExportChange(const Json& a) const
{
    require(stemExport && !edit->getTransport().isPlaying(), "先停止并核验完整Stems批次");
    const auto after = stemExport->ready(a.at("batch_id"));
    return {{"command", "export.stems.publish"},
            {"before", saved(metadata)},
            {"after", after},
            {"project_edit", "replace saved Stems configuration and last export reference"},
            {"external_files", "exclusive atomic publication of new verified batch directory"},
            {"undo_files", "retained; Undo restores project metadata only"},
            {"audio_verified", true}};
}
void Commands::executeStemExport(const Json& a, Json& objects)
{
    const auto record = stemExport->publish(a.at("batch_id"));
    const auto text = record.dump();
    require(text.size() <= 4 * 1024 * 1024, "Stems工程记录超过4MiB预算；已发布文件保留");
    auto& um = edit->getUndoManager();
    const auto old = metadata.getChildWithName("STEM_EXPORT");
    if (old.isValid())
        metadata.removeChild(old, &um);
    juce::ValueTree node("STEM_EXPORT");
    node.setProperty("schema", 1, nullptr);
    node.setProperty("batch_id", juce::String(record["batch_id"].get<std::string>()), nullptr);
    node.setProperty("record", juce::String(text), nullptr);
    metadata.addChild(node, -1, &um);
    objects.push_back({{"kind", "stem_export"},
                       {"batch_id", record["batch_id"]},
                       {"directory", record["directory"]},
                       {"files", record["files"]},
                       {"audio_verified", true},
                       {"external_files_undo", "retained"}});
}
} // namespace ndaw::v2
