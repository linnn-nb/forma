#include "Workspace.h"
#include "RecoveryStore.h"
#include <juce_cryptography/juce_cryptography.h>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace
{
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma new-session tests"), folder(std::move(f)) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
struct Scratch
{
    juce::File path = juce::File("/tmp").getChildFile("forma-new-" + juce::Uuid().toString().substring(0, 12));
    ~Scratch()
    {
        path.deleteRecursively();
    }
};
int checks = 0;
Json metrics = Json::array();
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void fails(F f, const char* why)
{
    bool failed = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        failed = true;
    }
    check(failed, why);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
}
template <class F> Json idle(F state)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (state().value("busy", false))
    {
        if (std::chrono::steady_clock::now() > end)
            throw std::runtime_error("new-session work exceeds the fixed five-second budget");
        pump();
    }
    return state();
}
Json wait(Commands& c)
{
    return idle([&] { return c.recoveryStatus(); });
}
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", args}};
}
Json run(Commands& c, Json ops)
{
    auto p = c.makePlan("human", std::move(ops));
    c.commit(p);
    pump();
    return p;
}
Json request(Commands& c, std::string name = "Fresh session")
{
    return {{"name", std::move(name)},
            {"session_token", c.sessionToken()},
            {"base_revision", c.querySummary()["revision"]}};
}
void fixture(const juce::File& file)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    if (!writer)
        throw std::runtime_error("signal writer unavailable");
    juce::AudioBuffer<float> b(2, 144000);
    for (int c = 0; c < 2; ++c)
        for (int n = 0; n < 144000; ++n)
            b.setSample(c, n, (c == 0 ? .08f : .04f) * std::sin(2 * juce::MathConstants<double>::pi * 440 * n / 48000));
    if (!writer->writeFromAudioSampleBuffer(b, 0, 144000))
        throw std::runtime_error("signal write failed");
}
std::string render(Commands& c, const juce::File& path)
{
    auto receipt = c.render(path, 0, 144000);
    check(receipt["frames"] == 144000 && receipt["channels"] == 2 && receipt["render_ms"].get<double>() < 10000,
          "actual three-second stereo render remains within the fixed ten-second budget");
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(fm.createReaderFor(path));
    juce::AudioBuffer<float> b(2, 144000);
    if (!reader || !reader->read(&b, 0, 144000, 0, true, true))
        throw std::runtime_error("render decode failed");
    std::string pcm;
    for (int n = 0; n < 2; ++n)
        pcm.append(reinterpret_cast<const char*>(b.getReadPointer(n)), 144000 * sizeof(float));
    metrics.push_back(receipt);
    return juce::SHA256(pcm.data(), pcm.size()).toHexString().toStdString();
}
void core(const juce::File& folder)
{
    folder.createDirectory();
    auto source = folder.getChildFile("signal.wav");
    fixture(source);
    auto hash = Commands::mediaHash(source);
    Commands c(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
    c.recoveryControl("session.recovery.start", Json::object());
    wait(c);
    run(c,
        Json::array(
            {op("track.create", {{"name", "Voice"}, {"ref", "$v"}}),
             op("clip.import",
                {{"track", "$v"}, {"path", source.getFullPathName().toStdString()}, {"position_samples", 0}}),
             op("plugin.insert", {{"track", "$v"}, {"type", "4bandEq"}}),
             op("track.create", {{"name", "Verb"}, {"type", "aux"}, {"ref", "$a"}}),
             op("send.create", {{"track", "$v"}, {"target", "$a"}, {"db", -18}, {"position", "post"}}),
             op("track.create", {{"name", "MIDI"}, {"type", "midi"}, {"ref", "$m"}}),
             op("midi.clip.create", {{"track", "$m"},
                                     {"ref", "$c"},
                                     {"name", "Notes"},
                                     {"position_samples", 0},
                                     {"length_samples", 96000}}),
             op("midi.note.add",
                {{"clip", "$c"}, {"pitch", 64}, {"velocity", 90}, {"position_samples", 0}, {"length_samples", 24000}}),
             op("automation.point.add", {{"track", "$v"},
                                         {"parameter", "volume"},
                                         {"ref", "$p"},
                                         {"position_samples", 0},
                                         {"value", -6},
                                         {"curve", 0}}),
             op("tempo.set", {{"position_samples", 0}, {"bpm", 96}}),
             op("meter.set", {{"position_samples", 0}, {"numerator", 3}, {"denominator", 4}})}));
    Json ops = Json::array();
    for (int n = 3; n < 32; ++n)
        ops.push_back(op("track.create", {{"name", "Track " + std::to_string(n)}, {"ref", "$t" + std::to_string(n)}}));
    run(c, ops);
    const auto before = c.query();
    check(before["tracks"].size() == 32,
          "workload has exactly 32 actual audio/MIDI/Aux tracks, EQ, send, automation and non-default tempo/meter");
    auto original = folder.getChildFile("original.tracktionedit");
    c.save(original);
    auto originalHash = Commands::mediaHash(original);
    auto pcm = render(c, folder.getChildFile("before.wav"));
    auto stale = c.makePlan("human", Json::array({op("track.create", {{"name", "Old plan"}, {"ref", "$old"}})}));
    auto token = c.sessionToken();
    auto args = request(c, "新制作工程");
    auto started = c.recoveryControl("session.new", args);
    check(started["busy"] && c.sessionToken() == token && c.query()["tracks"] == before["tracks"],
          "local confirmation starts real disk work without clearing the running Edit early");
    auto done = wait(c);
    check(done["state"] == "created" && done["receipt_current_session"] && c.sessionToken() != token,
          "acknowledged backup precedes independent new-session receipt");
    metrics.push_back(done["receipt"]["previous_session_backup"]);
    auto fresh = c.query();
    check(fresh["tracks"].empty() && fresh["position_samples"] == 0 && !fresh["playing"].get<bool>() &&
              !fresh["can_undo"].get<bool>() && !fresh["can_redo"].get<bool>(),
          "fresh Edit has no tracks, retained monitoring, playback or invented Undo history");
    const auto music = c.querySummary()["music"];
    std::cout << Json{{"fresh_music", music}, {"master_gain_db", fresh["master_gain_db"]}}.dump() << std::endl;
    check(music["bpm"] == 120 && music["numerator"] == 4 && music["denominator"] == 4 &&
              std::abs(fresh["master_gain_db"].get<double>()) < 1e-6,
          "tempo, meter and Master reset rather than inheriting old mix");
    check(c.querySummary()["session_name"] == "新制作工程", "name is an actual saved L1 fact");
    stale["base_revision"] = fresh["revision"];
    fails([&] { c.commit(stale); }, "old direct L1 Plan is rejected even when its revision matches the new session");
    fails([&] { c.recoveryControl("session.new", args); }, "old local preview cannot create another session");
    auto injected = request(c);
    injected["actor"] = "human";
    fails([&] { c.recoveryControl("session.new", injected); },
          "injected identity cannot expand local control arguments");
    for (auto name :
         {std::string("   "), std::string("Bad\nname"), std::string(129, 'x'), std::string("hidden\0name", 11)})
        fails([&] { c.recoveryControl("session.new", request(c, name)); },
              "invalid or control-character session name is rejected before switching");
    fails([&] { c.makePlan("agent:test", Json::array({op("session.new", request(c))})); },
          "session replacement is never an external edit Plan");
    auto backup = done["receipt"]["previous_session_backup"];
    auto parsed = recovery::read(juce::File(done["directory"].get<std::string>()), backup["id"], backup["sha256"]);
    check(parsed.state.isValid() && backup["revision"] == before["revision"] &&
              backup["snapshot_capture_ms"].get<double>() < 1000,
          "previous Edit backup is actually readable and capture is within the fixed one-second budget");
    auto newFile = folder.getChildFile("fresh.tracktionedit");
    c.save(newFile);
    c.open(newFile);
    check(c.querySummary()["session_name"] == "新制作工程" && c.query()["tracks"].empty() &&
              c.recoveryStatus()["state"] == "idle",
          "fresh document saves/reopens and old created receipt is not applied to reopened session");
    c.recoveryControl("session.recovery.restore", {{"id", backup["id"]},
                                                   {"sha256", backup["sha256"]},
                                                   {"base_revision", c.querySummary()["revision"]},
                                                   {"session_token", c.sessionToken()}});
    check(wait(c)["state"] == "restored" && c.query()["tracks"] == before["tracks"],
          "prior audio/MIDI/automation/routing Edit remains recoverable after new/save/reopen");
    check(render(c, folder.getChildFile("restored.wav")) == pcm,
          "previous production render PCM is identical after restoring the new-session backup");
    token = c.sessionToken();
    c.recoveryControl("session.new", request(c));
    c.recoveryControl("session.recovery.cancel", Json::object());
    check(wait(c)["state"] == "cancelled" && c.sessionToken() == token && c.query()["tracks"] == before["tracks"],
          "cancel prevents switching and preserves actual earlier state");
    c.recoveryControl("session.new", request(c));
    run(c, Json::array({op("track.rename", {{"track", before["tracks"][0]["id"]}, {"name", "Newer human edit"}})}));
    check(wait(c)["state"] == "failed" && c.sessionToken() == token &&
              c.query()["tracks"][0]["name"] == "Newer human edit",
          "interleaved human command makes asynchronous new-session switch fail without overwriting it");
    check(Commands::mediaHash(source) == hash && Commands::mediaHash(original) == originalHash,
          "original source and saved document remain byte-identical across switching and recovery");
    auto badPrefs = folder.getChildFile("bad-prefs");
    badPrefs.createDirectory();
    badPrefs.getChildFile("Recovery").replaceWithText("existing regular file");
    Commands bad(false, std::make_unique<Storage>(badPrefs));
    bad.recoveryControl("session.recovery.start", Json::object());
    wait(bad);
    run(bad, Json::array({op("track.create", {{"name", "Keep"}, {"ref", "$keep"}})}));
    auto badToken = bad.sessionToken();
    bad.recoveryControl("session.new", request(bad));
    check(wait(bad)["state"] == "failed" && bad.sessionToken() == badToken && bad.query()["tracks"].size() == 1,
          "actual OS backup write failure cannot discard the current Edit");
}
juce::Component* find(juce::Component& p, const juce::String& id)
{
    if (!p.isVisible())
        return nullptr;
    if (p.getComponentID() == id)
        return &p;
    for (auto* child : p.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
juce::Button* button(juce::Component& p, const juce::String& id)
{
    auto* b = dynamic_cast<juce::Button*>(find(p, id));
    if (!b || !b->isEnabled())
        throw std::runtime_error("new-session button missing or disabled: " + id.toStdString());
    return b;
}
juce::Button* readyButton(ndaw::desktop::Workspace& w, const juce::String& id)
{
    // Recovery completion and the Workspace's next 50 ms UI refresh are
    // separate events. Observe the actual enabled control, without extending
    // the existing five-second async budget or calling private callbacks.
    const auto began = std::chrono::steady_clock::now();
    while (true)
    {
        if (auto* b = dynamic_cast<juce::Button*>(find(w, id)); b && b->isEnabled())
        {
            metrics.push_back(
                {{"case", "new_session_control_ready"},
                 {"control", id.toStdString()},
                 {"elapsed_ms",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count()}});
            return b;
        }
        if (std::chrono::steady_clock::now() - began > std::chrono::seconds(5))
            throw std::runtime_error("new-session UI readiness exceeds fixed five-second budget: " +
                                     w.queryRecovery().dump());
        pump();
    }
}
struct ClickDelivery final : juce::Button::Listener
{
    int delivered = 0;
    std::chrono::steady_clock::time_point at;
    void buttonClicked(juce::Button*) override
    {
        ++delivered;
        at = std::chrono::steady_clock::now();
    }
};
void click(juce::Component& p, const juce::String& id)
{
    // JUCE triggerClick posts an asynchronous message. A 30 ms pump can stop
    // after another expensive queued refresh, before this click is delivered.
    // Wait for exactly this real notification; never retry or invoke onClick.
    auto* b = button(p, id);
    ClickDelivery delivery;
    b->addListener(&delivery);
    const auto began = std::chrono::steady_clock::now();
    const auto deadline = began + std::chrono::milliseconds(500);
    b->triggerClick();
    while (delivery.delivered == 0 && std::chrono::steady_clock::now() < deadline)
        pump();
    b->removeListener(&delivery);
    if (delivery.delivered != 1 || delivery.at > deadline)
        throw std::runtime_error("native button message exceeds fixed 500 ms dispatch budget: " + id.toStdString());
    metrics.push_back({{"case", "native_button_dispatch"},
                       {"control", id.toStdString()},
                       {"delivered", delivery.delivered},
                       {"budget_ms", 500},
                       {"elapsed_ms", std::chrono::duration<double, std::milli>(delivery.at - began).count()}});
}
void ui(const juce::File& folder)
{
    ndaw::desktop::Workspace w(false, std::make_unique<Storage>(folder));
    w.setVisible(true);
    w.setSize(1120, 700);
    click(w, "track.create");
    const auto before = w.query()["tracks"];
    w.menuItemSelected(41, 0);
    idle([&] { return w.queryRecovery(); });
    check(find(w, "session.new.panel") && readyButton(w, "session.new.confirm"),
          "File New opens real versioned local preview at minimum window size");
    click(w, "session.new.cancel");
    check(!find(w, "session.new.panel") && w.query()["tracks"] == before,
          "cancelled GUI preview leaves the project and history unchanged");
    w.menuItemSelected(41, 0);
    pump();
    auto* name = dynamic_cast<juce::TextEditor*>(find(w, "session.new.name"));
    check(name != nullptr, "native new-session name is editable");
    name->setText("GUI recording session", false);
    w.startMcp(Permission::Preview, folder.getChildFile("gateway/socket"));
    const auto token = w.queryRecovery()["session_token"];
    readyButton(w, "session.new.confirm")->onClick();
    check(w.queryRecovery()["busy"] && w.queryRecovery()["session_token"] == token &&
              w.queryMcpStatus()["permission"]["mode"] == "read_only",
          "GUI local confirmation revokes external writing immediately, before backup completes");
    idle([&] { return w.queryRecovery(); });
    pump();
    check(w.queryRecovery()["state"] == "created" && w.query()["tracks"].empty() && !find(w, "session.new.panel") &&
              w.queryRecovery()["catalog"]["entries"].size() == 1,
          "actual production callbacks switch only after backup and close the preview");
    check(!w.query()["can_undo"].get<bool>() && w.queryMcpStatus()["permission"]["mode"] == "read_only",
          "old Undo and external Preview are not carried into new Edit");
    click(w, "track.create");
    if (!(w.query()["tracks"].size() == 1 && w.query()["can_undo"].get<bool>()))
    {
        auto* label = dynamic_cast<juce::Label*>(find(w, "workspace.status"));
        std::cerr << Json{{"case", "new_session_track_create_failure"},
                          {"project", w.query()},
                          {"recovery", w.queryRecovery()},
                          {"button_enabled", button(w, "track.create")->isEnabled()},
                          {"status", label ? label->getText().toStdString() : std::string("missing")}}
                         .dump()
                  << std::endl;
    }
    check(w.query()["tracks"].size() == 1 && w.query()["can_undo"],
          "ordinary native track creation works in the new session");
    click(w, "history.undo");
    check(w.query()["tracks"].empty(), "new-session human Undo operates only on its own new history");
    w.showNewSession();
    pump();
    w.setSize(1600, 1000);
    pump();
    check(find(w, "session.new.confirm")->getBounds().getWidth() > 0 &&
              find(w, "session.new.panel")->getBounds() == w.getLocalBounds(),
          "new-session preview resizes with actual workspace");
    check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !find(w, "session.new.panel"),
          "Escape cancels the local panel instead of controlling hidden transport");
    w.stopMcp();
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        Scratch scratch;
        scratch.path.createDirectory();
        core(scratch.path.getChildFile("core"));
        ui(scratch.path.getChildFile("ui"));
        Json report{{"result", "passed"},
                    {"test", "M1-NEW-01"},
                    {"checks", checks},
                    {"metrics", metrics},
                    {"scope", "actual Edit, renderer, filesystem failure, new/save/reopen/recovery and production JUCE "
                              "callbacks; desktop and microphone recording are separate qualifications"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
