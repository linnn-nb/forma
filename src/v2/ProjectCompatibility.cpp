// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "PersistentHistory.h"
#include "ProjectDocument.h"

namespace ndaw::v2
{
Json Commands::projectCompatibility() const
{
    checkThread();
    project_document::Requirements current;
    project_document::collect(edit->state, current);
    const auto retained = persistentHistory->requirements();
    auto required = retained;
    for (const auto& [id, v] : current)
        required[id] = std::max(required[id], v);
    Json rows = Json::array(), prints = Json::array(), sidechains = Json::array(), issues = Json::array();
    bool supported = true;
    for (const auto& [id, v] : required)
    {
        const auto reader = project_document::supported().find(id);
        const int available = reader == project_document::supported().end() ? 0 : reader->second;
        const int now = current.contains(id) ? current.at(id) : 0;
        rows.push_back({{"id", id},
                        {"required", v},
                        {"reader", available},
                        {"current", now},
                        {"history_only", now < v},
                        {"supported", available >= v}});
        supported = supported && available >= v;
    }
    for (auto* t : te::getAudioTracks(*edit))
    {
        const auto p = printQuery(*t);
        if (p["frozen"].get<bool>() || p["bounce_active"].get<bool>())
            prints.push_back(
                {{"track", t->itemID.toString().toStdString()}, {"name", t->getName().toStdString()}, {"state", p}});
        if (p["frozen"].get<bool>() && !p["file_available"].get<bool>())
            issues.push_back("冻结缓存缺失：" + t->getName().toStdString() + "；保留原引用，请重定位或解冻");
    }
    for (auto* p : te::getAllPlugins(*edit, true))
    {
        if (p->getSidechainSourceID().isValid())
        {
            const auto q = sidechainQuery(*p);
            sidechains.push_back(
                {{"plugin", p->itemID.toString().toStdString()}, {"name", p->getName().toStdString()}, {"state", q}});
            if (!q["source_available"].get<bool>())
                issues.push_back("旁链源轨道不可用：" + p->getName().toStdString() + "；原ID保留");
        }
        if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p); ext && !ext->getAudioPluginInstance())
            issues.push_back("外部插件未载入：" + p->getName().toStdString() + "；保留状态，冻结或印刻音频仍可能可用");
    }
    return {{"session_token", sessionToken()},
            {"revision", revision},
            {"project", projectFile()},
            {"required_capabilities", required},
            {"capabilities", rows},
            {"reader_supported", supported},
            {"history", persistentHistory->status()},
            {"prints", prints},
            {"sidechains", sidechains},
            {"issues", issues},
            {"media_hashes_checked", false},
            {"plugin_dsp_verified", false},
            {"legacy_print_audit", legacyPrintAudit},
            {"notice", "只读读取能力与引用报告；未复验媒体内容、插件DSP或实时性能。旧印刻音频不会自动重渲染。"}};
}
} // namespace ndaw::v2
