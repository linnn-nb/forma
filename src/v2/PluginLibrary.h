// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/PluginScanning.h>
// Included inside ndaw::desktop. The worker only scans; all Edit writes remain in L1.
class PluginLibrary final : public juce::Component, private juce::ListBoxModel
{
public:
    PluginLibrary(std::function<void()> refreshed, std::function<void(std::string)> preview,
                  std::function<void()> close)
        : refreshed(std::move(refreshed)), preview(std::move(preview)), close(std::move(close))
    {
        setComponentID("plugin.library");
        list.setModel(this);
        list.setRowHeight(52);
        list.setComponentID("plugin.library.list");
        for (auto* c :
             std::initializer_list<juce::Component*>{&title, &target, &search, &list, &details, &discoverButton,
                                                     &scanButton, &cancelButton, &insert, &closeButton})
            addAndMakeVisible(c);
        title.setText(text("\u63d2\u4ef6\u5e93 \u00b7 AU / VST3"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        search.setTextToShowWhenEmpty(text("\u6309\u540d\u79f0\u6216\u683c\u5f0f\u7b5b\u9009"),
                                      juce::Colour(0xff8498aa));
        search.onTextChange = [this] { filter(); };
        details.setMultiLine(true);
        details.setReadOnly(true);
        details.setFont(juce::FontOptions(13));
        discoverButton.setComponentID("plugin.library.discover");
        scanButton.setComponentID("plugin.library.scan");
        cancelButton.setComponentID("plugin.library.cancel");
        insert.setComponentID("plugin.library.preview");
        closeButton.setComponentID("plugin.library.close");
        discoverButton.onClick = [this] { discover(); };
        scanButton.onClick = [this] { scanSelected(); };
        cancelButton.onClick = [this]
        {
            cancelled->store(true);
            details.setText(
                text("\u6b63\u5728\u53d6\u6d88\u626b\u63cf \u00b7 \u7b49\u5f85\u8fdb\u7a0b\u9000\u51fa\u56de\u6267"));
        };
        closeButton.onClick = [this]
        {
            cancelled->store(true);
            this->close();
        };
        insert.onClick = [this]
        {
            auto p = selected();
            if (p.is_null() || p.value("status", std::string{}) != "verified")
                return;
            try
            {
                this->preview(p.at("descriptor"));
            }
            catch (const std::exception& e)
            {
                details.setText(text("\u672a\u751f\u6210\u8ba1\u5212\uff1a") + text(e.what()));
            }
        };
        reload();
        setSize(850, 550);
        controls();
    }
    ~PluginLibrary() override
    {
        cancelled->store(true);
        jobs.removeAllJobs(true, 2500);
        list.setModel(nullptr);
    }
    void setTarget(const std::string& name, bool allowed)
    {
        target.setText(text("\u63d2\u5165\u76ee\u6807\uff1a") + text(name), juce::dontSendNotification);
        targetAllowed = allowed;
        controls();
    }
    Json query() const
    {
        return {{"busy", busy}, {"rows", rows}, {"result", last}, {"target_allowed", targetAllowed}};
    }
    bool selectDescriptor(const std::string& id)
    {
        for (size_t i = 0; i < filtered.size(); ++i)
            if (rows[filtered[i]].value("descriptor", std::string{}) == id)
            {
                list.selectRow(int(i));
                return true;
            }
        return false;
    }
    void discover()
    {
        start([](PluginCatalog& c, std::atomic<bool>& cancel) { return c.discover(&cancel); });
    }
    void scanSelected()
    {
        auto p = selected();
        if (p.is_null())
            return;
        auto format = p.at("format").get<std::string>(), candidate = p.at("candidate").get<std::string>();
        start([format, candidate](PluginCatalog& c, std::atomic<bool>& cancel)
              { return c.scan(format, candidate, true, &cancel); });
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
        g.setColour(accent());
        g.fillRect(0, 0, getWidth(), 3);
    }
    void resized() override
    {
        title.setBounds(20, 14, getWidth() - 170, 35);
        closeButton.setBounds(getWidth() - 115, 18, 95, 30);
        target.setBounds(20, 54, getWidth() - 40, 28);
        search.setBounds(20, 94, getWidth() - 390, 30);
        discoverButton.setBounds(getWidth() - 350, 94, 105, 30);
        scanButton.setBounds(getWidth() - 237, 94, 105, 30);
        cancelButton.setBounds(getWidth() - 124, 94, 104, 30);
        int left = std::max(280, getWidth() * 3 / 5);
        list.setBounds(20, 140, left - 30, getHeight() - 205);
        details.setBounds(left + 8, 140, getWidth() - left - 28, getHeight() - 205);
        insert.setBounds(getWidth() - 270, getHeight() - 50, 250, 32);
    }

private:
    struct Job final : juce::ThreadPoolJob
    {
        using Action = std::function<Json(PluginCatalog&, std::atomic<bool>&)>;
        Job(juce::Component::SafePointer<PluginLibrary> owner, std::shared_ptr<std::atomic<bool>> cancelled,
            Action action)
            : ThreadPoolJob("isolated-plugin-scan"), owner(owner), cancelled(std::move(cancelled)),
              action(std::move(action))
        {
        }
        JobStatus runJob() override
        {
            Json result;
            try
            {
                PluginCatalog c;
                result = action(c, *cancelled);
            }
            catch (const std::exception& e)
            {
                result = {{"status", "failed"}, {"error", e.what()}};
            }
            juce::MessageManager::callAsync(
                [owner = owner, result]
                {
                    if (!owner)
                        return;
                    owner->busy = false;
                    owner->last = result;
                    if (result.value("action", std::string{}) == "discover" ||
                        result.contains("response") && result["response"].value("action", std::string{}) == "discover")
                        if (result.contains("response"))
                            owner->candidates = result["response"].value("candidates", Json::array());
                    owner->reload();
                    try
                    {
                        owner->refreshed();
                    }
                    catch (const std::exception& e)
                    {
                        owner->last["inventory_error"] = e.what();
                    }
                    owner->details.setText(text(owner->last.dump(2)));
                    owner->controls();
                });
            return jobHasFinished;
        }
        juce::Component::SafePointer<PluginLibrary> owner;
        std::shared_ptr<std::atomic<bool>> cancelled;
        Action action;
    };
    void start(Job::Action action)
    {
        if (busy)
            return;
        busy = true;
        last = {{"status", "running"}};
        cancelled->store(false);
        controls();
        details.setText(text("\u9694\u79bb\u626b\u63cf\u4e2d \u00b7 \u4e0d\u5904\u7406\u64ad\u653e\u97f3\u9891"));
        jobs.addJob(new Job(this, cancelled, std::move(action)), true);
    }
    void reload()
    {
        try
        {
            auto inventory = readPluginCatalog(defaultPluginCatalogDirectory());
            rows = Json::array();
            std::set<std::string> scanned;
            for (const auto& [_, e] : inventory["entries"].items())
            {
                std::string format = e["fingerprint"]["format"], candidate = e["fingerprint"]["candidate"];
                scanned.insert(format + ":" + candidate);
                if (e["status"] == "verified" && !e.value("blacklisted", false))
                    for (const auto& p : e["plugins"])
                        rows.push_back({{"format", format},
                                        {"candidate", candidate},
                                        {"name", p["name"]},
                                        {"status", "verified"},
                                        {"descriptor", p["id"]},
                                        {"version", p["version"]},
                                        {"manufacturer", p["manufacturer"]},
                                        {"parameter_count", p["parameters"].size()},
                                        {"latency_samples", p["reported_latency_frames"]}});
                else
                    rows.push_back({{"format", format},
                                    {"candidate", candidate},
                                    {"name", candidate},
                                    {"status", e["status"]},
                                    {"blacklisted", e.value("blacklisted", false)},
                                    {"error", e.value("error", Json(nullptr))}});
            }
            for (const auto& p : candidates)
            {
                std::string format = p["format"], candidate = p["candidate"];
                if (!scanned.contains(format + ":" + candidate))
                    rows.push_back(
                        {{"format", format}, {"candidate", candidate}, {"name", candidate}, {"status", "unscanned"}});
            }
            filter();
        }
        catch (const std::exception& e)
        {
            last = {{"status", "failed"}, {"error", e.what()}};
            details.setText(text(last.dump(2)));
        }
    }
    void filter()
    {
        filtered.clear();
        auto q = search.getText();
        for (size_t i = 0; i < rows.size(); ++i)
            if (q.isEmpty() || text(rows[i]["name"].get<std::string>()).containsIgnoreCase(q) ||
                text(rows[i]["format"].get<std::string>()).containsIgnoreCase(q))
                filtered.push_back(i);
        list.deselectAllRows();
        list.updateContent();
        list.repaint();
        controls();
    }
    Json selected() const
    {
        auto i = list.getSelectedRow();
        return i >= 0 && i < int(filtered.size()) ? rows[filtered[size_t(i)]] : Json(nullptr);
    }
    int getNumRows() override
    {
        return int(filtered.size());
    }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool on) override
    {
        if (row < 0 || row >= int(filtered.size()))
            return;
        auto p = rows[filtered[size_t(row)]];
        g.fillAll(on ? juce::Colour(0xff28494c) : juce::Colour(0xff222b35));
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(14));
        g.drawText(text(p["name"].get<std::string>()), 12, 4, w - 24, 25, juce::Justification::left);
        g.setColour(p["status"] == "verified" ? accent() : juce::Colour(0xffa6b5c6));
        g.setFont(juce::FontOptions(11));
        g.drawText(text(p["format"].get<std::string>()) + " \u00b7 " + text(p["status"].get<std::string>()) + "  " +
                       text(p.value("version", std::string{})),
                   12, 29, w - 24, 20, juce::Justification::left);
    }
    void selectedRowsChanged(int) override
    {
        auto p = selected();
        if (!p.is_null())
            details.setText(
                text(p.dump(2)) +
                text("\n\n\u53ea\u6709 verified "
                     "\u6761\u76ee\u53ef\u751f\u6210\u63d2\u5165\u8ba1\u5212\u3002\u5904\u7406\u9ed8\u8ba4\u8fdb\u7a0b"
                     "\u5185\uff1b\u626b\u63cf\u9694\u79bb\u4e0d\u7b49\u4e8e\u64ad\u653e\u6c99\u7bb1\u3002"));
        controls();
    }
    void controls()
    {
        auto p = selected();
        discoverButton.setEnabled(!busy);
        scanButton.setEnabled(!busy && !p.is_null());
        cancelButton.setEnabled(busy);
        insert.setEnabled(!busy && targetAllowed && !p.is_null() && p["status"] == "verified");
    }
    std::function<void()> refreshed, close;
    std::function<void(std::string)> preview;
    juce::ThreadPool jobs{1};
    std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
    Json rows = Json::array(), candidates = Json::array(), last = nullptr;
    std::vector<size_t> filtered;
    bool busy = false, targetAllowed = false;
    juce::Label title, target;
    juce::TextEditor search, details;
    juce::ListBox list;
    juce::TextButton discoverButton{text("\u53d1\u73b0\u63d2\u4ef6")}, scanButton{text("\u626b\u63cf / \u91cd\u626b")},
        cancelButton{text("\u505c\u6b62\u626b\u63cf")},
        insert{text("\u9884\u89c8\u63d2\u5165\u5230\u76ee\u6807\u8f68\u9053")},
        closeButton{text("\u8fd4\u56de\u5de5\u7a0b")};
};
